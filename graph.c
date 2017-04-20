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

#include <stdint.h>
#include <gtk/gtk.h>

#include "quickplot.h"

#include "config.h"
#include "debug.h"
#include "list.h"
#include "channel.h"
#include "channel_double.h"
#include "callbacks.h"
#include "qp.h"
#include "plot.h"
#include "zoom.h"
#include "color_gen.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


static
size_t gr_create_count = 0;


/* drawing area minimal size restriction are
 * required so that the graph drawing algorithms do not fail. */
const gint EDGE_BUF_MIN = 10;



/* returns a malloc() allocated string
 * Return a unique name for the qp */
static inline
char *unique_name(struct qp_win *qp, const char *name)
{
  char buf[32];
  char *test_name = NULL;
  size_t len = 0;
  int i = 1;

  ++gr_create_count;

  if(!name || !name[0])
  {
    snprintf(buf, 32, "graph %zu", gr_create_count);
    name = buf;
  }

  test_name = (char*) (name= qp_strdup(name));

  while(1)
  {
    struct qp_graph *gr;
    /* We compare with the name of all graphs
     * in the qp main window. */
    for(gr=qp_sllist_begin(qp->graphs); gr;
      gr=qp_sllist_next(qp->graphs))
      if(strcmp(test_name, gr->name) == 0)
        break;

    if(gr)
    {
      if(test_name == name)
        test_name = (char*) qp_malloc(len = (strlen(name)+16));
      snprintf(test_name, len, "%s[%d]", name, ++i);
    }
    else
    {
      if(test_name != name)
        free((char*) name);
      return test_name;
    }
  }
}

void add_graph_close_button(struct qp_graph *gr)
{
  GtkWidget *close_button, *image;
  ASSERT(gr->close_button == NULL);

  gr->close_button = (close_button = gtk_button_new());
  gtk_widget_set_size_request(close_button, 5, 5);

  image =
    gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_button_set_image(GTK_BUTTON(close_button), image);
  gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
  gtk_widget_show(image);

  gtk_box_pack_start(GTK_BOX(gr->tab_label_hbox), close_button,
	FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(close_button), "clicked",
	G_CALLBACK(cb_close_tab), gr);

  gtk_widget_set_name(close_button, "tab_close_button");
  gtk_widget_set_tooltip_text(close_button,"Close tab");
  gtk_widget_show(close_button);
}

/* the gr is assumed to be a gr that was just created and 
 * has not been drawn yet. */
void qp_graph_copy(struct qp_graph *gr, struct qp_graph *old_gr)
{
  struct qp_plot *p;
  ASSERT(gr->qp != old_gr->qp);
  ASSERT(gr->name);
  free(gr->name);


  gr->name = qp_strdup(old_gr->name);
  gtk_label_set_text(GTK_LABEL(gr->tab_label), gr->name);

  if(!old_gr->x11 && gr->x11)
  {
    free(gr->x11);
    gr->x11 = NULL;
  }
  else if(old_gr->x11 && !gr->x11)
  {
    gr->x11 = qp_malloc(sizeof(*(gr->x11)));
    gr->x11->gc = 0;
    gr->x11->pixmap = 0;
    gr->x11->dsp = 0;
    gr->x11->background = 0;
    gr->x11->background_set = 0;
  }
 
  for(p=qp_sllist_begin(old_gr->plots);p;p=qp_sllist_next(old_gr->plots))
    qp_plot_copy_create(gr, p)->plot_num = p->plot_num;

  memcpy(gr->color_gen, old_gr->color_gen, sizeof(*(gr->color_gen)));
  memcpy(&gr->background_color, &old_gr->background_color, sizeof(gr->background_color));
  memcpy(&gr->grid_line_color, &old_gr->grid_line_color, sizeof(gr->grid_line_color));
  memcpy(&gr->grid_text_color, &old_gr->grid_text_color, sizeof(gr->grid_text_color));

  gr->same_x_limits = old_gr->same_x_limits;
  gr->same_y_limits = old_gr->same_y_limits;
  gr->show_grid = old_gr->show_grid;
  gr->grid_numbers = old_gr->grid_numbers;
  gr->grid_line_width = old_gr->grid_line_width;
  gr->grid_on_top = old_gr->grid_on_top;

  gr->grid_x_space = old_gr->grid_x_space;
  gr->grid_y_space = old_gr->grid_y_space;

  gr->gaps = old_gr->gaps;
  gr->lines = old_gr->lines;
  gr->points = old_gr->points;
  gr->line_width = old_gr->line_width;
  gr->point_size = old_gr->point_size;

  gr->grab_x = old_gr->grab_x;
  gr->grab_y = old_gr->grab_y;
  gr->pixbuf_x = old_gr->pixbuf_x;
  gr->pixbuf_y = old_gr->pixbuf_y;
  gr->value_mode = old_gr->value_mode;
  qp_zoom_copy(gr->z, old_gr->z);
  gr->zoom_level = old_gr->zoom_level;
  gr->value_pick_x = old_gr->value_pick_x;
  gr->value_pick_y = old_gr->value_pick_y;

  if(old_gr->grid_font)
  {
    if(gr->grid_font)
      free(gr->grid_font);
    gr->grid_font = qp_strdup(old_gr->grid_font);
  }

  /* TODO: copy pangolayout??  May save memory by adding another
   * reference to it.  Also copying the pangolayout maybe
   * better incase the old_gr->grid_font is not a valid font. */

  /* does not copy the X11 shape mode */

  if(gr->qp != old_gr->qp)
  {
    if(gr->qp->graph_create_count < old_gr->qp->graph_create_count)
      gr->qp->graph_create_count = old_gr->qp->graph_create_count;
    gr->graph_num = old_gr->graph_num;
  }
}

qp_graph_t qp_graph_create(qp_win_t qp, const char *name)
{
  struct qp_graph *gr;
  if(!qp) qp = default_qp;

  ASSERT(qp);
  ASSERT(qp->graphs);
  ASSERT(qp->window);
  /* graphs do not exist unless they are
   * in a qp_win with a gtk window. */
  if(!qp || !qp->window)
    return NULL;

  gr = (struct qp_graph *) qp_malloc(sizeof(*gr));
  memset(gr, 0, sizeof(*gr));
  gr->ref_count = 1;
  gr->graph_num = ++qp->graph_create_count;
  gr->name = unique_name(qp, name);
  gr->color_gen = qp_color_gen_create();
  gr->zoom_level = 0;
  gr->plots = qp_sllist_create(NULL);
  qp_sllist_append(qp->graphs, gr);

  gr->gaps = qp->gaps;
  gr->lines = qp->lines;
  gr->points = qp->points;
  gr->line_width = qp->line_width;
  gr->point_size = qp->point_size;

  gr->show_grid = qp->grid;
  gr->grid_numbers = qp->grid_numbers;
  gr->grid_x_space = qp->grid_x_space;
  gr->grid_y_space = qp->grid_y_space;
  gr->grid_line_width = qp->grid_line_width;

  gr->qp = qp;
  gr->pangolayout = NULL;
  gr->grid_font = qp_strdup(qp->grid_font);

  gr->drawing_area = NULL;
  gr->close_button = NULL;
  gr->pixbuf_surface = NULL;

  gr->same_x_scale = 1;
  gr->same_y_scale = 1;
  gr->same_x_limits = 1;
  gr->same_y_limits = 1;
  gr->value_mode = (2<<2) | 1;
  gr->draw_value_pick = 0;

  /* the first configure event will fix the struct qp_graph 
   * xscale, yscale, xshift, and yshift.  We initialize
   * xscale to 0 so we can tell it is not initialized. */
  gr->xscale = 0;
  gr->yscale = 0;

  /* This is the initial zoom in normalized units */
  /* qp_zoom_create(xscale, xshift, yscale, yshift) */
  gr->z = qp_zoom_create(0.95,0.025,0.92,0.04);

  memcpy(&gr->background_color, &qp->background_color,
      sizeof(gr->background_color));
  memcpy(&gr->grid_line_color, &qp->grid_line_color,
      sizeof(gr->grid_line_color));
  memcpy(&gr->grid_text_color, &qp->grid_text_color,
      sizeof(gr->grid_text_color));

  gr->bg_alpha_preshape = gr->background_color.a;

  if(gr->qp->shape)
  {
    /* must be like in cb_view_shape() in callbacks.c */
    if(gr->background_color.a >= 0.5)
      gr->background_color.a = 0.4;
  }


  gr->grab_x = 0;
  gr->grab_y = 0;

  if(qp->x11_draw)
  {
    gr->x11 = qp_malloc(sizeof(*(gr->x11)));
    gr->x11->gc = 0;
    gr->x11->pixmap = 0;
    gr->x11->dsp = 0;
    gr->x11->background = 0;
    gr->x11->background_set = 0;
  }
  else
    gr->x11 = NULL;

  gr->pixbuf_width = 0;
  gr->pixbuf_height = 0;
  gr->draw_zoom_box = 0;
  gr->draw_value_pick = 0;

  /* cairo/pango set antialiasing by default
   * we need this flag for when we unset
   * antialiasing in X11 shape drawing mode. */
  gr->font_antialias_set = 1; /* boolean */

  ASSERT(app->root_window_width > 10);

  gr->drawing_area = gtk_drawing_area_new();
  gtk_widget_set_app_paintable(gr->drawing_area, TRUE);


  g_object_set_data(G_OBJECT(gr->drawing_area), "qp_graph", gr);

  gtk_widget_set_events(gr->drawing_area,
                  gtk_widget_get_events(gr->drawing_area) |
                  GDK_EXPOSURE_MASK |
                  GDK_SCROLL_MASK |
		  GDK_LEAVE_NOTIFY_MASK |
		  GDK_BUTTON_PRESS_MASK |
		  GDK_BUTTON_RELEASE_MASK |
		  GDK_POINTER_MOTION_MASK);

  /* We will do our own version of double buffering.  Not that it is
   * better than GTK, but we add an additional user interaction at the
   * same time we copy image buffers.  Before we found this we were
   * inadvertently triple buffering the drawing of the graph. */
  /* With GTK+3.0 version 3.14.5 gtk_widget_set_double_buffered() seems to
   * be broken.  Calling gtk_widget_set_double_buffered(gr->drawing_area,
   * FALSE) brakes the Cario draw mode.  We may be doing extra unnecessary
   * buffering.  gtk_widget_set_double_buffered() is depreciated and has
   * no fix to replace it. */
  //gtk_widget_set_double_buffered(gr->drawing_area, FALSE);

#if 1
  {
    const GdkRGBA rgba = QP_DA_BG_RGBA;
    gtk_widget_override_background_color(gr->drawing_area,
        GTK_STATE_FLAG_NORMAL, &rgba);
  }
#endif

  gr->tab_label_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_set_homogeneous(GTK_BOX(gr->tab_label_hbox), FALSE);
  g_object_set(G_OBJECT(gr->tab_label_hbox), "border-width", 0, NULL);
  
  {
    /************************************************************
     *                      Tab Label                           *
     ************************************************************/
    GtkWidget *image;
    size_t num_graphs;

    image = gtk_image_new_from_stock(GTK_STOCK_DND, GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(gr->tab_label_hbox), image, FALSE, FALSE, 0);
    gtk_widget_show(image);

    gr->tab_label = gtk_label_new(gr->name);
    //gtk_label_set_selectable(GTK_LABEL(gr->tab_label), TRUE);
    //gtk_widget_set_size_request(gr->tab_label, 60, 24);
    gtk_box_pack_start(GTK_BOX(gr->tab_label_hbox), gr->tab_label,
        FALSE, FALSE, 0);
    gtk_widget_show(gr->tab_label);

    num_graphs = qp_sllist_length(qp->graphs);
    if(num_graphs > 1)
      add_graph_close_button(gr);
    if(num_graphs == 2)
    {
      ASSERT(qp_sllist_first(qp->graphs) != (void *)gr);
      add_graph_close_button((struct qp_graph*)qp_sllist_first(qp->graphs));
    }
  }
  
  gtk_notebook_append_page(GTK_NOTEBOOK(qp->notebook), gr->drawing_area,
			   gr->tab_label_hbox);


#if 0
  gtk_notebook_set_tab_label_packing(GTK_NOTEBOOK(qp->notebook),
		 gr->drawing_area,
		 FALSE, /* expand */
		 FALSE, /* fill */
		 GTK_PACK_START); // GTK_PACK_START or GTK_PACK_END
#endif
  gtk_widget_set_size_request(gr->drawing_area,
			      2*EDGE_BUF_MIN+1,
			      2*EDGE_BUF_MIN+1);

  g_signal_connect(G_OBJECT(gr->drawing_area), "draw",
      G_CALLBACK(ecb_graph_draw), gr);
  g_signal_connect(G_OBJECT(gr->drawing_area),"configure-event",
      G_CALLBACK(ecb_graph_configure), gr);
  g_signal_connect(G_OBJECT(gr->drawing_area), "button-press-event",
      G_CALLBACK(ecb_graph_button_press), gr);
  g_signal_connect(G_OBJECT(gr->drawing_area), "button-release-event",
      G_CALLBACK(ecb_graph_button_release), gr);
  g_signal_connect(G_OBJECT(gr->drawing_area), "motion-notify-event",
      G_CALLBACK(ecb_graph_pointer_motion), gr);
  g_signal_connect(G_OBJECT(gr->drawing_area), "scroll-event",
      G_CALLBACK(ecb_graph_scroll), qp);

  //g_object_set_data(G_OBJECT(gr->drawing_area), "Graph", &wrapper);

  // Give gr->drawing_area a minimum size so things do not fail.
  
  gtk_notebook_set_current_page(GTK_NOTEBOOK(qp->notebook),
      gtk_notebook_page_num(GTK_NOTEBOOK(qp->notebook), gr->drawing_area));

  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_graph_tabs)))
    gtk_widget_show(gr->tab_label_hbox);
  gtk_widget_show(gr->drawing_area);

  /* GtkNotebook refuses to switch to a page unless the child
   * widget is visible.  So this is after the show */
  gtk_notebook_set_current_page(GTK_NOTEBOOK(qp->notebook),
      gtk_notebook_get_n_pages(GTK_NOTEBOOK(qp->notebook)) - 1);


  return gr;
}

static inline
void set_x11_draw_mode(struct qp_graph *gr)
{
  struct qp_plot *p;

  ASSERT(gr->x11 == NULL);

  gr->x11 = qp_malloc(sizeof(*(gr->x11)));
  gr->x11->gc = 0;
  gr->x11->pixmap = 0;
  gr->x11->dsp = 0;
  gr->x11->background = 0;
  gr->x11->background_set = 0;

  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    /* gr->x11 needs to be set so the X11 colors
     * may be made */
    qp_plot_set_x11_draw_mode(p, gr);
}

static inline
void set_cairo_draw_mode(struct qp_graph *gr)
{
  struct qp_plot *p;

  ASSERT(gr->x11);


  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    /* gr->x11 needs to be set so the X11 colors
     * may be freed */
    qp_plot_set_cairo_draw_mode(p, gr);


  if(gr->x11)
  {
    if(gr->x11->gc)
      XFreeGC(gr->x11->dsp, gr->x11->gc);
    if(gr->x11->pixmap)
        XFreePixmap(gr->x11->dsp, gr->x11->pixmap);
    free(gr->x11);
    gr->x11 = NULL;
  }
}

void qp_graph_switch_draw_mode(struct qp_graph *gr)
{

  if((gr->x11 && gr->qp->x11_draw) ||
      (!gr->x11 && !gr->qp->x11_draw))
    return;

  if(gr->qp->x11_draw)
    set_x11_draw_mode(gr);
  else
    set_cairo_draw_mode(gr);

  if(gr->pixbuf_surface)
  {
    /* This gets recreated in the drawing */
    cairo_surface_destroy(gr->pixbuf_surface);
    gr->pixbuf_surface = NULL;
  }
  gr->pixbuf_needs_draw = 1;
}

void qp_graph_destroy(qp_graph_t gr)
{
  struct qp_win *qp;
  ASSERT(gr);
  ASSERT(gr->qp);
  ASSERT(gr->qp->graphs);
  ASSERT(gr->name);
  ASSERT(gr->plots);
  if(!gr) return;

  if(gr->ref_count != 1)
  {
    ASSERT(gr->ref_count > 1);
    /* We cannot use just a counter because there could be an
     * arbitary number of button destroying clicks that
     * we wish to delay acting on. */
    gr->destroy_called = 1;
    return;
  }

  qp = gr->qp;
  ASSERT(qp);
  ASSERT(qp->window);
 
  {
    struct qp_plot *p;
    p = qp_sllist_begin(gr->plots);
    for(; p; p = qp_sllist_next(gr->plots))
      qp_plot_destroy(p, gr);
    qp_sllist_destroy(gr->plots, 0);
  }

  /* this will remove the tab */
  gtk_widget_destroy(gr->drawing_area);
  qp_color_gen_destroy(gr->color_gen);

  free(gr->name);
  qp_sllist_remove(gr->qp->graphs, gr, 0);
  qp_zoom_destroy(gr->z);
 
  if(gr->pixbuf_surface)
    cairo_surface_destroy(gr->pixbuf_surface);

  if(gr->x11)
  {
    /* TODO: Not sure how to free X11 colors that
     * may be still in use by other graphs.  There
     * is not likely much client side memory used
     * and the X Server will likely free the colors
     * when the program exits. Adding a list to manage
     * the X colors may use more memory than it
     * saves. ??? */

    if(gr->x11->gc)
      XFreeGC(gr->x11->dsp, gr->x11->gc);
    if(gr->x11->pixmap)
        XFreePixmap(gr->x11->dsp, gr->x11->pixmap);
    free(gr->x11);
  }

  ASSERT(gr->grid_font);

  free(gr->grid_font);

  if(gr->pangolayout)
    g_object_unref(G_OBJECT(gr->pangolayout));

  free(gr);
  /* done with this graph */

  if(qp_sllist_length(qp->graphs) == 1)
  {
    /* Reusing the gr pointer */
    /* Remove the tab close button */
    gr = qp_sllist_first(qp->graphs);
    ASSERT(gr->close_button);
    gtk_widget_destroy(gr->close_button);
    gr->close_button = NULL;
  }
}

void qp_graph_zoom_out(struct qp_graph *gr, int all)
{
  if(gr->zoom_level == 0 && !gr->grab_x && !gr->grab_y)
    return;

  if(all)
  {
    /* zoom all the way out */
    if(gr->zoom_level)
      gr->pixbuf_needs_draw = 1;
    gr->zoom_level = 0;
    qp_zoom_pop_all(&(gr->z));
    gr->grab_x = gr->grab_y = 0;
    gdk_window_set_cursor(gtk_widget_get_window(gr->qp->window), app->waitCursor);
  }
  else if(gr->grab_x || gr->grab_y)
  {
    gr->grab_x = gr->grab_y = 0;
  }
  else
  {
    /* zoom out one level */
    --gr->zoom_level;
    gr->pixbuf_needs_draw = 1;
    qp_zoom_pop(&(gr->z));
    gdk_window_set_cursor(gtk_widget_get_window(gr->qp->window), app->waitCursor);
  }

  qp_win_set_status(gr->qp);
  gtk_widget_queue_draw(gr->drawing_area);
}

static
void _qp_graph_rescale_plots(struct qp_graph *gr)
{
  /* We must rescale all the plots */
  /* TODO: make this code suck less */
  double dx_min = INFINITY, xmin = INFINITY, xmax = -INFINITY,
         dy_min = INFINITY, ymin = INFINITY, ymax = -INFINITY;
  struct qp_channel_series *csx0, *csy0;
  struct qp_plot *p;

  p=qp_sllist_begin(gr->plots);
  ASSERT(p->x->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(p->y->form == QP_CHANNEL_FORM_SERIES);
  csx0 = &(p->x->series);
  csy0 = &(p->y->series);
  gr->same_x_limits = 1;
  gr->same_y_limits = 1;

  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
  {
    double dx, dy;
    struct qp_channel_series *cs;
    ASSERT(p->x->form == QP_CHANNEL_FORM_SERIES);
    ASSERT(p->y->form == QP_CHANNEL_FORM_SERIES);
    cs = &(p->x->series);
    dx = cs->max - cs->min;
    if(xmin > cs->min)
      xmin = cs->min;
    if(xmax < cs->max)
      xmax = cs->max;
    if(dx > SMALL_DOUBLE && dx_min > dx)
      dx_min = dx;
    if(csx0->max != cs->max || csx0->min != cs->min)
      gr->same_x_limits = 0;

    cs = &(p->y->series);
    dy = cs->max - cs->min;
    if(ymin > cs->min)
      ymin = cs->min;
    if(ymax < cs->max)
      ymax = cs->max;
    if(dy > SMALL_DOUBLE && dy_min > dy)
      dy_min = dy;
    if(csy0->max != cs->max || csy0->min != cs->min)
      gr->same_y_limits = 0;
  }

  if(gr->same_x_limits)
  {
    gr->same_x_scale = 1;
  }

  if(gr->same_x_scale)
  {
    if(xmax == xmin)
    {
      /* It could be just one point so lets give it a
       * decent scale, not like the 1e-14 crap. */
      xmax += 1;
      xmin -= 1;
    }
    else if(xmax - xmin < SMALL_DOUBLE)
    {
      xmax += SMALL_DOUBLE;
      xmin -= SMALL_DOUBLE;
    }
  }
  else if(xmax == xmin)
  {
    /* It could be just one point so lets give it a
     * decent scale, not like the 1e-14 crap. */
    xmax += 1;
    xmin -= 1;
  }
  else if(xmax - xmin < SMALL_DOUBLE)
  {
    /* they requested different scales but the values
     * are too close together, so we make it same scale */
    xmax += SMALL_DOUBLE;
    xmin -= SMALL_DOUBLE;
  }

  if(gr->same_y_limits)
    gr->same_y_scale = 1;

  if(gr->same_y_scale)
  {
    if(ymax == ymin)
    {
      /* It could be just one point so lets give it a
       * decent scale, not like the 1e-14 crap. */
      ymax += 1;
      ymin -= 1;
    }
    else if(ymax - ymin < SMALL_DOUBLE)
    {
      ymax += SMALL_DOUBLE;
      ymin -= SMALL_DOUBLE;
    }
  }
  else if(ymax == ymin)
  {
    /* It could be just one point so lets give it a
     * decent scale, not like the 1e-14 crap. */
    ymax += 1;
    ymin -= 1;
  }
  else if(ymax - ymin < SMALL_DOUBLE)
  {
    /* they requested different scales but the values
     * are too close together, so we make it same scale */
    ymax += SMALL_DOUBLE;
    ymin -= SMALL_DOUBLE;
  }

  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
  {
    qp_plot_x_rescale(p, xmin, xmax);
    qp_plot_y_rescale(p, ymin, ymax);
  }
}

static 
void _qp_graph_set_redraw(struct qp_graph *gr)
{
  gdk_window_set_cursor(gtk_widget_get_window(gr->qp->window), app->waitCursor);
  if(gr->qp->graph_detail)
    gtk_widget_queue_draw(gr->qp->graph_detail->selecter_drawing_area);
    /* We will queue the Graph draw in the selecter_drawing_area draw
     * so that we see the selecter draw before the graph */
  gr->pixbuf_needs_draw = 1;
  gr->draw_value_pick = 0;

  if(gr->qp->graph_detail && gr->qp->current_graph == gr)
  {
    qp_graph_detail_set_value_mode(gr);
    qp_graph_detail_plot_list_remake(gr->qp);
  }
}

void qp_graph_remove_plot(struct qp_graph *gr, struct qp_plot *p)
{
  ASSERT(gr);
  ASSERT(p);

  qp_sllist_remove(gr->plots, p, 0);
  qp_plot_destroy(p, gr);

  if(qp_sllist_length(gr->plots))
    _qp_graph_rescale_plots(gr);

  _qp_graph_set_redraw(gr);
}

void qp_graph_add_plot(struct qp_graph *gr,
    struct qp_source *sx, struct qp_source *sy,
    size_t x_channel_num, size_t y_channel_num)
{
  char pname[128];
  struct qp_channel *x, *y;

  ASSERT(gr);
  ASSERT(sx);
  ASSERT(sy);
  ASSERT(sx->num_channels > x_channel_num);
  ASSERT(sy->num_channels > y_channel_num);

  x = sx->channels[x_channel_num];
  y = sy->channels[y_channel_num];

  ASSERT(x->form == QP_CHANNEL_FORM_SERIES);
  ASSERT(y->form == QP_CHANNEL_FORM_SERIES);

  qp_source_get_plot_name(pname, 128, sx, sy,
      x_channel_num, y_channel_num);

  qp_plot_create(gr, x, y, pname,
        x->series.min, x->series.max,
        y->series.min, y->series.max);

  _qp_graph_rescale_plots(gr);
  _qp_graph_set_redraw(gr);
}

void qp_graph_same_x_scale(struct qp_graph *gr, int same_x_scale /* 0 or 1 */)
{
  if(gr->same_x_scale == same_x_scale || gr->same_x_limits)
    return;

  gr->same_x_scale = same_x_scale;

  if(same_x_scale)
  {
    double min=INFINITY, max=-INFINITY;
    struct qp_plot *p;
    for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    {
      ASSERT(p->x->form == QP_CHANNEL_FORM_SERIES);
      if(max < p->x->series.max)
        max = p->x->series.max;
      if(min > p->x->series.min)
        min = p->x->series.min;
    }
    ASSERT(max >= min);
    if(max == min)
    {
      max += 1;
      min -= 1;
    }
    else if(max - min < SMALL_DOUBLE)
    {
      max += SMALL_DOUBLE;
      min -= SMALL_DOUBLE;
    }

    for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
      qp_plot_x_rescale(p, min, max);
  }
 else
  {
    struct qp_plot *p;
    for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    {
      double min, max;
      max = p->x->series.max;
      min = p->x->series.min;
      if(max == min)
      {
        max += 1;
        min -= 1;
      }
      else if(max - min < SMALL_DOUBLE)
      {
        max += SMALL_DOUBLE;
        min -= SMALL_DOUBLE;
      }
      qp_plot_x_rescale(p, min, max);
    }
  }

  gr->pixbuf_needs_draw = 1;
}

void qp_graph_same_y_scale(struct qp_graph *gr, int same_y_scale /* 0 or 1 */)
{
  if(gr->same_y_scale == same_y_scale || (gr->same_y_limits && !same_y_scale))
    return;

  gr->same_y_scale = same_y_scale;

  if(same_y_scale)
  {
    double min=INFINITY, max=-INFINITY;
    struct qp_plot *p;
    for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    {
      ASSERT(p->y->form == QP_CHANNEL_FORM_SERIES);
      if(max < p->y->series.max)
        max = p->y->series.max;
      if(min > p->y->series.min)
        min = p->y->series.min;
    }
    ASSERT(max >= min);
    if(max == min)
    {
      max += 1;
      min -= 1;
    }
    else if(max - min < SMALL_DOUBLE)
    {
      max += SMALL_DOUBLE;
      min -= SMALL_DOUBLE;
    }
    for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
      qp_plot_y_rescale(p, min, max);
  }
 else
  {
    /* different scales for each plot */

    struct qp_plot *p;
    for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    {
      double min, max;
      max = p->y->series.max;
      min = p->y->series.min;
      if(max == min)
      {
        max += 1;
        min -= 1;
      }
      else if(max - min < SMALL_DOUBLE)
      {
        max += SMALL_DOUBLE;
        min -= SMALL_DOUBLE;
      }
      qp_plot_y_rescale(p, min, max);
    }
  }

  gr->pixbuf_needs_draw = 1;
}

