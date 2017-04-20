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

/* This file will not be used if --enable-debug was not set
 * as a ./configure option.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>

#include "config.h"
#include "spew.h"
#include "term_color.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


static int spew_level = -1;
static FILE *spew_file = NULL;
static int last_spewed = 0; /* bool */

int qp_set_spew_level(int level)
{
  int old_level = spew_level;
  spew_level = level;
  return old_level;
}

int qp_get_spew_level(void)
{
  return spew_level;
}

void qp_spew_init(int level)
{
  char *env;

  qp_term_color_init();


  spew_file = stdout; /* default value */
#ifdef QP_DEBUG
  spew_level = 0; /* DEBUG default value */
#else
  spew_level = 2; /* non-DEBUG default value */
#endif

  env = getenv("QUICKPLOT_SPEW_FILE");

  if(env && env[0])
  {
    if(strncasecmp(env, "none", 4) == 0 ||
        env[0] == '\0')
    {
      spew_level = 5; /* OFF */
      spew_file = NULL;
    }
    else if(strncasecmp(env, "stdout", 4) == 0 ||
        strncasecmp(env, "out", 1) == 0 ||
        env[0] == '1')
      ;
    else if(strncasecmp(env, "stderr", 4) == 0 ||
        strncasecmp(env, "err", 1) == 0 ||
        env[0] == '2')
      spew_file = stderr;
    else
    {
      spew_file = fopen(env, "a");
      if(!spew_file)
        spew_file = stdout;
    }
  }

  env = getenv("QUICKPLOT_SPEW_LEVEL");

  if(spew_level == 5 /* NO SPEW */)
    env = NULL;

  if(env && env[0])
  {
    if(strncasecmp(env, "off", 2) == 0 ||
        strcasecmp(env, "no") == 0)
      spew_level = 5; /* NO SPEW */
    else if( strncasecmp(env, "error", 1) == 0 ||
        env[0] == '4')
      spew_level = 4; /* ERROR */
    else if(strncasecmp(env, "on", 1) == 0 ||
        strncasecmp(env, "info", 1) == 0 ||
        strncasecmp(env, "yes", 1) == 0 ||
        strncasecmp(env, "debug", 1) == 0 ||
        /* there is no debug level so we set
         * debug to info */
        env[0] == '1')
      spew_level = 1; /* INFO */
    else if(strncasecmp(env, "notice", 3) == 0 ||
        env[0] == '2')
      spew_level = 2; /* NOTICE */
    else if(strncasecmp(env, "debug", 1) == 0 ||
        env[0] == '0')
      spew_level = 1; /* INFO, there is no DEBUG */
    else if(strncasecmp(env, "warn", 1) == 0||
        env[0] == '3')
      spew_level = 3; /* WARN */
  }
  else if(level >= 0)
    spew_level = level;
}


void qp_spew(int level, int show_errno, const char *format, ...)
{
  if(spew_level == -1)
    qp_spew_init(-1);

  if(level > 4)
    level = 4;
  else if(level < 1)
    level = 1;

  if(level >= spew_level)
  {
    va_list ap;
    last_spewed = 1;

    
    if(level == 2)
      fprintf(spew_file, "%sQP:%s ", blu, trm);
    else if(level == 3)
      fprintf(spew_file, "%sQP:%s ", byel, trm);
    else if(level == 4)
      fprintf(spew_file, "%sQP:%s ", bred, trm);
    else
      fprintf(spew_file, "QP: ");

    va_start(ap, format);
    vfprintf(spew_file, format, ap);
    fflush(spew_file);
    va_end(ap);

    if(show_errno)
    {
      char errstr[128];
      /* thread safe strerror() */
      strerror_r(errno, errstr, 128);
      fprintf(spew_file, " errno=%d:\"%s\"\n", errno, errstr);
    }

  }
  else
    last_spewed = 0;
}

void qp_spew_append(const char *format, ...)
{
  if(last_spewed)
  {
    va_list ap;
    va_start(ap, format);
    vfprintf(spew_file, format, ap);
    fflush(spew_file);
    va_end(ap);
  }
}

