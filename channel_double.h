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

#include <sys/types.h>
#include <math.h>


#ifndef _QP_DEBUG_H_
#error "You must include qp_debug.h before this file."
#endif


#ifndef NAN
#error "number NAN is not defined"
#endif
#ifndef INFINITY
#error "number INFINITY not defined"
#endif

#define END_DOUBLE  (NAN)


extern
void qp_channel_series_double_append(qp_channel_t channel, double val);

extern
double qp_channel_series_double_begin(qp_channel_t channel);

extern
double qp_channel_series_double_end(qp_channel_t channel);


/* Find the value that is less than and next to or as close as possible
 * For series that is increasing
 * *v in is the value to search on
 * *v out is the value found from the channel
 * sets the current read to that value in the channel
 * returns the index to the value in the channel
 * returns 0 if a value was not found and the value
 * is set to the first value
 * */
extern
size_t qp_channel_series_double_find_lt(qp_channel_t channel, double *v);


/* Find the value that is greater than and next to or as close as possible
 * For series that is increasing
 * *v in is the value to search on
 * *v out is the value found from the channel
 * sets the current read to that value in the channel
 * returns the index to the value in the channel
 * returns length - 1 if the value was not found
 * and the value is set to the last value
 * */
extern
size_t qp_channel_series_double_find_gt(qp_channel_t channel, double *v);



/* Check that i is in bounds before this call */
/* This is a little slow, so don't iterate with it
 * you can use it to start an iteration at a point
 * not necessarily at the first or last value.  It is
 * a lot faster than iterating to the i-th value
 * with qp_channel_series_double_next() and
 * qp_channel_series_double_prev()
 *
 *  Returns the i-th value where i = [ 0, length-1 ] and
 *  sets the current value to that value */
static inline
double qp_channel_series_double_index(qp_channel_t c, size_t i)
{
  double *array;
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(i >= 0);
  ASSERT(i < qp_channel_series_length(c));
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(*(c->series.ref_count) > 0);

  array = qp_dllist_begin(c->series.arrays);
  for(;i >= ARRAY_LENGTH; i -= ARRAY_LENGTH)
    array = qp_dllist_next(c->series.arrays);

  c->series.array_current_index = i;

  return array[i]; 
}

/* This is not so fast.  Do not use in a tight loop. */
/* This returns the current index or (size_t) -1 if their is none. */
static inline
size_t qp_channel_series_double_get_index(qp_channel_t c)
{
  size_t ret = 0;
  void *current_array, *array;
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(qp_channel_series_is_reading(c));
  ASSERT(*(c->series.ref_count) > 0);

  if(!qp_channel_series_is_reading(c))
    return (size_t) -1;

  ret += c->series.array_current_index;
  current_array = qp_dllist_val(c->series.arrays);

  array = qp_dllist_begin(c->series.arrays);
  while(array != current_array)
  {
    ret += ARRAY_LENGTH;
    array = qp_dllist_next(c->series.arrays);
    ASSERT(array);
  }

  return ret;
}

static inline
double qp_channel_series_double_next(qp_channel_t c)
{
  double *array;
  struct qp_channel_series *cs;
  ASSERT(c);
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES ||
      c->form == QP_CHANNEL_FORM_FUNC);
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(c->series.arrays);
  ASSERT(*(c->series.ref_count) > 0);

  cs = &c->series;

  ++(cs->array_current_index);

  array = (double *) qp_dllist_val(cs->arrays);

  if(!array) return END_DOUBLE;

  if(array != cs->last_array)
  {
    if(cs->array_current_index < ARRAY_LENGTH)
      return array[cs->array_current_index];
    array = (double *) qp_dllist_next(cs->arrays);
    cs->array_current_index = 0;
  }
  if(array != cs->last_array)
    return array[cs->array_current_index];
  if(array && cs->array_current_index <= cs->array_last_index)
    return array[cs->array_current_index];
  if(array)
  {
    /* push it past the end */
    qp_dllist_next(cs->arrays);
    ASSERT(!qp_dllist_val(cs->arrays));
    return END_DOUBLE;
  }
  /* there are no values left or there are no values */
  return END_DOUBLE;
}

static inline
double qp_channel_series_double_prev(qp_channel_t c)
{
  double *array;
  struct qp_channel_series *cs;
  ASSERT(c);
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES ||
      c->form == QP_CHANNEL_FORM_FUNC);
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(*(c->series.ref_count) > 0);

  ASSERT(c->series.arrays);

  cs = &c->series;

  --(cs->array_current_index);

  array = (double *) qp_dllist_val(cs->arrays);

  if(array)
  {
    if(cs->array_current_index != (size_t) -1)
    {
      return array[cs->array_current_index];
    }
    else
    {
      array = (double *) qp_dllist_prev(cs->arrays);
      if(array)
      {
        cs->array_current_index = ARRAY_LENGTH - 1;
        return array[cs->array_current_index];
      }
    }
  }

  return END_DOUBLE;
}

