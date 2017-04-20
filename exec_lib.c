/*
  Quickplot - an interactive 2D plotter

  Copyright (C) 1998-2011  Lance Arsenault


  This file is part of Quickplot.

  Quickplot is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  Quickplot is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Quickplot.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdlib.h>
#include <gnu/lib-names.h>

#include <gtk/gtk.h>
#include <sndfile.h>
#include "config.h"

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else /* !defined(HAVE_READLINE_H) */
#    undef HAVE_LIBREADLINE
#  endif
#endif

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


/* This is the stuff that will tell the compiler to make this library
 * one that can be executed. */
#ifdef __i386__
# define LIBDIR  "/lib/"
#endif

#ifdef __x86_64__
# define LIBDIR  "/lib64/"
#endif


#ifndef LIBDIR
# error "The compiler did not define __i386__ or __x86_64__"
#endif

const char my_interp[] __attribute__((section(".interp"))) = LIBDIR LD_SO;



/* Here's what to execute when this library is executed. */
void qp_exec_lib(void)
{
  char *sndfile_version;
  sndfile_version = (char *) sf_version_string();
  while(*sndfile_version && (*sndfile_version < '0' || *sndfile_version > '9'))
      ++sndfile_version;

  printf(" =============================================\n"
         "    " PACKAGE_NAME "  version: " VERSION     "\n"
         " =============================================\n"
         "\n"
         "  Compiled ("__FILE__") on date: "__DATE__ "\n"
         "                             at: "__TIME__ "\n"
	 "\n"
	 "       libquickplot version: " LIB_VERSION     "\n"
         "\n"
         "  Was built with\n"
         "               GTK+ version: %d.%d.%d\n"
         "         libsndfile version: %s\n"
#ifdef HAVE_LIBREADLINE
         "        libreadline version: %d.%d\n"
#endif
         " ------------------------------------------------\n"
#ifdef QP_DEBUG
         "    QP_DEBUG was defined in this build.\n"
#else    
         "    QP_DEBUG was NOT defined in this build.\n"
#endif
         " ------------------------------------------------\n"
         "\n"
         "  The Quickplot homepage is:\n"
         "   "PACKAGE_URL "\n"
         "\n"
         "  Copyright (C) 1998-2011 - Lance Arsenault\n"
         "  This is free software licensed under the GNU GPL (v3).\n",
          GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
          sndfile_version
#ifdef HAVE_LIBREADLINE
          , RL_VERSION_MAJOR, RL_VERSION_MINOR
#endif
         );

  /* This needs to call exit and not return 0 since this is not main()
   * and main is not called. */
  exit(0);
}

