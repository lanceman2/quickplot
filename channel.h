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
#include <stdint.h>
#include <string.h>
#include <math.h>


#ifndef _QP_DEBUG_H_
#error "You must include qp_debug.h before this file."
#endif



/* QP_CHANNEL_FORM_SERIES 0
   QP_CHANNEL_FORM_FUNC   1

   QP_TYPE_UNKNOWN            0
   QP_TYPE_SHORT              1
   QP_TYPE_INT                2
   .
   .
   etc
   .
   .
   .
   QP_TYPE_MAX
 */


#define ARRAY_LENGTH  (4*1024)

/* a small positive double used to avoid dividing
 * by zero when scaling plot values. */
#define SMALL_DOUBLE  (100.0*DBL_EPSILON)
#define LARGE_DOUBLE  (DBL_MAX/10.0)


struct qp_channel_series
{
  /* TYPE_SHORT or TYPE_INT ... etc type in the arrays */

  /* each element in the qp_dllist is an array of values */
  size_t array_current_index; /* index of the current value
                                 read in the current array */
  size_t array_last_index; /* the index of the last value
                              in the last array */
  void *last_array; /* points to last array */

  /* The copies of this channel will share these arrays.
   * So the larger memory usage is not replicated. */
  struct qp_dllist *arrays; /* list of arrays.  The value
                             * in each list element is just
                             * the array of value_types that
                             * was made with malloc(). */

  int is_increasing;
  int is_decreasing;
  int has_nan; /* has values like +/-NAN or +/-INF */
   
  double min, max; /* for zoom/scale calculations */

  /* We can make copies of qp_channel_series.
   * The qp_channel_series is just a writer/reader of the
   * arrays.  Once you start making copies you can no
   * longer write. */
  int *ref_count;
};



struct qp_channel_func_double
{
  double (*func)(double x);
  double xmax, xmin;
  double ymax, ymin;
};


struct qp_channel
{
  int form;
  int value_type;
  /* A pointer to put extra stuff at */
  void *data;
  uint64_t id;  /* unique id for this channel and all copies */
  union
  {
    struct qp_channel_series series;
    struct qp_channel_func_double func_double;
  };
};

static
inline
int is_good_double(double x)
{
  if(x > -LARGE_DOUBLE && x < LARGE_DOUBLE &&
      x != NAN && x != -NAN)
    return 1;
  return 0;
}


/* return 1 if the value last read is valid and not past
 * a leading or trailing edge. */
static inline
int qp_channel_series_is_reading(qp_channel_t channel)
{
  ASSERT(channel->form == QP_CHANNEL_FORM_SERIES);
  return (qp_dllist_val(channel->series.arrays))?1:0;
}

extern
qp_channel_t qp_channel_create(int channel_form, int value_type);

extern
qp_channel_t qp_channel_linear_create(double start, double step);

/* if orig != NULL this returns a channel that points to the
 * same data as orig.  After a copy is made you may not write
 * to the channel any more. */

extern
qp_channel_t qp_channel_func_double_create(double (*func)(double),
    double xmin, double xmax, double ymin, double ymax);


static inline
qp_channel_t qp_channel_series_create(qp_channel_t orig, int value_type)
{
  if(orig)
  {
    /* make a copy that is a channel reader */
    struct qp_channel *c;

    ASSERT(orig->value_type > 0 && orig->value_type <= QP_TYPE_MAX);
    ASSERT(orig->form == QP_CHANNEL_FORM_SERIES);
    ASSERT(orig->series.arrays);

    c = (struct qp_channel *) qp_malloc(sizeof(*c));
    c->form = QP_CHANNEL_FORM_SERIES;
    c->id = orig->id;
    c->value_type = orig->value_type;
    c->series.array_current_index = (size_t) -1;
    c->series.array_last_index = orig->series.array_last_index;
    c->series.last_array = orig->series.last_array;
    c->series.arrays = qp_dllist_create(orig->series.arrays);
    c->series.ref_count = orig->series.ref_count;
    c->series.is_increasing = orig->series.is_increasing;
    c->series.is_decreasing = orig->series.is_decreasing;
    c->series.min = orig->series.min;
    c->series.max = orig->series.max;
    c->series.has_nan = orig->series.has_nan;

    ++(*(c->series.ref_count));
    return c;
  }
  else
    /* make a new one */
    return qp_channel_create(QP_CHANNEL_FORM_SERIES, value_type);
}


extern
void qp_channel_destroy(qp_channel_t channel);


static inline
size_t qp_channel_series_length(struct qp_channel *c)
{
  ASSERT(c);
  ASSERT(c->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(c->series.arrays);

  return (qp_dllist_length(c->series.arrays)-1)*ARRAY_LENGTH +
    c->series.array_last_index + 1;
}


static inline
int qp_channel_equals(struct qp_channel *c1, struct qp_channel *c2)
{
  ASSERT(c1);
  ASSERT(c2);
  if(!c1 || !c2)
    return 0;
  if(c1->id == c2->id)
    return 1;
  return 0;
}



#ifdef QP_DEBUG
extern
void qp_channel_debug_append(struct qp_channel *c);
#endif

