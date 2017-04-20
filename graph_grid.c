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
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

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



#define SMALL_NUM         SMALL_DOUBLE
#define TICKMIN           5.0
#define TICKMAX           8.0
#define TWO_DIGIT_FORMAT  "%.2g"
#define SCAN_FORMAT       "%lf"
#define FORMAT_LEN         8
/* must be smaller than FORMAT_LEN */
#define H_GRID_PRINT_FORMAT "%.15g" // High res
#define L_GRID_PRINT_FORMAT "%.6g"  // Low res
#define POW               pow
#define TEN               10.0

#define MAX_ABS(x,y)   ((ABSVAL(x) > ABSVAL(y))?ABSVAL(x):ABSVAL(y))


/* This is not too trival.  Number round-off must be avoided.  We
 * require the grid lines be drawn with the same method that the plot
 * points plot lines, so that they are not off by one pixel.  We must
 * not have plots that miss-lead the user in any way. */
static
void PreDrawGrid(struct qp_graph *gr, struct qp_plot *z, 

			int64_t *xmin_mat, int64_t *xmax_mat,
			int64_t *xinc, double *xpow_part,

			int64_t *ymin_mat, int64_t *ymax_mat,
			int64_t *yinc, double *ypow_part,
                        int width, int height)
{
  double xmin = qp_plot_get_xval(z, 0);
  double xmax = qp_plot_get_xval(z, width);
  double ymax = qp_plot_get_yval(z, 0);
  double ymin = qp_plot_get_yval(z, height);

  /***************************************************/
  /*        get the x value lines calculated         */
  /***************************************************/

  //DEBUG("pix width=%d  values xmin=%g xmax=%g\n", width, xmin, xmax);
  //DEBUG("pix height=%d values ymin=%g ymax=%g\n", height, ymin, ymax);


  double delta;
  /* change in x values between vertical grid lines */
  delta = (xmax - xmin)/(2 + ((double) width/gr->grid_x_space));

  //printf("xdelta=%g  gr->grid_x_space=%d\n", delta,
  //gr->grid_x_space);

  gint delta_p = (gint) log10(ABSVAL(delta) > SMALL_NUM ?
			      ABSVAL(delta) : SMALL_NUM);

  if(delta_p <= 0) delta_p--;

 
  *xpow_part = POW(TEN, delta_p);

  //printf("xdelta_p=%d  xpow_part=%g\n", delta_p, *xpow_part);

  {
    // strip off the round-off error off of xpow_part.
    char s[16];
    //for example: xpow_part=0.20005465 --> 
    //               s=0.20 --> xpow_part=0.2000000
    sprintf(s, TWO_DIGIT_FORMAT, *xpow_part);
    sscanf(s, SCAN_FORMAT, xpow_part);
    //printf("s=%s  xpow_part=%.15g\n",s, xpow_part);
  }

  *xmin_mat = (int64_t) (xmin/(*xpow_part));
  *xmax_mat = (int64_t) (xmax/(*xpow_part));

  //printf(" xmin = %ldx10%+d xmax = %ldx10%+d\n",
  //	 *xmin_mat, delta_p, *xmax_mat, delta_p);

  if(width > gr->grid_x_space)
    *xinc = (int64_t) ((*xmax_mat - *xmin_mat)/
		      ((double) width/
		       gr->grid_x_space));
  else
    *xinc = *xmax_mat - *xmin_mat;

  //printf("               xinc=%ld\n", *xinc);

  // round to 1, 2, 5, times 10^N
  if     (*xinc < (int64_t) 2)   *xinc = 1;
  else if(*xinc < (int64_t) 5)   *xinc = 2;
  else if(*xinc < (int64_t) 10)  *xinc = 5;
  else if(*xinc < (int64_t) 20)  *xinc = 10;
  else if(*xinc < (int64_t) 50)  *xinc = 20;
  else if(*xinc < (int64_t) 100) *xinc = 50;
  else if(*xinc < (int64_t) 200) *xinc = 100;
  else                           *xinc = 200;


  //DEBUG("xinc=%ld xmin_mat=%ld\n", *xinc, *xmin_mat);

  if(*xmin_mat > (int64_t) 0 && (*xmin_mat)%(*xinc))
    *xmin_mat -= (*xmin_mat)%(*xinc);
  else if(*xmin_mat < (int64_t) 0 && (*xmin_mat)%(*xinc))
    *xmin_mat -= *xinc + (*xmin_mat)%(*xinc);


  //printf("               xinc=%ld\n", *xinc);
  
  /***************************************************/
  /*        get the y value lines calculated         */
  /***************************************************/

  delta = (ymax - ymin)/(2 + ((double) height/
			      gr->grid_y_space));
 
  delta_p = (gint) log10(ABSVAL(delta) > SMALL_NUM ?
			 ABSVAL(delta) : SMALL_NUM);

  if(delta_p <= 0) delta_p--;

  //printf("ydelta_p=%d\n", delta_p);

  *ypow_part = POW(TEN, delta_p);
  {
    // strip off the round-off error off of ypow_part.
    char s[16];
    //for example: ypow_part=0.20005465 -->
    //            s=0.20 --> ypow_part=0.2000000
    sprintf(s, TWO_DIGIT_FORMAT, *ypow_part);
    sscanf(s, SCAN_FORMAT, ypow_part);
  }

  *ymin_mat = (int64_t) (ymin/(*ypow_part));
  *ymax_mat = (int64_t) (ymax/(*ypow_part));

  //printf(" ymin = %ldx10%+d ymax = %ldx10%+d\n",
  //  *ymin_mat, delta_p, *ymax_mat, delta_p);

  if(height > gr->grid_y_space)
    *yinc = (int64_t) ((*ymax_mat - *ymin_mat)/
		      ((double) height/
		       gr->grid_y_space));
  else
    *yinc = *ymax_mat - *ymin_mat;

  if     (*yinc < (int64_t) 2)   *yinc = 1;
  else if(*yinc < (int64_t) 5)   *yinc = 2;
  else if(*yinc < (int64_t) 10)  *yinc = 5;
  else if(*yinc < (int64_t) 20)  *yinc = 10;
  else if(*yinc < (int64_t) 50)  *yinc = 20;
  else if(*yinc < (int64_t) 100) *yinc = 50;
  else if(*yinc < (int64_t) 200) *yinc = 100;
  else                           *yinc = 200;


  //printf("yinc=%ld\n", *yinc);

  if(*ymin_mat > (int64_t) 0 && (*ymin_mat)%(*yinc))
    *ymin_mat -= (*ymin_mat)%(*yinc);
  else if(*ymin_mat < (int64_t) 0 && (*ymin_mat)%(*yinc))
    *ymin_mat -= *yinc + (*ymin_mat)%(*yinc);


  
}

static inline
void DrawXGrid(cairo_t *cr, PangoLayout *pangolayout,
        struct qp_graph *gr, struct qp_plot *z,
	int64_t xmin_mat, int64_t xmax_mat,
	int64_t xinc, double xpow_part,
	int64_t ymin_mat, int64_t ymax_mat,
	int64_t yinc, double ypow_part,
        int width, int height, int view_height)
{
  /******************************************************/
  /* find where to put the value text for x lines       */
  /******************************************************/

  int64_t yLabel_start, yLabel_inc, yLabel_max;
  int64_t i, max;
  char FMT[FORMAT_LEN];
  
  if(gr->grid_numbers)
    {
      /* We can get better Y position precision if we
       * increase the range of the numbers in Y. */ 
      yinc *= 100;
      ymin_mat *= 100;
      ymax_mat *= 100;
      ypow_part /= 100;

      if(gr->same_y_scale)
	{
	  yLabel_inc = (qp_plot_get_yval(z, 0) -
			qp_plot_get_yval(z, view_height))/
	    ypow_part;

	  // make yLabel_inc a multiple of yinc
          if(yLabel_inc < 14*yinc/10)
            yLabel_inc = yinc;
          else if(yLabel_inc < 24*yinc/10)
	    yLabel_inc = 2*yinc;
          else if(yLabel_inc < 34*yinc/10)
            yLabel_inc = 3*yinc;
          else
            yLabel_inc -= yLabel_inc%yinc;
     

	  yLabel_start = qp_plot_get_yval(z, view_height - 15 +
				      gr->pixbuf_y)/
	    ypow_part;


	  yLabel_start -=
	    ((int64_t)(yLabel_start - ymin_mat + yinc*0.5))%yinc;

	  if(qp_plot_get_ypixel(z, yLabel_start*ypow_part) >
	     view_height + gr->pixbuf_y
	     - 10/* font height */
	     - 3 /* padding */)
	    yLabel_start += yinc;

	  yLabel_start += - ((int64_t)(yLabel_start -
				       ymin_mat + yinc*0.5)) +
	    ((int64_t)(yLabel_start - ymin_mat + yinc*0.5))%yLabel_inc;

	  yLabel_max = ymax_mat - yinc*0.5;
	  yLabel_max -= ((yLabel_max - yLabel_start)% yLabel_inc);
	}
      else
	{
	  yLabel_inc = (qp_plot_get_yval(z,0) -
			qp_plot_get_yval(z,view_height))/
	    ypow_part;

	   yLabel_start = qp_plot_get_yval(z,view_height - 15 +
				    gr->pixbuf_y)/
	     ypow_part;

	   yLabel_start += - ((int64_t)(yLabel_start - ymin_mat)) +
	     ((int64_t)(yLabel_start - ymin_mat))%yLabel_inc;

	   yLabel_max = ymax_mat;
	   yLabel_max -= ((yLabel_max - yLabel_start)% yLabel_inc);
	}
    }


  max = ABSVAL(xmin_mat);
  if(ABSVAL(xmax_mat) > max)
    max = ABSVAL(xmax_mat);


  if(max/xinc > 10000) {
    strcpy(FMT, H_GRID_PRINT_FORMAT);
    //printf("x high res\n");
  }
  else
    strcpy(FMT, L_GRID_PRINT_FORMAT);


  /****************************************************
   *     draw x labels 
   ****************************************************/
  if(gr->grid_numbers)
  {
    cairo_set_source_rgba(cr, gr->grid_text_color.r,
              gr->grid_text_color.g, gr->grid_text_color.b,
              gr->grid_text_color.a);

    for(i=xmin_mat; i <= xmax_mat; i += xinc)
    {
      char str[64];
      int x;
      int64_t j;
      
      snprintf(str,64, FMT, i*xpow_part);

      //printf(" xpow_part=%.15g\n",xpow_part);
      //printf(GRID_PRINT_FORMAT" ", i*xpow_part);

      x = qp_plot_get_xpixel(z, i*xpow_part);
        
      for(j = yLabel_start; j <= yLabel_max; j += yLabel_inc)
      {
        cairo_translate(cr, x+3+ gr->grid_line_width/2,
	    qp_plot_get_ypixel(z, j*ypow_part)
	    - 5 /* 1/2 font height */);
        pango_layout_set_text(pangolayout, str, -1);

        pango_cairo_update_layout(cr, pangolayout);
        pango_cairo_show_layout(cr, pangolayout);

        /* clear the translation */
        cairo_identity_matrix(cr);
            
	//printf("%g\n", j*ypow_part);
      }
    }
  }

  /****************************************************
   *     draw vertical lines at x values 
   ****************************************************/
  cairo_set_source_rgba(cr, gr->grid_line_color.r,
              gr->grid_line_color.g, gr->grid_line_color.b,
              gr->grid_line_color.a);
  cairo_set_line_width(cr, gr->grid_line_width);

  for(i=xmin_mat; i <= xmax_mat; i += xinc)
  {
    int x;
    x = qp_plot_get_xpixel(z, i*xpow_part); // gr->grid_line_width;
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, height);
  }
  /* draw the lines */
  cairo_stroke(cr);
}

static inline
void DrawYGrid(cairo_t *cr, PangoLayout *pangolayout,
        struct qp_graph *gr, struct qp_plot *z,
	int64_t xmin_mat, int64_t xmax_mat,
	int64_t xinc, double xpow_part,
	int64_t ymin_mat, int64_t ymax_mat,
	int64_t yinc, double ypow_part,
        int width, int height, int view_width)
{
  /******************************************************/
  /* find where to put the value text for y lines       */
  /******************************************************/

  int64_t xLabel_start, xLabel_inc, xLabel_max;
  gint x_ylinetext;
  int64_t i, max;
  char FMT[FORMAT_LEN];


  x_ylinetext = qp_plot_get_xpixel(z, (xmin_mat + xinc*0.2)*xpow_part);
  if(x_ylinetext < 3)
    x_ylinetext = qp_plot_get_xpixel(z, (xmin_mat + xinc*1.2)*xpow_part);


  if(gr->grid_numbers)
    {
      /* We can get better X position precision if we
       * increase the range of the numbers in X. */

      xmin_mat *= 100;
      xmax_mat *= 100;
      xinc *= 100;
      xpow_part /= 100;

      if(gr->same_x_scale)
	{
	  xLabel_inc = (qp_plot_get_xval(z, view_width) -
			qp_plot_get_xval(z, 0))/
	    xpow_part;

	  // make xLabel_inc a multiple of xinc
          if(xLabel_inc < 14*xinc/10)
            xLabel_inc = xinc;
          else if(xLabel_inc < 24*xinc/10)
	    xLabel_inc = 2*xinc;
          else if(xLabel_inc < 34*xinc/10)
            xLabel_inc = 3*xinc;
          else
            xLabel_inc -= xLabel_inc%xinc;

	  xLabel_start = qp_plot_get_xval(z, gr->pixbuf_x
		       - 14 /* padding from grid line */)/
	    xpow_part;

	  xLabel_start -=
	    ((int64_t)(xLabel_start - xmin_mat))%xinc;

	  if(qp_plot_get_xpixel(z, xLabel_start*xpow_part) <
	     gr->pixbuf_x - 14 /* padding */)
	    xLabel_start += xinc;

	  xLabel_start += - ((int64_t)(xLabel_start - xmin_mat)) +
	    ((int64_t)(xLabel_start - xmin_mat))%xLabel_inc;

	  xLabel_max = xmax_mat;
	  xLabel_max -= ((xLabel_max - xLabel_start)%xLabel_inc);
	}
      else
	{
	  xLabel_inc = (
              qp_plot_get_xval(z, view_width) -
              qp_plot_get_xval(z, 0)
              )/xpow_part;
	  
	  xLabel_start = qp_plot_get_xval(z, gr->pixbuf_x)/xpow_part;
	  
	  xLabel_start += - ((int64_t)(xLabel_start - xmin_mat)) +
	    ((int64_t)(xLabel_start - xmin_mat))%xLabel_inc;
	  
	  xLabel_max = xmax_mat;
	  xLabel_max -= ((xLabel_max - xLabel_start)%xLabel_inc);
	}
    }


  max = ABSVAL(ymin_mat);
  if(ABSVAL(ymax_mat) > max)
    max = ABSVAL(ymax_mat);


  if(max/yinc > 10000) {
    strcpy(FMT, H_GRID_PRINT_FORMAT);
    //printf("y high res\n");
  }  else
    strcpy(FMT, L_GRID_PRINT_FORMAT);


  /****************************************************
   *     draw y labels 
   ****************************************************/
  if(gr->grid_numbers)
  {
    cairo_set_source_rgba(cr, gr->grid_text_color.r,
              gr->grid_text_color.g, gr->grid_text_color.b,
              gr->grid_text_color.a);

    for(i=ymin_mat; i <= ymax_mat; i += yinc)
    {
      char str[64];
      int y;
      int64_t j;

      snprintf(str,64, FMT, i*ypow_part);

      //snprintf(str,64, "%*.*g", digit_count,digit_count, i*ypow_part);
      //printf(GRID_PRINT_FORMAT" ", str);

      y = qp_plot_get_ypixel(z, i*ypow_part);

      for(j = xLabel_start; j <= xLabel_max; j += xLabel_inc)
      {
        cairo_translate(cr,
            qp_plot_get_xpixel(z, j*xpow_part) + gr->grid_line_width/2 + 10,
            y+ gr->grid_line_width/2);
        pango_layout_set_text(pangolayout, str, -1);
        pango_cairo_update_layout(cr, pangolayout);
        pango_cairo_show_layout(cr, pangolayout);
        /* clear the translation */
        cairo_identity_matrix(cr);
      }
    }
  }
  /****************************************************
   *     draw horizontal lines at y values 
   ****************************************************/
  cairo_set_source_rgba(cr, gr->grid_line_color.r,
              gr->grid_line_color.g, gr->grid_line_color.b,
              gr->grid_line_color.a);
  cairo_set_line_width(cr, gr->grid_line_width);

  for(i=ymin_mat; i <= ymax_mat; i += yinc)
  {
    int y;
    y = qp_plot_get_ypixel(z, i*ypow_part); // - gr->grid_line_width;
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, width, y);
  }
  /* draw the lines */
  cairo_stroke(cr);
}

void qp_graph_set_grid_font(struct qp_graph *gr)
{
  PangoFontDescription *desc = NULL;
  desc = pango_font_description_from_string(gr->grid_font);
  if(!desc)
      QP_WARN("Failed to get grid font \"%s\"\n", gr->grid_font);
  if(!desc)
    desc = pango_font_description_from_string(DEFAULT_GRID_FONT);
  ASSERT(desc);
  if(!desc)
  {
    QP_WARN("Failed to get default grid font \"%s\".\n"
        " Will not show numbers on the grid.\n", DEFAULT_GRID_FONT);
    gr->grid_numbers = 0;
  }
  else
  {
    pango_layout_set_font_description(gr->pangolayout, desc);
    pango_font_description_free(desc);
  }
}

static
PangoLayout *get_pangolayout(struct qp_graph *gr, cairo_t *cr)
{
  if(!gr->pangolayout)
  {
    gr->pangolayout = pango_cairo_create_layout(cr);
    qp_graph_set_grid_font(gr);
  }
  else
  {
    pango_cairo_update_layout(cr, gr->pangolayout);
  }

  if((gr->font_antialias_set && gr->qp->shape) ||
    (!gr->font_antialias_set && !gr->qp->shape))
  {
    /* Doing a lot of crap just to set one bit */
    cairo_font_options_t *fopt;
    PangoContext *pc;

    fopt = cairo_font_options_create();
  
    if(gr->qp->shape)
    {
      cairo_font_options_set_antialias(fopt, CAIRO_ANTIALIAS_NONE);
      gr->font_antialias_set = 0;
    }
    else
    {
      cairo_font_options_set_antialias(fopt, CAIRO_ANTIALIAS_DEFAULT);
      gr->font_antialias_set = 1;
    }

    pc = pango_layout_get_context(gr->pangolayout);
    pango_cairo_context_set_font_options(pc, fopt);
    cairo_font_options_destroy(fopt);
  }


  return gr->pangolayout;
}

void qp_graph_grid_draw(struct qp_graph *gr, struct qp_plot *p,
    cairo_t *cr, int width, int height)
{
  int64_t
        xmin_mat, xmax_mat, xinc,
	ymin_mat, ymax_mat, yinc;
  double xpow_part, ypow_part;
  PangoLayout *pangolayout = NULL;
  int view_width, view_height;
  
  ASSERT(gr);
  ASSERT(p);
  ASSERT(gr->drawing_area);
  ASSERT(gr->qp);
  ASSERT(gr->qp->window);

  if(!gr->show_grid) return;

  PreDrawGrid(gr, p,
	  &xmin_mat, &xmax_mat, &xinc, &xpow_part,
	  &ymin_mat, &ymax_mat, &yinc, &ypow_part,
          width, height);

  view_width = gtk_widget_get_allocated_width(gr->drawing_area);
  view_height = gtk_widget_get_allocated_height(gr->drawing_area);


  if(gr->grid_numbers)
    pangolayout = get_pangolayout(gr, cr);



#if 0
  DEBUG("\nxmin=%g xmax=%g\n",
      qp_plot_get_xval(p, 0), qp_plot_get_xval(p, width));
  APPEND("ymin=%g ymax=%g\n",
      qp_plot_get_yval(p, height), qp_plot_get_yval(p, 0));
  APPEND("xmin_mat=%ld xmax_mat=%ld xinc=%ld xpow_part=%g \n",
      xmin_mat, xmax_mat, xinc, xpow_part);
  APPEND("ymin_mat=%ld ymax_mat=%ld yinc=%ld ypow_part=%g \n",
      ymin_mat, ymax_mat, yinc, ypow_part);
  APPEND("delta_x pixels between lines=%d\n",
      qp_plot_get_xpixel(p, xpow_part*(xmin_mat + xinc)) -
      qp_plot_get_xpixel(p, xpow_part*(xmin_mat)));
  APPEND("delta_y pixels between lines=%d\n",
      qp_plot_get_ypixel(p, ypow_part*ymin_mat) -
      qp_plot_get_ypixel(p, ypow_part*(ymin_mat + yinc)));
#endif

  if(gr->same_x_scale)
      DrawXGrid(cr, pangolayout, gr, p,
	    xmin_mat, xmax_mat, xinc, xpow_part,
	    ymin_mat, ymax_mat, yinc, ypow_part,
            width, height, view_height);
  if(gr->same_y_scale)
      DrawYGrid(cr, pangolayout, gr, p,
	    xmin_mat, xmax_mat, xinc, xpow_part,
	    ymin_mat, ymax_mat, yinc, ypow_part,
            width, height, view_width);
}

