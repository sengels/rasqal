dnl -*- Mode: autoconf -*-
dnl
dnl configure.ac - autoconf file for rasqal
dnl (Process this file with autoconf to produce a configure script.)
dnl
dnl Copyright (C) 2003-2011, David Beckett http://www.dajobe.org/
dnl Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
dnl 
dnl This package is Free Software and part of Redland http://librdf.org/
dnl 
dnl It is licensed under the following three licenses as alternatives:
dnl   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
dnl   2. GNU General Public License (GPL) V2 or any newer version
dnl   3. Apache License, V2.0 or any newer version
dnl 
dnl You may not use this file except in compliance with at least one of
dnl the above three licenses.
dnl 
dnl See LICENSE.html or LICENSE.txt at the top of this package for the
dnl complete terms and further detail along with the license texts for
dnl the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
dnl 


AC_PREREQ(2.62)
AC_INIT(Rasqal RDF Query Library, 0.9.27, http://bugs.librdf.org/, rasqal)
AC_CONFIG_SRCDIR([src/rasqal_general.c])
AM_CONFIG_HEADER([src/rasqal_config.h])
AC_CONFIG_AUX_DIR(build)
AC_CONFIG_MACRO_DIR(build)
AM_INIT_AUTOMAKE([1.11 check-news std-options -Wall])

AM_MAINTAINER_MODE

release_version=no
AC_ARG_ENABLE(release, [  --enable-release        Turn on optimizations (for maintainer).  ], \
 if test "$enableval" = "yes"; then \
	  release_version=yes
fi;)	

if test "$USE_MAINTAINER_MODE" = yes -a $release_version = no; then 
  CFLAGS=`echo $CFLAGS | sed -e "s/-O[A-Za-z0-9]*//"`
  CXXFLAGS=`echo $CXXFLAGS | sed -e "s/-O[A-Za-z0-9]*//"`
  CPPFLAGS=`echo $CPPFLAGS | sed -e "s/-O[A-Za-z0-9]*//"`
fi

dnl Checks for programs.
AC_CANONICAL_HOST
AM_SANITY_CHECK
AC_PROG_CC
AM_PROG_CC_C_O

m4_undefine([AC_PROG_CXX])
m4_defun([AC_PROG_CXX],[])
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET


AM_PROG_LEX
if test "X$LEX" != "X:" ; then
  if echo "$LEX" | grep flex >/dev/null 2>&1; then
    :
  else
    LEX="$SHELL $missing_dir/missing flex"
    AC_SUBST(LEX_OUTPUT_ROOT, lex.yy)
    AC_SUBST(LEXLIB, '')
  fi
fi
 
FLEX_MIN_VERSION=2.5.19
FLEX_REC_VERSION=2.5.33
if test "$USE_MAINTAINER_MODE" = yes; then
  AC_MSG_CHECKING(flex version)
  if test "X$LEX" != "X:"; then
    FLEX_VERSION=`$LEX -V 2>&1 | awk '{print $NF}'`
    FLEX_VERSION_DEC=`echo $FLEX_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

    FLEX_MIN_VERSION_DEC=`echo $FLEX_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

    if test $FLEX_VERSION_DEC -ge $FLEX_MIN_VERSION_DEC; then
      AC_MSG_RESULT($FLEX_VERSION - OK)
    else
      AC_MSG_RESULT(version $FLEX_VERSION - too old)
      AC_MSG_WARN(Please get flex from http://flex.sourceforge.net/)
      AC_MSG_WARN(version $FLEX_MIN_VERSION ($FLEX_REC_VERSION recommended))
      AC_MSG_FAILURE(flex too old)
    fi
  else
    AC_MSG_RESULT(not present)
    AC_MSG_WARN(Please get flex from http://flex.sourceforge.net/)
    AC_MSG_WARN(version $FLEX_MIN_VERSION ($FLEX_REC_VERSION recommended))
    AC_MSG_FAILURE(flex not present)
  fi
fi

AC_PROG_YACC
if test "$USE_MAINTAINER_MODE" = yes; then
  AC_MSG_CHECKING(for GNU bison)
  # Match these styles of versions
  # GNU Bison version 1.28
  # bison (GNU Bison) 1.875
dnl need to change quotes to allow square brackets
changequote(<<, >>)dnl
  YACC_VERSION=`$YACC --version 2>&1 | sed -ne 's/^.*GNU Bison[^0-9]*//p'`
changequote([, ])dnl
  if test "X$YACC_VERSION" != X; then
    AC_MSG_RESULT($YACC_VERSION - OK)
  else
    AC_MSG_FAILURE($YACC is not GNU bison)
  fi
fi


# Find a tar command for 'make dist'
AC_CHECK_PROGS(TAR, gnutar gtar tar)
AC_CHECK_PROGS(PERL, perl)

AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)

AC_CHECK_PROGS(RECHO, echo)
RECHO_C=
RECHO_N=
case `$RECHO -n x` in
  -n*)
    case `$RECHO 'xy\c'` in
      *c*)
         ;;
      xy)
         RECHO_C='\c'
         ;;
    esac;;
  *)
    RECHO_N='-n'
    ;;
esac



dnl compiler checks

AC_DEFUN([REDLAND_CC_TRY_FLAG], [
  AC_MSG_CHECKING([whether $CC supports $1])
  redland_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $1"
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([ ])], [redland_cc_flag=yes], [redland_cc_flag=no])
  CFLAGS="$redland_save_CFLAGS"
  if test "X$redland_cc_flag" = "Xyes"; then
    ifelse([$2], , :, [$2])
  else
    ifelse([$3], , :, [$3])
  fi
  AC_MSG_RESULT($redland_cc_flag)
])

# GCC warning options
# http://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
#
# Too noisy:
#  -Wconversion
#  -Wunused-parameter : variables can be marked  __attribute__('unused')
#  -Wformat-nonliteral : variables are used in raptor_rdfxml.c for format strings: fixable with code change
#  -Wsign-conversion: : many int / unsigned int / size_t
#  -Wfloat-equal : too many false positives; it's ok to compare with whole small integers like 0.0
#  -Wundef : using undefined macros is just fine
#
# Apple gcc specific (probably): -Wshorten-64-to-32
possible_warnings="\
-Wall \
-Wc++-compat \
-Wextra \
-Wunused \
\
-Waggregate-return \
-Wbad-function-cast \
-Wcast-align \
-Wdeclaration-after-statement \
-Wdisabled-optimization \
-Wdiv-by-zero \
-Wendif-labels \
-Werror-implicit-function-declaration \
-Wformat-security \
-Wframe-larger-than=4096 \
-Winit-self \
-Winline \
-Wmissing-declarations \
-Wmissing-format-attribute \
-Wmissing-noreturn \
-Wmissing-prototypes \
-Wnested-externs \
-Wold-style-definition \
-Wpacked \
-Wpointer-arith \
-Wredundant-decls \
-Wshadow \
-Wsign-compare \
-Wstrict-prototypes \
-Wswitch-enum \
-Wunsafe-loop-optimizations \
-Wwrite-strings \
\
-Wshorten-64-to-32
\
-Wno-conversion \
-Wno-sign-conversion \
-Wno-missing-field-initializers \
-Wno-system-headers \
-Wno-unused-parameter \
"

warning_cflags=
if test "$USE_MAINTAINER_MODE" = yes; then
  AC_MSG_CHECKING(for supported $CC warning flags)
  AC_MSG_RESULT($warning_cflags)
  for warning in $possible_warnings; do
    REDLAND_CC_TRY_FLAG([$warning], [warning_cflags="$warning_cflags $warning"])
  done
  AC_MSG_CHECKING($CC supports warning flags)
  AC_MSG_RESULT($warning_cflags)
fi

STANDARD_CFLAGS=
MAINTAINER_CPPFLAGS="$warning_cflags"

# Externally linked libraries - appear in rasqal-config
# -Dfoo -Idir
RASQAL_CPPFLAGS=$CPPFLAGS
# -Ldir
RASQAL_LDFLAGS=$LDFLAGS
# -llib
RASQAL_LIBS=$LIBS

# Internally linked libraries - never given to users
# -Dfoo -Idir
RASQAL_INTERNAL_CPPFLAGS=
# -Ldir -llib
RASQAL_INTERNAL_LIBS=

# Libraries linked externally also may be internal - just raptor
# -Ldir -llib
RASQAL_EXTERNAL_LIBS=
# Extra pkgconfig requires
PKGCONFIG_REQUIRES=
PKGCONFIG_LIBS=
PKGCONFIG_CFLAGS=

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(errno.h stdlib.h stdint.h unistd.h string.h getopt.h regex.h sys/time.h time.h math.h limits.h errno.h)
AC_HEADER_TIME

if test "$ac_cv_header_sys_time_h" = "yes"; then
  HAVE_SYS_TIME_H=1
else
  HAVE_SYS_TIME_H=0
fi
AC_SUBST(HAVE_SYS_TIME_H)
if test "$ac_cv_header_time_h" = "yes"; then
  HAVE_TIME_H=1
else
  HAVE_TIME_H=0
fi
AC_SUBST(HAVE_TIME_H)


dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN



# Libtool versioning
#
# CURRENT
#      The most recent interface number that this library implements.
# 
# REVISION
#      The implementation number of the CURRENT interface.
# 
# AGE
#      The difference between the newest and oldest interfaces that this
#      library implements.  In other words, the library implements all the
#      interface numbers in the range from number `CURRENT - AGE' to
#      `CURRENT'.
#
# Rules:
#  1. Start with version information of `0:0:0' for each libtool library.
#
#  2. Update the version information only immediately before a public
#     release of your software.  More frequent updates are unnecessary,
#     and only guarantee that the current interface number gets larger
#     faster.
#
#  3. If the library source code has changed at all since the last
#     update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
#
#  4. If any interfaces have been added, removed, or changed since the
#     last update, increment CURRENT, and set REVISION to 0.
#
#  5. If any interfaces have been added since the last public release,
#     then increment AGE.
#
#  6. If any interfaces have been removed since the last public release,
#     then set AGE to 0.
#
# syntax: CURRENT[:REVISION[:AGE]]
RASQAL_LIBTOOL_VERSION=3:0:0
AC_SUBST(RASQAL_LIBTOOL_VERSION)


dnl need to change quotes to allow square brackets
changequote(<<, >>)dnl
version_major=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\1/'`
version_minor=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\2/'`
version_release=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\3/'`
changequote([, ])dnl

version_decimal=`expr $version_major \* 10000 + $version_minor \* 100 + $version_release`

AC_DEFINE_UNQUOTED(RASQAL_VERSION_MAJOR, $version_major, [Major version number])
AC_DEFINE_UNQUOTED(RASQAL_VERSION_MINOR, $version_minor, [Minor version number])
AC_DEFINE_UNQUOTED(RASQAL_VERSION_RELEASE, $version_release, [Release version number])
AC_DEFINE_UNQUOTED(RASQAL_VERSION_DECIMAL, $version_decimal, [Release version as a decimal])

# for rasqal-config.in
RASQAL_VERSION_MAJOR=$version_major
RASQAL_VERSION_MINOR=$version_minor
RASQAL_VERSION_RELEASE=$version_release
RASQAL_VERSION_DECIMAL=$version_decimal
AC_SUBST(RASQAL_VERSION)
AC_SUBST(RASQAL_VERSION_MAJOR)
AC_SUBST(RASQAL_VERSION_MINOR)
AC_SUBST(RASQAL_VERSION_RELEASE)
AC_SUBST(RASQAL_VERSION_DECIMAL)


dnl Checks for library functions.
AC_CHECK_FUNCS(getopt getopt_long stricmp strcasecmp vsnprintf srandomdev gmtime_r rand_r timegm)

AM_CONDITIONAL(STRCASECMP, test $ac_cv_func_stricmp = no -a $ac_cv_func_strcasecmp = no)
AM_CONDITIONAL(GETOPT, test $ac_cv_func_getopt = no -a $ac_cv_func_getopt_long = no)
AM_CONDITIONAL(TIMEGM, test $ac_cv_func_timegm = no)


AC_MSG_CHECKING(whether need to declare optind)
AC_TRY_LINK([#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
], [int x=optind;],
            AC_MSG_RESULT(no),
            AC_DEFINE(NEED_OPTIND_DECLARATION, 1, [need 'extern int optind' declaration?])
            AC_MSG_RESULT(yes))


AC_MSG_CHECKING(whether struct tm has tm_gmtoff field)
AC_TRY_COMPILE([#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
], [struct tm my_time; my_time.tm_gmtoff = 0;],
    have_tm_gmtoff="yes",
    have_tm_gmtoff="no")
AC_MSG_RESULT($have_tm_gmtoff)
if test $have_tm_gmtoff = "yes" ; then
  AC_DEFINE(HAVE_TM_GMTOFF, 1, [struct tm has tm_gmtoff field])
fi


# raptor V2 is REQUIRED; --enable-raptor2 option was removed
RAPTOR_MIN_VERSION=2.0.0


AC_CHECK_PROGS(PKG_CONFIG, pkg-config)
if test "X$PKG_CONFIG" = X; then
  AC_MSG_ERROR(pkg-config not found. configuring raptor V2 requires it.  Get it from http://pkg-config.freedesktop.org/)
fi

AC_MSG_CHECKING(for raptor)

if $PKG_CONFIG raptor2 --exists; then
  RAPTOR_VERSION=`$PKG_CONFIG raptor2 --modversion 2>/dev/null`
  RAPTOR_VERSION_DEC=`echo $RAPTOR_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
  AC_MSG_RESULT(system $RAPTOR_VERSION)
else
  AC_MSG_RESULT(not found)

  AC_MSG_ERROR(Raptor V2 is not installed - see http://librdf.org/raptor/ to get a version newer than $RAPTOR_MIN_VERSION)
fi

AC_SUBST(RAPTOR_VERSION_DEC)
AC_SUBST(RAPTOR_MIN_VERSION)


dnl Checks for regex libraries
have_regex_pcre=0
have_regex_posix=0
need_regex_pcre=0
need_regex_posix=0

AC_ARG_ENABLE(pcre,
	[  --disable-pcre                  Disable PCRE support (default auto)],
	[enable_pcre=$enableval], [enable_pcre="auto"])

if test "x$enable_pcre" != "xno" ; then

  AC_CHECK_PROG(PCRE_CONFIG, pcre-config, pcre-config)
  AC_ARG_WITH(pcre, [  --with-pcre-config=PATH     Location of PCRE pcre-config (auto)], pcre_config="$withval", pcre_config="")

  if test "X$pcre_config" != "X" ; then
    AC_MSG_CHECKING(for $pcre_config)

    if test -f $pcre_config ; then
      PCRE_CONFIG=$pcre_config
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no - searching PATH)
    fi
  fi
  if test "X$PCRE_CONFIG" = "X"; then
    AC_CHECK_PROGS(PCRE_CONFIG, pcre-config)
  fi


  AC_MSG_CHECKING(for pcre)
  PCRE_VERSION=`$PCRE_CONFIG --version 2>/dev/null`
  PCRE_MIN_VERSION=3.9

  PCRE_VERSION_DEC=`echo $PCRE_VERSION | $AWK -F. '{printf("%d\n", 100*$1 + $2)};'`
  PCRE_MIN_VERSION_DEC=`echo $PCRE_MIN_VERSION | $AWK -F. '{printf("%d\n", 100*$1 + $2)};'`
  if test "X$PCRE_VERSION" = X; then
    AC_MSG_RESULT(not present)
  elif test "X$PCRE_VERSION" -a $PCRE_VERSION_DEC -ge $PCRE_MIN_VERSION_DEC; then
    have_regex_pcre=1
    AC_MSG_RESULT($PCRE_VERSION)
  else
    AC_MSG_WARN($PCRE_VERSION - too old - need $PCRE_MIN_VERSION)
  fi

  if test $have_regex_pcre = 1; then
    AC_DEFINE(HAVE_REGEX_PCRE, 1, [have PCRE regex - Perl Compatible Regular Expressions])
  elif test "x$enable_pcre" = "xyes"; then
    AC_MSG_ERROR(PCRE requested but not found)
  fi

fi


AC_MSG_CHECKING(for posix regex library)
oLIBS="$LIBS"
if test $ac_cv_header_regex_h = yes; then
  AC_TRY_LINK([#include <sys/types.h>
              #include <regex.h>],
             [regex_t reg; char *pat="abc"; regcomp(&reg, pat, 0);],
             worked=yes, worked=no, worked=no)
  if test $worked = yes; then
    have_regex_posix=1
  fi
fi

LIBS="$oLIBS"

if test $have_regex_posix = 1; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_REGEX_PCRE, 1, [have PCRE regex - Perl Compatible Regular Expressions])
else
  AC_MSG_RESULT(no)
fi



AC_ARG_WITH(regex-library, [  --with-regex-library=NAME   Use regex library - posix, pcre (auto)], regex_library="$withval", regex_library="") 

for regex_library_name in $regex_library posix pcre; do
  case $regex_library_name in
    pcre)
      if test $have_regex_pcre = 1; then
        need_regex_pcre=1
        AC_DEFINE(RASQAL_REGEX_PCRE, 1, [Use PCRE regex library])
        break
      fi
      ;;

    posix)
      if test $have_regex_posix = 1; then
        need_regex_posix=1
        AC_DEFINE(RASQAL_REGEX_POSIX, 1, [Use posix regex library])
        break
      fi
      ;;

    *)
      AC_MSG_ERROR(No such regex library $regex_library_name)
      ;;
esac
done



AC_MSG_CHECKING(regex library to use)
regex_library=
if test $need_regex_pcre = 1; then
  regex_library=pcre
elif test $need_regex_posix = 1; then
  regex_library=posix
else
  regex_library=none
  AC_DEFINE(RASQAL_REGEX_NONE, 1, [Use no regex library])
  AC_MSG_WARN(No regex library available - please install pcre or a POSIX regex library)
fi
AC_MSG_RESULT($regex_library)


dnl Checks for message digest libraries
have_digest_mhash=no
have_digest_gcrypt=no
dnl always available
have_digest_internal=yes
need_digest_mhash=no
need_digest_gcrypt=no
need_digest_internal=no

AC_ARG_WITH(mhash,
        [  --with-mhash=DIR  mhash installation prefix (no default)],
	[mhash_dir=$withval], [mhash_dir=""])

oCPPFLAGS="$CPPFLAGS"
oLIBS="$LIBS"
if test "X$mhash_dir" != "X" ; then
  CPPFLAGS="$CPPFLAGS -I$mhash_dir/include"
  LIBS="$LIBS -L$mhash_dir/lib"
fi
LIBS="$LIBS -lmhash"

AC_CHECK_HEADERS(mhash.h)

AC_MSG_CHECKING(for mhash)

AC_TRY_LINK([
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION
#include <mhash.h>],
[ MHASH m; mhash_init(MHASH_MD5); ],
  have_digest_mhash=yes,
  have_digest_mhash=no)

CPPFLAGS="$LIBRDF_CPPFLAGS"
LIBS="$oLIBS"

AC_MSG_RESULT($have_digest_mhash)


AC_ARG_WITH(digest-library, [  --with-digest-library=NAME   Use message digest library - mhash, gcrypt, internal (auto)], digest_library="$withval", digest_library="")

for digest_library_name in $digest_library mhash gcrypt internal; do
  case $digest_library_name in
    mhash)
      if test $have_digest_mhash = yes; then
        need_digest_mhash=yes
        AC_DEFINE(RASQAL_DIGEST_MHASH, 1, [Use mhash library for message digests])
        break
      fi
      ;;

    gcrypt)
      if test $have_digest_gcrypt = yes; then
        need_digest_gcrypt=yes
        AC_DEFINE(RASQAL_DIGEST_GCRYPT, 1, [Use gcrypt library for message digests])
        break
      fi
      ;;

    internal)
      need_digest_internal=yes
      AC_DEFINE(RASQAL_DIGEST_INTERNAL, 1, [Use internal routines for message digests])
      break
      ;;

    *)
      AC_MSG_ERROR(No such message digest library $digest_library_name)
      ;;
esac
done

AC_MSG_CHECKING(message digest library to use)
digest_library=
if test $need_digest_mhash = yes; then
  digest_library=mhash
elif test $need_digest_gcrypt = yes; then
  digest_library=gcrypt
elif test $need_digest_internal = yes; then
  digest_library=internal
else
  digest_library=none
  AC_DEFINE(RASQAL_DIGEST_NONE, 1, [Use no message digest library])
  AC_MSG_WARN(No message digest library selected - no message digest support will work)
fi
AC_MSG_RESULT($digest_library)

AM_CONDITIONAL(RASQAL_DIGEST_INTERNAL, test $digest_library = internal)



have_libxml=0
need_libxml=0

AC_ARG_ENABLE(xml2,
	[  --disable-xml2                  Disable XML2 support (default auto)],
	[enable_xml2=$enableval], [enable_xml2="auto"])

if test "x$enable_xml2" != "xno" ; then

  AC_ARG_WITH(xml2-config, [  --with-xml2-config=PATH     Location of libxml xml2-config ()], xml2_config="$withval", xml2_config="")

  if test "X$xml2_config" != "X" ; then
    AC_MSG_CHECKING(for $xml2_config)

    if test -f $xml2_config ; then
      XML_CONFIG=$xml2_config
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no - searching PATH)
    fi
  fi
  if test "X$XML_CONFIG" = "X"; then
    AC_CHECK_PROGS(XML_CONFIG, xml2-config)
  fi


  LIBXML_MIN_VERSION=2.6.8
  AC_MSG_CHECKING(for libxml2 library)
  oCPPFLAGS="$CPPFLAGS"
  oLIBS="$LIBS"
  if test "X$XML_CONFIG" != X; then
    LIBS="$LIBS `$XML_CONFIG --libs`"
    CPPFLAGS="`$XML_CONFIG --cflags` $CPPFLAGS"
    LIBXML_VERSION=`$XML_CONFIG --version`
    libxml_version_dec=`echo $LIBXML_VERSION | awk -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
    LIBXML_MIN_VERSION_dec=`echo $LIBXML_MIN_VERSION | awk -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
    AC_MSG_RESULT(yes - version $LIBXML_VERSION)
    if test $libxml_version_dec -lt $LIBXML_MIN_VERSION_dec; then
       AC_MSG_WARN(Using libxml $LIBXML_VERSION is unsupported - $LIBXML_MIN_VERSION or newer required.)
    else
      AC_CHECK_HEADERS(libxml/xmlschemas.h libxml/xmlschemastypes.h libxml/schemasInternals.h)
      if test "$ac_cv_header_libxml_xmlschemas_h" = no; then
        AC_MSG_WARN(libxml library found but not xml schema headers - disabling)
      else
        have_libxml=1
      fi
    fi
  else
    AC_MSG_RESULT(no)
  fi
  CPPFLAGS="$oCPPFLAGS"
  LIBS="$oLIBS"

  if test $have_libxml = 0 && test "x$enable_xml2" = "xyes" ; then
    AC_MSG_ERROR(libxml2 requested but not found)
  fi

fi


dnl RDF Query Languages
rdql_query_language=no
sparql_query_language=no
laqrs_query_language=no

rdf_query_languages_available="rdql sparql laqrs"
rdf_query_languages_enabled=


# This is needed because autoheader can't work out which computed
# symbols must be pulled from acconfig.h into config.h.in
if test "x" = "y"; then
  AC_DEFINE(RASQAL_QUERY_RDQL, 1, [Building RDQL query])
  AC_DEFINE(RASQAL_QUERY_LAQRS, 1, [Building LAQRS query])
  AC_DEFINE(RASQAL_QUERY_SPARQL, 1, [Building SPARQL query])
fi

AC_MSG_CHECKING(query languages required)
AC_ARG_ENABLE(query-languages, [  --enable-query-languages=LIST   Enable query languages (default=all)], query_languages="$enableval") 
if test "X$query_languages" = Xall -o "X$query_languages" = X; then
  query_languages="$rdf_query_languages_available"
  AC_MSG_RESULT(all)
else
  AC_MSG_RESULT($query_languages)
fi

for query_language in $query_languages; do
  p=$query_language
  eval $p'_query_language=yes'
  NAME=`echo $p | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
  n=RASQAL_QUERY_${NAME}
  AC_DEFINE_UNQUOTED($n)
  rdf_query_languages_enabled="$rdf_query_languages_enabled $query_language"
done

AM_CONDITIONAL(RASQAL_QUERY_RDQL, test $rdql_query_language = yes)
AM_CONDITIONAL(RASQAL_QUERY_LAQRS, test $laqrs_query_language = yes)
AM_CONDITIONAL(RASQAL_QUERY_SPARQL, test $sparql_query_language = yes)



RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS `$PKG_CONFIG raptor2 --cflags`"
RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `$PKG_CONFIG raptor2 --libs`"
RAPPER_PATH=""

if test $need_regex_pcre = 1; then
  C=`$PCRE_CONFIG --cflags`
  L=`$PCRE_CONFIG --libs`
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $C"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $L"

  PKGCONFIG_CFLAGS="$PKGCONFIG_CFLAGS $C"
  PKGCONFIG_LIBS="$PKGCONFIG_LIBS $L"
  unset C
  unset L
fi


if test $need_digest_mhash = yes; then
  C=""
  L="-lmhash"
  if test "Xmhash_dir" != "X" ; then
    C="-I$mhash_dir/include"
    L="-L$mhash_dir/lib $L"
  fi

  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $C"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $L"

  unset C
  unset L
fi


mpfr_lib_dir=
mpfr_include_dir=
AC_ARG_WITH(mpfr, [  --with-mpfr=DIR          MPFR install area], mpfr_prefix="$withval", mpfr_prefix="none") 
if test "X$mpfr_prefix" != "Xnone" ; then
  mpfr_libs="-L$mpfr_prefix/lib"
  mpfr_includes="-I$mpfr_prefix/include"
fi

oCPPFLAGS="$CPPFLAGS"
AC_CHECK_HEADERS(gmp.h)
CPPFLAGS="$oCPPFLAGS"

CPPFLAGS="$CPPFLAGS $mpfr_includes"
AC_CHECK_HEADERS([mpfr.h], [], [], [
#ifdef HAVE_GMP_H
#include <gmp.h>
#endif
])
CPPFLAGS="$oCPPFLAGS"


have_mpfr=0
have_gmp=0

oCPPFLAGS="$CPPFLAGS"
oLIBS="$LIBS"
if test "$ac_cv_header_mpfr_h" = yes; then
  CPPFLAGS="$CPPFLAGS $mpfr_includes"
  LIBS="$LIBS $mpfr_libs -lmpfr"
  AC_CHECK_FUNC(mpfr_init, have_mpfr_init=yes, have_mpfr_init=no)
  # Several functions are used that are not in GMP version of MPFR:
  #   mpfr_get_default_rounding_mode mpfr_zero_p mpfr_equal_p mpfr_free_str
  # All added in MPFR 2.1.0 released 3 Nov 2004
  # http://www.mpfr.org/mpfr-2.1.0/
  # so just test for one.
  AC_CHECK_FUNC(mpfr_zero_p, have_mpfr_zero_p=yes, have_mpfr_zero_p=no)
  LIBS="$oLIBS"
  CPPFLAGS="$oCPPFLAGS"
  AC_MSG_CHECKING(for working MPFR library)
  if test $have_mpfr_init = yes -a $have_mpfr_zero_p = yes; then
    have_mpfr=1
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
fi

have_gmp_version=0
if test "$ac_cv_header_gmp_h" = yes; then
  CPPFLAGS="$CPPFLAGS "
  LIBS="$LIBS -lgmp"
  AC_MSG_CHECKING(for working GNU MP library)
  AC_TRY_LINK([#include <gmp.h>], [mpz_t n; mpz_init(n);],
              have_gmp=1
	      AC_MSG_RESULT(yes),
	      AC_MSG_RESULT(no))
  AC_MSG_CHECKING(for gmp_version)
  AC_TRY_LINK([#include <gmp.h>], [const char* v=gmp_version; ],
              have_gmp_version=1
	      AC_MSG_RESULT(yes),
	      AC_MSG_RESULT(no))
  LIBS="$oLIBS"
  CPPFLAGS="$oCPPFLAGS"
fi

if test $have_gmp_version = 1; then
  AC_DEFINE(HAVE_GMP_VERSION, 1, [Have gmp_version constant])
fi

need_mpfr=0
need_gmp=0

AC_ARG_WITH(decimal, [  --with-decimal=NAME           Use decimal library - mpfr (default), gmp, none], decimal="$withval", decimal="mpfr") 
for decimal_name in $decimal mpfr gmp none; do
  case $decimal_name in
    mpfr)
      if test $have_mpfr = 1; then
        need_mpfr=1
        AC_DEFINE([RASQAL_DECIMAL_MPFR], 1, [Decimal from MPFR library])
        break
      fi
      ;;

    gmp)
      if test $have_gmp = 1; then
        need_gmp=1
	AC_DEFINE([RASQAL_DECIMAL_GMP], 1, [Decimal from GNU MP library])
        break
      fi
      ;;

    none)
      need_mpfr=0
      need_gmp=0
      AC_DEFINE([RASQAL_DECIMAL_NONE], 1, [Decimal without a library])
      break
      ;;

    *)
      AC_MSG_ERROR(No such decimal library $decimal_name)
      ;;
esac
done


AC_MSG_CHECKING(decimal libraries available)
decimal_libraries_available=
if test $have_mpfr = 1; then
  decimal_libraries_available="$decimal_libraries_available mpfr"
fi
if test $have_gmp = 1; then
  decimal_libraries_available="$decimal_libraries_available gmp"
fi
AC_MSG_RESULT($decimal_libraries_available)


need_libm=0
AC_MSG_CHECKING(decimal library to use)
decimal_library=
if test $need_mpfr = 1; then
  decimal_library="mpfr $MPFR_VERSION"
elif test $need_gmp = 1; then
  decimal_library="gmp $GMP_VERSION"
else
  decimal_library=none
fi
AC_MSG_RESULT($decimal_library)

dnl Always need ceil, floor and round in rasqal_literal_ceil() etc.
AC_MSG_CHECKING([for ceil, floor and round in default libs])
ceil_lib=none
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_MATH_H
#include <math.h>
#endif
]], [[
double d = ceil(1.0F) + floor(1.0F) + round(1.0F);
]])],
   AC_MSG_RESULT(yes)
   ceil_lib=libc,
   AC_MSG_RESULT(no),
   ceil_lib=none)
if test $ceil_lib = none; then
  LIBS="$LIBS -lm"
  AC_MSG_CHECKING([for ceil, floor and round with -lm])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_MATH_H
#include <math.h>
#endif
]], [[
double d = ceil(1.0F) + floor(1.0F) + round(1.0F);
]])],
     AC_MSG_RESULT(yes)
     need_libm=1
     ceil_lib=libm,
     AC_MSG_RESULT(no),
     ceil_lib=none)
  LIBS="$oLIBS"
fi
if test $ceil_lib = none; then
  AC_MSG_ERROR([Could not find ceil, floor, round in default libs or with -lm])
fi
if test $need_libm = 1; then
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS -lm"
fi


DECIMAL_INCLUDES=
DECIMAL_LIBS=
if test $need_mpfr = 1; then
  DECIMAL_INCLUDES="$mpfr_includes"
  DECIMAL_LIBS="$mpfr_libs -lmpfr"
fi
if test $need_gmp = 1; then
  DECIMAL_LIBS="-lgmp"
fi
RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $DECIMAL_INCLUDES"
RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $DECIMAL_LIBS"


if test $need_libxml = 1; then
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS `$XML_CONFIG --cflags`"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `$XML_CONFIG --libs`"
  if test "X$PKGCONFIG_REQUIRES" != X; then
    PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES,"
  fi
  PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES libxml-2.0 >= $LIBXML_MIN_VERSION"
fi


# Make final changes to cflags
MEM=
MEM_LIBS=
CPPFLAGS="-DRASQAL_INTERNAL=1 $CPPFLAGS"

debug_messages=no
if test "$USE_MAINTAINER_MODE" = yes; then
  debug_messages=yes
fi

AC_ARG_ENABLE(debug, [  --enable-debug          Enable debug messages (default no).  ], debug_messages=$enableval)
if test "$debug_messages" = "yes"; then
  CPPFLAGS="-g -DRASQAL_DEBUG=1 $CPPFLAGS"
fi

AC_ARG_WITH(memory-signing, [  --with-memory-signing       Sign allocated memory (default=no)], use_memory_signing="$withval", use_memory_signing="no") 
AC_MSG_CHECKING(using memory signing)
AC_MSG_RESULT($use_memory_signing);
if test $use_memory_signing = yes; then
  MEM=-DRASQAL_MEMORY_SIGN=1
  MEM_LIBS=
fi

STANDARD_CFLAGS="$STANDARD_CFLAGS $CFLAGS"
if test "$USE_MAINTAINER_MODE" = yes; then
  CPPFLAGS="$MAINTAINER_CPPFLAGS $CPPFLAGS"
fi

AC_SUBST(RASQAL_EXTERNAL_LIBS)

AC_SUBST(RASQAL_INTERNAL_CPPFLAGS)
AC_SUBST(RASQAL_INTERNAL_LIBS)
AC_SUBST(RAPPER_PATH)

AC_SUBST(MEM)
AC_SUBST(MEM_LIBS)
AC_SUBST(STANDARD_CFLAGS)

AC_SUBST(RECHO)
AC_SUBST(RECHO_N)
AC_SUBST(RECHO_C)

RASQAL_LIBTOOLLIBS=librasqal.la
AC_SUBST(RASQAL_LIBTOOLLIBS)

AC_SUBST(PKGCONFIG_REQUIRES)
AC_SUBST(PKGCONFIG_LIBS)
AC_SUBST(PKGCONFIG_CFLAGS)

abs_top_srcdir=`cd $srcdir; pwd`
AC_SUBST(abs_top_srcdir)
abs_top_builddir=`pwd`
AC_SUBST(abs_top_builddir)

dnl automake 1.11
AM_SILENT_RULES([no])

AC_CONFIG_FILES([Makefile
data/Makefile
docs/Makefile
docs/version.xml
scripts/Makefile
src/Makefile
src/rasqal.h
src/win32_rasqal_config.h
tests/Makefile
tests/algebra/Makefile
tests/engine/Makefile
tests/rdql/Makefile
tests/rdql/testsuite/Makefile
tests/laqrs/Makefile
tests/laqrs/syntax/Makefile
tests/sparql/Makefile
tests/sparql/bugs/Makefile
tests/sparql/simple/Makefile
tests/sparql/part1/Makefile
tests/sparql/examples/Makefile
tests/sparql/syntax/Makefile
tests/sparql/bound/Makefile
tests/sparql/survey/Makefile
tests/sparql/SyntaxFull/Makefile
tests/sparql/sort/Makefile
tests/sparql/ValueTesting/Makefile
tests/sparql/regex/Makefile
tests/sparql/Expr1/Makefile
tests/sparql/Expr2/Makefile
tests/sparql/ExprBuiltins/Makefile
tests/sparql/ExprEquals/Makefile
tests/sparql/SyntaxDev/Makefile
tests/sparql/SyntaxDev/Syntax-SPARQL/Makefile
tests/sparql/SyntaxDev/Syntax-SPARQL2/Makefile
tests/sparql/SyntaxDev/Syntax-SPARQL3/Makefile
tests/sparql/update/Makefile
tests/sparql/aggregate/Makefile
tests/sparql/sparql11/Makefile
tests/sparql/federated/Makefile
tests/sparql/warnings/Makefile
utils/Makefile
win32/Makefile
rasqal.spec
rasqal.rdf
rasqal.pc])
AC_CONFIG_FILES([src/rasqal-config], [chmod +x src/rasqal-config])
AC_CONFIG_FILES([rasqal-src-config], [chmod +x rasqal-src-config])

dnl Check for gtk-doc and docbook
GTK_DOC_CHECK([1.3])

AC_OUTPUT

AC_MSG_RESULT([
Rasqal build summary:
  RDF query languages available : $rdf_query_languages_available
  RDF query languages enabled   :$rdf_query_languages_enabled
  Decimal library               : $decimal_library
  Regex library                 : $regex_library
  Message digest library        : $digest_library
])
