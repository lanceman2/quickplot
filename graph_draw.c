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
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "quickplot.h"

#include "config.h"
#include "spew.h"
#include "debug.h"
#include "list.h"
#include "callbacks.h"
#include "channel.h"
#include "channel_double.h"
#include "qp.h"
#include "plot.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


static
inline
void x11_DrawLine(struct qp_graph *gr,
    int *new_line,
    double from_x, double from_y,
    double to_x, double to_y)
{
  /* This draws a line at least 10 times faster than Cairo */
  XDrawLine(gr->x11->dsp, gr->x11->pixmap, gr->x11->gc,
      INT(from_x), INT(from_y), INT(to_x), INT(to_y));
}

/*
 * This must be FAST!
 *
 * terminate   is a boolean  1 -> terminate the line and next time
 *             start a new line
 *
 * new_line    is a boolean that tells us if the last time this was
 *             called the line terminated, and we need to start a
 *             new line. This must be initialized to 1 on the first call.
 */

static
inline
void cairo_DrawLine(struct qp_graph *gr,
    int *new_line,
    double from_x, double from_y,
    double to_x, double to_y)
{
/* Tests show that defining this EXTRA_CULL makes
 * drawing a little faster when there are greater
 * than 1,000,000 points.  But this speed
 * increase is not significant.  Just like 0.25
 * seconds out of 4 seconds.  We think simple is
 * better.  It appears that the cairo line drawing
 * is already doing something to give good results. */
//#define EXTRA_CULL
#ifdef EXTRA_CULL
  /* yes this is not thread safe */
  static int prev_to_x = INT_MAX, prev_to_y = INT_MAX;
  int x, y;
#endif

  /* If numbers of out of range we need to know. If the line width
   * is too large this will not work. */
  ASSERT(to_x > -50. && to_x < 3.0*app->root_window_width + 50);
  ASSERT(from_x > -50. && from_x < 3.0*app->root_window_width + 50);
  ASSERT(to_y > -50. && to_y < 3.0*app->root_window_height + 50);
  ASSERT(from_y > -50. && from_y < 3.0*app->root_window_height + 50);


  if(*new_line)
  {
    /* Tests show that casting to int makes drawing faster in cairo */
    cairo_move_to(gr->cr, INT(from_x), INT(from_y));
#ifdef EXTRA_CULL
    /* reset the prev_to values */
    prev_to_x = INT_MAX;
    prev_to_y = INT_MAX;
#endif
  }

#ifdef EXTRA_CULL
  x = INT(to_x);
  y = INT(to_y);
  /* Do not draw if we are drawing to the same point again.
   * This is slower for small data files, but a big gain
   * in speed for large data files. */
  if(prev_to_x != x || prev_to_y != y)
    cairo_line_to(gr->cr, x, y);
  prev_to_x = x;
  prev_to_y = y;
#else
    cairo_line_to(gr->cr, INT(to_x), INT(to_y));
#endif


  *new_line = 0;
}


/*
 * This must be FAST!
 *
 * new_line   is a boolean that tells us if the last time this was
 *            called the line terminated, and we need to start a
 *            new line. This must be initialized to 1 on the first call.
 *
 *  Quickplot must cull when drawing lines, after zooming,
 *  so that we do not try to draw lines "out of bounds".
 *  Like for example, after zooming we may have points that
 *  are way out side the drawing area.  Drawing lines with
 *  end points outside the drawing area will/may give undefined
 *  results.  We have seen examples without culling where
 *  the lines look like they are drawn from random end points,
 *  when that was clearly not what we wanted to see.  Even
 *  though cario uses doubles it does not map end points of
 *  lines that are outside the drawing area to edges where
 *  we would expect to see them, at least not all the time.
 *
 *  Besides that, it turns out that culling out lines
 *  is faster than trying to draw them.  Which is what
 *  you would expect.
 *
 *  In this function, we find the points on the drawing area
 *  edges where the line would intersect and draw lines "in
 *  bounds" to represent lines that would otherwise be drawn
 *  off the drawing area.
 */
static
inline
void CullDrawLine(struct qp_graph *gr,
    
    int *new_line, /* if (*new_line) then we start a
                    * new line, else we continue the last
                    * line */ 

    /* We treat the drawing area (x0,y0) to (x1,y1) like it is
     * (minusLWidthP1,minusLWidthP1) to (widthPlus,heightPlus)
     * though the real drawing area is smaller we use a larger
     * area in order to catch edges of thick lines that may
     * pass through the edge of the real drawing area.
     */
    double minusLWidthP1, double widthPlus, double heightPlus,

    /* fromX, fromY and toX, toY are in pixels that define
     * a line that we wish to draw. But if they are
     * not in the drawing region we must find the line that
     * they would make though the drawing region by finding
     * side intersection points.  You can not draw lines from
     * points like (x,y) 10, -1e10 to 10, 1e10 but we can find
     * edge intersections of a line that we can draw from. 
     * The number 1e10 is to large to be used to draw lines
     * with. */
    double fromX, double fromY,
    double toX, double toY)
{
  if(!is_good_double(fromX) || !is_good_double(fromY) ||
      !is_good_double(toX) || !is_good_double(toY))
  {
    /* We need to do this uncommon case first because bad
     * values can not be used well in the code that follows. */
    *new_line = 1;
    return;
  }

  
  // The common cases come first.
  
  // If both points are in the drawing area
  if(fromX > minusLWidthP1 && fromX < widthPlus &&
     toX > minusLWidthP1 && toX < widthPlus &&
     fromY > minusLWidthP1 && fromY < heightPlus &&
     toY > minusLWidthP1 && toY < heightPlus)
    {
      gr->DrawLine(gr, new_line, fromX, fromY, toX, toY);
      return;
    }

  
  // Do some quick Culls for both points way off to one side: right,
  // left, up, or down.  This will Cull MOST points out!  Speed is
  // the result.

  //points all on right side,
  if((fromX >= widthPlus && toX >= widthPlus) ||
     //  on the left side,
     (fromX <= minusLWidthP1 && toX <= minusLWidthP1) ||
     //  on top (up),
     (fromY <= minusLWidthP1 && toY <= minusLWidthP1) ||
     // or on bottom (down)
     (fromY >= heightPlus && toY >= heightPlus))
  {
    /* we start over the next line draw */
    *new_line = 1;
    return; // Culled
  }
  

  // now both points are not in the drawing area (maybe one point is).
  // Below we refer to the line formed by the two points fromX, fromY and
  // toX, toY as "line" or "the line" ...
  

  /* close to horizontal lines are more common than close
   * to vertical lines */


  if(ABSVAL(toX - fromX) > 0.01)
  {
    double m; /* slope of the line */
    m = (toY - fromY)/(toX - fromX);

    if(ABSVAL(m) < 1.0e+5)
    {
      /* The slope is not large */
      double a;
      a = fromY - m * fromX;
      
      /* the equation for the line is:  Y = m * X + a  */

      if(fromX < minusLWidthP1)
      {
        fromX = minusLWidthP1;
        fromY = m * fromX + a;
      }
      else if(fromX > widthPlus)
      {
        fromX = widthPlus;
        fromY = m * fromX + a;
      }

      if(toX < minusLWidthP1)
      {
        toX = minusLWidthP1;
        toY = m * toX + a;
      }
      else if(toX > widthPlus)
      {
        toX = widthPlus;
        toY = m * toX + a;
      }

      if((fromY < minusLWidthP1 && toY < minusLWidthP1)
          ||
        (fromY > heightPlus && toY > heightPlus))
      {
        /* The line is above or below the drawing area */
        *new_line = 1;
        return; // Culled
      }
  
      if(toY < minusLWidthP1)
      {
        toY = minusLWidthP1;
        toX = (toY - a)/m;
      }
      else if(toY > heightPlus)
      {
        toY = heightPlus;
        toX = (toY - a)/m;
      }

      if(fromY < minusLWidthP1)
      {
        fromY = minusLWidthP1;
        fromX = (fromY - a)/m;
      }
      else if(fromY > heightPlus)
      {
        fromY = heightPlus;
        fromX = (fromY - a)/m;
      }

      gr->DrawLine(gr, new_line, fromX, fromY, toX, toY);
      return;
    }
  }


  /* Now we do the more vertical line case */

  if(ABSVAL(toY - fromY) > 0.01)
  {
    double u; /* inverse slope of the line */
    u = (toX - fromX)/(toY - fromY);

    if(ABSVAL(u) < 1.0e5)
    {
      /* The inverse slope is not too large */
      double b;
      b = fromX - u * fromY;
      
      /* the equation for the line is:  X = u * Y + b  */

      if(fromY < minusLWidthP1)
      {
        fromY = minusLWidthP1;
        fromX = u * fromY + b;
      }
      else if(fromY > heightPlus)
      {
        fromY = heightPlus;
        fromX = u * fromY + b;
      }
      
      if(toY < minusLWidthP1)
      {
        toY = minusLWidthP1;
        toX = u * toY + b;
      }
      else if(toY > heightPlus)
      {  
        toY = heightPlus;
        toX = u * toY + b;
      }

      if((fromX < minusLWidthP1 && toX < minusLWidthP1)
          ||
        (fromX > widthPlus && toX > widthPlus))
      {
        /* The line is to the left or right of the drawing area */
        *new_line = 1;
        return; // Culled
      }
       
      /* The line goes through the drawing area */
  
      if(toX < minusLWidthP1)
      {
        toX = minusLWidthP1;
        toY = (toX - b)/u;
      }
      else if(toX > widthPlus)
      {
        toX = widthPlus;
        toY = (toX - b)/u;
      }

      if(fromX < minusLWidthP1)
      {
        fromX = minusLWidthP1;
        fromY = (fromX - b)/u;
      }
      else if(fromX > widthPlus)
      {
        fromX = widthPlus;
        fromY = (fromX - b)/u;
      }

      gr->DrawLine(gr, new_line, fromX, fromY, toX, toY);
      return;
    }
  }

  /* The points are close to each other and not in the
   * drawing area */

  *new_line = 1;
  // Culled
}

static inline
void draw_grid(struct qp_graph *gr, cairo_t *cr,
      double xscale, double xshift, double yscale, double yshift,
      int width, int height)
{
  if((gr->same_x_scale || gr->same_y_scale) &&
      qp_sllist_length(gr->plots) > 0 &&
      gr->show_grid)
  {
    struct qp_plot *p;
    p = qp_sllist_first(gr->plots);
    ASSERT(p);
    /* We need to initialize the plot scaling for this
     * graph grid drawing */
    qp_plot_scale(p, xscale, xshift, yscale, yshift);
    qp_graph_grid_draw(gr, p, cr, width, height);
  }
}


/* width, height      give the size of the thing being drawn
 *                    on in pixels
 *
 * x, y           draw at x, y on this cairo surface.
 *                translate the plots, in pixels, which
 *                is where the origin is on the
 *                width by height surface
 *
 * the graph (gr) keep a shift and scale that map the plot
 * data from qp_plot_begin_x() and qp_plot_nextx() and etc
 * from an square area of in doubles x,y [0,0 to 1,1] to an
 * area that is the size of the drawing_area in pixels like
 * x,y [0,0 to 800,600]
 *
 * width, height   is not necessarily the same size as the
 *                 drawing_area widget, likely it is larger
 *                 like 2000 by 3000
 */
static inline
void graph_draw(struct qp_graph *gr, cairo_t *cr,
    int x, int y, int width, int height)
{
  struct qp_plot *p;
  /* These doubles will hold the net result of zoom and pixel scaling 
   * the zoom, gr->z, changes as the user zooms in and out and the
   * pixel scaling changes with the drawing area widget size allocation */
  double xscale, xshift, yscale, yshift;
  xscale = gr->xscale*gr->z->xscale;
  yscale = gr->yscale*gr->z->yscale;
  xshift = gr->xscale*gr->z->xshift + gr->xshift + x;
  yshift = gr->yscale*gr->z->yshift + gr->yshift + y;

  if(gr->x11 && gr->background_color.a < 0.05)
  {
    /* For some reason when drawing with X11
     * and having mostly-transparent background the
     * drawing seems to fail to cover the old
     * drawing.  Painting the whole area to
     * start with seems to fix it. */
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1,1,1,1.0);
    cairo_paint(cr);
  }

  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

  cairo_set_source_rgba(cr, gr->background_color.r,
      gr->background_color.g, gr->background_color.b,
      gr->background_color.a);

  cairo_paint(cr);

  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);


  if(gr->qp->shape)
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
  else
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);


  draw_grid(gr, cr, xscale, xshift, yscale, yshift, width, height);

  if(gr->show_grid && (gr->same_x_scale || gr->same_y_scale) &&
      qp_sllist_length(gr->plots) > 0)
  {
    struct qp_plot *p;
    p = (struct qp_plot *)qp_sllist_first(gr->plots);
    /* We need to initialize the plot scaling for this
     * graph grid drawing */
    qp_plot_scale(p, xscale, xshift, yscale, yshift);
    qp_graph_grid_draw(gr, p, cr, width, height);
  }

  if(gr->x11)
  {
    /* Get ready to draw with X11 API calls now */
    cairo_surface_flush(cairo_get_target(cr));
    gr->DrawLine = x11_DrawLine;
  }
  else
  {
    gr->DrawLine = cairo_DrawLine;
    gr->cr = cr;
  }

  p = (struct qp_plot *) qp_sllist_begin(gr->plots);

  while(p)
  {
    double x_val, y_val;

    if(p->lines)
    {
      /* draw lines */

      double minusLineWidthPlus1, widthPlus, heightPlus;
      double prev_x, prev_y;
      int new_line = 1;

      minusLineWidthPlus1 = - p->line_width - 1;
      widthPlus = width + p->line_width;
      heightPlus = height + p->line_width;

      if(gr->x11)
      {
        XSetLineAttributes(gr->x11->dsp, gr->x11->gc, INT(p->line_width),
            LineSolid, CapRound, JoinRound);
        XSetForeground(gr->x11->dsp, gr->x11->gc, p->l.x);
      }
      else
      {
        cairo_set_source_rgba(cr, p->l.c.r, p->l.c.g, p->l.c.b, p->l.c.a);
        cairo_set_line_width(cr, p->line_width);
      }

      if(qp_plot_begin(p, xscale, xshift, yscale, yshift,
            INT(minusLineWidthPlus1), INT(minusLineWidthPlus1),
            INT(widthPlus), INT(heightPlus),
            &prev_x, &prev_y))
      {
        /* We start with a good point in  prev_x, prev_y */
        while((!is_good_double(prev_x) || !is_good_double(prev_y)) &&
            qp_plot_next(p, &prev_x, &prev_y));

        while(qp_plot_next(p, &x_val, &y_val))
        {
          CullDrawLine(gr, &new_line,
              minusLineWidthPlus1, widthPlus, heightPlus,
              prev_x, prev_y, x_val, y_val);
          if(p->gaps)
          {
            prev_x = x_val;
            prev_y = y_val;
          }
          else /* no gaps */
          {
            /* do not lift up the pen if no gaps */
            if(is_good_double(x_val) && is_good_double(y_val))
            {
              /* This may be any number of points from before
               * if there where an NAN or something. */
              prev_x = x_val;
              prev_y = y_val;
            }
            if(new_line)
              new_line = 0;
          }
        }

        if(!gr->x11)
          cairo_stroke(cr);
      }
    }

    if(p->points)
    {
      double point_w, point_w2;
      int ipoint_w;
      double point_min, point_xmax, point_ymax;
      point_w2 = (point_w = p->point_size)/2;
      point_min = - point_w2 - 2;
      point_xmax = width + point_w2 + 1;
      point_ymax = height + point_w2 + 1;
      ipoint_w = INT(point_w);

      if(gr->x11)
        XSetForeground(gr->x11->dsp, gr->x11->gc, p->p.x);
      else
        cairo_set_source_rgba(cr, p->p.c.r, p->p.c.g, p->p.c.b, p->p.c.a);


      /* Putting the point width offset (point_w2) into
       * the plot data reader object is faster than adding
       * the point width offset in the tight loop in the
       * cairo_rectangle() call where we would add it
       * every loop interation. */
      
      if(qp_plot_begin(p, xscale, xshift - point_w2,
                       yscale, yshift - point_w2,
                       INT(point_min), INT(point_min),
                       INT(point_xmax), INT(point_ymax),
                       &x_val, &y_val))
      {
        int prev_x = INT_MAX, prev_y = INT_MAX;
        do
        {
          //DEBUG("%g %g\n", x_val, y_val);
          if(is_good_double(x_val) && is_good_double(y_val) &&
              /* point culling is easy */
              point_min < x_val && point_min < y_val &&
              x_val < point_xmax && y_val < point_ymax)
          {
            int x, y;
            x = INT(x_val);
            y = INT(y_val);
            /* speed up point drawing by not drawing points
             * that are on top of adjacent points more than once.
             * This can be the biggest time saver when there are
             * over 100,000 points.  Note this assumes that
             * points that as close in x,y space are adjacent
             * in the series (channel). This will slow down
             * plotting of small files, but not enough that
             * we can measure.  Tests show that cairo rectangle
             * drawing is much slower than line drawing.  Cairo
             * does not appear to be optimised for small rectangle
             * drawing.  Single pixel drawing in cairo uses
             * 1x1 rectangles, which are no faster to draw.
             * We convert the doubles to ints in the call to
             * cairo_rectangle() just because it speeds up
             * drawing. */
            if(prev_x != x || prev_y != y)
            {
              if(gr->x11)
                XFillRectangle(gr->x11->dsp, gr->x11->pixmap,
                    gr->x11->gc, x, y, ipoint_w, ipoint_w);
              else
                cairo_rectangle(cr, x, y, point_w, point_w);
            }
            prev_x = x;
            prev_y = y;
          }
        } while(qp_plot_next(p, &x_val, &y_val));

        if(!gr->x11)
          cairo_fill(cr);
      }
    }
    /* The mouse pointer value picker needs this to be reset from the
     * - point_w2 offset above.  Needed for all plots when
     * using the value picker GUI */
    qp_plot_scale(p, xscale, xshift, yscale, yshift);
 
    p = (struct qp_plot *) qp_sllist_next(gr->plots);
  }
}

static inline
void draw_from_pixbuf(cairo_t *cr, struct qp_graph *gr,
    int gr_pixel_width, int gr_pixel_height)
{
  /* This is where we draw from the back buffer to another buffer */
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface(cr, gr->pixbuf_surface,
      -INT(gr->pixbuf_x+gr->grab_x), -INT(gr->pixbuf_y+gr->grab_y));
  cairo_rectangle(cr, 0, 0, gr_pixel_width, gr_pixel_height);
  cairo_fill(cr);

  // debuging by seeing a PNG of what we drew
  //cairo_surface_write_to_png(cairo_get_target(gdk_cr), "y.png");
}

static
gboolean idle_callback(gpointer data)
{
  struct qp_graph *gr;
  gr = (struct qp_graph*) data;

  --gr->ref_count;

  if(gr->destroy_called)
  {
    qp_graph_destroy(gr);
    return FALSE;
  }

  ASSERT(gr->ref_count > 0);

  gr->waiting_to_resize_draw = 0;
  gtk_widget_queue_draw(gr->drawing_area);
  
  //WARN("QUEUED the draw\n");
  return FALSE;
}

static
inline
void draw_zoom_box(cairo_t *gdk_cr, struct qp_graph *gr)
{
  cairo_set_operator(gdk_cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba(gdk_cr, 0.84, 0.9, 1.0, 0.5);
  cairo_rectangle(gdk_cr, gr->z_x, gr->z_y, gr->z_w, gr->z_h);
  cairo_fill(gdk_cr);
  gr->draw_zoom_box = 2;
}

static inline
void draw_value_pick_line(cairo_t *gdk_cr, struct qp_graph *gr,
    int width, int height)
{
  int x, y, mode;
  mode = gr->value_mode & 3;
  x = gr->value_pick_x - gr->pixbuf_x - gr->grab_x;
  y = gr->value_pick_y - gr->pixbuf_y - gr->grab_y;

  cairo_set_operator(gdk_cr, CAIRO_OPERATOR_OVER);
  cairo_set_line_width(gdk_cr, 4);
  cairo_set_source_rgba(gdk_cr, 0.94, 0.94, 1.0, 0.65);
  cairo_move_to(gdk_cr, x, 0);
  cairo_line_to(gdk_cr, x, height);
  if(!mode)
  {
    cairo_move_to(gdk_cr, 0, y);
    cairo_line_to(gdk_cr, width, y);
  }
  cairo_stroke(gdk_cr);
  cairo_set_line_width(gdk_cr, 2);
  cairo_set_source_rgba(gdk_cr, 0.004, 0.004, .004, 0.7);
  cairo_move_to(gdk_cr, x, 0);
  cairo_line_to(gdk_cr, x, height);
  if(!mode)
  {
    cairo_move_to(gdk_cr, 0, y);
    cairo_line_to(gdk_cr, width, y);
  }
  cairo_stroke(gdk_cr);
}


static inline
cairo_region_t *
get_cairo_region_create_from_surface(struct qp_graph *gr,
    cairo_surface_t *surface, int width, int height)
{
  /* TODO: this is a resource pig.  Make it better. */

  cairo_rectangle_int_t rect;
  cairo_surface_t *image;
  cairo_region_t *region;
  cairo_t *cr;
  uint32_t *data, bg;
  int x, y, stride;

  if(!gr->x11)
    /* Creates region that covers the area where the
     * given surface is more than 50% opaque.
     * The below code copies this method.  This GDK
     * code is a pig too. */
    return gdk_cairo_region_create_from_surface(surface);

  if(!gr->x11->background_set)
  {
    /* We need to see what the background color is when it is
     * applied to an image.  A small image. */
    image = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1, 1);
    cr = cairo_create(image);
    cairo_set_source_rgba(cr, gr->background_color.r,
        gr->background_color.g, gr->background_color.b,
        gr->background_color.a);
    cairo_paint (cr);
    cairo_destroy (cr);
    data = (void *) cairo_image_surface_get_data(image);
    gr->x11->background = (data[0] & 0x00FFFFFF);
    cairo_surface_destroy(image);
    gr->x11->background_set = 1;
  }

  bg = gr->x11->background;

  image = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
  cr = cairo_create(image);
  cairo_set_source_surface(cr, surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  data = (void *) cairo_image_surface_get_data(image);
  stride = cairo_image_surface_get_stride(image);

  region = cairo_region_create();

  for(y=0; y < height; y++)
    {
      for(x=0; x < width; x++)
        {
          /* Search for a continuous range of "background pixels"*/
          gint x0=x;
          while(x < width)
            {
              if((data[x] & 0x00FFFFFF) == bg)
                /* This pixel is the background color */
                break;
              x++;
            }

          if(x > x0)
            {
              /* Add the pixels (x0, y) to (x, y+1) as a new rectangle
               * in the region
               */
              rect.x = x0;
              rect.width = x - x0;
              rect.y = y;
              rect.height = 1;

              cairo_region_union_rectangle(region, &rect);
            }
        }
      data += stride/4;
    }

  cairo_surface_destroy(image);

  return region;
}


/* We double buffer the image.  It looks nice and it enables
 * grabbing the graph with the pointer and translating it.
 * We draw on our own larger surface and then copy part of that
 * to the gdk surface.
 *
 * We assume that this function is drawing to an exposed/showing
 * drawing area, so the status update will reflect the current
 * exposed/showing drawing area. */
void qp_graph_draw(struct qp_graph *gr, cairo_t *gdk_cr)
{
  GtkAllocation allocation;

  if(gr->waiting_to_resize_draw && !gr->qp->shape)
  {
    //WARN("gr=%p gr->name=\"%s\" gr->ref_count=%d\n", gr, gr->name, gr->ref_count);
    cairo_set_source_rgba(gdk_cr, gr->background_color.r,
      gr->background_color.g, gr->background_color.b,
      gr->background_color.a);

    cairo_paint(gdk_cr);

    g_idle_add_full(G_PRIORITY_LOW, idle_callback, gr, NULL);
    /* fight qp_graph_destroy() race condition with flag */
    ++gr->ref_count;
    /* We draw after the other widgets are drawn, in case drawing
     * takes a long time.  This waiting also gives a chance
     * for the watch cursor to show.  But that seems to only
     * show if the window had focus at the right time. */
    return;
  }

  gtk_widget_get_allocation(gr->drawing_area, &allocation);
  
  if(gr->pixbuf_needs_draw)
  {
    cairo_t *db_cr; /* double buffer cr */

    db_cr = cairo_create(gr->pixbuf_surface);
    graph_draw(gr, db_cr, gr->pixbuf_x, gr->pixbuf_y,
                  gr->pixbuf_width, gr->pixbuf_height);
    cairo_destroy(db_cr);
    // debuging
    //cairo_surface_write_to_png(gr->pixbuf_surface, "x.png");
    qp_win_set_status(gr->qp);
  }

  /* the GTK cairo_t *gdk_cr has no alpha bits so all the
   * alpha drawn to it will be smushed. */
  //WARN("content=0x%lx\n", (unsigned long)cairo_get_target(gdk_cr));


  if(!gr->qp->shape)
  {
    /* Not using the shape X11 extension */

    /* This is where we go from the back buffer to the drawing area */
    draw_from_pixbuf(gdk_cr, gr, allocation.width, allocation.height);

    if(gr->draw_zoom_box == 1)
      draw_zoom_box(gdk_cr, gr);
    if(gr->draw_value_pick)
      draw_value_pick_line(gdk_cr, gr, allocation.width, allocation.height);


    if(gr->pixbuf_needs_draw)
    {
      gdk_window_set_cursor(gtk_widget_get_window(gr->qp->window), NULL);
      gr->pixbuf_needs_draw = 0;
      // gr->qp->wait_warning_showing = 0;
    }
  }
  else
  {
    /* Use the X11 shape extension */


    /* TODO: This is a resource pig.  Fix it. */

    cairo_region_t *reg_draw_area, *window_region;
    /* empty flag */ 
    int empty;
    cairo_surface_t *mask_surface;
    GtkAllocation all;

    /* Make sure the surface is up to date */
    //cairo_surface_flush(gr->pixbuf_surface);

    /* make a sub surface that is the size of the graph drawing area */
    mask_surface = cairo_surface_create_for_rectangle(gr->pixbuf_surface,
        INT(gr->pixbuf_x+gr->grab_x),
        INT(gr->pixbuf_y+gr->grab_y),
        allocation.width, allocation.height);
    
    reg_draw_area = get_cairo_region_create_from_surface(gr,
        mask_surface, allocation.width, allocation.height);

    cairo_surface_destroy(mask_surface);

    cairo_region_translate(reg_draw_area, allocation.x, allocation.y);

    gtk_widget_get_allocation(gr->qp->window, &all);
    all.x = all.y = 0;

    window_region = cairo_region_create_rectangle(&all);

    cairo_region_subtract_rectangle(window_region, &allocation);

    empty = cairo_region_is_empty(reg_draw_area);

    if(!empty)  
      cairo_region_union(window_region, reg_draw_area);

    cairo_region_destroy(reg_draw_area);

    /* window_region is a region with a hole in it the
     * size of the drawing area with the graph and grid added back. */


    if(gr->draw_zoom_box && !empty)
    {
      cairo_rectangle_int_t rec;
      rec.x = allocation.x + gr->z_x;
      rec.y = allocation.y + gr->z_y;
      rec.width = gr->z_w;
      rec.height = gr->z_h;
      /* regions do not like negitive values or
       * maybe shapes do not like negitive values
       * in any case we keep width and height
       * positive */
      if(rec.width < 0)
      {
        rec.width *= -1;
        rec.x -= rec.width;
      }
      if(rec.height < 0)
      {
        rec.height *= -1;
        rec.y -= rec.height;
      }
        
      cairo_region_union_rectangle(window_region, &rec);
      /* now we have the zoom box added to window_region */
    }


    /* This is where we go from the back buffer to the drawing area */
    draw_from_pixbuf(gdk_cr, gr, allocation.width, allocation.height);
    if(gr->draw_zoom_box)
      draw_zoom_box(gdk_cr, gr);
    if(gr->draw_value_pick)
      draw_value_pick_line(gdk_cr, gr, allocation.width, allocation.height);


    if(empty)
    {
      /* we have nothing to make a shape with */
      if(gr->qp->last_shape_region)
      {
        cairo_region_destroy(gr->qp->last_shape_region);
        gr->qp->last_shape_region = NULL;
      }
      cairo_region_destroy(window_region);
      /* remove the old shape region */
      gtk_widget_shape_combine_region(gr->qp->window, NULL);
    }
    else if(!gr->qp->last_shape_region ||
        !cairo_region_equal(gr->qp->last_shape_region, window_region))
    {
      // DEBUG("creating new shape region\n");
      
      /* We need to undo the old shape first */
      gtk_widget_shape_combine_region(gr->qp->window, NULL);

      gtk_widget_shape_combine_region(gr->qp->window, window_region);

      if(gr->qp->last_shape_region)
        cairo_region_destroy(gr->qp->last_shape_region);

      gr->qp->last_shape_region = window_region;
    }
    else
      cairo_region_destroy(window_region);


    gr->pixbuf_needs_draw = 0;

    gdk_window_set_cursor(gtk_widget_get_window(gr->qp->window), NULL);
    // debuging
    //cairo_surface_write_to_png(cairo_get_target(gdk_cr), "y.png");
  }

  if(gr->qp->update_graph_detail && gr->qp->graph_detail)
  {
    gr->qp->update_graph_detail = 0;
    /* make the graph configure window show stuff about this graph */
    qp_win_graph_detail_init(gr->qp);
  }
}


int qp_win_save_png(struct qp_win *qp,
    struct qp_graph *gr, const char *filename)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GtkAllocation allocation;
  int ret = 0;

  ASSERT(qp);
  ASSERT(qp->window);
  if(!gr)
  {
    int page_num;
    GtkWidget *w;
    page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(qp->notebook));
    w = gtk_notebook_get_nth_page(GTK_NOTEBOOK(qp->notebook), page_num);
    gr = (struct qp_graph*) g_object_get_data(G_OBJECT(w), "qp_graph");
  }

  gtk_widget_get_allocation(gr->drawing_area, &allocation);
  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
      allocation.width, allocation.height);
  cr = cairo_create(surface);


  /* This is where we go from the back buffer to the image */
  draw_from_pixbuf(cr, gr, allocation.width, allocation.height);

  errno = 0;
  if(CAIRO_STATUS_SUCCESS ==
      cairo_surface_write_to_png(surface, filename))
    QP_NOTICE("Saved %s\n", filename);
  else
  {
    WARN("Failed to save: %s\n", filename);
    QP_WARN("Failed to save: %s\n", filename);
    ret = 1;
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);

  return ret;
}

