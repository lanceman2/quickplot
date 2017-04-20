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


extern
void qp_plot_set_cairo_draw_mode(struct qp_plot *p, struct qp_graph *gr);

extern
void qp_plot_set_x11_draw_mode(struct qp_plot *p, struct qp_graph *gr);





static inline
int qp_plot_series_double_is_reading(struct qp_channel *c)
{
  /* This qp_channel_series_is_reading() is inline too */
  return qp_channel_series_is_reading(c);
}

static inline
int qp_plot_is_reading(struct qp_plot *p)
{
  return (p->x_is_reading(p->x) && p->y_is_reading(p->y));
}


/* qp_plot_begin() or qp_plot_end() must be called before this */
/* This is getting the inverse of the plots values.  It gets the
 * plot value that would be for a particular graph pixel x position. */
static inline
double qp_plot_get_xval(struct qp_plot *p, int x)
{
  return (x - p->xshift)/p->xscale;
}

/* qp_plot_begin_y() or qp_plot_end_y() must be called before this */
/* This is getting the inverse of the plots values.  It gets the
 * plot value that would be for a particular graph pixel y position. */
static inline
double qp_plot_get_yval(struct qp_plot *p, int y)
{
  return (y - p->yshift)/p->yscale;
}

/* qp_plot_begin_y() or qp_plot_end_y() must be called before this */
static inline
int qp_plot_get_xpixel(struct qp_plot *p, double x)
{
  return INT(p->xscale*x + p->xshift);
}

/* qp_plot_begin_y() or qp_plot_end_y() must be called before this */
static inline
int qp_plot_get_ypixel(struct qp_plot *p, double y)
{
  return INT(p->yscale*y + p->yshift);
}

static inline
void qp_plot_get_sig_fig(struct qp_plot *p, int width, int height)
{
  double a, b, k;

  if(p->sig_fig_x && p->sig_fig_y) return;

  a = qp_plot_get_xval(p, 0);
  b = qp_plot_get_xval(p, 1);
  ASSERT(b > a);
  k = 1.0/(b - a);

  b = ABSVAL(qp_plot_get_xval(p, width));
  a = ABSVAL(a);
  if(b < a)
    b = a;
  a = log10(b*k);
  p->sig_fig_x = INT(a);
  if(p->sig_fig_x < 1)
    p->sig_fig_x = 1;



  a = qp_plot_get_yval(p, 1);
  b = qp_plot_get_yval(p, 0);
  ASSERT(b > a);
  k = 1.0/(b - a);

  a = ABSVAL(qp_plot_get_yval(p, height));
  b = ABSVAL(b);
  if(b < a)
    b = a;
  a = log10(b*k);
  p->sig_fig_y = INT(a);
  if(p->sig_fig_y < 1)
    p->sig_fig_y = 1;
}

static inline
void qp_plot_scale(struct qp_plot *p,
      double xscale, double xshift, double yscale, double yshift)
{
  p->xscale = xscale*p->xscale0;
  p->xshift = INT(xscale*p->xshift0 + xshift);
  p->yscale = yscale*p->yscale0;
  p->yshift = INT(yscale*p->yshift0 + yshift);

  p->sig_fig_x = 0;
  p->sig_fig_y = 0;
}

static inline
void qp_plot_x_rescale(struct qp_plot *p, double xmin, double xmax)
{
  p->xscale0 = 1.0/(xmax - xmin);
  p->xshift0 = - xmin/(xmax - xmin);

  p->xscale = 0;
  p->sig_fig_x = 0;
}

static inline
void qp_plot_y_rescale(struct qp_plot *p, double ymin, double ymax)
{
  p->yscale0 = 1.0/(ymax - ymin);
  p->yshift0 = - ymin/(ymax - ymin);

  p->yscale = 0;
  p->sig_fig_y = 0;
}


/*****************************************************************/
/* All the functions below return a graph pixel x or y position. */
/*****************************************************************/


static inline
int qp_plot_begin(struct qp_plot *p,
      double xscale, double xshift, double yscale, double yshift,
      int xpix_min, int ypix_min, int xpix_max, int ypix_max,      
      double *x, double *y)
{
  qp_plot_scale(p, xscale, xshift, yscale, yshift);

  p->num_read = (size_t) -1;


  if(p->x->form == QP_CHANNEL_FORM_SERIES &&
      p->y->form == QP_CHANNEL_FORM_SERIES &&
      p->x->series.is_increasing &&
      qp_channel_series_length(p->x) == qp_channel_series_length(p->y) &&
      !p->x->series.has_nan && !p->y->series.has_nan)
  {
    /* culling */
    /* this is likely the most common case
     ****************************************
     *  A UNIFORMLY INCREASING X CHANNEL
     ****************************************/

    double xmin, xmax;
    size_t i, j, len;
    /* get the corresponding limiting plot channel values */
    /* Any values outside [xmin,xmax] would be culled */
    xmin = qp_plot_get_xval(p, xpix_min);
    xmax = qp_plot_get_xval(p, xpix_max);

    if(p->x->series.min > xmax || p->x->series.max < xmin)
      /* case: no data in bounds */
      goto nodata;

    if((p->x->series.min >= xmin && p->x->series.max <= xmax) ||
        p->x->series.min == p->x->series.max)
      /* all the channel data is in bounds */
      goto begining;

    len = qp_channel_series_length(p->x);
    j   = qp_channel_series_length(p->y);
    if(j < len)
      len = j;
    j = qp_channel_series_double_find_gt(p->x, &xmax);
    i = qp_channel_series_double_find_lt(p->x, &xmin);

    if(j < len -1)
      /* the number left to read after this returns */
      p->num_read = j - i;

    //DEBUG("plot %s  with indexes %zu to %zu \n", p->name, i, j);

    *x = p->xscale*xmin + p->xshift;
    *y = p->yscale*p->channel_series_y_index(p->y, i) + p->yshift;
    return (p->x_is_reading(p->x) && p->y_is_reading(p->y));
  }

begining:

  *x = p->xscale*p->channel_x_begin(p->x) + p->xshift;
  *y = p->yscale*p->channel_y_begin(p->y) + p->yshift;

  return (p->x_is_reading(p->x) && p->y_is_reading(p->y));


nodata:

  p->channel_x_end(p->x);
  p->channel_x_next(p->x);
  *x = END_DOUBLE;
  *y = END_DOUBLE;
  return 0; /* done reading, no values in bounds */
}


static inline
int qp_plot_end(struct qp_plot *p,
    double xscale, double xshift, double yscale, double yshift,
    double *x, double *y)
{
  p->num_read = (size_t) -1;

  qp_plot_scale(p, xscale, xshift, yscale, yshift);

  *x = p->xscale*p->channel_x_end(p->x) + p->xshift;
  *y = p->yscale*p->channel_y_end(p->y) + p->yshift;

  return (p->x_is_reading(p->x) && p->y_is_reading(p->y));

}

static inline
int qp_plot_next(struct qp_plot *p, double *x, double *y)
{
  *x = p->xscale*p->channel_x_next(p->x) + p->xshift;
  *y = p->yscale*p->channel_y_next(p->y) + p->yshift;

  if(p->num_read != (size_t) -1)
    return ((p->num_read)--);

  return (p->x_is_reading(p->x) && p->y_is_reading(p->y));
}

static inline
double qp_plot_prev(struct qp_plot *p, double *x, double *y)
{
  *x = p->xscale*p->channel_x_prev(p->x) + p->xshift;
  *y = p->yscale*p->channel_y_prev(p->y) + p->yshift;

  if(p->num_read)
  {
    --p->num_read;
    return p->num_read;
  }

  return (p->x_is_reading(p->x) && p->y_is_reading(p->y));
}

