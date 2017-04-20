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


static
uint64_t channel_create_count = 0;


qp_channel_t qp_channel_func_double_create(double (*func)(double),
    double xmin, double xmax, double ymin, double ymax)
{
  struct qp_channel *c;

  c = (struct qp_channel *) qp_malloc(sizeof(*c));
  c->form = QP_CHANNEL_FORM_FUNC;
  c->value_type = QP_TYPE_DOUBLE;
  c->id = (++channel_create_count);
  c->func_double.func = func;
  c->func_double.xmin = xmin;
  c->func_double.xmax = xmax;
  c->func_double.ymin = ymin;
  c->func_double.ymax = ymax;
  return c;
}

qp_channel_t qp_channel_linear_create(double start, double step)
{
  struct qp_channel *c;

  /* TODO: rewrite this making this a function and not a
   * big memory pig like it is now. */

  c = qp_channel_create(QP_CHANNEL_FORM_SERIES, QP_TYPE_DOUBLE);
  c->data = qp_malloc(2*sizeof(double));
  ((double *) c->data)[0] = start;
  ((double *) c->data)[1] = step;

  /* TODO: We setup the values in source.c */

  return c;
}


qp_channel_t qp_channel_create(int form, int value_type)
{
  struct qp_channel *channel;

  ASSERT(value_type >= 0 && value_type <= QP_TYPE_MAX);
  ASSERT(form == QP_CHANNEL_FORM_SERIES ||
      form == QP_CHANNEL_FORM_FUNC);

  if(form != QP_CHANNEL_FORM_SERIES &&
      form != QP_CHANNEL_FORM_FUNC)
  {
    WARN("Bad form arg\n");
    QP_NOTICE("Bad form arg\n");
    return NULL;
  }
  if(value_type < 0 || value_type > QP_TYPE_MAX)
  {
    WARN("Bad value_type arg\n");
    QP_NOTICE("Bad value_type arg\n");
    return NULL;
  }

  channel = (struct qp_channel *) qp_malloc(sizeof(*channel));
  memset(channel, 0, sizeof(*channel));
  channel->form = form;
  channel->value_type = value_type;
  channel->id = (++channel_create_count);
  channel->data = 0;


  switch(form)
  {
    case QP_CHANNEL_FORM_SERIES:
      {
        channel->series.array_current_index = (size_t) -1;
        channel->series.array_last_index = ARRAY_LENGTH -1;
        channel->series.last_array = NULL;
        channel->series.arrays = qp_dllist_create(NULL);
        channel->series.ref_count = (int *)qp_malloc(sizeof(int));
        *(channel->series.ref_count) = 1;
      }
      break;

    /* TODO: add other value_type cases */

    case QP_CHANNEL_FORM_FUNC:
      /* TODO: this code */
      VASSERT(0, "Need more code here\n");
      break;

#ifdef QP_DEBUG
    default:
      VASSERT(0, "Bad form arg\n");
      break;
#endif
    }
  
  return channel;
}


void qp_channel_destroy(qp_channel_t channel)
{
  ASSERT(channel);
  if(!channel) return;
  
  /* TODO: add other value types */
  ASSERT(channel->value_type == QP_TYPE_UNKNOWN ||
      channel->value_type == QP_TYPE_DOUBLE);

  ASSERT(channel->form == QP_CHANNEL_FORM_SERIES ||
      channel->form == QP_CHANNEL_FORM_FUNC);

  switch(channel->form)
  {
    case QP_CHANNEL_FORM_SERIES:
      ASSERT(*(channel->series.ref_count) > 0);
      if(*(channel->series.ref_count) == 1)
      {
        /* This will free the arrays too */
        qp_dllist_destroy(channel->series.arrays, 1);
        free(channel->series.ref_count);
      }
      else
      {
        /* This will not free the arrays, just the list reader */
        qp_dllist_destroy(channel->series.arrays, 0);
        --(*(channel->series.ref_count));
      }
      break;

    case QP_CHANNEL_FORM_FUNC:
      /* nothing needed here for QP_CHANNEL_FORM_FUNC */
      break;

#ifdef QP_DEBUG
    default:
      ASSERT(0);
      break;
#endif
  }

  free(channel);
}

#ifdef QP_DEBUG
void qp_channel_debug_append(struct qp_channel *c)
{
  ASSERT(c);
  switch(c->form)
  {
    case QP_CHANNEL_FORM_SERIES:
     
      ASSERT(*(c->series.ref_count) > 0);

      switch(c->value_type)
      {
        case QP_TYPE_DOUBLE:
          {
#if 1
            double val;
            APPEND(" [%zu values, min=%G, max=%g "
                "is_increasing=%d is_decreasing=%d]",
                qp_channel_series_length(c),
                c->series.min,
                c->series.max,
                c->series.is_increasing,
                c->series.is_decreasing);
            for(val = qp_channel_series_double_begin(c);
                qp_channel_series_is_reading(c);
                val = qp_channel_series_double_next(c))
              APPEND(" %G", val);
#if 1
            APPEND("\nreverse [%zu]",
                qp_channel_series_length(c));
            for(val = qp_channel_series_double_end(c);
                qp_channel_series_is_reading(c);
                val = qp_channel_series_double_prev(c))
              APPEND(" %G", val);
#endif

#else
            double *array;
            struct qp_channel_series *cs;
            cs = &c->series;
            for(array = (double *) qp_dllist_begin(cs->arrays);
                array;
                array = (double *) qp_dllist_next(cs->arrays))
            {
              size_t i;
              size_t len;
              len = (array != cs->last_array)?
                ARRAY_LENGTH:
                cs->array_last_index+1;
              ASSERT(len > 0);
              for(i=0; i<len; ++i)
                APPEND(" %G", array[i]);
            }
#endif
          }
          break;

          /* TODO: add other cases */
        default:
          ASSERT(0);
          break;
      }
    break;

    /* TODO: Write function case */

    default:
    ASSERT(0);
    break;
  }
}
#endif

