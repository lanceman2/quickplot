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

#include "quickplot.h"

#include "config.h"
#include "debug.h"
#include "list.h"
#include "channel.h"
#include "spew.h"
#include "channel_double.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


size_t qp_channel_series_double_find_lt(qp_channel_t c, double *v)
{
  size_t i, len;
  double val, r;
  struct qp_channel_series *cs;
  val = *v;
  ASSERT(c);
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(c->series.is_increasing);
  cs = &c->series;
  ASSERT(cs->min != cs->max);
  len = qp_channel_series_length(c);

  if(val <= cs->min)
  {
    *v = qp_channel_series_double_begin(c);
    ASSERT(*v == cs->min);
    return 0;
  }
  if(val > cs->max)
  {
    *v = qp_channel_series_double_end(c);
    ASSERT(*v == cs->max);
    return len -1;
  }

  /* guess that the values increase linearly */
  i = len*(cs->max - val)/(cs->max - cs->min);
  if(i < 0) i = 0;
  else if(i > len -1) i = len -1;

  r = qp_channel_series_double_index(c, i);
  while(r < val)
    r = qp_channel_series_double_next(c);
  ASSERT(qp_channel_series_is_reading(c));

  while(r >= val)
    r = qp_channel_series_double_prev(c);
  ASSERT(qp_channel_series_is_reading(c));

  *v = r;
  return qp_channel_series_double_get_index(c);
}

size_t qp_channel_series_double_find_gt(qp_channel_t c, double *v)
{
  size_t i, len;
  double val, r;
  struct qp_channel_series *cs;
  val = *v;
  ASSERT(c);
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(c->series.is_increasing);
  cs = &c->series;
  ASSERT(cs->min != cs->max);
  len = qp_channel_series_length(c);

  if(val < cs->min)
  {
    *v = qp_channel_series_double_begin(c);
    ASSERT(*v == cs->min);
    return 0;
  }
  if(val >= cs->max)
  {
    *v = qp_channel_series_double_end(c);
    ASSERT(*v == cs->max);
    return len -1;
  }

  /* guess that the values increase linearly */
  i = len*(cs->max - val)/(cs->max - cs->min);
  if(i < 0) i = 0;
  else if(i > len -1) i = len -1;

  r = qp_channel_series_double_index(c, i);
  while(r > val)
    r = qp_channel_series_double_prev(c);
  ASSERT(qp_channel_series_is_reading(c));

  while(r <= val)
    r = qp_channel_series_double_next(c);
  ASSERT(qp_channel_series_is_reading(c));

  *v = r;
  return qp_channel_series_double_get_index(c);
}

double qp_channel_series_double_begin(qp_channel_t c)
{
  double *array;
  struct qp_channel_series *cs;
  ASSERT(c);
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(c->series.arrays);
  ASSERT(*(c->series.ref_count) > 0);

  cs = &c->series;

  array = (double *) qp_dllist_begin(cs->arrays);
  if(array)
  {
    cs->array_current_index = 0;
    return array[0];
  }
  ASSERT(qp_dllist_length(cs->arrays) == 0);
  ASSERT(cs->last_array == NULL);
  return END_DOUBLE;
}

double qp_channel_series_double_end(qp_channel_t c)
{
  double *array;
  struct qp_channel_series *cs;
  ASSERT(c);
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(c->series.arrays);
  ASSERT(*(c->series.ref_count) > 0);

  cs = &c->series;

  array = (double *) qp_dllist_end(cs->arrays);
  if(array)
  {
    ASSERT(array == cs->last_array);

    cs->array_current_index = cs->array_last_index;
    return array[cs->array_current_index];
  }
  ASSERT(qp_dllist_length(cs->arrays) == 0);
  ASSERT(cs->last_array == NULL);
  return END_DOUBLE;
}

static inline
void check_min_max(struct qp_channel_series *cs, double val)
{
  if(!is_good_double(val))
  {
    cs->has_nan = 1;
    return;
  }

  if(val > cs->max)
    cs->max = val;
  else
    cs->is_increasing = 0;

  if(val < cs->min)
    cs->min = val;
  else
    cs->is_decreasing = 0;
}

void qp_channel_series_double_append(qp_channel_t c, double val)
{
  double *array;
  struct qp_channel_series *cs;

  ASSERT(c);
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(c->value_type == QP_TYPE_DOUBLE);
  ASSERT(c->series.arrays);
  /* We disallow having the channel written to when there
   * is more than one copy of the channel. */
  ASSERT(*(c->series.ref_count) == 1);

  cs = &c->series;
  
  /* get the last without moving the list current pointer */
  array = (double *) qp_dllist_last(cs->arrays);

  ASSERT(array == cs->last_array);

  if(array)
  {
    ASSERT(cs->array_last_index < ARRAY_LENGTH);
    ASSERT(cs->last_array);

    ++(cs->array_last_index);

    if(cs->array_last_index == ARRAY_LENGTH)
    {
      /* add an array */
      array = (double *)
        qp_malloc(sizeof(*array)*ARRAY_LENGTH);
      cs->array_last_index = 0;
      qp_dllist_append(cs->arrays, array);
      cs->last_array = array;
    }
    array[cs->array_last_index] = val;
    check_min_max(cs, val);
    return;
  }

  ASSERT(qp_dllist_length(cs->arrays) == 0);
  ASSERT(cs->last_array == NULL);

  /* We must make the first array */
  array = (double *) qp_malloc(sizeof(*array)*ARRAY_LENGTH);
  qp_dllist_append(cs->arrays, array);
  cs->array_last_index = 0;
  cs->array_current_index = 0;
  cs->last_array = array;
  cs->max = -INFINITY;
  cs->min = INFINITY;
  check_min_max(cs, val);
  cs->is_increasing = 1;
  cs->is_decreasing = 1;
  /* and add the value */
  array[0] = val;
}

