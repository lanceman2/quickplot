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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "quickplot.h"

#include "config.h"
#include "debug.h"
#include "spew.h"
#include "list.h"
#include "qp.h"
#include "get_opt.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


struct qp_gtk_options
{
  int argc;
  char **argv;
};


static
inline
void strip_opt(struct qp_gtk_options *gop, 
    int from_i, int to_i, int *argc, char ***argv, char *op)
{
  int num, i;

  num = to_i - from_i + 1;

  gop->argc += num;

  gop->argv = qp_realloc(gop->argv, sizeof(char *)*(gop->argc+1));

  for(i=0;i<num;++i)
    gop->argv[gop->argc-num + i] = (*argv)[from_i + i];
  gop->argv[gop->argc] = NULL;


  for(i=from_i;i<=((*argc)-num);++i)
    (*argv)[i] = (*argv)[i + num];
  *argc -= num;
  
}

struct qp_gtk_options *strip_gtk_options(int *argc, char ***argv)
{
  struct qp_gtk_options *opt = NULL;
  int i = 1;
  char *with_arg[] =
  { 
    "--class",
    "--display",
    "--gdk-debug",
    "--gdk-no-debug",
    "--gtk-debug",
    "--gtk-module",
    "--gtk-no-debug",
    "--gxid-host",
    "--gxid-port",
    "--name",
    "--screen",
    0
  };
  char *no_arg[] =
  {
    "--g-fatal-warnings",
    "--sync",
    0
  };


  opt = qp_malloc(sizeof(*opt));

  opt->argc = 1;
  opt->argv = qp_malloc(sizeof(char *)*2);
  opt->argv[0] = (*argv)[0];
  opt->argv[1] = NULL;

  while(i < *argc)
  {
    char **op;
    int j;
    j = i;

    op = with_arg;
    while(*op)
    {
      if(get_opt(0, *op, *argc, *argv, &i))
      {
        strip_opt(opt, j, i-1, argc, argv, *op);
        i = j;
        break;
      }
      ++op;
    }

    if(*op) continue;

    op = no_arg;
    while(*op)
    {
      if(!strcmp(*op, (*argv)[i]))
      {
        strip_opt(opt, j, i, argc, argv, *op);
        /* no need to increment i given we removed
         * that index from the argv  array */
        break;
      }
      ++op;
    }
    if(!(*op))
      ++i;
  }

#if 1
#ifdef QP_DEBUG
  {
    char **a;
    DEBUG("%d ARGS left now are:\n", *argc);
    for(a=(*argv);*a;++a)
      APPEND("%s ", *a);
    APPEND("%s\n", *a);
  }
#endif
#endif

  return opt;
}


int qp_gtk_init_check(struct qp_gtk_options *opt)
{
  int ret;

  ASSERT(opt);

#if 1
#ifdef QP_DEBUG
  {
    char **argv;
    DEBUG("%d GTK ARGS are:\n", opt->argc);
    for(argv = opt->argv;*argv;++argv)
      APPEND("%s ", *argv);
    APPEND("%s\n", *argv);
  }
#endif
#endif

  ret = qp_app_init(&opt->argc, &opt->argv);

  if(opt->argv);
    free(opt->argv);
  free(opt);

  return ret; /* success */
}
