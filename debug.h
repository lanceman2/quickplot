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

#ifndef _QP_DEBUG_H_
#define _QP_DEBUG_H_

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>


#ifndef PACKAGE_TARNAME
#  error  "You must include config.h before this file"
#endif




#ifndef QP_DEBUG


#define DEBUG(fmt, ...)  /* empty macro */
#define INFO(fmt, ...)   /* empty macro */
#define NOTICE(fmt, ...) /* empty macro */
#define WARN(fmt, ...)   /* empty macro */
#define ERROR(fmt, ...)  /* empty macro */
#define EDEBUG(fmt, ...)  /* empty macro */
#define EINFO(fmt, ...)   /* empty macro */
#define ENOTICE(fmt, ...) /* empty macro */
#define EWARN(fmt, ...)   /* empty macro */
#define EERROR(fmt, ...)  /* empty macro */
#define APPEND(fmt, ...) /* empty macro */
#define ASSERT(x)        /* empty macro */
#define VASSERT(x, ...)  /* empty macro */
#define APPEND_ON()        (0)
#define SPEW_LEVEL()       (5) /* no debug spew */


#else /* #ifndef QP_DEBUG */



/* This adds addition compiler checks for the printf formating. */
#define __printf(a,b)  __attribute__((format(printf,a,b)))

extern
void _qp_spew(const char *file, int line, const char *func, int level,
    int show_errno, const char *format, ...)
  __printf(6,7);

extern
void _qp_spew_append(const char *format, ...)
  __printf(1,2);

extern
void _qp_assert(const char *file, int line,
    const char *func, long bool_arg,
    const char *arg, const char *format, ...)
  __printf(6,7);


#define DEBUG(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 0, 0, fmt, ##__VA_ARGS__ )
#define INFO(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 1, 0, fmt, ##__VA_ARGS__ )
#define NOTICE(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 2, 0, fmt, ##__VA_ARGS__ )
#define WARN(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 3, 0, fmt, ##__VA_ARGS__ )
#define ERROR(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 4, 0, fmt, ##__VA_ARGS__ )

#define EDEBUG(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 0, 1, fmt, ##__VA_ARGS__ )
#define EINFO(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 1, 1, fmt, ##__VA_ARGS__ )
#define ENOTICE(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 2, 1, fmt, ##__VA_ARGS__ )
#define EWARN(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 3, 1, fmt, ##__VA_ARGS__ )
#define EERROR(fmt, ...) \
  _qp_spew(__FILE__, __LINE__, __func__, 4, 1, fmt, ##__VA_ARGS__ )


#define APPEND(fmt, ...) \
  _qp_spew_append(fmt, ##__VA_ARGS__ )


#define ASSERT(x) _qp_assert(__FILE__, __LINE__, \
    __func__, (long) (x), #x, " ")
#define VASSERT(x, fmt, ...)    \
    _qp_assert(__FILE__, __LINE__, __func__, \
        (long) (x), #x, fmt, ##__VA_ARGS__)



extern int
_qp_spew_debug_append_is_on(void);
#define APPEND_ON() _qp_spew_debug_append_is_on()

extern int _qp_spew_level(void);
#define SPEW_LEVEL()   _qp_spew_level()    

#endif /* #ifndef QP_DEBUG  #else */


#define qp_malloc(x) _qp_malloc((x), __FILE__, __LINE__, __func__)

/** malloc() wrapper that checks for failure */  
static inline
void *_qp_malloc(size_t s, const char *file, int line,
    const char *func)
{
  void *p;
  errno = 0;
  p = malloc(s);
#ifdef QP_DEBUG
  _qp_assert(file, line, func, (long) p, "malloc()",
      " malloc(%zu) failed\n", s);
#endif
  if(!s) /* for non-debug build case failure */
  {
    char errstr[128];
    strerror_r(errno, errstr, 128);
    printf("%s:%d:%s() malloc(%zu) failed: errno=%d: %s\n",
        file, line, func, s, errno, errstr);
    exit(1);
  }
  return p;
}

#define qp_realloc(x,s) _qp_realloc((x),(s), __FILE__, __LINE__, __func__)

/** realloc() wrapper that checks for failure */  
static inline
void *_qp_realloc(void *x, size_t s, const char *file, int line,
    const char *func)
{
  void *p;
  errno = 0;
  p = realloc(x,s);
#ifdef QP_DEBUG
  _qp_assert(file, line, func, (long) p, "realloc()",
      " realloc(%p,%zu) failed\n", x, s);
#endif
  if(!s) /* for non-debug build case failure */
  {
    char errstr[128];
    strerror_r(errno, errstr, 128);
    printf("%s:%d:%s() realloc(%p,%zu) failed: errno=%d: %s\n",
        file, line, func, x, s, errno, errstr);
    exit(1);
  }
  return p;
}

#define qp_strdup(x) _qp_strdup((x), __FILE__, __LINE__, __func__)

/** strdup() wrapper that checks for failure */  
static inline
char *_qp_strdup(const char *s, const char *file, int line,
    const char *func)
{
  char *p;
  errno = 0;
  p = strdup(s);
#ifdef QP_DEBUG
  _qp_assert(file, line, func, (long) p, "strdup()",
      " strdup(\"%s\") failed\n", s);
#endif
  if(!s) /* for non-debug build case failure */
  {
    char errstr[128];
    strerror_r(errno, errstr, 128);
    printf("%s:%d:%s() strdup(\"%s\") failed: errno=%d: %s\n",
        file, line, func, s, errno, errstr);
    exit(1);
  }
  return p;
}

#define qp_strndup(x, n) _qp_strndup((x), (n), __FILE__, __LINE__, __func__)

/** strndup() wrapper that checks for failure */  
static inline
char *_qp_strndup(const char *s, size_t n, const char *file, int line,
    const char *func)
{
  char *p;
  errno = 0;
  p = strndup(s, n);
#ifdef QP_DEBUG
  _qp_assert(file, line, func, (long) p, "strndup()",
      " strndup(\"%s\", %zu) failed\n", s, n);
#endif
  if(!s) /* for non-debug build case failure */
  {
    char errstr[128];
    strerror_r(errno, errstr, 128);
    printf("%s:%d:%s() strndup(\"%s\", %zu) failed: errno=%d: %s\n",
        file, line, func, s, n, errno, errstr);
    exit(1);
  }
  return p;
}


      



#else /* #ifndef _QP_DEBUG_H_ */

#error "You included this file twice."

#endif /* #ifndef _QP_DEBUG_H_     */
