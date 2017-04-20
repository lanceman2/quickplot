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

#include "config.h"
#include "debug.h"
#include "term_color.h"
#include "list.h"

#ifndef QP_DEBUG
#  error This file should not be complied with QP_DEBUG not defined
#endif


#ifdef DMALLOC
#  include "dmalloc.h"
#endif

static int spew_level = -1;
static FILE *spew_file = NULL;
static int last_spewed = 0; /* bool */


struct flag
{
  char *file;
  int level;
  int line;
};

/* list of spew levels for a given source code file */
static struct qp_sllist *file_spew_list = NULL;


static char *color[] =
{ 
  "\033[1;032m", /* DEBUG   0 */
  "\033[1;036m", /* INFO    1 */
  "\033[1;035m", /* NOTICE  2 */
  "\033[1;033m", /* WARN    3 */
  "\033[1;031m"  /* ERROR   4 */
};

static const char * const prefix[] =
{ 
  "QP_DEBUG",  /* 0 */
  "QP_INFO",   /* 1 */
  "QP_NOTICE", /* 2 */
  "QP_WARNING",/* 3 */
  "QP_ERROR"   /* 4 */
};


static void _qp_spew_init(void)
{
  char *env;

  spew_file = stdout; /* default value */
  spew_level = 2; /* default value */

  env = getenv("SPEW_FILE");
  if(!env)
    env = getenv("QP_SPEW_FILE");

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


  env = getenv("SPEW_LEVEL");
  if(!env)
    env = getenv("QP_SPEW_LEVEL");

  if(spew_level == 5 /* if NO SPEW */)
    env = NULL;

  if(env && env[0])
  {
    if(strncasecmp(env, "off", 2) == 0 ||
        strcasecmp(env, "no") == 0 ||
        env[0] == '5' || env[0] == '6' ||
        env[0] == '7' || env[0] == '8' ||
        env[0] == '9')
      spew_level = 5; /* NO SPEW */
    else if( strncasecmp(env, "error", 1) == 0 ||
        env[0] == '4')
      spew_level = 4; /* ERROR */
    else if(strncasecmp(env, "on", 1) == 0 ||
        strncasecmp(env, "info", 1) == 0 ||
        strncasecmp(env, "yes", 1) == 0 ||
        env[0] == '1')
      spew_level = 1; /* INFO */
    else if(strncasecmp(env, "notice", 3) == 0 ||
        env[0] == '2')
      spew_level = 2; /* NOTICE */
    else if(strncasecmp(env, "debug", 1) == 0 ||
        env[0] == '0')
      spew_level = 0; /* DEBUG */
    else if(strncasecmp(env, "warn", 1) == 0 ||
        env[0] == '3')
      spew_level = 3; /* WARN */
  }

  if(!qp_term_color_init())
  {
    int i;
    for(i=0; i<5; ++i)
      color[i] = nul;
  }


  env = getenv("SPEW_FILES");
  if(!env)
    env = getenv("QP_SPEW_FILES");

  if(env && env[0])
  {
    file_spew_list = qp_sllist_create(NULL);

    /* form: QP_SPEW_FILES="file:[level:[line:]]file:..."
     * Since levels are just 0,1,2,3,4 the valid line numbers
     * will always be larger.
     * QP_SPEW_FILES="file1.c:321:2:file2.h:file3.c:450:1" 
     * Extra colons (:) are ignored. */

    while(*env == ':')
      ++env;

    while(*env)
    {
      struct flag *f;
      char *p;
      int i;
      int got_line = 0;
      int got_level = 0;

      p = env+1;
      f = (struct flag *) qp_malloc(sizeof(*f));
      f->level = spew_level;
      f->line = 0;
      while(*p && *p != ':')
        ++p;
      f->file = strndup(env, (size_t) (p - env));
      VASSERT(f->file, "strndup() failed\n");

      qp_sllist_append(file_spew_list, f);

      if(*p)
        ++p;
      while(*p == ':')
        ++p;

      for(i=0; *p && i<2; ++i)
      {
        int j;
        char *endptr = NULL;

        j = strtol(p, &endptr, 10);
        //DEBUG("strtol(\"%s\",)=%d  endptr=\"%s\"\n", p, j, endptr);

        if(endptr[0] != '\0' &&
          endptr[0] != ':')
          /* it did not read to the next ':' or '\0' */
          break;

        if(j > 5)
        {
          f->line = j;
          got_line = 1;
        }
        else
        {
          f->level = (j>0)?j:0;
          got_level = 1;
        }

        ++p;
        while(*p && *p != ':')
          ++p;
        while(*p == ':')
          ++p;
      }

      if(got_line && !got_level)
        f->level = 0;
      
      env = p;
    }

    if(!qp_sllist_begin(file_spew_list))
    {
      WARN("Got %s set with no files listed\n", env);
      qp_sllist_destroy(file_spew_list, 0);
      file_spew_list = NULL;
    }

#if 1
    if(qp_sllist_begin(file_spew_list))
    {
      struct flag *f;
      DEBUG("Got the following per file spew list with %zu entries\n"
               "    file                    line   level\n", 
               qp_sllist_length(file_spew_list));
      f = qp_sllist_begin(file_spew_list);
      for(;f; f = qp_sllist_next(file_spew_list))
        APPEND("                                     %d\r"
               "                            %d\r"
               "%s\n",
            f->level, f->line, f->file);
    }
    else
      DEBUG("Got %s set with no files listed\n", env);
#endif

  }
}

/* returns:  -1  spew off  level from QP_SPEW_FILE was > 4
 *            0  spew set from globel spew level
 *            1  spew on
 */
static inline int
got_per_file_spew(const char *file, int level, int line)
{
  struct flag *f;
  int ret = 0;
  if(!file_spew_list) return 0;

  for(f = (struct flag *) qp_sllist_begin(file_spew_list);
      f ; f = (struct flag *) qp_sllist_next(file_spew_list))
    if(strcmp(f->file, file) == 0)
    {
      /* Files may be listed more than once
       * so we let the last entry in the list
       * have the last word unless we get
       * file and line */
      if(line == f->line)
      {
        if(level >= f->level)
          return 1;
        else
          return -1;
      }

      if(f->line == 0)
      {
        if(level >= f->level)
          ret = 1;
        else
          ret = -1;
      }
    }
  return ret;
}

int _qp_spew_level(void)
{
  if(spew_level == -1)
    _qp_spew_init();
  return spew_level;
}

void _qp_spew(const char *file, int line, const char *func,
    int level, int show_errno, const char *format, ...)
{
  int env_spew_set;
  if(spew_level == -1)
    _qp_spew_init();

  if(level > 4)
    level = 4;
  else if(level < 0)
    level = 0;

  env_spew_set = got_per_file_spew(file, level, line);

  if((level >= spew_level && env_spew_set != -1) ||
      env_spew_set == 1)
  {
    last_spewed = 1;

    if(!show_errno)
      fprintf(spew_file, "%s%s%s:%s%s:%d%s:%s%s()%s: ",
        color[level], prefix[level], trm,
        bblu, file, line, trm, btur, func, trm);
    else
    {
      char errstr[128];
      /* thread safe strerror() */
      strerror_r(errno, errstr, 128);
      fprintf(spew_file, "%s%s%s:%s%s:%d%s:%s%s()%s:errno=%d:\"%s\": ",
        color[level], prefix[level], trm,
        bblu, file, line, trm, btur, func, trm, errno, errstr);
      }

    va_list ap;
    va_start(ap, format);
    vfprintf(spew_file, format, ap);
    fflush(spew_file);
    va_end(ap);
  }
  else
    last_spewed = 0;
}

int _qp_spew_debug_append_is_on(void)
{
  return last_spewed;
}

void _qp_spew_append(const char *format, ...)
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

void _qp_assert(const char *file, int line,
    const char *func, long bool_arg,
    const char *arg, const char *format, ...)
{
  if(!bool_arg)
  {
    va_list ap;

    _qp_spew(file, line, func, 4 /* ERROR */, 1,
        "%sASSERTION(%s%s%s%s%s) FAILED%s: pid: %d\n",
        btur, trm, yel, arg, trm, btur, trm, getpid());
    va_start(ap, format);
    vfprintf(spew_file, format, ap);
    va_end(ap);
    _qp_spew_append("\nsignal to exit, quickplot sleeping here ...");

    while(1)
      sleep(10);
  }
}

