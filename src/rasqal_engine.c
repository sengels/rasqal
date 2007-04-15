/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine.c - Rasqal query engine
 *
 * $Id$
 *
 * Copyright (C) 2004-2007, David Beckett http://purl.org/net/dajobe/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
 * 
 * This package is Free Software and part of Redland http://librdf.org/
 * 
 * It is licensed under the following three licenses as alternatives:
 *   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
 *   2. GNU General Public License (GPL) V2 or any newer version
 *   3. Apache License, V2.0 or any newer version
 * 
 * You may not use this file except in compliance with at least one of
 * the above three licenses.
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * complete terms and further detail along with the license texts for
 * the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
 * 
 * 
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


/* local types */

typedef enum {
  STEP_UNKNOWN,
  STEP_SEARCHING,
  STEP_GOT_MATCH,
  STEP_FINISHED,
  STEP_ERROR,
  STEP_IGNORE,

  STEP_LAST=STEP_IGNORE
} rasqal_engine_step;

#ifdef RASQAL_DEBUG
static const char * rasqal_engine_step_names[STEP_LAST+1]={
  "<unknown>",
  "searching",
  "got match",
  "finished",
  "error",
  "ignore"
};
#endif



/* local prototypes */
static void rasqal_free_engine_execution_data(rasqal_query* query, rasqal_query_results* query_results, void *data);
static rasqal_engine_step rasqal_engine_check_constraint(rasqal_query *query, rasqal_graph_pattern *gp);
static rasqal_engine_step rasqal_engine_execute_run_graph_pattern(rasqal_query_results* query_results, rasqal_graph_pattern* gp);
static rasqal_engine_step rasqal_group_graph_pattern_execute_bind(rasqal_query_results* query_results, rasqal_graph_pattern* gp);
static int rasqal_engine_graph_pattern_init(rasqal_query_results* query_results, rasqal_graph_pattern *gp);



int
rasqal_engine_expand_triple_qnames(rasqal_query* rq)
{
  int i;

  if(!rq->triples)
    return 0;
  
  /* expand qnames in triples */
  for(i=0; i< raptor_sequence_size(rq->triples); i++) {
    rasqal_triple* t=(rasqal_triple*)raptor_sequence_get_at(rq->triples, i);
    if(rasqal_literal_expand_qname(rq, t->subject) ||
       rasqal_literal_expand_qname(rq, t->predicate) ||
       rasqal_literal_expand_qname(rq, t->object))
      return 1;
  }

  return 0;
}


int
rasqal_engine_sequence_has_qname(raptor_sequence *seq)
{
  int i;

  if(!seq)
    return 0;
  
  /* expand qnames in triples */
  for(i=0; i< raptor_sequence_size(seq); i++) {
    rasqal_triple* t=(rasqal_triple*)raptor_sequence_get_at(seq, i);
    if(rasqal_literal_has_qname(t->subject) ||
       rasqal_literal_has_qname(t->predicate) ||
       rasqal_literal_has_qname(t->object))
      return 1;
  }

  return 0;
}


int
rasqal_engine_query_constraints_has_qname(rasqal_query* rq) 
{
  if(!rq->query_graph_pattern)
    return 0;
  
  return rasqal_engine_graph_pattern_constraints_has_qname(rq->query_graph_pattern);
}


int
rasqal_engine_graph_pattern_constraints_has_qname(rasqal_graph_pattern* gp) 
{
  int i;
  
  /* check for qnames in sub graph patterns */
  if(gp->graph_patterns) {
    /* check for constraint qnames in rasqal_graph_patterns */
    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_engine_graph_pattern_constraints_has_qname(sgp))
        return 1;
    }
  }

  if(!gp->constraints)
    return 0;
  
  /* check for qnames in constraint expressions */
  for(i=0; i<raptor_sequence_size(gp->constraints); i++) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(gp->constraints, i);
    if(rasqal_expression_visit(e, rasqal_expression_has_qname, gp))
      return 1;
  }

  return 0;
}


int
rasqal_engine_expand_graph_pattern_constraints_qnames(rasqal_query *rq,
                                                      rasqal_graph_pattern* gp)
{
  int i;
  
  /* expand qnames in sub graph patterns */
  if(gp->graph_patterns) {
    /* check for constraint qnames in rasqal_graph_patterns */
    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_engine_expand_graph_pattern_constraints_qnames(rq, sgp))
        return 1;
    }
  }

  if(!gp->constraints)
    return 0;
  
  /* expand qnames in constraint expressions */
  for(i=0; i<raptor_sequence_size(gp->constraints); i++) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(gp->constraints, i);
    if(rasqal_expression_visit(e, rasqal_expression_expand_qname, rq))
      return 1;
  }

  return 0;
}


int
rasqal_engine_expand_query_constraints_qnames(rasqal_query *rq) 
{
  return rasqal_engine_expand_graph_pattern_constraints_qnames(rq, 
                                                               rq->query_graph_pattern);
}


int
rasqal_engine_build_constraints_expression(rasqal_graph_pattern* gp)
{
  rasqal_expression* newe=NULL;
  int i;

  if(!gp)
    return 1;
  
  /* build constraints in sub graph patterns */
  if(gp->graph_patterns) {

    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_engine_build_constraints_expression(sgp))
        return 1;
    }
  }

  if(!gp->constraints)
    return 0;
  
  for(i=raptor_sequence_size(gp->constraints)-1; i>=0 ; i--) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(gp->constraints, i);
    e=rasqal_new_expression_from_expression(e);
    if(!newe)
      newe=e;
    else
      /* must make a conjunction */
      newe=rasqal_new_2op_expression(RASQAL_EXPR_AND, e, newe);
  }
  gp->constraints_expression=newe;

  return 0;
}


static void
rasqal_engine_convert_blank_node_to_anonymous_variable(rasqal_query *rq,
                                                       rasqal_literal *l) {
  rasqal_variable* v;
  
  v=rasqal_new_variable_typed(rq, 
                              RASQAL_VARIABLE_TYPE_ANONYMOUS,
                              (unsigned char*)l->string, NULL);

  /* Convert the blank node literal into a variable literal */
  l->string=NULL;

  l->type=RASQAL_LITERAL_VARIABLE;
  l->value.variable=v;
}


static int
rasqal_engine_build_anonymous_variables(rasqal_query* rq)
{
  int i;
  raptor_sequence *s=rq->triples;
  
  for(i=0; i < raptor_sequence_size(s); i++) {
    rasqal_triple* t=(rasqal_triple*)raptor_sequence_get_at(s, i);
    if(t->subject->type == RASQAL_LITERAL_BLANK)
      rasqal_engine_convert_blank_node_to_anonymous_variable(rq, t->subject);
    if(t->predicate->type == RASQAL_LITERAL_BLANK)
      rasqal_engine_convert_blank_node_to_anonymous_variable(rq, t->predicate);
    if(t->object->type == RASQAL_LITERAL_BLANK)
      rasqal_engine_convert_blank_node_to_anonymous_variable(rq, t->object);
  }

  return 0;
}


static int
rasqal_engine_expand_wildcards(rasqal_query* rq)
{
  int i;
  raptor_sequence *s;
  int rc=0;

  if(!rq->wildcard)
    return rc;
  
  switch(rq->verb) {
    case  RASQAL_QUERY_VERB_SELECT:
    /* If 'SELECT *' was given, make the selects be a list of all variables */
      rq->selects=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
      
      for(i=0; i< rq->variables_count; i++)
        raptor_sequence_push(rq->selects, raptor_sequence_get_at(rq->variables_sequence, i));
      break;
      
    case RASQAL_QUERY_VERB_CONSTRUCT:
      /* If 'CONSTRUCT *' was given, make the constructs be all triples */
      rq->constructs=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
      s=((rasqal_query*)rq)->triples;
      
      for(i=0; i < raptor_sequence_size(s); i++) {
        rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(s, i);
        raptor_sequence_push(rq->constructs, rasqal_new_triple_from_triple(t));
      }
      break;

    case RASQAL_QUERY_VERB_UNKNOWN:
    case RASQAL_QUERY_VERB_DESCRIBE:
    case RASQAL_QUERY_VERB_ASK:
    case RASQAL_QUERY_VERB_DELETE:
    case RASQAL_QUERY_VERB_INSERT:
    default:
      rasqal_query_error(rq, "Cannot use wildcard * with query verb %s",
                         rasqal_query_verb_as_string(rq->verb));
      rc=1;
      break;
  }

  return rc;
}


static int
rasqal_select_NULL_last_compare(const void *a, const void *b)
{
  rasqal_variable *var_a=*(rasqal_variable**)a;
  rasqal_variable *var_b=*(rasqal_variable**)b;

  /* Put NULLs last */
  if(!var_a || !var_b) {
    if(!var_a && !var_b)
      return (unsigned long)b - (unsigned long)a;
    else
      return var_a ? -1 : 1;
  }
  return var_b - var_a;
}


int
rasqal_engine_assign_variables(rasqal_query* rq)
{
  int i;
  int offset;
  int size=0;
  raptor_sequence* var_sequence=NULL;
  
  if(rq->constructs)
    var_sequence=rq->variables_sequence;
  else if(rq->selects)
    var_sequence=rq->selects;
  
  if(var_sequence)
    size=raptor_sequence_size(var_sequence);

  if(var_sequence) {
    int modified=0;

    for(i=0; i < size; i++) {
      int j;
      rasqal_variable *v=(rasqal_variable*)raptor_sequence_get_at(var_sequence, i);
      int warned=0;
      if(!v)
        continue;
      
      for(j=0; j < size; j++) {
        rasqal_variable *v2=(rasqal_variable*)raptor_sequence_get_at(var_sequence, j);
        if(j == i)
          continue;
        
        if(v == v2) {
          if(!warned) {
            rasqal_query_warning(rq, "Variable %s duplicated in SELECT.", 
                                 v->name);
            warned=1;
          }
          raptor_sequence_set_at(var_sequence, j, NULL);
          modified=1;
        }
      }
    }

    if(modified) {
      /* Delete NULLs - sort to put NULLs last */
      raptor_sequence_sort(var_sequence, rasqal_select_NULL_last_compare);
      do {
      /* and pop them from the end until they are gone */
        raptor_sequence_pop(var_sequence);
        size=raptor_sequence_size(var_sequence);
      } while(!raptor_sequence_get_at(var_sequence, size-1));
    }
  }

  if(rq->selects)
    rq->select_variables_count=size;

  if(size) {
    rq->variable_names=(const unsigned char**)RASQAL_MALLOC(cstrings,sizeof(const unsigned char*)*(size+1));
    rq->binding_values=(rasqal_literal**)RASQAL_MALLOC(rasqal_literals,sizeof(rasqal_literal*)*(size+1));
  }
  
  rq->variables=(rasqal_variable**)RASQAL_MALLOC(varrary, sizeof(rasqal_variable*)*(rq->variables_count + rq->anon_variables_count));
  rq->variables_declared_in=(int*)RASQAL_CALLOC(intarray, rq->variables_count + rq->anon_variables_count + 1, sizeof(int));

  offset=0;
  for(i=0; i< rq->variables_count; i++) {
    rq->variables_declared_in[offset]= -1;
    rq->variables[offset]=(rasqal_variable*)raptor_sequence_get_at(rq->variables_sequence, i);
    if(i < size)
      rq->variable_names[offset]=rq->variables[offset]->name;
    offset++;
  }

  for(i=0; i< rq->anon_variables_count; i++) {
    rq->variables_declared_in[offset]= -1;
    rq->variables[offset]=(rasqal_variable*)raptor_sequence_get_at(rq->anon_variables_sequence, i);
    /* only now can we make this offset absolute into the full list of vars */
    rq->variables[offset]->offset += rq->variables_count;
    offset++;
  }

  if(rq->variable_names) {
    rq->variable_names[size]=NULL;
    rq->binding_values[size]=NULL;
  }
  return 0;
}
  

/* static */
static rasqal_triples_source_factory Triples_Source_Factory;


/**
 * rasqal_set_triples_source_factory:
 * @register_fn: registration function
 * @user_data: user data for registration
 *
 * Register the factory to return triple sources.
 * 
 * Registers the factory that returns triples sources.  Note that
 * there is only one of these per runtime. 
 *
 * The rasqal_triples_source_factory factory method new_triples_source is
 * called with the user data for some query and rasqal_triples_source.
 * 
 **/
void
rasqal_set_triples_source_factory(void (*register_fn)(rasqal_triples_source_factory *factory), void* user_data) {
  Triples_Source_Factory.user_data=user_data;
  register_fn(&Triples_Source_Factory);
}


rasqal_triples_source*
rasqal_new_triples_source(rasqal_query_results* query_results)
{
  rasqal_query* query=query_results->query;
  rasqal_triples_source* rts;
  int rc=0;
  
  rts=(rasqal_triples_source*)RASQAL_CALLOC(rasqal_triples_source, 1, sizeof(rasqal_triples_source));
  if(!rts)
    return NULL;

  rts->user_data=RASQAL_CALLOC(user_data, 1,
                               Triples_Source_Factory.user_data_size);
  if(!rts->user_data) {
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  rts->query=query;

  rc=Triples_Source_Factory.new_triples_source(query, 
                                               Triples_Source_Factory.user_data,
                                               rts->user_data, rts);
  if(rc) {
    query_results->failed=1;
    if(rc > 0) {
      rasqal_query_error(query, "Failed to make triples source.");
    } else {
      rasqal_query_error(query, "No data to query.");
    }
    RASQAL_FREE(user_data, rts->user_data);
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  
  return rts;
}


void
rasqal_free_triples_source(rasqal_triples_source *rts)
{
  if(rts->user_data) {
    rts->free_triples_source(rts->user_data);
    RASQAL_FREE(user_data, rts->user_data);
    rts->user_data=NULL;
  }
  
  RASQAL_FREE(rasqal_triples_source, rts);
}


static int
rasqal_triples_source_triple_present(rasqal_triples_source *rts,
                                     rasqal_triple *t)
{
  return rts->triple_present(rts, rts->user_data, t);
}


static rasqal_triples_match*
rasqal_new_triples_match(rasqal_query_results* query_results, void *user_data,
                         rasqal_triple_meta *m, rasqal_triple *t)
{
  rasqal_triples_match* rtm;

  if(!query_results->triples_source)
    return NULL;

  rtm=(rasqal_triples_match *)RASQAL_CALLOC(rasqal_triples_match, 1,
                                            sizeof(rasqal_triples_match));
  if(query_results->triples_source->init_triples_match(rtm,
                                               query_results->triples_source,
                                               query_results->triples_source->user_data,
                                               m, t)) {
    RASQAL_FREE(rasqal_triples_match, rtm);
    rtm=NULL;
  }

  return rtm;
}


static void
rasqal_free_triples_match(rasqal_triples_match* rtm)
{
  rtm->finish(rtm, rtm->user_data);
  RASQAL_FREE(rasqal_triples_match, rtm);
}


/* methods */
static int
rasqal_triples_match_bind_match(struct rasqal_triples_match_s* rtm, 
                                rasqal_variable *bindings[4],
                                rasqal_triple_parts parts)
{
  return rtm->bind_match(rtm,  rtm->user_data, bindings, parts);
}


static void
rasqal_triples_match_next_match(struct rasqal_triples_match_s* rtm)
{
  rtm->next_match(rtm,  rtm->user_data);
}


static int
rasqal_triples_match_is_end(struct rasqal_triples_match_s* rtm)
{
  return rtm->is_end(rtm,  rtm->user_data);
}


/*
 * rasqal_reset_triple_meta:
 * @m: Triple pattern metadata
 * 
 * INTERNAL - reset the metadata associated with a triple pattern
 * 
 * Return value: number of parts of the triple that were reset (0..4)
 **/
int
rasqal_reset_triple_meta(rasqal_triple_meta* m)
{
  int resets=0;
  
  if(m->triples_match) {
    rasqal_free_triples_match(m->triples_match);
    m->triples_match=NULL;
  }

  if(m->bindings[0] && (m->parts & RASQAL_TRIPLE_SUBJECT)) {
    rasqal_variable_set_value(m->bindings[0],  NULL);
    resets++;
  }
  if(m->bindings[1] && (m->parts & RASQAL_TRIPLE_PREDICATE)) {
    rasqal_variable_set_value(m->bindings[1],  NULL);
    resets++;
  }
  if(m->bindings[2] && (m->parts & RASQAL_TRIPLE_OBJECT)) {
    rasqal_variable_set_value(m->bindings[2],  NULL);
    resets++;
  }
  if(m->bindings[3] && (m->parts & RASQAL_TRIPLE_ORIGIN)) {
    rasqal_variable_set_value(m->bindings[3],  NULL);
    resets++;
  }

  m->executed=0;
  
  return resets;
}


typedef struct {
  int (*execute_init)(rasqal_query_results* query_results, rasqal_graph_pattern* gp);
  rasqal_engine_step (*execute_bind)(rasqal_query_results* query_results, rasqal_graph_pattern* gp);
  int (*execute_finish)(rasqal_query_results* query_results, rasqal_graph_pattern* gp);
} rasqal_engine_execute_gp_handler;


typedef struct {
  rasqal_graph_pattern* gp;
  
  /* An array of items, one per triple in the pattern graph */
  rasqal_triple_meta* triple_meta;

  /* Executing column in the current graph pattern */
  int column;

  /* current position in the sequence */
  int current_graph_pattern;

  /* current start position in the sequence */
  int start_graph_pattern;

  /* Functions to use to execute the query against this graph pattern */
  rasqal_engine_execute_gp_handler* handler;

  /* true when this graph pattern matched last time */
  int matched;

  /* true when an optional graph pattern finished last time round */
  int finished;

  /* count of variable bindings in this GP */
  int matches;
} rasqal_engine_gp_data;


static rasqal_engine_gp_data*
rasqal_new_engine_gp_data(rasqal_graph_pattern* gp) 
{
  rasqal_engine_gp_data* gp_data;
  
  gp_data=(rasqal_engine_gp_data*)RASQAL_CALLOC(rasqal_engine_gp_data, 1, sizeof(rasqal_engine_gp_data));
  if(!gp_data)
    return NULL;
  
  gp_data->gp=gp;
  
  gp_data->column= -1;

  return gp_data;
}


static void
rasqal_free_gp_data(rasqal_engine_gp_data* gp_data)
{
  rasqal_graph_pattern* gp=gp_data->gp;

  if(gp_data->triple_meta) {
    while(gp_data->column >= gp->start_column) {
      rasqal_triple_meta *m;
      m=&gp_data->triple_meta[gp_data->column - gp->start_column];
      rasqal_reset_triple_meta(m);
      gp_data->column--;
    }
    RASQAL_FREE(rasqal_triple_meta, gp_data->triple_meta);
    gp_data->triple_meta=NULL;
  }

  RASQAL_FREE(rasqal_engine_gp_data, gp_data);
  return;
}


static RASQAL_INLINE void
rasqal_query_graph_pattern_build_declared_in_variable(rasqal_query* query,
                                                      rasqal_variable *v,
                                                      int col)
{
  if(!v)
    return;
  
  if(query->variables_declared_in[v->offset] < 0)
    query->variables_declared_in[v->offset]=col;
}


/*
 * rasqal_query_graph_pattern_build_declared_in:
 * @query; the #rasqal_query to find the variables in
 *
 * Mark where variables are first declared in a graph_pattern.
 * 
 **/
static void
rasqal_query_graph_pattern_build_declared_in(rasqal_query* query,
                                             rasqal_graph_pattern *gp)
{
  int col;
      
  if(gp->graph_patterns) {
    int i;

    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      rasqal_query_graph_pattern_build_declared_in(query, sgp);
    }
  }

  if(!gp->triples)
    return;
    
  for(col=gp->start_column; col <= gp->end_column; col++) {
    rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, col);

    rasqal_query_graph_pattern_build_declared_in_variable(query,
                                                          rasqal_literal_as_variable(t->subject),
                                                          col);
    rasqal_query_graph_pattern_build_declared_in_variable(query,
                                                          rasqal_literal_as_variable(t->predicate),
                                                          col);
    rasqal_query_graph_pattern_build_declared_in_variable(query,
                                                          rasqal_literal_as_variable(t->object),
                                                          col);
    if(t->origin)
      rasqal_query_graph_pattern_build_declared_in_variable(query,
                                                            rasqal_literal_as_variable(t->origin),
                                                            col);
  }
  
}


/*
 * rasqal_query_build_declared_in:
 * @query; the #rasqal_query to find the variables in
 *
 * Mark where variables are first declared.
 * 
 **/
static void
rasqal_query_build_declared_in(rasqal_query* query) 
{
  int i;
  rasqal_graph_pattern *gp=query->query_graph_pattern;

  if(!gp)
    return;
  
  rasqal_query_graph_pattern_build_declared_in(query, gp);

  for(i=0; i< query->variables_count; i++) {
    rasqal_variable *v=query->variables[i];
    int column=query->variables_declared_in[i];

    if(column >= 0) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG4("Variable %s (%d) was declared in column %d\n",
                    v->name, i, column);
#endif
    } else if(!v->expression)
      rasqal_query_warning(query, 
                           "Variable %s was selected but is unused in the query.", 
                           v->name);
  }


}


/*
 * rasqal_engine_triple_graph_pattern_get_next_match:
 * @query_results:
 * @gp: 
 *
 * Get the next match in a triple graph pattern
 *
 * return: <0 failure, 0 end of results, >0 match
 */
static int
rasqal_engine_triple_graph_pattern_get_next_match(rasqal_query_results* query_results,
                                                  rasqal_graph_pattern* gp,
                                                  int* matches_p) 
{
  rasqal_query* query=query_results->query;
  int rc=0;
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_execution_data* execution_data=NULL;
  int matches=0;
  
  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);

  while(gp_data->column >= gp->start_column) {
    rasqal_triple_meta *m;
    rasqal_triple *t;

    m=&gp_data->triple_meta[gp_data->column - gp->start_column];
    t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, gp_data->column);

    rc=1;

    if(!m) {
      /* error recovery - no match */
      gp_data->column--;
      rc= -1;
      break;
    }
    
    if(m->executed) {
      RASQAL_DEBUG2("triplesMatch already executed in column %d\n", 
                    gp_data->column);
      gp_data->column--;
      continue;
    }
      
    if (m->is_exact) {
      /* exact triple match wanted */

      if(!rasqal_triples_source_triple_present(query_results->triples_source, t)) {
        /* failed */
        RASQAL_DEBUG2("exact match failed for column %d\n", gp_data->column);
        gp_data->column--;
      } else
        RASQAL_DEBUG2("exact match OK for column %d\n", gp_data->column);

      RASQAL_DEBUG2("end of exact triplesMatch for column %d\n", 
                    gp_data->column);
      m->executed=1;
      
    } else {
      /* triple pattern match wanted */
      int parts;

      if(!m->triples_match) {
        /* Column has no triplesMatch so create a new query */
        m->triples_match=rasqal_new_triples_match(query_results, m, m, t);
        if(!m->triples_match) {
          rasqal_query_error(query,
                             "Failed to make a triple match for column%d",
                             gp_data->column);
          /* failed to match */
          gp_data->column--;
          rc= -1;
          break;
        }
        RASQAL_DEBUG2("made new triplesMatch for column %d\n", gp_data->column);
      }


      if(rasqal_triples_match_is_end(m->triples_match)) {
        int resets=0;

        RASQAL_DEBUG2("end of pattern triplesMatch for column %d\n",
                      gp_data->column);
        m->executed=1;

        resets=rasqal_reset_triple_meta(m);
        matches -= resets;
        if(matches < 0)
          matches=0;

        gp_data->column--;
        continue;
      }

      if(m->parts) {
        parts=rasqal_triples_match_bind_match(m->triples_match, m->bindings,
                                              m->parts);
        RASQAL_DEBUG3("bind_match for column %d returned parts %d\n",
                      gp_data->column, parts);
        if(!parts)
          rc=0;
        if(parts & RASQAL_TRIPLE_SUBJECT)
          matches++;
        if(parts & RASQAL_TRIPLE_PREDICATE)
          matches++;
        if(parts & RASQAL_TRIPLE_OBJECT)
          matches++;
        if(parts & RASQAL_TRIPLE_ORIGIN)
          matches++;
      } else {
        RASQAL_DEBUG2("Nothing to bind_match for column %d\n", gp_data->column);
      }

      rasqal_triples_match_next_match(m->triples_match);
      if(!rc)
        continue;

    }
    
    if(gp_data->column == gp->end_column) {
      /* Done all conjunctions */ 
      
      /* exact match, so column must have ended */
      if(m->is_exact)
        gp_data->column--;

      /* return with result (rc is 1) */
      rc=1;
      goto done;
    } else if (gp_data->column >= gp->start_column)
      gp_data->column++;

  } /* end while gp_data->column >= gp->start_column */


  if(gp_data->column < gp->start_column)
    rc=0;
  
  done:
  if(rc > 0)
    *matches_p=matches;

  return rc;
}



/*
 * rasqal_engine_prepare:
 *
 * INTERNAL - initialise the remainder of the query structures
 *
 * Does not do any execution prepration - this is once-only stuff.
 *
 * Return value: non-0 on failure
 */
int
rasqal_engine_prepare(rasqal_query *query)
{
  if(!query->triples)
    return 1;
  
  if(!query->variables) {

    rasqal_engine_build_anonymous_variables(query);

    /* Expand 'SELECT *' and 'CONSTRUCT *' */
    rasqal_engine_expand_wildcards(query);

    /* create the query->variables array */
    rasqal_engine_assign_variables(query);

    rasqal_query_build_declared_in(query);
    
    rasqal_engine_query_fold_expressions(query);
  }

  return 0;
}


static int
rasqal_basic_graph_pattern_execute_init(rasqal_query_results* query_results,
                                        rasqal_graph_pattern* gp)
{
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_execution_data* execution_data=NULL;

  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);
  
  if(gp->triples) {
    int triples_count=gp->end_column - gp->start_column+1;
    
    gp_data->column=gp->start_column;
    if(gp_data->triple_meta) {
      /* reset any previous execution */
      rasqal_reset_triple_meta(gp_data->triple_meta);
      memset(gp_data->triple_meta, '\0', sizeof(rasqal_triple_meta)*triples_count);
    } else
      gp_data->triple_meta=(rasqal_triple_meta*)RASQAL_CALLOC(rasqal_triple_meta, triples_count, sizeof(rasqal_triple_meta));

  }

  return 0;
}


static rasqal_engine_step
rasqal_basic_graph_pattern_execute_bind(rasqal_query_results* query_results,
                                        rasqal_graph_pattern* gp)
{
  rasqal_query* query=query_results->query;
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_execution_data* execution_data=NULL;
  rasqal_engine_step step=STEP_GOT_MATCH;
  
  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);

  gp_data->matches=0;
  
  while(1) {
    int rc;
    int tp_matches=0;
    
    /*  return: <0 failure, 0 end of results, >0 match */
    rc=rasqal_engine_triple_graph_pattern_get_next_match(query_results, gp,
                                                         &tp_matches);
    
    RASQAL_DEBUG4("Triple pattern match in graph pattern #%d returned %d and %d matches\n",
                  gp->gp_index, rc, tp_matches);
  
    gp_data->matches += tp_matches;

    if(rc <= 0) {
      step=(rc < 0) ? STEP_ERROR : STEP_FINISHED;
      break;
    }

    if(gp->constraints_expression) {
      step=rasqal_engine_check_constraint(query, gp);
      if(step != STEP_GOT_MATCH)
        continue;
    }

    step=STEP_GOT_MATCH;
    break;
  }
  
  return step;
}


static int
rasqal_optional_graph_pattern_execute_init(rasqal_query_results* query_results,
                                           rasqal_graph_pattern* gp)
{
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_execution_data* execution_data=NULL;

  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);
  
  if(gp->triples)
    rasqal_basic_graph_pattern_execute_init(query_results, gp);

  if(gp->graph_patterns) {
    gp_data->start_graph_pattern= 0;
    gp_data->current_graph_pattern= -1;
  }

  return 0;
}


static rasqal_engine_step
rasqal_optional_graph_pattern_execute_bind(rasqal_query_results* query_results,
                                           rasqal_graph_pattern* gp)
{
  rasqal_engine_execution_data* execution_data=NULL;
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_step step=STEP_IGNORE;
  
  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);

  if(gp_data->finished)
    return STEP_IGNORE;
  
  if(gp->triples)
    step=rasqal_basic_graph_pattern_execute_bind(query_results, gp);
  
  if(gp->graph_patterns && (step == STEP_GOT_MATCH || step == STEP_IGNORE))
    step=rasqal_group_graph_pattern_execute_bind(query_results, gp);

  if(step == STEP_FINISHED) {
    gp_data->finished=1;
    step=STEP_IGNORE;
  }
  return step;
}


static int
rasqal_group_graph_pattern_execute_init(rasqal_query_results* query_results,
                                        rasqal_graph_pattern* gp)
{
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_execution_data* execution_data=NULL;

  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);

  if(gp->graph_patterns) {
    gp_data->start_graph_pattern= 0;
    gp_data->current_graph_pattern= -1;
  }
  return 0;
}


static rasqal_engine_step
rasqal_group_graph_pattern_execute_bind(rasqal_query_results* query_results,
                                        rasqal_graph_pattern* gp)
{
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_execution_data* execution_data=NULL;
  int graph_patterns_size=raptor_sequence_size(gp->graph_patterns);
  rasqal_engine_step step=STEP_GOT_MATCH;
  int reset=0;
  
  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);

  gp_data->matches=0;
  
  RASQAL_DEBUG5("Executing group graph pattern #%d of size %d, start=%d current=%d\n",
                gp->gp_index, graph_patterns_size, 
                gp_data->start_graph_pattern,
                gp_data->current_graph_pattern);

  if(!graph_patterns_size)
    return STEP_IGNORE;

  if(gp_data->current_graph_pattern < 0) {
    RASQAL_DEBUG2("Resetting group graph pattern #%d to GP 0\n", gp->gp_index);
    gp_data->start_graph_pattern=0;
    gp_data->current_graph_pattern=0;
    reset=1;
  }
  
  if(gp_data->finished)
    return STEP_FINISHED;

  while(gp_data->current_graph_pattern < graph_patterns_size) {
    rasqal_graph_pattern *sgp;
    rasqal_engine_gp_data* sgp_data;
    sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 
                                                      gp_data->current_graph_pattern);
    sgp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                            sgp->gp_index);

    RASQAL_DEBUG5("Group graph pattern #%d sub-graph pattern %d) #%d (reset %d) executing\n", 
                  gp->gp_index, gp_data->current_graph_pattern,
                  sgp->gp_index, reset);
    if(reset) {
      rasqal_engine_graph_pattern_init(query_results, sgp);
      reset=0;
    }

    step=rasqal_engine_execute_run_graph_pattern(query_results, sgp);
    RASQAL_DEBUG6("Group graph pattern #%d sub-graph pattern %d) #%d returned %s and %d matches\n", 
                  gp->gp_index, gp_data->current_graph_pattern,
                  sgp->gp_index, rasqal_engine_step_names[step],
                  sgp_data->matches);
    if(step == STEP_ERROR)
      break;

    gp_data->matches += sgp_data->matches;
    
    if(step == STEP_FINISHED ||
       (step == STEP_IGNORE && 
        gp_data->current_graph_pattern == graph_patterns_size-1)) {
      if(gp_data->current_graph_pattern == gp_data->start_graph_pattern) {
        /* move start onwards, go back and reset */
        gp_data->start_graph_pattern++;
        gp_data->current_graph_pattern=gp_data->start_graph_pattern;
        RASQAL_DEBUG3("Moving group graph pattern #%d back to GP %d (new start)\n", 
                      gp->gp_index, gp_data->current_graph_pattern);
      } else if(gp_data->current_graph_pattern < graph_patterns_size) {
        /* move current forward and reset */
        gp_data->current_graph_pattern++;
        RASQAL_DEBUG3("Moving group graph pattern #%d forward to GP %d\n", 
                      gp->gp_index, gp_data->current_graph_pattern);
      } else {
        gp_data->finished=1;
        break;
      }
      reset=1;
    } else {
      if(gp_data->current_graph_pattern == graph_patterns_size-1)
        break;
      gp_data->matches += sgp_data->matches;
      gp_data->current_graph_pattern++;
    }
  }

  return step;
}


static rasqal_engine_execute_gp_handler rasqal_engine_execute_gp_handlers[RASQAL_GRAPH_PATTERN_OPERATOR_LAST+1]={
  { NULL, NULL, NULL }, /* unknown */
  { rasqal_basic_graph_pattern_execute_init,
    rasqal_basic_graph_pattern_execute_bind, 
    NULL }, /* basic */
  { rasqal_optional_graph_pattern_execute_init,
    rasqal_optional_graph_pattern_execute_bind,
    NULL }, /* optional */
  { NULL, NULL, NULL }, /* union */
  { rasqal_group_graph_pattern_execute_init, 
    rasqal_group_graph_pattern_execute_bind, 
    NULL }, /* group */
  { NULL, NULL, NULL }, /* graph */
};


/*
 * rasqal_new_engine_execution_data:
 * @query_results: query results object
 * 
 * INTERNAL - Initialize the per-query-results execution data and the
 * per-graph pattern execution data
 * 
 * Return value: pointer to the execution data array or NULL on failure.
 **/
static void*
rasqal_new_engine_execution_data(rasqal_query_results* query_results) 
{
  rasqal_query* query=query_results->query;
  rasqal_engine_execution_data* execution_data=NULL;
  int i;

  execution_data=RASQAL_MALLOC(rasqal_engine_execution_data, sizeof(rasqal_engine_execution_data));
  if(!execution_data)
    return NULL;
  execution_data->seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_gp_data, NULL);
  if(!execution_data->seq) {
    rasqal_free_engine_execution_data(query, query_results, execution_data);
    return NULL;
  }

  if(query->graph_patterns_sequence) {
    for(i=0; i < query->graph_pattern_count; i++) {
      rasqal_graph_pattern* gp;
      rasqal_engine_gp_data* gp_data;
    
      gp=(rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns_sequence, i);
      gp_data=rasqal_new_engine_gp_data(gp);
      if(!gp_data) {
        rasqal_free_engine_execution_data(query, query_results, execution_data);
        return NULL;
      }
      gp_data->handler=&rasqal_engine_execute_gp_handlers[gp->op];
      
      raptor_sequence_set_at(execution_data->seq, i, gp_data);
    }
  }

  return execution_data;
}


static void
rasqal_free_engine_execution_data(rasqal_query* query, 
                                  rasqal_query_results* query_results,
                                  void *data)
{
  rasqal_engine_execution_data* execution_data=(rasqal_engine_execution_data*)data;

  if(execution_data->seq)
    raptor_free_sequence(execution_data->seq);
  RASQAL_FREE(rasqal_engine_execution_data, execution_data);
}


/*
 * rasqal_engine_graph_pattern_init:
 * @query_results: query results to work with
 * @gp: graph pattern in query results.
 *
 * Internal - once only per execution initialisation of a graph pattern.
 * 
 **/
static int
rasqal_engine_graph_pattern_init(rasqal_query_results* query_results,
                                 rasqal_graph_pattern *gp)
{
  rasqal_query *query=query_results->query;
  rasqal_engine_execution_data* execution_data=NULL;
  rasqal_engine_gp_data* gp_data;

  RASQAL_DEBUG2("Initing execution graph pattern #%d\n", gp->gp_index);

  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);
  
  gp_data->current_graph_pattern= -1;
  gp_data->column= -1;
  gp_data->matched= 0;
  gp_data->finished= 0;

  if(!gp_data->handler) {
    RASQAL_DEBUG2("graph pattern #%d has no handler\n", gp->gp_index);
    return 1;
  } else if(!gp_data->handler->execute_init) {
    RASQAL_DEBUG2("WARNING: graph pattern #%d has no EXECUTE_INIT() handler\n", 
                  gp->gp_index);
  } else {
    RASQAL_DEBUG2("Execute init graph pattern #%d\n", gp->gp_index);
    gp_data->handler->execute_init(query_results, gp);
  }


  if(gp->triples) {
    int i;
    
    for(i=gp->start_column; i <= gp->end_column; i++) {
      rasqal_triple_meta *m;
      rasqal_triple *t;
      rasqal_variable* v;

      t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
      m=&gp_data->triple_meta[i - gp->start_column];
      m->parts=(rasqal_triple_parts)0;
      
      if((v=rasqal_literal_as_variable(t->subject)) &&
         query->variables_declared_in[v->offset] == i)
        m->parts= (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_SUBJECT);
      
      if((v=rasqal_literal_as_variable(t->predicate)) &&
         query->variables_declared_in[v->offset] == i)
        m->parts= (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_PREDICATE);
      
      if((v=rasqal_literal_as_variable(t->object)) &&
         query->variables_declared_in[v->offset] == i)
        m->parts= (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_OBJECT);

      if(t->origin &&
         (v=rasqal_literal_as_variable(t->origin)) &&
         query->variables_declared_in[v->offset] == i)
        m->parts= (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_ORIGIN);

      RASQAL_DEBUG4("graph pattern #%d Triple %d has parts %d\n",
                    gp->gp_index, i, m->parts);

      /* exact if there are no variables in the triple parts */
      m->is_exact = 1;
      if(rasqal_literal_as_variable(t->predicate) ||
         rasqal_literal_as_variable(t->subject) ||
         rasqal_literal_as_variable(t->object))
        m->is_exact = 0;

    }

  }
  
  return 0;
}


/*
 * rasqal_engine_execute_init:
 * @query_results: query results object
 * 
 * INTERNAL - Prepare a query results for execution
 *
 * Initialises all the per-query results and all per-graph pattern
 * state for all grpah patterns in the query
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_engine_execute_init(rasqal_query_results* query_results) 
{
  rasqal_query* query=query_results->query;
  rasqal_engine_execution_data* execution_data=NULL;
  rasqal_graph_pattern *gp;
  
  if(!query->triples)
    return 1;
  
  if(!query_results->triples_source) {
    query_results->triples_source=rasqal_new_triples_source(query_results);
    if(!query_results->triples_source) {
      query_results->failed=1;
      return 1;
    }
  }

  execution_data=rasqal_new_engine_execution_data(query_results);

  /* reset query_results to empty */
  rasqal_query_results_reset(query_results);

  query_results->execution_data=execution_data;
  query_results->free_execution_data=rasqal_free_engine_execution_data;

  gp=query->query_graph_pattern;

  if(gp)
    return rasqal_engine_graph_pattern_init(query_results, gp);
    
  return 0;
}


int
rasqal_engine_execute_finish(rasqal_query_results* query_results)
{
  if(query_results->triples_source) {
    rasqal_free_triples_source(query_results->triples_source);
    query_results->triples_source=NULL;
  }

  return 0;
}


static rasqal_engine_step
rasqal_engine_check_constraint(rasqal_query *query, rasqal_graph_pattern *gp)
{
  rasqal_engine_step step=STEP_SEARCHING;
  rasqal_literal* result;
  int bresult=1; /* constraint succeeds */
  int error=0;
    
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("constraint expression:\n");
  rasqal_expression_print(gp->constraints_expression, stderr);
  fputc('\n', stderr);
#endif
    
  result=rasqal_expression_evaluate(query, gp->constraints_expression, 
                                    query->compare_flags);
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("constraint expression result:\n");
  if(!result)
    fputs("type error", stderr);
  else
    rasqal_literal_print(result, stderr);
  fputc('\n', stderr);
#endif
  if(!result)
    bresult=0;
  else {
    bresult=rasqal_literal_as_boolean(result, &error);
    if(error) {
      RASQAL_DEBUG1("constraint boolean expression returned error\n");
      step= STEP_ERROR;
    } else
      RASQAL_DEBUG2("constraint boolean expression result: %d\n", bresult);
    rasqal_free_literal(result);
  }

  if(!bresult)
    /* Constraint failed so move to try next match */
    return STEP_SEARCHING;
  
  return STEP_GOT_MATCH;
}


void
rasqal_engine_move_constraints(rasqal_graph_pattern* dest_gp, 
                               rasqal_graph_pattern* src_gp)
{
  int i;
  
  if(!src_gp->constraints)
    return;
  
  for(i=0; i< raptor_sequence_size(src_gp->constraints); i++) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(src_gp->constraints, i);
    e=rasqal_new_expression_from_expression(e);
    rasqal_graph_pattern_add_constraint(dest_gp, e);
  }
}


void
rasqal_engine_join_graph_patterns(rasqal_graph_pattern *dest_gp,
                                  rasqal_graph_pattern *src_gp)
{
  if(!src_gp || !dest_gp)
    return;

  if(src_gp->op != dest_gp->op) {
    RASQAL_DEBUG3("Source operator %s != Destination operator %s, ending\n",
                  rasqal_graph_pattern_operator_as_string(src_gp->op),
                  rasqal_graph_pattern_operator_as_string(dest_gp->op));
    return;
  }

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Joining graph pattern #%d\n  ", src_gp->gp_index);
  rasqal_graph_pattern_print(src_gp, stderr);
  fprintf(stderr, "\nto graph pattern #%d\n  ", dest_gp->gp_index);
  rasqal_graph_pattern_print(dest_gp, stderr);
  fprintf(stderr, "\nboth of operator %s\n",
          rasqal_graph_pattern_operator_as_string(src_gp->op));
#endif
    

  if(src_gp->graph_patterns) {
    if(!dest_gp->graph_patterns)
      dest_gp->graph_patterns=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);

    raptor_sequence_join(dest_gp->graph_patterns, src_gp->graph_patterns);
  }

  if(src_gp->triples) {
    int start_c=src_gp->start_column;
    int end_c=src_gp->end_column;
    
    /* if this is our first triple, save a free/alloc */
    dest_gp->triples=src_gp->triples;
    src_gp->triples=NULL;
    
    if((dest_gp->start_column < 0) || start_c < dest_gp->start_column)
      dest_gp->start_column=start_c;
    if((dest_gp->end_column < 0) || end_c > dest_gp->end_column)
      dest_gp->end_column=end_c;
    
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Moved triples from columns %d to %d\n", start_c, end_c);
    RASQAL_DEBUG3("Columns now %d to %d\n", dest_gp->start_column, dest_gp->end_column);
#endif
  }

  rasqal_engine_move_constraints(dest_gp, src_gp);

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Result graph pattern #%d\n  ", dest_gp->gp_index);
  rasqal_graph_pattern_print(dest_gp, stdout);
  fputs("\n", stdout);
#endif
}


/*
 * rasqal_engine_merge_graph_patterns:
 * @query: query (not used here)
 * @gp: current graph pattern
 * @data: visit data (not used here)
 *
 * Merge graph patterns where possible
 *
 * When size = 1 (never for UNION)
 * GROUP { A } -> A
 * OPTIONAL { A } -> OPTIONAL { A }
 *
 * When size > 1
 * GROUP { BASIC{2,} } -> merge-BASIC
 * OPTIONAL { BASIC{2,} } -> OPTIONAL { merge-BASIC }
 *
 * Never merged: UNION
 */
int
rasqal_engine_merge_graph_patterns(rasqal_query* query,
                                   rasqal_graph_pattern* gp,
                                   void* data)
{
  rasqal_graph_pattern_operator op;
  int merge_gp_ok=0;
  int all_gp_op_same=0;
  int i;
  int size;
  int* modified=(int*)data;
  
#if RASQAL_DEBUG > 1
  printf("rasqal_engine_merge_graph_patterns: Checking graph pattern #%d:\n  ",
         gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
  RASQAL_DEBUG3("Columns %d to %d\n", gp->start_column, gp->end_column);
#endif

  if(!gp->graph_patterns) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Ending graph pattern #%d - operator %s: no sub-graph patterns\n", gp->gp_index,
                  rasqal_graph_pattern_operator_as_string(gp->op));
#endif
    return 0;
  }

  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_GROUP &&
     gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Ending graph patterns %d - operator %s: not GROUP or OPTIONAL\n", gp->gp_index,
                  rasqal_graph_pattern_operator_as_string(gp->op));
#endif
    return 0;
  }

  size=raptor_sequence_size(gp->graph_patterns);
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG3("Doing %d sub-graph patterns of %d\n", size, gp->gp_index);
#endif
  op=RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN;
  all_gp_op_same=1;
  for(i=0; i < size; i++) {
    rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(op == RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN) {
      op=sgp->op;
    } else {
      if(op != sgp->op) {
#if RASQAL_DEBUG > 1
        RASQAL_DEBUG4("Sub-graph pattern #%d is %s different from first %s, cannot merge\n", 
                      i, rasqal_graph_pattern_operator_as_string(sgp->op), 
                      rasqal_graph_pattern_operator_as_string(op));
#endif
        all_gp_op_same=0;
      }
    }
  }
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Sub-graph patterns of %d done\n", gp->gp_index);
#endif
  
  if(!all_gp_op_same) {
    merge_gp_ok=0;
    goto merge_check_done;
  }

  if(size == 1) {
    merge_gp_ok=1;
    goto merge_check_done;
  }


  /* check if ALL sub-graph patterns are basic graph patterns
   * and either:
   * 1) a single triple
   * 2) a single constraint
   */
  for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
    rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    
    if(sgp->op != RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG3("Found %s sub-graph pattern #%d\n",
                    rasqal_graph_pattern_operator_as_string(sgp->op), 
                    sgp->gp_index);
#endif
      merge_gp_ok=0;
      break;
    }
    
    /* not ok if there are >1 triples */
    if(sgp->triples && (sgp->end_column-sgp->end_column+1) > 1) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG2("Found >1 triples in sub-graph pattern #%d\n", sgp->gp_index);
#endif
      merge_gp_ok=0;
      break;
    }
    
    /* not ok if there >1 constraints */
    if(sgp->constraints && raptor_sequence_size(sgp->constraints) != 1) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG2("Found >1 constraints in sub-graph pattern #%d\n", sgp->gp_index);
#endif
      merge_gp_ok=0;
      break;
    }
    
    /* not ok if there are triples and constraints */
    if(sgp->triples && sgp->constraints) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG2("Found triples and constraints in sub-graph pattern #%d\n", sgp->gp_index);
#endif
      merge_gp_ok=0;
      break;
    }
    
    /* was at least 1 OK sub graph-pattern */
    merge_gp_ok=1;
  }

  merge_check_done:
  
  if(merge_gp_ok) {
    raptor_sequence *seq;
    
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("OK to merge sub-graph patterns of %d\n", gp->gp_index);

    RASQAL_DEBUG3("Initial columns %d to %d\n", gp->start_column, gp->end_column);
#endif

    /* Pretend dest is an empty basic graph pattern */
    seq=gp->graph_patterns;
    gp->graph_patterns=NULL;
    /* Update operator GROUP => BASIC, but do not change OPTIONAL */
    if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_GROUP)
      gp->op=RASQAL_GRAPH_PATTERN_OPERATOR_BASIC;
    
    while(raptor_sequence_size(seq) > 0) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_unshift(seq);
      if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_UNION)
        /* this happens with GROUP { UNION } */
        gp->op=RASQAL_GRAPH_PATTERN_OPERATOR_UNION;

      /* fake this so that the join happens */
      sgp->op=gp->op;
      rasqal_engine_join_graph_patterns(gp, sgp);
      rasqal_free_graph_pattern(sgp);
    }

    /* If result is 'basic' but contains graph patterns, turn it into a group */
    if(gp->graph_patterns && gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC)
      gp->op=RASQAL_GRAPH_PATTERN_OPERATOR_GROUP;

    /* Delete any evidence of sub graph patterns */
    raptor_free_sequence(seq);

    *modified=1;
    
  } else {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("NOT OK to merge sub-graph patterns of %d\n", gp->gp_index);
#endif
  }

#if RASQAL_DEBUG > 1
  if(merge_gp_ok) {
    RASQAL_DEBUG2("Ending graph pattern #%d\n  ", gp->gp_index);
    rasqal_graph_pattern_print(gp, stdout);
    fputs("\n\n", stdout);
  }
#endif

  return 0;
}


/**
 * rasqal_engine_check_limit_offset:
 *
 * Check the query result count is in the limit and offset range if any.
 *
 * Return value: before range -1, in range 0, after range 1
 */
int
rasqal_engine_check_limit_offset(rasqal_query_results *query_results)
{
  rasqal_query* query=query_results->query;

  if(query->offset > 0) {
    /* offset */
    if((query_results->result_count-1) <= query->offset)
      return -1;
    
    if(query->limit >= 0) {
      /* offset and limit */
      if(query_results->result_count > (query->offset + query->limit)) {
        query_results->finished=1;
      }
    }
    
  } else if(query->limit >= 0) {
    /* limit */
    if(query_results->result_count > query->limit) {
      query_results->finished=1;
    }
  }

  return query_results->finished;
}


/*
 * rasqal_engine_merge_triples:
 * @query: query (not used here)
 * @gp: current graph pattern
 * @data: visit data (not used here)
 *
 * Join triple patterns in adjacent basic graph patterns into
 * single basic graph pattern.
 *
 * For group graph pattern move all triples
 *  from { { a } { b } { c }  D... } 
 *  to { a b c  D... }
 *  if the types of a, b, c are all BASIC GPs (just triples)
 *   D... is anything else
 * 
 */
int
rasqal_engine_merge_triples(rasqal_query* query,
                            rasqal_graph_pattern* gp,
                            void* data)
{
  int* modified=(int*)data;
  int checking;
  int offset;

#if RASQAL_DEBUG > 1
  printf("rasqal_engine_merge_triples: Checking graph pattern #%d:\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
  RASQAL_DEBUG3("Columns %d to %d\n", gp->start_column, gp->end_column);
#endif
    
  if(!gp->graph_patterns) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("Ending graph patterns %d - no sub-graph patterns\n", gp->gp_index);
#endif
    return 0;
  }

  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_GROUP) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Ending graph patterns %d - operator %s\n", gp->gp_index,
                  rasqal_graph_pattern_operator_as_string(gp->op));
#endif
    return 0;
  }


  checking=1;
  offset=0;
  while(checking) {
    int bgp_count;
    rasqal_graph_pattern *dest_bgp;
    raptor_sequence *seq;
    int i, j;
    int first, last;
    int size=raptor_sequence_size(gp->graph_patterns);
    
    /* find first basic graph pattern starting at offset */
    for(i= offset; i < size; i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);

      if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
        first=i;
        break;
      }
    }
    
    /* None found */
    if(i >= size)
      break;

    /* Next time, start after this BGP */
    offset=i+1;
    
    /* count basic graph patterns */
    bgp_count=0;
    dest_bgp=NULL; /* destination graph pattern */
    for(j=i; j < size; j++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, j);

      if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
        bgp_count++;
        if(!dest_bgp)
          dest_bgp=sgp;
        last=j;
      } else
        break;
    }


  #if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Found sequence of %d basic sub-graph patterns in %d\n", bgp_count, gp->gp_index);
  #endif
    if(bgp_count < 2)
      continue;

  #if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("OK to merge %d basic sub-graph patterns of %d\n", bgp_count, gp->gp_index);

    RASQAL_DEBUG3("Initial columns %d to %d\n", gp->start_column, gp->end_column);
  #endif
    seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
    for(i=0; raptor_sequence_size(gp->graph_patterns) > 0; i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_unshift(gp->graph_patterns);
      if(i >= first && i <= last) {
        if(sgp != dest_bgp) {
          rasqal_engine_join_graph_patterns(dest_bgp, sgp);
          rasqal_free_graph_pattern(sgp);
        } else
          raptor_sequence_push(seq, sgp);
      } else
        raptor_sequence_push(seq, sgp);
    }
    raptor_free_sequence(gp->graph_patterns);
    gp->graph_patterns=seq;

    *modified=1;

  } /* end while checking */
  

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG3("Ending columns %d to %d\n", gp->start_column, gp->end_column);

  RASQAL_DEBUG2("Ending graph pattern #%d\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n\n", stdout);
#endif

  return 0;
}


struct folding_state {
  rasqal_query* query;
  int changes;
  int failed;
};
  

static int
rasqal_engine_expression_foreach_fold(void *user_data, rasqal_expression *e)
{
  struct folding_state *st=(struct folding_state*)user_data;
  rasqal_literal* l;

  /* skip if already a  literal or this expression tree is not constant */
  if(e->op == RASQAL_EXPR_LITERAL || !rasqal_expression_is_constant(e))
    return 0;
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("folding expression %p: ", e);
  rasqal_expression_print(e, stderr);
  fprintf(stderr, "\n");
#endif
  
  l=rasqal_expression_evaluate(st->query, e, st->query->compare_flags);
  if(!l) {
    st->failed++;
    return 1;
  }

  /* In-situ conversion of 'e' to a literal expression */
  rasqal_expression_convert_to_literal(e, l);
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("folded expression now: ");
  rasqal_expression_print(e, stderr);
  fputc('\n', stderr);
#endif

  /* change made */
  st->changes++;
  
  return 0;
}


int
rasqal_engine_expression_fold(rasqal_query* rq, rasqal_expression* e)
{
  struct folding_state st;

  st.query=rq;
  while(1) {
    st.changes=0;
    st.failed=0;
    rasqal_expression_visit(e, rasqal_engine_expression_foreach_fold, 
                            (void*)&st);
    if(!st.changes || st.failed)
      break;
  }

  return st.failed;
}


int
rasqal_engine_graph_pattern_fold_expressions(rasqal_query* rq,
                                             rasqal_graph_pattern* gp)
{
  if(!gp)
    return 1;
  
  /* fold expressions in sub graph patterns */
  if(gp->graph_patterns) {
    int i;
    
    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_engine_graph_pattern_fold_expressions(rq, sgp))
        return 1;
    }
  }

  if(gp->constraints_expression)
    return rasqal_engine_expression_fold(rq, gp->constraints_expression);

  return 0;
}


int
rasqal_engine_query_fold_expressions(rasqal_query* rq)
{
  rasqal_graph_pattern *gp=rq->query_graph_pattern;
  int order_size;

  if(gp)
    rasqal_engine_graph_pattern_fold_expressions(rq, gp);

  if(!rq->order_conditions_sequence)
    return 0;
  
  order_size=raptor_sequence_size(rq->order_conditions_sequence);
  if(order_size) {
    int i;
    
    for(i=0; i < order_size; i++) {
      rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(rq->order_conditions_sequence, i);
      rasqal_engine_expression_fold(rq, e);
    }
  }

  return 0;
}


/**
 * rasqal_engine_new_basic_graph_pattern_from_formula:
 * @query: #rasqal_graph_pattern query object
 * @formula: triples sequence containing the graph pattern
 * @op: enum #rasqal_graph_pattern_operator operator
 *
 * Create a new graph pattern object over a formula.
 * 
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_engine_new_basic_graph_pattern_from_formula(rasqal_query* query,
                                                   rasqal_formula* formula)
{
  rasqal_graph_pattern* gp;
  raptor_sequence *triples=query->triples;
  raptor_sequence *formula_triples=formula->triples;
  int offset=raptor_sequence_size(triples);
  int triple_pattern_size=0;

  if(formula_triples) {
    /* Move formula triples to end of main triples sequence */
    triple_pattern_size=raptor_sequence_size(formula_triples);
    raptor_sequence_join(triples, formula_triples);
  }

  rasqal_free_formula(formula);

  gp=rasqal_new_basic_graph_pattern(query, triples, 
                                    offset, 
                                    offset+triple_pattern_size-1);
  return gp;
}


rasqal_graph_pattern*
rasqal_engine_group_2_graph_patterns(rasqal_query* query,
                                     rasqal_graph_pattern* first_gp,
                                     rasqal_graph_pattern* second_gp)
{
  if(!first_gp && !second_gp)
    return NULL;
  
  if(first_gp && second_gp) {
    raptor_sequence *seq;

    seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
    raptor_sequence_push(seq, first_gp);
    raptor_sequence_push(seq, second_gp);

    first_gp=rasqal_new_graph_pattern_from_sequence(query, seq,
                                                    RASQAL_GRAPH_PATTERN_OPERATOR_GROUP);
  } else if(!first_gp)
    first_gp=second_gp;

  return first_gp;
}


/*
 * rasqal_engine_remove_empty_group_graph_patterns:
 * @query: query (not used here)
 * @gp: current graph pattern
 * @data: visit data (not used here)
 *
 * Remove empty group graph patterns
 *
 */
int
rasqal_engine_remove_empty_group_graph_patterns(rasqal_query* query,
                                                rasqal_graph_pattern* gp,
                                                void* data)
{
  int i;
  int saw_empty_gp=0;
  raptor_sequence *seq;
  int* modified=(int*)data;
  
  if(!gp->graph_patterns)
    return 0;

#if RASQAL_DEBUG > 1
  printf("rasqal_engine_remove_empty_group_graph_patterns: Checking graph pattern #%d:\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
#endif

  for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
    rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(sgp->graph_patterns && !raptor_sequence_size(sgp->graph_patterns)) {
      /* One is enough to know we need to rewrite */
      saw_empty_gp=1;
      break;
    }
  }

  if(!saw_empty_gp) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("Ending graph patterns %d - saw no empty groups\n", gp->gp_index);
#endif
    return 0;
  }
  
  
  seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  while(raptor_sequence_size(gp->graph_patterns) > 0) {
    rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_unshift(gp->graph_patterns);
    if(sgp->graph_patterns && !raptor_sequence_size(sgp->graph_patterns)) {
      rasqal_engine_move_constraints(gp, sgp);
      rasqal_free_graph_pattern(sgp);
      continue;
    }
    raptor_sequence_push(seq, sgp);
  }
  raptor_free_sequence(gp->graph_patterns);
  gp->graph_patterns=seq;

  *modified=1;
  
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Ending graph pattern #%d\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n\n", stdout);
#endif

  return 0;
}


static int
rasqal_engine_query_result_row_update(rasqal_query_result_row* row, int offset)
{
  rasqal_query_results *query_results=row->results;
  rasqal_query* query;
  int i;
  int size;
  
  if(!rasqal_query_results_is_bindings(query_results) &&
     !rasqal_query_results_is_graph(query_results))
    return 1;

  query=query_results->query;

  if(query->constructs)
    size=raptor_sequence_size(query->variables_sequence);
  else
    size=query->select_variables_count;

  for(i=0; i< size; i++)
    query->binding_values[i]=query->variables[i]->value;

  for(i=0; i < row->size; i++) {
    rasqal_literal *l=query->binding_values[i];
    if(row->values[i])
      rasqal_free_literal(row->values[i]);
    if(l)
      row->values[i]=rasqal_literal_as_node(l);
    else
      row->values[i]=NULL;
  }

  if(row->order_size) {
    for(i=0; i < row->order_size; i++) {
      rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(query->order_conditions_sequence, i);
      rasqal_literal *l=rasqal_expression_evaluate(query, e, 
                                                   query->compare_flags);
      if(row->order_values[i])
        rasqal_free_literal(row->order_values[i]);
      if(l) {
        row->order_values[i]=rasqal_literal_as_node(l);
        rasqal_free_literal(l);
      } else
        row->order_values[i]=NULL;
    }
  }
  
  row->offset=offset;
  
  return 0;
}


static rasqal_query_result_row*
rasqal_engine_new_query_result_row(rasqal_query_results* query_results, int offset)
{
  rasqal_query* query=query_results->query;
  int size;
  int order_size;
  rasqal_query_result_row* row;
  
  if(!rasqal_query_results_is_bindings(query_results) &&
     !rasqal_query_results_is_graph(query_results))
    return NULL;

  /* do not use rasqal_query_results_get_bindings_count() as it is 0
   * for a graph result which is also executed handled here
   */
  if(query->constructs)
    size=raptor_sequence_size(query->variables_sequence);
  else
    size=query->select_variables_count;

  row=(rasqal_query_result_row*)RASQAL_CALLOC(rasqal_query_result_row, 1,
                                              sizeof(rasqal_query_result_row));
  row->usage=1;
  row->results=query_results;

  row->size=size;
  row->values=(rasqal_literal**)RASQAL_CALLOC(array, size,
					      sizeof(rasqal_literal*));

  if(query->order_conditions_sequence)
    order_size=raptor_sequence_size(query->order_conditions_sequence);
  else
    order_size=0;
  
  if(order_size) {
    row->order_size=order_size;
    row->order_values=(rasqal_literal**)RASQAL_CALLOC(array,  order_size,
                                                      sizeof(rasqal_literal*));
  }
  
  rasqal_engine_query_result_row_update(row, offset);
  
  return row;
}


static rasqal_query_result_row*
rasqal_engine_new_query_result_row_from_query_result_row(rasqal_query_result_row* row)
{
  row->usage++;
  return row;
}


void 
rasqal_engine_free_query_result_row(rasqal_query_result_row* row)
{
  if(--row->usage)
    return;
  
  if(row->values) {
    int i; 
    for(i=0; i < row->size; i++) {
      if(row->values[i])
        rasqal_free_literal(row->values[i]);
    }
    RASQAL_FREE(array, row->values);
  }
  if(row->order_values) {
    int i; 
    for(i=0; i < row->order_size; i++) {
      if(row->order_values[i])
        rasqal_free_literal(row->order_values[i]);
    }
    RASQAL_FREE(array, row->order_values);
  }

  RASQAL_FREE(rasqal_query_result_row, row);
}


rasqal_literal**
rasqal_engine_get_results_values(rasqal_query_results* query_results)
{
  rasqal_query_result_row* row;

  if(query_results->results_sequence)
    /* Ordered Results */
    row=(rasqal_query_result_row*)raptor_sequence_get_at(query_results->results_sequence,
                                                         query_results->result_count-1);
  else
    /* Streamed Results */
    row=query_results->row;

  if(row)
    return row->values;
  
  query_results->finished=1;
  return NULL;
}
  

rasqal_literal*
rasqal_engine_get_result_value(rasqal_query_results* query_results, int offset)
{
  rasqal_query_result_row* row=NULL;

  /* Ordered Results */
  if(query_results->results_sequence)
    row=(rasqal_query_result_row*)raptor_sequence_get_at(query_results->results_sequence, 
                                                         query_results->result_count-1);
  else
    row=query_results->row;

  if(row)
    return row->values[offset];

  query_results->finished=1;
  return NULL;
}


static void 
rasqal_engine_query_result_row_print(rasqal_query_result_row* row, FILE* fh)
{
  rasqal_query* query=row->results->query;
  int i;
  
  fputs("result[", fh);
  for(i=0; i < row->size; i++) {
    /* Do not use rasqal_query_results_get_binding_name(row->results, i); 
     * as it does not work for a construct result
     */
    const unsigned char *name=(query->variables[i] ? query->variables[i]->name : NULL);
    rasqal_literal *value=row->values[i];
    
    if(i > 0)
      fputs(", ", fh);
    fprintf(fh, "%s=", name);

    if(value)
      rasqal_literal_print(value, fh);
    else
      fputs("NULL", fh);
  }

  fputs(" with ordering values [", fh);

  if(row->order_size) {

    for(i=0; i < row->order_size; i++) {
      rasqal_literal *value=row->order_values[i];
      
      if(i > 0)
        fputs(", ", fh);
      if(value)
        rasqal_literal_print(value, fh);
      else
        fputs("NULL", fh);
    }
    fputs("]", fh);
  }

  fprintf(fh, " offset %d]", row->offset);
}


static int
rasqal_query_result_literal_sequence_compare(rasqal_query* query,
                                             rasqal_literal** values_a,
                                             rasqal_literal** values_b,
                                             raptor_sequence* expr_sequence,
                                             int size)
{
  int result=0;
  int i;

  for(i=0; i < size; i++) {
    rasqal_expression* e=NULL;
    int error=0;
    rasqal_literal* literal_a=values_a[i];
    rasqal_literal* literal_b=values_b[i];
    
    if(expr_sequence)
      e=(rasqal_expression*)raptor_sequence_get_at(expr_sequence, i);

#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, stderr);
    fputs(" to ", stderr);
    rasqal_literal_print(literal_b, stderr);
    fputs("\n", stderr);
#endif

    if(!literal_a || !literal_b) {
      if(!literal_a && !literal_b)
        result= 0;
      else {
        result= literal_a ? 1 : -1;
#ifdef RASQAL_DEBUG
        RASQAL_DEBUG2("Got one NULL literal comparison, returning %d\n", result);
#endif
        break;
      }
    }
    
    result=rasqal_literal_compare(literal_a, literal_b, query->compare_flags,
                                  &error);

    if(error) {
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG2("Got literal comparison error at expression %d, returning 0\n", i);
#endif
      result=0;
      break;
    }
        
    if(!result)
      continue;

    if(e && e->op == RASQAL_EXPR_ORDER_COND_DESC)
      result= -result;
    /* else Order condition is RASQAL_EXPR_ORDER_COND_ASC so nothing to do */
    
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG3("Returning comparison result %d at expression %d\n", result, i);
#endif
    break;
  }

  return result;
}


static int
rasqal_engine_query_result_row_compare(const void *a, const void *b)
{
  rasqal_query_result_row* row_a;
  rasqal_query_result_row* row_b;
  rasqal_query_results* results;
  rasqal_query* query;
  int result=0;

  row_a=*(rasqal_query_result_row**)a;
  row_b=*(rasqal_query_result_row**)b;
  results=row_a->results;
  query=results->query;
  
  if(query->distinct) {
    if(query->distinct == 1)
      result=rasqal_query_result_literal_sequence_compare(query,
                                                          row_a->values,
                                                          row_b->values,
                                                          NULL,
                                                          row_a->size);
    
    if(!result)
      /* duplicate, so return that */
      return 0;
  }
  
  /* now order it */
  result=rasqal_query_result_literal_sequence_compare(query,
                                                      row_a->order_values,
                                                      row_b->order_values,
                                                      query->order_conditions_sequence,
                                                      row_a->order_size);
  
  /* still equal?  make sort stable by using the original order */
  if(!result) {
    result= row_a->offset - row_b->offset;
    RASQAL_DEBUG2("Got equality result so using offsets, returning %d\n",
                  result);
  }
  
  return result;
}


static void
rasqal_engine_map_free_query_result_row(const void *key, const void *value)
{
  if(key)
    rasqal_engine_free_query_result_row((rasqal_query_result_row*)key);
  if(value)
    rasqal_engine_free_query_result_row((rasqal_query_result_row*)value);
}


static void
rasqal_engine_map_print_query_result_row(void *object, FILE *fh)
{
  if(object)
    rasqal_engine_query_result_row_print((rasqal_query_result_row*)object, fh);
  else
    fputs("NULL", fh);
}


static void
rasqal_engine_map_add_to_sequence(void *key, void *value, void *user_data)
{
  rasqal_query_result_row* row;
  row=rasqal_engine_new_query_result_row_from_query_result_row((rasqal_query_result_row*)key);
  raptor_sequence_push((raptor_sequence*)user_data, row);
}


static void
rasqal_engine_bind_construct_variables(rasqal_query_results* query_results)
{
  rasqal_query *query=query_results->query;
  int i;

  if(!query->constructs)
    return;
  
  if(!query_results->results_sequence)
    return;

  /* bind the construct variables again if running through a sequence */
  for(i=0; i< query->variables_count; i++) {
    rasqal_literal* value=rasqal_engine_get_result_value(query_results, i);
    rasqal_variable_set_value(query->variables[i],  value);
  }
}


static
void rasqal_engine_store_query_results(rasqal_query_results* query_results) 
{
  rasqal_query_result_row* row;
  rasqal_engine_execution_data* execution_data=NULL;
#ifdef RASQAL_DEBUG
  rasqal_query *query=query_results->query;
  int size;
  int i;
#endif

#ifdef RASQAL_DEBUG
  if(query->constructs)
    size=raptor_sequence_size(query->variables_sequence);
  else
    size=query->select_variables_count;

  RASQAL_DEBUG1("Returning solution[");
  for(i=0; i< size; i++) {
    const unsigned char *name=query->variables[i]->name;
    rasqal_literal *value=query->variables[i]->value;
    if(i>0)
      fputs(", ", stderr);
    fprintf(stderr, "%s=", name);
    if(value)
      rasqal_literal_print(value, stderr);
    else
      fputs("NULL", stderr);
  }
  fputs("]\n", stderr);
#endif

  execution_data=query_results->execution_data;

  row=rasqal_engine_new_query_result_row(query_results, execution_data->offset);
  
  if(!execution_data->map)
    /* not ordering and not distinct */
    raptor_sequence_push(query_results->results_sequence, row);
  else {
    /* ordering or distinct */
    
    /* after this, row is owned by map */
    if(!rasqal_map_add_kv(execution_data->map, row, NULL))
      execution_data->offset++;
    else {
      /* duplicate, and not added so delete it */
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG1("Got duplicate row ");
      rasqal_engine_query_result_row_print(row, stderr);
      fputc('\n', stderr);
#endif
      rasqal_engine_free_query_result_row(row);
    }
  }
}


static rasqal_engine_step
rasqal_engine_execute_run_graph_pattern(rasqal_query_results* query_results,
                                        rasqal_graph_pattern* gp) 
{
  rasqal_engine_execution_data* execution_data=NULL;
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_step step;
  
  RASQAL_DEBUG3("Executing bind graph_pattern #%d %s\n", gp->gp_index,
                rasqal_graph_pattern_operator_as_string(gp->op));

  execution_data=query_results->execution_data;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);

  if(!gp_data->handler) {
    RASQAL_DEBUG2("graph pattern #%d has no handler\n", gp->gp_index);
    return STEP_ERROR;
  }
  if(!gp_data->handler->execute_bind) {
    RASQAL_DEBUG2("graph pattern #%d has no BIND()x handler\n", gp->gp_index);
    return STEP_ERROR;
  }

  step=gp_data->handler->execute_bind(query_results, gp);
  RASQAL_DEBUG5("Execute bind graph pattern #%d %s returned step %s and %d matches\n",
                gp->gp_index, rasqal_graph_pattern_operator_as_string(gp->op),
                rasqal_engine_step_names[step],
                gp_data->matches);
  return step;
}


int
rasqal_engine_execute_run(rasqal_query_results* query_results)
{
  rasqal_query *query=query_results->query;
  rasqal_engine_step step=STEP_GOT_MATCH;
  rasqal_engine_execution_data* execution_data=NULL;
  rasqal_graph_pattern *gp;
  int rc;
  
  /* start at the outer graph pattern */
  gp=query->query_graph_pattern;
  if(!gp) {
    rasqal_query_error(query, "No graph pattern in query to execute.");
    query_results->failed=1;
    return 1;
  }
  

  /* LAZY: was if(query->order_conditions_sequence || query->distinct) */
  /* NON LAZY: always store results */

  /* Initialise execution data */
  execution_data=query_results->execution_data;
  execution_data->offset=0;
  execution_data->map=NULL;
  if(query->order_conditions_sequence || query->distinct)
    /* get all query results and order them if have a map
     * make a row:NULL map only if necessary
     */
    execution_data->map=rasqal_new_map(rasqal_engine_query_result_row_compare,
                                       rasqal_engine_map_free_query_result_row, 
                                       rasqal_engine_map_print_query_result_row,
                                       NULL,
                                       0);
  query_results->results_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_engine_free_query_result_row, (raptor_sequence_print_handler*)rasqal_engine_query_result_row_print);

  /* Execution loop */
  while(step == STEP_GOT_MATCH) {
    rasqal_engine_gp_data* gp_data;
    gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                           gp->gp_index);

    step=rasqal_engine_execute_run_graph_pattern(query_results, gp);
    RASQAL_DEBUG3("rasqal_engine_execute_run_graph_pattern returned step %s and %d matches\n", 
                  rasqal_engine_step_names[step],
                  gp_data->matches);

    if(step == STEP_GOT_MATCH)
      rasqal_engine_store_query_results(query_results);
  }

  if(step == STEP_FINISHED)
    query_results->finished=1;

  if(step == STEP_ERROR) {
    /* <0 failure */
    query_results->finished=1;
    query_results->failed=1;
    
    if(execution_data->map)
      rasqal_free_map(execution_data->map);
    raptor_free_sequence(query_results->results_sequence);
    query_results->results_sequence=NULL;
  }


  /* Got a result set or an ordered map */
  
#ifdef RASQAL_DEBUG
  if(execution_data->map) {
    fputs("resulting ", stderr);
    rasqal_map_print(execution_data->map, stderr);
    fputs("\n", stderr);
  }
#endif
  
  if(execution_data->map) {
    /* Got a map so need to form the results set in the order wanted */
    rasqal_map_visit(execution_data->map,
                     rasqal_engine_map_add_to_sequence, 
                     (void*)query_results->results_sequence);
    rasqal_free_map(execution_data->map);
  }
  
  query_results->finished= (raptor_sequence_size(query_results->results_sequence) == 0);
    
  if(!query->limit)
    query_results->finished=1;
  
  if(!query_results->finished) {
    int results_size=raptor_sequence_size(query_results->results_sequence);
    
    /* Reset to first result, index-1 into sequence of results */
    query_results->result_count= 1;
    
    /* skip past any OFFSET */
    if(query->offset > 0) {
      query_results->result_count += query->offset;
      if(query_results->result_count >= results_size)
        query_results->finished=1;
    }
    
  }
  
  if(query_results->finished)
    query_results->result_count= 0;
  else
    rasqal_engine_bind_construct_variables(query_results);

  /* LAZY: was
    } else {
    - No order sequence -
    rasqal_engine_query_results_update(query_results);
    query_results->row=rasqal_engine_new_query_result_row(query_results, 
                                                          query_results->result_count);
  }
  */

  if(step == STEP_ERROR)
    rc= -1;
  else if(step == STEP_FINISHED)
    rc=0;
  else
    rc=1;

  return rc;
}


int
rasqal_engine_execute_next(rasqal_query_results* query_results)
{
  rasqal_query* query;
  int size;
  
  query=query_results->query;

  /* Ordered Results */

  /* LAZY: was if(query_results->results_sequence) */
  size=raptor_sequence_size(query_results->results_sequence);

  while(1) {
    if(query_results->result_count >= size) {
      query_results->finished=1;
      break;
    }
    
    query_results->result_count++;
    
    /* finished if beyond result range */
    if(rasqal_engine_check_limit_offset(query_results) > 0) {
      query_results->result_count--;
      break;
    }
    
    /* continue if before start of result range */
    if(rasqal_engine_check_limit_offset(query_results) < 0)
      continue;
    
    /* else got result or finished */
    rasqal_engine_bind_construct_variables(query_results);
    break;
  }

  /* LAZY was:
  } else {
    rasqal_engine_query_results_update(query_results);
    if(!query_results->finished)
      rasqal_engine_query_result_row_update(query_results->row, 
                                            query_results->result_count);
  }
  */

  return query_results->finished;
}
