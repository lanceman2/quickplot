# Quickplot - an interactive 2D plotter
#
# Copyright (C) 1998-2011  Lance Arsenault

# This file is part of Quickplot.
#
# Quickplot is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# Quickplot is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Quickplot.  If not, see <http://www.gnu.org/licenses/>.
#


# require autoconf version
AC_PREREQ(2.59)

#################################################################
############### Configure a release here ########################
#################################################################


# AC_INIT( PACKAGE, VERSION, BUGREPORT, TARNAME , URL)
AC_INIT([quickplot], 1.0.1rc,
        [http://quickplot.sourceforge.net/],
        [quickplot],
        [http://quickplot.sourceforge.net/])


# LIB_VERSION=LIB_CURRENT:LIB_REVISION:LIB_AGE

LIB_CURRENT=0
LIB_REVISION=1
LIB_AGE=0

#
# See LIB_VERSION notes at the very bottom of this file
#


#################################################################
############## END: Configure a release here ####################
#################################################################


# Stuff that may be configured to change constants that
# get compiled in.


NUMBER_OF_PLOTS="12" # default number of plots at startup


# if auto scale is set this is the factor that the span of the
# plots values may differ by to be on the same scales
SCALE_FACTOR="50"


DEFAULT_GRID_FONT="Sans 10"



################################################################
# setting many variables before  AC_PROG_*
################################################################


#AM_INIT_AUTOMAKE(option1 option2 ...)
AM_INIT_AUTOMAKE(dist-bzip2 dist-xz)
AC_CONFIG_MACRO_DIR([m4])

AM_CONFIG_HEADER(config.h)
AC_PROG_CC_C_O
AM_PROG_CC_C_O

AC_PROG_LIBTOOL
AC_LANG(C)

#################################################################

LIB_VERSION="${LIB_CURRENT}:${LIB_REVISION}:${LIB_AGE}"

################################################################

# initialize the error flag
error=no


AC_ARG_ENABLE([developer],
    AS_HELP_STRING([--enable-developer],
                   [if you wish to build and edit Quickplot source image\
 files and Quickplot documentation files enabling this option will add\
 addition make file targets which are built using the programs lynx and\
 the ImageMagick program convert.  This option is required if you are\
 building Quickplot from the repository source.\
 (default is no developer targets).]),
 [enable_developer="$enableval"],
 [enable_developer=no])

USE_LYNX=no
USE_CONVERT=no

if test "$enable_developer" != "no"; then
  USE_LYNX=yes
  USE_CONVERT=yes
  AC_CHECK_PROG(HAVE_LYNX, lynx, [yes], [no])
  AC_CHECK_PROG(HAVE_CONVERT, convert, [yes], [no])
  
  if test x$HAVE_LYNX != xyes; then
    error=yes
    AC_MSG_WARN([
        lynx is required with --enable-developer
        homepage: http://lynx.isc.org/])
  fi
  if test x$HAVE_CONVERT != xyes; then
    error=yes
    AC_MSG_WARN([
        convert from ImageMagick is required with --enable-developer
        homepage: http://www.imagemagick.org/])
  fi
fi

AM_CONDITIONAL([USE_LYNX], [test x$USE_LYNX = xyes])
AM_CONDITIONAL([USE_CONVERT], [test x$USE_CONVERT = xyes])


AC_CHECK_HEADERS(math.h)

AC_CHECK_FUNCS(
vsnprintf \
powl
)

m4_include([ax_lib_readline.m4])
AX_LIB_READLINE
if test "$ax_cv_lib_readline" != "no"; then
  readline_LIBS="$ax_cv_lib_readline"
  AC_SUBST(readline_LIBS)
else
  AC_MSG_WARN([
        libreadline version >= 6.2 is required
        homepage: http://tiswww.case.edu/php/chet/readline/rltop.html])
  error=yes
fi
#AM_CONDITIONAL([HAVE_READLINE], [test "$ax_cv_lib_readline" != "no"])


PKG_CHECK_MODULES([gtk_3], [gtk+-3.0 >= 3.0.12],
    [],
    [error=yes
     AC_MSG_WARN([
        gtk+-3.0 version >= 3.0.12 is required
        homepage: http://www.gtk.org/])
    ]
)

PKG_CHECK_MODULES([sndfile], [sndfile >= 1.0.5],
    [],
    [error=yes
     AC_MSG_WARN([
        lib sndfile version >= 1.0.5 is required
        homepage: http://www.mega-nerd.com/libsndfile/])
    ]
)


################################################################
#                 --enable-debug
################################################################

AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug],
    [compile more debugging code into quickplot and libquickplot\
 (default is no debugging code).  You do not want this unless\
 you are developing quickplot.  This is unrelated to debug compile\
 options.  Adding this option makes Quickplot become Slowplot.]),
 [enable_debug="$enableval"],
 [enable_debug=no])

case "$enable_debug" in
    y* | Y* )
    debug=yes
    ;;
    * )
    debug=no
    ;;
esac


################################################################
#    test for gnu/lib-names.h
################################################################

HAVE_GNU_LIB_NAMES_H=no

# This is so we can see if the file gnu/lib-names.h is present and
# works so that we can make the libraries executable. See that this
# compiles will be a good enough test.  Used in
# libquickplot/lib_main.c
AC_MSG_CHECKING([if gnu/lib-names.h is present and works\
 so we can make libraries executable])

# Below the real code requires a "const char my_interp[] __att.... "
# etc, but I don't know how to make a '[' in an autoconf string.

AC_RUN_IFELSE([AC_LANG_SOURCE([[

#include <gnu/lib-names.h>

#ifdef __i386__
# define LIBDIR "/lib/"
#endif

#ifdef __x86_64__
# define LIBDIR "/lib64/"
#endif

#ifndef LIBDIR
# error "The compiler did not define __i386__ or __x86_64__"
#endif

const char my_interp[] __attribute__
    ((section(".interp")))= LIBDIR LD_SO;

int main(void)
{
  /* do something so the compiler does not give a warning */
  char *ptr = (char *) my_interp;
  ptr += 1;
  return 0;
}
]])],
  [AC_MSG_RESULT([yes])]
  HAVE_GNU_LIB_NAMES_H=yes,
  [AC_MSG_RESULT([no])],
  [AC_MSG_RESULT([CROSS-COMPILING])]
)

AM_CONDITIONAL([HAVE_GNU_LIB_NAMES_H],
               [test x$HAVE_GNU_LIB_NAMES_H = xyes])


#################################################################

# AC_SUBST() go into Makefile.am files and other .in files
# AC_DEFINE*() go into config.h

# replace @STYLE_CSS@ with the file style.css
STYLE_CSS="${srcdir}/style.css"
AC_SUBST_FILE(STYLE_CSS)

# replace @FOOTER_HTM@ with the file footer.htm
FOOTER_HTM="footer.htm"
AC_SUBST_FILE(FOOTER_HTM)

RELEASE_DATE="`${srcdir}/release_date.bash ${srcdir}/RELEASE_DATE`"
AC_SUBST(RELEASE_DATE)

AC_SUBST(DEFAULT_GRID_FONT)
AC_DEFINE_UNQUOTED(DEFAULT_GRID_FONT, "$DEFAULT_GRID_FONT",
        [The default font used when drawing the numbers on the grid])



AC_DEFINE_UNQUOTED(LIB_VERSION, "$LIB_VERSION",
  [Quickplot Library version number])

AC_SUBST(LIB_VERSION)
AC_SUBST(LIB_CURRENT)
AC_SUBST(LIB_REVISION)
AC_SUBST(LIB_AGE)

AM_CONDITIONAL([QP_DEBUG], [test x$debug = xyes])

if test x$debug == xyes ; then
  AC_DEFINE_UNQUOTED(QP_DEBUG,[1],
    [If QP_DEBUG is defined we add extra debug \
code to quickplot and libquickplot.])
fi


AC_SUBST(SCALE_FACTOR)
AC_DEFINE_UNQUOTED(SCALE_FACTOR, $SCALE_FACTOR,
    [Factor switch plot values may differ to be \
on the same scale when auto scale is selected])



AC_SUBST(NUMBER_OF_PLOTS)
AC_DEFINE_UNQUOTED(NUMBER_OF_PLOTS, $NUMBER_OF_PLOTS,
    [The default maximum number of plot per file])


AM_CONDITIONAL([IS_SVN_CO], [test -f .git/config])

AM_CONDITIONAL([HAVE_LOCAL_SCREENSHOTS], [test -f screenshots.html.in])




#################################################################
# We show all warnings to the user before erroring out.  This
# beats having the user fix one error in order to get to the next.
# Instead we show all the warnings (errors) that we can.

if test x$error = xyes ; then
  AC_MSG_ERROR([

  Failed to configure quickplot version $VERSION.
  See above WARNING(s).

])
fi

#################################################################

AC_CONFIG_FILES(
Makefile
quickplot.pc
footer.htm
about.htm
help_head.htm
help_tail.htm
index.htm
quickplot.1.head
quickplot.1.tail
quickplot_shell.1
)

AC_OUTPUT

# you can add spew here

AC_MSG_RESULT([
  ------------------------------------------------------------
    done configuring: $PACKAGE_NAME  version $VERSION
  ------------------------------------------------------------

               extra debug code: $debug
        extra developer targets: $enable_developer

                C Compiler (CC): $CC
      C Compiler Flags (CFLAGS): $CFLAGS
       C Linker Flags (LDFLAGS): $LDFLAGS

   Installation prefix (prefix): $prefix
                         srcdir: $srcdir
])


######################################################################
#                     LIB_VERSION notes
######################################################################

#  Updating library version interface information
#  (from libtool version 1.4.3 info page)

# GNU LibTool library interface numbers
#
# CURRENT
#     The most recent interface number that this library implements.
#
# REVISION
#     The implementation number of the CURRENT interface.
#
# AGE
#     The difference between the newest and oldest interfaces that
#     this library implements.  In other words, the library implements
#     all the interface numbers in the range from number `CURRENT -
#     AGE' to `CURRENT'.
#

#
#  1. Start with version information of `0:0:0' for each libtool
#     library.
#
#  2. Update the version information only immediately before a public
#     release of your software.  More frequent updates are
#     unnecessary, and only guarantee that the current interface
#     number gets larger faster.  We do it just after a release.
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
#  6. If any interfaces have been removed since the last public
#     release, then set AGE to 0.
#
