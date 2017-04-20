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


#ifndef _QP_SPEW_H_
#define _QP_SPEW_H_

#include <stdio.h>

extern
int qp_get_spew_level(void);

/* INFO 1  NOTICE 2  WARN 3  ERROR 4  NONE 5 */
extern
void qp_spew_init(int level);

extern
int qp_set_spew_level(int level);


/* This adds addition compiler checks for the printf formating. */
#define __printf(a,b)  __attribute__((format(printf,a,b)))

extern
void qp_spew(int level, int show_errno, const char *format, ...)
  __printf(3,4);

extern
void qp_spew_append(const char *format, ...)
  __printf(1,2);


#define QP_INFO(fmt, ...) \
  qp_spew(1, 0, fmt, ##__VA_ARGS__ )
#define QP_NOTICE(fmt, ...) \
  qp_spew(2, 0, fmt, ##__VA_ARGS__ )
#define QP_WARN(fmt, ...) \
  qp_spew(3, 0, fmt, ##__VA_ARGS__ )
#define QP_ERROR(fmt, ...) \
  qp_spew(4, 0, fmt, ##__VA_ARGS__ )

#define QP_EINFO(fmt, ...) \
  qp_spew(1, 1, fmt, ##__VA_ARGS__ )
#define QP_ENOTICE(fmt, ...) \
  qp_spew(2, 1, fmt, ##__VA_ARGS__ )
#define QP_EWARN(fmt, ...) \
  qp_spew(3, 1, fmt, ##__VA_ARGS__ )
#define QP_EERROR(fmt, ...) \
  qp_spew(4, 1, fmt, ##__VA_ARGS__ )


#define QP_APPEND(fmt, ...) \
  qp_spew_append(fmt, ##__VA_ARGS__ )



#else /* #ifndef _QP_SPEW_H_ */

#error "You included this file twice."

#endif /* #ifndef _QP_SPEW_H_     */
