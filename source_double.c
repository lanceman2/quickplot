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
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>

#include "quickplot.h"

#include "config.h"
#include "qp.h"
#include "debug.h"
#include "spew.h"
#include "term_color.h"
#include "list.h"
#include "channel.h"
#include "channel_double.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


/* Gets doubles ignoring any chars that cannot
 * be part of a decimal number.
 * +/-NAN +/-INF are valid doubles to get.
 * returns 1 if gets a double
 * returns 0 if there is no more doubles to get
 */
static inline
int get_next_double(double *val, char **line)
{
  char *s, *ptr;
  ASSERT(line);
  ASSERT(val);

  if(**line == '\0') return 0;

  s = ptr = *line;

  while(*s)
  {
    *val = strtod(s, &ptr);
    if(s != ptr)
    {
      *line = ptr;
      return 1;
    }
    ++s;
  }

  return 0;
}


/* returns:   0  line was skipped or empty
 *            1  got data                  */
int qp_source_parse_doubles(struct qp_source *source, char *line_in)
{
  char *s, *line;
  struct qp_channel **c;
  double value;

  line = line_in;

  if(!line_in || !line_in[0])
    return 0;

  /* take off any ending '\n' or '\r' */
  for(s = &line[strlen(line)-1];
      s >= line && (*s == '\n' || *s == '\r');
      ++s) *s = '\0';

  //DEBUG("parsing line: %s\n", line);

  for(; *line && isspace(*line); ++line);

  if(line[0] == '\0' ||
     (line[0] >= '!' && line[0] <= ')') ||
      line[0] == '/' ||
      (line[0] >= '<' && line[0] <= '@') ||
      line[0] == 'C' ||
      line[0] == 'c')
    return 0;


  if(!get_next_double(&value, &line))
    return 0;

  c = source->channels;

  do
  {
    if(!(*c))
    {
      struct qp_channel *new_chan;

      new_chan = qp_channel_create(QP_CHANNEL_FORM_SERIES, QP_TYPE_DOUBLE);
      ASSERT(new_chan);
      ASSERT(new_chan->form == QP_CHANNEL_FORM_SERIES);
      ASSERT(new_chan->value_type == QP_TYPE_DOUBLE);
      ASSERT(new_chan->series.arrays);

      ++source->num_channels;
      source->channels = (struct qp_channel **)
        qp_realloc(source->channels,
            (source->num_channels+1)*sizeof(struct qp_channel*));
      source->channels[source->num_channels-1] = new_chan;
      source->channels[source->num_channels] = NULL;
      c = source->channels + source->num_channels-1;

      if(source->num_values) /* we have other 
                                channels with data */
      {
        ASSERT(source->num_channels > 1);

        /* We pad the top of this channel with blank
         * values to make it have the same number
         * of values as the other channels. */
        struct qp_channel *first_chan;
        size_t len;
        /* get the first channel without moving
         * the list pointer. */
        first_chan = *(source->channels);
        ASSERT(first_chan);
        len = qp_channel_series_length(first_chan) - 1;
        while(len--)
          /* put len blank values in the begining */
          qp_channel_series_double_append(new_chan, NAN);
      }
    }

    qp_channel_series_double_append(*c, value);
    ++c;

  } while(get_next_double(&value, &line));


 
  /* If there are any more channels with no values from
   * this line than put the value DOUBLE_NAN in those
   * channels. */
  while(*c)
    qp_channel_series_double_append(*(c++), NAN);

  ++(source->num_values);

  return 1; /* got data */
}

