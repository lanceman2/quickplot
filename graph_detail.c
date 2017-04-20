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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "quickplot.h"

#include "config.h"
#include "debug.h"
#include "list.h"
#include "qp.h"
#include "callbacks.h"

#include "channel.h"
#include "channel_double.h"
#include "plot.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


static __thread int _cb_view_graph_detail_reenter = 0;

static
gboolean graph_detail_hide(struct qp_win *qp)
{
  ASSERT(qp);
  ASSERT(qp->graph_detail);
  ASSERT(qp->graph_detail->window);

  if(qp->graph_detail->window)
    gtk_widget_hide(qp->graph_detail->window);
  return TRUE; /* TRUE means event is handled */
}

static inline
void set_color_button(GtkWidget *w, struct qp_colora *c)
{
  GdkRGBA rgba;

  rgba.red = c->r;
  rgba.green = c->g;
  rgba.blue = c->b;
  rgba.alpha = c->a;
  //DEBUG("%g %g %g %g\n", c->r, c->g, c->b, c->a);
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), &rgba);
}

static inline
void set_combo_box_text(GtkWidget *w, int same_scale,
    int same_limits)
{
  ASSERT((same_limits && same_scale) || !same_limits);


  gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(w), GTK_SENSITIVITY_ON);
  gtk_combo_box_set_active(GTK_COMBO_BOX(w), same_scale);

  if(same_limits)
    gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(w), GTK_SENSITIVITY_OFF);
  else
    gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(w), GTK_SENSITIVITY_ON);
}

static
void cb_same_y_scale(GtkComboBox *w, gpointer data)
{
  struct qp_graph *gr;

  gr = ((struct qp_win *)data)->current_graph;
  if(!gr)
    return;
  
  ASSERT(!gr->same_y_limits);

  qp_graph_same_y_scale(gr, gtk_combo_box_get_active(w)?1:0);
}

static __thread int _ignore_slider_cb = 0;

static
void set_slider_val(int val, struct qp_slider *s)
{
  if(s->val)
    *(s->val) = val;
  else if(s->dval)
    *(s->dval) = val;
  else if(s->is_plot_line_width)
  {
    struct qp_sllist *plots;
    struct qp_plot *p;
    ASSERT(s->gr);
    plots = s->gr->plots;
    for(p=qp_sllist_begin(plots);p;p=qp_sllist_next(plots))
      p->line_width = val;
  }
  else
  {
    struct qp_sllist *plots;
    struct qp_plot *p;
    ASSERT(s->gr);
    plots = s->gr->plots;
    for(p=qp_sllist_begin(plots);p;p=qp_sllist_next(plots))
      p->point_size = val;
  }

  if(s->callback)
    s->callback(s);
}

static
void cb_scale_change(GtkRange *range, struct qp_slider *s)
{
  int val;
  char str[8];

  if(_ignore_slider_cb)
    return;

  val = INT((s->max - s->min)*gtk_range_get_value(range) + s->min);
  snprintf(str, 8, "%d", val);

  _ignore_slider_cb = 1;
  gtk_entry_set_text(GTK_ENTRY(s->entry), str);
  _ignore_slider_cb = 0;

  set_slider_val(val, s);
}

static
void cb_text_entry(GtkEntry *entry, struct qp_slider *s)
{
  char str[8], *ss;
  const char *text;
  int val;
  double dval;

  if(_ignore_slider_cb)
    return;

  text = gtk_entry_get_text(entry);

  strncpy(str, text, 7);
  str[7] = '\0';

  for(ss = str; *ss; ++ss)
    if(*ss < '0' && *ss > '9')
    {
      /* Remove non-number chars */
      char *c;
      for(c = ss; *c; ++c)
        *c = *(c+1);
    }


  val = strtol(str, NULL, 10);

  if(val < s->min)
    val = s->min;
  else if(val > s->max)
  {
    if(val > s->extended_max)
      val = s->max = s->extended_max;
    else
      s->max = val;
  }

  snprintf(str, 8, "%d", val);

  if(strcmp(str, text))
    gtk_entry_set_text(entry, str);

  dval = s->max - s->min;
  dval = val/dval - s->min/dval;

  _ignore_slider_cb = 1;
  gtk_range_set_value(GTK_RANGE(s->scale), dval);
  _ignore_slider_cb = 0;

  set_slider_val(val, s);
}

static
struct qp_slider *create_slider_input(const char *label,
    GtkWidget **vbox,
    int min, int max, int extended_max)
{
  GtkWidget *frame, *scale, *entry, *hbox;
  int max_len;
  double v,step;
  char text[16];
  struct qp_slider *slider;

  slider = qp_malloc(sizeof(*slider));
  slider->min = min;
  slider->max = max;
  slider->callback = NULL;
  slider->extended_max = extended_max;
  slider->val = NULL;
  slider->dval = NULL;
  slider->is_plot_line_width = 0;
  slider->gr = NULL;

  frame = gtk_frame_new(label);
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);

  v = 0.3;
  step = 1.0/((double)(max - min));

  scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.001);
  gtk_range_set_increments(GTK_RANGE(scale), step, 2*step);
  gtk_range_set_value(GTK_RANGE(scale), v);
  gtk_widget_set_size_request(scale, 100, -1);
  gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
  g_signal_connect(G_OBJECT(scale), "value-changed", G_CALLBACK(cb_scale_change), slider);
  gtk_box_pack_start(GTK_BOX(hbox), scale, TRUE, TRUE, 2);
  gtk_widget_show(scale);

  entry = gtk_entry_new();
  snprintf(text, 16, "%d", extended_max);
  max_len = strlen(text);
  ASSERT(max_len <= 4);
  gtk_entry_set_max_length(GTK_ENTRY(entry), max_len);
  gtk_entry_set_width_chars(GTK_ENTRY(entry), 4);
  //g_signal_connect(G_OBJECT(entry), "focus-out-event", G_CALLBACK(cb_text_entry_focus_out), slider);
  g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(cb_text_entry), slider);
  gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 2);
  gtk_widget_show(entry);

  slider->scale = scale;
  slider->entry = entry;

  gtk_container_add(GTK_CONTAINER(frame), hbox);
  gtk_widget_show(hbox);
  gtk_widget_show(frame);
  if(*vbox)
    gtk_box_pack_start(GTK_BOX(*vbox), frame, FALSE, FALSE, 8);
  else
    /* let the user stuff the frame in something */
    *vbox = frame;
  return slider;
}

static
void set_slider_entry(struct qp_slider *slider, int val)
{
  char text[8];
  snprintf(text, 8, "%d", val);
  gtk_entry_set_text(GTK_ENTRY(slider->entry), text);
  cb_text_entry(GTK_ENTRY(slider->entry), slider);
}

static
gboolean cb_plot_list_draw(GtkWidget *w, cairo_t *cr, struct qp_win *qp)
{
  struct qp_graph *gr;
  GtkAllocation da_a;
  struct qp_plot *p;
  size_t i = 0;

  if(qp->update_graph_detail)
    /* We need to wait for a graph draw
     * and then this will be called again. */
    return TRUE;

  gtk_widget_get_allocation(w, &da_a);

  gr = qp->current_graph;

  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

  cairo_set_source_rgba(cr, gr->background_color.r,
      gr->background_color.g, gr->background_color.b,
      gr->background_color.a);
  cairo_paint(cr);

  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
  {
    GtkAllocation a;
    int width;
    double w2, y;

    gtk_widget_get_allocation(p->x_entry, &a);
    y = a.y - da_a.y + a.height/2.0;

    if(p->lines || !(p->lines || p->points))
    {
      width = p->line_width;
      if(width > 18)
        width = 18;

      cairo_set_source_rgba(cr, p->l.c.r, p->l.c.g, p->l.c.b, p->l.c.a);
      cairo_set_line_width(cr, width);

      cairo_move_to(cr, 0,          y);
      cairo_line_to(cr, da_a.width, y);
      cairo_stroke(cr);
    }

    if(p->points || !(p->lines || p->points))
    {
      width = p->point_size;
      if(width > 18)
        width = 18;
      w2 = width/2.0;
      y -= w2;

      cairo_set_source_rgba(cr, p->p.c.r, p->p.c.g, p->p.c.b, p->p.c.a);
      cairo_rectangle(cr,   da_a.width/3.0 - w2, y, width, width);
      cairo_rectangle(cr, 2*da_a.width/3.0 - w2, y, width, width);
      cairo_fill(cr);
    }

    ++i;
  }

  return TRUE;
}

void qp_graph_detail_set_value_mode(struct qp_graph *gr)
{
  struct qp_plot *p, *p0;
  if(!gr->same_x_scale)
  {
    gr->value_mode = 0;
    return;
  }

  p0 = qp_sllist_begin(gr->plots);
  if(!p0)
  {
    gr->value_mode = 0;
    return;
  }
  if(!p0->x->series.is_increasing)
  {
    gr->value_mode = 0;
    return;
  }

  for(p=qp_sllist_next(gr->plots);p;p=qp_sllist_next(gr->plots))
  {
    if(!qp_channel_equals(p0->x, p->x) || !p->x->series.is_increasing)
    {
      gr->value_mode = 0;
      return;
    }
  }

  gr->value_mode = (1 | (2<<2));
 
}

void cb_plot_list_redraw(struct qp_slider *s)
{
  if(s->gr->qp->graph_detail->plot_list_drawing_area)
    gtk_widget_queue_draw(s->gr->qp->graph_detail->plot_list_drawing_area);
}

static inline
void set_rgba(GtkColorButton *w, struct qp_colora *c)
{
  GdkRGBA rgba;
  gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(w), &rgba);
  c->r = rgba.red;
  c->g = rgba.green;
  c->b = rgba.blue;
  c->a = rgba.alpha;
}

static
void cb_plot_line_color(GtkColorButton *w, struct qp_plot *p)
{
  set_rgba(w, &(p->l.c));
  if(p->gr->x11)
    qp_plot_set_X11_color(p, &(p->l));
  gtk_widget_queue_draw(p->gr->qp->graph_detail->plot_list_drawing_area);
}

static
void cb_plot_point_color(GtkColorButton *w, struct qp_plot *p)
{
  set_rgba(w, &(p->p.c));
  if(p->gr->x11)
    qp_plot_set_X11_color(p, &(p->p));
  gtk_widget_queue_draw(p->gr->qp->graph_detail->plot_list_drawing_area);
}

static inline
GtkWidget *color_button_create(const char *text,
    void (*callback)(GtkColorButton *w, struct qp_plot *p),
    struct qp_colora *c,
    void *data)
{
  GtkWidget *label, *picker, *hbox;
  char t[64];
  snprintf(t, 64, "%s:", text);

  label = gtk_label_new(t);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  picker = gtk_color_button_new();
  gtk_color_button_set_title(GTK_COLOR_BUTTON(picker), text);
  gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(picker), TRUE);
  set_color_button(picker, c);

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), picker, FALSE, FALSE, 0);

  g_signal_connect(G_OBJECT(picker), "color-set", G_CALLBACK(callback), data);
  gtk_widget_show(label);
  gtk_widget_show(picker);
  gtk_widget_show(hbox);

  return hbox;
}

static inline
void set_margin(GtkWidget *w, int left, int right, int top, int bottom)
{
  gtk_widget_set_margin_left(w, left);
  gtk_widget_set_margin_right(w, right);
  gtk_widget_set_margin_top(w, top);
  gtk_widget_set_margin_bottom(w, bottom);
}

static
void set_plot_lines(GtkToggleButton *button, struct qp_plot *p) 
{
  p->lines = gtk_toggle_button_get_active(button);
  gtk_widget_queue_draw(p->gr->qp->graph_detail->plot_list_drawing_area);
}

static
void set_plot_points(GtkToggleButton *button, struct qp_plot *p) 
{
  p->points = gtk_toggle_button_get_active(button);
  gtk_widget_queue_draw(p->gr->qp->graph_detail->plot_list_drawing_area);
}


#define CHAR_WIDTH (24)

/* make plot list for a particular graph */
static
void plot_list_make(struct qp_win *qp)
{
  GtkWidget *grid, *w;
  const GdkRGBA rgba = QP_DA_BG_RGBA;
  int row = 0;
  struct qp_graph *gr;
  struct qp_plot *p;
  struct qp_graph_detail *gd;
  PangoFontDescription *pfd;
  int num_plots;
  gr = qp->current_graph;
  gd = qp->graph_detail;

  if(qp_sllist_length(gr->plots) == 0)
    return;

  pfd = pango_font_description_from_string("Monospace Bold 11");

  num_plots = qp_sllist_length(gr->plots);

  gd->plot_line_width_slider = qp_malloc(
      sizeof(struct qp_slider *)*(num_plots+1));
  gd->plot_point_size_slider = qp_malloc(
      sizeof(struct qp_slider *)*(num_plots+1));
  gd->plot_list_button_lines = qp_malloc(
      sizeof(GtkWidget *)*(num_plots+1));
  gd->plot_list_button_points = qp_malloc(
      sizeof(GtkWidget *)*(num_plots+1));

  gd->plot_line_width_slider[num_plots] = NULL;
  gd->plot_point_size_slider[num_plots] = NULL;
  gd->plot_list_button_lines[num_plots] = NULL;
  gd->plot_list_button_points[num_plots] = NULL;


  grid = gtk_grid_new();

  qp->graph_detail->plot_list_drawing_area = w = gtk_drawing_area_new();
  set_margin(w, 0, 0, 4, 4);
  gtk_widget_set_size_request(w, 100, -1);
  g_signal_connect(G_OBJECT(w), "draw", G_CALLBACK(cb_plot_list_draw), qp);
  gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 2*num_plots+2);
  gtk_widget_override_background_color(w, GTK_STATE_FLAG_NORMAL, &rgba);
  gtk_widget_show(w);

  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
  {
    GtkWidget *f;

    w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    set_margin(w, 0, 0, 4, 4);
    gtk_grid_attach(GTK_GRID(grid), w, 1, 2*row, 8, 1);
    gtk_widget_show(w);

    w = gtk_entry_new();
    set_margin(w, 5, 2, 0, 0);
    if(pfd)
      gtk_widget_override_font(w, pfd);
    p->x_entry = w;
    gtk_entry_set_width_chars(GTK_ENTRY(w), CHAR_WIDTH);
    gtk_grid_attach(GTK_GRID(grid), w, 1, 2*row+1, 1, 1);
    gtk_widget_show(w);

    w = gtk_entry_new();
    set_margin(w, 0, 0, 0, 0);
    if(pfd)
      gtk_widget_override_font(w, pfd);
    p->y_entry = w;
    gtk_entry_set_width_chars(GTK_ENTRY(w), CHAR_WIDTH);
    gtk_grid_attach(GTK_GRID(grid), w, 2, 2*row+1, 1, 1);
    gtk_widget_show(w);

    w = gtk_label_new(p->name);
    set_margin(w, 8, 8, 8, 8);
    gtk_grid_attach(GTK_GRID(grid), w, 3, 2*row+1, 1, 1);
    gtk_widget_show(w);

    f = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_box_set_homogeneous(GTK_BOX(f), TRUE);
    gd->plot_list_button_lines[row] =
    w = gtk_check_button_new_with_label("Show Lines");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), p->lines);
    gtk_box_pack_start(GTK_BOX(f), w, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(w), "toggled", G_CALLBACK(set_plot_lines), p);
    gtk_widget_show(w);
    gd->plot_list_button_points[row] =
    w = gtk_check_button_new_with_label("Show Points");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), p->points);
    gtk_box_pack_start(GTK_BOX(f), w, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(w), "toggled", G_CALLBACK(set_plot_points), p);
    gtk_widget_show(w);
    gtk_widget_set_margin_left(f, 5);
    gtk_widget_set_margin_right(f, 5);
    gtk_grid_attach(GTK_GRID(grid), f, 4, 2*row+1, 1, 1);
    gtk_widget_show(f);

    w = NULL;
    gd->plot_line_width_slider[row] = create_slider_input(
        "Line Width", &w, 1, 20, 40);
    set_margin(w, 4, 6, 0, 0);
    gd->plot_line_width_slider[row]->dval = &(p->line_width);
    gtk_grid_attach(GTK_GRID(grid), w, 5, 2*row+1, 1, 1);
    set_slider_entry(gd->plot_line_width_slider[row], p->line_width);
    gd->plot_line_width_slider[row]->callback = cb_plot_list_redraw;
    gd->plot_line_width_slider[row]->gr = gr;

    w = NULL;
    gd->plot_point_size_slider[row] = create_slider_input(
        "Point Size", &w, 1, 20, 40);
    set_margin(w, 4, 6, 0, 0);
    gd->plot_point_size_slider[row]->dval = &(p->point_size);
    gtk_grid_attach(GTK_GRID(grid), w, 6, 2*row+1, 1, 1);
    set_slider_entry(gd->plot_point_size_slider[row], p->point_size);
    gd->plot_point_size_slider[row]->callback = cb_plot_list_redraw;
    gd->plot_point_size_slider[row]->gr = gr;

    w = color_button_create("Line Color", cb_plot_line_color, &(p->l.c), p);
    set_margin(w, 4, 4, 6, 4);
    gtk_grid_attach(GTK_GRID(grid), w, 7, 2*row+1, 1, 1);

    w = color_button_create("Point Color", cb_plot_point_color, &(p->p.c), p);
    set_margin(w, 4, 8, 6, 4);
    gtk_grid_attach(GTK_GRID(grid), w, 8, 2*row+1, 1, 1);

    ++row;
  }

  w = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
  set_margin(w, 0, 1, 4, 4);
  gtk_grid_attach(GTK_GRID(grid), w, 9, 0, 1, 2*row+1);
  gtk_widget_show(w);

  w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  set_margin(w, 0, 0, 4, 5);
  gtk_grid_attach(GTK_GRID(grid), w, 1, 2*row, 8, 1);
  gtk_widget_show(w);


  if(pfd)
    pango_font_description_free(pfd);

  gtk_box_pack_start(GTK_BOX(qp->graph_detail->plot_list_hbox),
      grid, FALSE, FALSE, 8);
  gtk_widget_show(grid);

  if(gr->draw_value_pick)
    set_value_pick_entries(gr, gr->value_pick_x, gr->value_pick_y);
}

#define free_null_term_pointer_array(T, ptr) \
  do {              \
    T **p;          \
    p = (ptr);      \
    for(;*p;++p)    \
      free(*p);     \
    free(ptr);      \
    ptr = NULL;     \
  } while(0)

/* called this when the main window is destroyed */
void qp_graph_detail_destory(struct qp_win *qp)
{
  struct qp_graph_detail *gd;
  ASSERT(qp);
  gd = qp->graph_detail;
  ASSERT(gd);
  gtk_widget_destroy(gd->window);

  if(gd->plot_line_width_slider)
  {
    free_null_term_pointer_array(struct qp_slider, gd->plot_line_width_slider);
    free_null_term_pointer_array(struct qp_slider, gd->plot_point_size_slider);
    free(gd->plot_list_button_lines);
    free(gd->plot_list_button_points);
  }

  free(gd->line_width_slider);
  free(gd->point_size_slider);
  free(gd->grid_line_width_slider);
  free(gd->grid_x_line_space_slider);
  free(gd->grid_y_line_space_slider);

  free(gd);
  qp->graph_detail = NULL;
}

void qp_graph_detail_plot_list_remake(struct qp_win *qp)
{
  GList *l, *list;

  list = gtk_container_get_children(
      GTK_CONTAINER(qp->graph_detail->plot_list_hbox));
  for(l=list;l;l=l->next)
    gtk_widget_destroy(GTK_WIDGET(l->data));
  g_list_free(list);

  ASSERT(qp->graph_detail);

  if(qp->graph_detail->plot_line_width_slider)
  {
    free_null_term_pointer_array(struct qp_slider, qp->graph_detail->plot_line_width_slider);
    free_null_term_pointer_array(struct qp_slider, qp->graph_detail->plot_point_size_slider);
    free(qp->graph_detail->plot_list_button_lines);
    free(qp->graph_detail->plot_list_button_points);
    qp->graph_detail->plot_list_button_lines =
      qp->graph_detail->plot_list_button_points = NULL;
  }

  qp->graph_detail->plot_list_drawing_area = NULL;

  plot_list_make(qp);
}

#undef free_null_term_pointer_array


static
void plot_list_combo_box_init(struct qp_graph *gr, struct qp_graph_detail *gd)
{
  int modes, current_modes;
  GtkWidget *combo;
  combo = gd->plot_list_combo_box;


  modes = (((gr->value_mode) & (3<<2)) >> 2); /* possible modes */
  current_modes = (((gd->plot_list_modes) & (3<<2)) >> 2); /* current possible modes */

  if(current_modes != modes)
  {
    if(modes > current_modes)
    {
      if(current_modes < 1)
        gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combo), 1, "Interpolate Plot Values");
      if(modes == 2)
        gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combo), 2, "Pick Plot Values");
    }
    else
    {
      if(modes < 2 && current_modes == 2)
        gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(combo), 2);
      if(current_modes >= 1 && modes == 0)
        gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(combo), 1);
    }
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), gr->value_mode & 3);

  if(modes & 3)
    gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(combo), GTK_SENSITIVITY_ON);
  else
    gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(combo), GTK_SENSITIVITY_OFF);

  gd->plot_list_modes = ((gd->plot_list_modes) & PL_IS_SHOWING) | gr->value_mode;
}

static inline
void configure_graph_init(struct qp_win *qp)
{
  struct qp_graph *gr;
  struct qp_graph_detail *gd;

  ASSERT(qp);
  ASSERT(qp->graph_detail);
  ASSERT(qp->graph_detail->window);

  gd = qp->graph_detail;
  gr = qp->current_graph;

  {
    /**************************************************
     *       Setup Show Check Buttons
     **************************************************/
    struct qp_plot *p;
    int lines = 0, points = 0;
    GList *list, *l;
    GtkWidget *w;
    
    for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    {
      if(p->lines)
        lines = 1;
      if(p->points)
        points = 1;
    }

    /* Set to NULL so that the callbacks do not do anything. */
    qp->current_graph = NULL;

    l = list = gtk_container_get_children(GTK_CONTAINER(gd->show_container));
    /* Show Grid */
    w = l->data;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), gr->show_grid);
    /* Show Grid Numbers */
    l = l->next;
    w = l->data;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), gr->grid_numbers);
    /* Show Lines */
    l = l->next;
    w = l->data;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), lines);
    /* Show Points */
    l = l->next;
    w = l->data;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), points);
    /************************/
    g_list_free(list);
  }

  set_color_button(gd->background_color_picker,&(gr->background_color));
  set_color_button(gd->grid_color_picker,&(gr->grid_line_color));
  set_color_button(gd->numbers_color_picker,&(gr->grid_text_color));
  gtk_font_button_set_font_name(GTK_FONT_BUTTON(gd->font_picker), gr->grid_font);

  set_combo_box_text(gd->x_scale, gr->same_x_scale, gr->same_x_limits);
  set_combo_box_text(gd->y_scale, gr->same_y_scale, gr->same_y_limits);


  gd->line_width_slider->gr = gr;
  gd->line_width_slider->is_plot_line_width = 1;
  if(qp_sllist_first(gr->plots))
    set_slider_entry(gd->line_width_slider,
      ((struct qp_plot*)qp_sllist_first(gr->plots))->line_width);
  else
    set_slider_entry(gd->line_width_slider, 4);
  
  gd->point_size_slider->gr = gr;
  if(qp_sllist_first(gr->plots))
    set_slider_entry(gd->point_size_slider,
      ((struct qp_plot*)qp_sllist_first(gr->plots))->point_size);
  else
    set_slider_entry(gd->point_size_slider, 4);

  gd->grid_line_width_slider->val = &(gr->grid_line_width);
  set_slider_entry(gd->grid_line_width_slider, gr->grid_line_width);
  gd->grid_x_line_space_slider->val = &(gr->grid_x_space);
  set_slider_entry(gd->grid_x_line_space_slider, gr->grid_x_space);
  gd->grid_y_line_space_slider->val = &(gr->grid_y_space);
  set_slider_entry(gd->grid_y_line_space_slider, gr->grid_y_space);

  qp->current_graph = gr;
}

/* Setup the widget for a particular graph */
void qp_win_graph_detail_init(struct qp_win *qp)
{
  struct qp_graph *gr;
  struct qp_graph_detail *gd;

  ASSERT(qp);
  ASSERT(qp->graph_detail);
  ASSERT(qp->graph_detail->window);

  gd = qp->graph_detail;
  gr = qp->current_graph;

  {
    char title[256];
    snprintf(title, 256, "%s Graph Details", gr->name);
    gtk_window_set_title(GTK_WINDOW(gd->window), title);
  }
  {
    char text[128];        
    snprintf(text, 128, "Configure Graph: %s", gr->name);
    gtk_label_set_text(GTK_LABEL(gd->config_label), text);
  }

  if(gtk_notebook_get_current_page(GTK_NOTEBOOK(gd->notebook)) == 0)
  {
    qp->current_graph = gr;
    configure_graph_init(qp);
    qp->current_graph = NULL;
  }

  gtk_widget_queue_draw(qp->graph_detail->selecter_drawing_area);

  plot_list_combo_box_init(gr, gd);

  qp->current_graph = gr;

  qp_graph_detail_plot_list_remake(qp);
}

static
void cb_plot_list_mode(GtkComboBox *w, struct qp_win *qp)
{
  int mode;
  mode = gtk_combo_box_get_active(w);
  if(mode < 0 || mode > 2)
    return;

  if(qp->current_graph)
  {
    qp->current_graph->value_mode = 
      (((qp->current_graph->value_mode)   & (7<<2)) | mode);
    qp->graph_detail->plot_list_modes =
      ((qp->graph_detail->plot_list_modes & (7<<2)) | mode);
  }
}

static
void cb_show_grid(GtkWidget *w, void *data)
{
  struct qp_graph *gr;
  gr = ((struct qp_win *)data)->current_graph;

  if(!gr)
    return;

  gr->show_grid = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}

static
void cb_show_grid_numbers(GtkWidget *w, void *data)
{
  struct qp_graph *gr;
  gr = ((struct qp_win *)data)->current_graph;

  if(!gr)
    return;

 gr->grid_numbers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}

static
void cb_show_lines(GtkWidget *w, void *data)
{
  struct qp_graph *gr;
  struct qp_plot *p;
  int on;
  gr = ((struct qp_win *)data)->current_graph;

  if(!gr)
    return;

  on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    p->lines = on;
}

static
void cb_show_points(GtkWidget *w, void *data)
{
  struct qp_graph *gr;
  struct qp_plot *p;
  int on;
  gr = ((struct qp_win *)data)->current_graph;

  if(!gr)
    return;

  on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    p->points = on;
}

static
void cb_background_color(GtkColorButton *w, gpointer data)
{
  struct qp_graph *gr;
  gr = ((struct qp_win *)data)->current_graph;
  if(!gr)
    return;
  set_rgba(w, &(gr->background_color));
}

static
void cb_grid_color(GtkColorButton *w, gpointer data)
{
  struct qp_graph *gr;
  gr = ((struct qp_win *)data)->current_graph;
  if(!gr)
    return;
  set_rgba(w, &(gr->grid_line_color));
}

static
void cb_grid_numbers_color(GtkColorButton *w, gpointer data)
{
  struct qp_graph *gr;
  gr = ((struct qp_win *)data)->current_graph;
  if(!gr)
    return;
  set_rgba(w, &(gr->grid_text_color));
}

static
void cb_grid_font(GtkFontButton *w, gpointer data)
{
  struct qp_graph *gr;
  gr = ((struct qp_win *)data)->current_graph;
  if(!gr)
    return;

  if(gr->grid_font)
    free(gr->grid_font);

  gr->grid_font = qp_strdup(gtk_font_button_get_font_name(w));

  if(gr->pangolayout)
    /* If there is no pangolayout there is no reason to call this */
    qp_graph_set_grid_font(gr);
}

static
void create_show_check_button(const char *label,
    void (*callback)(GtkWidget*, void*), struct qp_win *qp,
    GtkWidget *vbox, gboolean checked)
{
  GtkWidget *b;
  b = gtk_check_button_new_with_label(label);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), checked);

  if(callback)
    g_signal_connect(G_OBJECT(b), "toggled", G_CALLBACK(callback), qp);
  gtk_box_pack_start(GTK_BOX(vbox), b, FALSE, FALSE, 0);
  gtk_widget_show(b);
}

static
gboolean ecb_close_graph_detail(
    GtkWidget *w, GdkEvent *event, gpointer data)
{
  cb_graph_detail_show_hide(NULL, data);
  return TRUE;/* TRUE means event is handled */
}

static
void cb_same_x_scale(GtkComboBox *w, struct qp_win *qp)
{
  struct qp_graph *gr;

  gr = qp->current_graph;
  if(!gr) return;

  ASSERT(!gr->same_x_limits);

  qp_graph_same_x_scale(gr, gtk_combo_box_get_active(w)?1:0);
}


static
void cb_redraw(GtkButton *button, struct qp_win *qp)
{
  if(qp->graph_detail->plot_list_drawing_area)
    gtk_widget_queue_draw(qp->graph_detail->plot_list_drawing_area);
  gtk_widget_queue_draw(qp->current_graph->drawing_area);
  qp->current_graph->pixbuf_needs_draw = 1;
  //qp->current_graph->draw_value_pick = 0;
  gdk_window_set_cursor(gtk_widget_get_window(qp->window), app->waitCursor);
}

static inline
void add_grid_label(GtkWidget *grid, const char *text,
    int with_frame, int vertical, int left, int top)
{
  GtkWidget *label, *frame;
  label = gtk_label_new(text);

  if(vertical)
    gtk_label_set_angle(GTK_LABEL(label), 80);

  frame = gtk_frame_new(NULL);


  if(!with_frame)
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
  
  gtk_container_add(GTK_CONTAINER(frame), label);
  gtk_widget_show(label);

  gtk_grid_attach(GTK_GRID(grid), frame, left, top, 1, 1);

  gtk_widget_show(frame);
}


struct qp_plotter
{
  struct qp_win *qp;
  GtkWidget *radio;
  ssize_t channel_num;
  struct qp_source *source;
  struct qp_channel *channel;
  int is_y; /* 0 = x  1 = y */
};

static
void cb_plotter(GtkButton *w, struct qp_plotter *pr)
{
  GtkWidget *vbox;
  GList *l, *list;
  struct qp_plot *p;
  struct qp_graph *gr;
  struct qp_plotter *o_pr = NULL, *x_pr = NULL, *y_pr = NULL;

  if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
    return;

  /* Get the other channel */
  if(pr->is_y)
  {
    vbox = pr->qp->graph_detail->selecter_x_vbox;
    y_pr = pr;
  }
  else
  {
    vbox = pr->qp->graph_detail->selecter_y_vbox;
    x_pr = pr;
  }
  l = list = gtk_container_get_children(GTK_CONTAINER(vbox));
  /* first check the "none" radio button */
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(l->data)))
    return; /* It is "none" */

  for(l=list->next;l;l=l->next)
  {
    o_pr = g_object_get_data(G_OBJECT(l->data), "plotter");
    if(o_pr && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(l->data)))
      break;
    else
      o_pr = NULL;
  }
  g_list_free(list);

  ASSERT(o_pr);
  if(!o_pr)
    return; /* WTF */

  if(x_pr)
    y_pr = o_pr;
  else
    x_pr = o_pr;

  gr = pr->qp->current_graph;

  /* we have the two channels x_pr and y_pr */
  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
    if(qp_channel_equals(p->x, x_pr->channel) && 
        qp_channel_equals(p->y, y_pr->channel))
      break;

  if(p)
    qp_graph_remove_plot(gr, p);
  else
    qp_graph_add_plot(gr, x_pr->source, y_pr->source,
        x_pr->channel_num, y_pr->channel_num);


  //qp_win_graph_detail_init(gr->qp);

#if 0
  DEBUG("chan_num=%zd toggled=%d\n",
      pr->channel_num,
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
#endif
}

static
GtkWidget *make_channel_selecter_column(GtkWidget *hbox, struct qp_win *qp, int is_y)
{
  GtkWidget *radio, *vbox;
  struct qp_source *s;

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);

  radio = gtk_radio_button_new_with_label(NULL, "none");
  gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);
  gtk_widget_show(radio);

  for(s=qp_sllist_begin(app->sources);s;s=qp_sllist_next(app->sources))
  {
    size_t i;
    GtkWidget *label;

    label = gtk_label_new(s->name);
    gtk_widget_set_margin_top(label, 8);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 8);
    gtk_widget_show(label);

    for(i=0;i<s->num_channels;++i)
    {
      char text[128];
      struct qp_plotter *pr;

      qp_source_get_label(s, i, text, 128);

      radio = gtk_radio_button_new_with_label_from_widget(
            GTK_RADIO_BUTTON(radio), text);
      pr = qp_malloc(sizeof(*pr));
      pr->qp = qp;
      pr->source = s;
      pr->radio = radio;
      pr->channel = s->channels[i];
      pr->channel_num = i;
      pr->is_y = is_y;
      g_object_set_data(G_OBJECT(radio), "plotter", pr);
      g_signal_connect(G_OBJECT(radio), "clicked", G_CALLBACK(cb_plotter), pr);
      gtk_widget_set_margin_top(radio, 0);
      gtk_widget_set_margin_bottom(radio, 0);
      gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);
      gtk_widget_show(radio);
    }
  }

  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show(vbox);
  return vbox;
}

static
gboolean cb_selecter_draw(GtkWidget *w, cairo_t *cr, struct qp_win *qp)
{
  struct qp_graph *gr;
  GtkAllocation da_a;
  GList *l, *list;
  struct qp_plot *p;

  if(qp->update_graph_detail)
    /* We need to wait for a graph draw
     * and then this will be called again. */
    return TRUE;

  gtk_widget_get_allocation(w, &da_a);

  gr = qp->current_graph;

  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);


  cairo_set_source_rgba(cr, gr->background_color.r,
      gr->background_color.g, gr->background_color.b,
      gr->background_color.a);
  cairo_paint(cr);

  cairo_set_source_rgba(cr, gr->grid_line_color.r,
      gr->grid_line_color.g, gr->grid_line_color.b,
      gr->grid_line_color.a);

  l = gtk_container_get_children(
      GTK_CONTAINER(qp->graph_detail->selecter_x_vbox));
  /* The first toggle button is the "none" button. */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l->data), TRUE);
  g_list_free(l);


  l = list = gtk_container_get_children(
      GTK_CONTAINER(qp->graph_detail->selecter_y_vbox));
  /* The first toggle button is the "none" button. */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l->data), TRUE);


  /* Set the X and Y Channel "none" radio buttons. */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(l->data), TRUE);

  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

  /* skip the "none" radio button */
  for(l=list->next;l;l=l->next)
  {
    GtkAllocation a;
    gtk_widget_get_allocation(GTK_WIDGET(l->data), &a);
    if(g_object_get_data(G_OBJECT(l->data), "plotter"))
    {
      cairo_rectangle(cr, 0, a.y - da_a.y + 2, a.height/2, a.height-4);
      cairo_rectangle(cr, da_a.width - a.height/2, a.y - da_a.y + 2,
            a.height/2, a.height-4);
    }
    else
      cairo_rectangle(cr, 0, a.y - da_a.y, da_a.width, a.height);
  }
  cairo_fill(cr);

  for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
  {
    int x_y = 0, y_y = 0, width;
    double w2;
   
    for(l=list->next;l;l=l->next)
    {
      struct qp_plotter *pr;
      pr = g_object_get_data(G_OBJECT(l->data), "plotter");
      if(!x_y && pr && qp_channel_equals(pr->channel, p->x))
      {
        GtkAllocation a;
        gtk_widget_get_allocation(GTK_WIDGET(l->data), &a);
        x_y = a.y - da_a.y + a.height/2;
        if(y_y)
          break;
      }
      if(!y_y && pr && qp_channel_equals(pr->channel, p->y))
      {
        GtkAllocation a;
        gtk_widget_get_allocation(GTK_WIDGET(l->data), &a);
        y_y = a.y - da_a.y + a.height/2;
        if(x_y)
          break;
      }
    }
    width = p->line_width;
    if(width > 22)
      width = 22;

    cairo_set_source_rgba(cr, p->l.c.r, p->l.c.g, p->l.c.b, p->l.c.a);
    cairo_set_line_width(cr, width);
    cairo_move_to(cr, 0, x_y);
    cairo_line_to(cr, da_a.width, y_y);
    cairo_stroke(cr);

    if(p->points)
    {
      width = p->point_size;
      if(width > 22)
        width = 22;

      w2 = width/2.0;

      cairo_set_source_rgba(cr, p->p.c.r, p->p.c.g, p->p.c.b, p->p.c.a);
      cairo_rectangle(cr, da_a.width/3 - w2, x_y + (y_y - x_y)/3 - w2,
        width, width);
      cairo_rectangle(cr, 2*da_a.width/3 - w2, x_y + 2*(y_y - x_y)/3 - w2,
        width, width);
      cairo_fill(cr);
    }
  }

  g_list_free(list);

  if(qp->current_graph->pixbuf_needs_draw)
    gtk_widget_queue_draw(qp->current_graph->drawing_area);

  return TRUE; /* TRUE means the event is handled. */
}

static
void plot_selecter_make(struct qp_win *qp)
{
  GtkWidget *hbox, *da;
  const GdkRGBA rgba = QP_DA_BG_RGBA;

  hbox = qp->graph_detail->selecter_hbox;

  qp->graph_detail->selecter_x_vbox = make_channel_selecter_column(hbox, qp, 0);

  qp->graph_detail->selecter_drawing_area = da = gtk_drawing_area_new();
  gtk_widget_set_size_request(da, 100, -1);
  g_signal_connect(G_OBJECT(da), "draw", G_CALLBACK(cb_selecter_draw), qp);
  gtk_box_pack_start(GTK_BOX(hbox), da, TRUE, TRUE, 0);
  gtk_widget_override_background_color(da, GTK_STATE_FLAG_NORMAL, &rgba);
  gtk_widget_show(da);

  qp->graph_detail->selecter_y_vbox = make_channel_selecter_column(hbox, qp, 1);
}

/* Call this if there are sources added or removed
 * and qp->graph_detail is non NULL. */
static
void plot_selecter_remake(struct qp_win *qp)
{
  GList *l, *list;

  l = list = gtk_container_get_children(
      GTK_CONTAINER(qp->graph_detail->selecter_x_vbox));
  for(l=list->next;l;l=l->next)
  {
    void *pr;
    pr = g_object_get_data(G_OBJECT(l->data), "plotter");
    if(pr)
      free(pr);
  }
  g_list_free(list);

  l = list = gtk_container_get_children(
      GTK_CONTAINER(qp->graph_detail->selecter_y_vbox));

  for(l=list->next;l;l=l->next)
  {
    void *pr;
    pr = g_object_get_data(G_OBJECT(l->data), "plotter");
    if(pr)
      free(pr);
  }
  g_list_free(list);

  /* Remove the widgets from the selecter_hbox */
  list = l = gtk_container_get_children(
        GTK_CONTAINER(qp->graph_detail->selecter_hbox));
  for(;l && l->data;l=l->next)
    gtk_widget_destroy(GTK_WIDGET(l->data));
  g_list_free(list);


  plot_selecter_make(qp);
}

static
GtkWidget *make_pretty_header_label(const char *text, GtkBox *vbox)
{
  GtkWidget *hbox, *frame, *label;

  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
  gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
  frame = gtk_frame_new(NULL);
  label = gtk_label_new(text);

  gtk_container_add(GTK_CONTAINER(frame), label);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 8);
  gtk_widget_show(frame);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);
  gtk_widget_show(hbox);
  return label;
}

static
void make_redraw_button(GtkWidget *vbox, struct qp_win *qp)
{
  GtkWidget *button, *hbox;
  hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
  gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
  button = gtk_button_new_with_label("Redraw Graph");
  g_signal_connect(button, "clicked", G_CALLBACK(cb_redraw), qp);
  gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 8);
  gtk_widget_show(button);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 8);
  gtk_widget_show(hbox);
}

/* Call this when a source is added or removed */
void qp_app_graph_detail_source_remake(void)
{
  struct qp_win *qp;
  ASSERT(app);
  ASSERT(app->qps);

  for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
    if(qp->graph_detail)
    {
      qp_graph_detail_plot_list_remake(qp);
      plot_selecter_remake(qp);
    }
}

gboolean cb_graph_detail_switch_page(GtkNotebook *notebook,
    GtkWidget *page, guint page_num, struct qp_win *qp)
{
  // page_num =  0 Configure Graph   1 Select Channels to Plot   2 Plots List
 
  if(page_num == 2 && qp->graph_detail &&
      qp->graph_detail->plot_line_width_slider) /* Plots List */
  {
    struct qp_slider **lws, **pss;
    struct qp_plot *p;
    struct qp_sllist *l;
    struct qp_graph *gr;
    GtkWidget **lines, **points;
    /* Go through 5 lists in one loop.  All lists the same length
     * which is the number of plots. */
    gr = qp->current_graph;
    l = gr->plots;
    p = qp_sllist_begin(l);
    pss = qp->graph_detail->plot_point_size_slider;
    lines = qp->graph_detail->plot_list_button_lines;
    points = qp->graph_detail->plot_list_button_points;

    for(lws=qp->graph_detail->plot_line_width_slider;lws && *lws;++lws)
    {
      int val;
      set_slider_entry(*lws, p->line_width);
      set_slider_entry(*pss, p->point_size);
      val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(*lines));
      if((val && !p->lines) || (!val && p->lines))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(*lines), p->lines);
      val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(*points));
      if((val && !p->points) || (!val && p->points))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(*points), p->points);
      p=qp_sllist_next(l);
      ++pss;
      ++lines;
      ++points;
    }
    qp->graph_detail->plot_list_modes |= PL_IS_SHOWING;
    plot_list_combo_box_init(gr, qp->graph_detail);
    if(gr->draw_value_pick)
      set_value_pick_entries(gr, gr->value_pick_x, gr->value_pick_y);
  }
  else if(qp->graph_detail)
    qp->graph_detail->plot_list_modes &= ~PL_IS_SHOWING;

  if(page_num == 0 && qp->graph_detail)
    configure_graph_init(qp);

  return TRUE;
}

/* Make a graph detail main window widget */
static
void graph_detail_create(struct qp_win *qp)
{
  struct qp_graph *gr;
  struct qp_graph_detail *gd;

  ASSERT(qp);

  gr = qp->current_graph;

  ASSERT(!qp->graph_detail);

  gd = (qp->graph_detail = qp_malloc(sizeof(*gd)));
  memset(gd, 0, sizeof(*gd));


  gd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_icon(GTK_WINDOW(gd->window),
      gtk_window_get_icon(GTK_WINDOW(qp->window)));

  gtk_window_set_default_size(GTK_WINDOW(gd->window), 600, 490);

  g_signal_connect(G_OBJECT(gd->window), "delete_event",
      G_CALLBACK(ecb_close_graph_detail), qp);
  g_signal_connect(G_OBJECT(gd->window), "key-press-event",
      G_CALLBACK(ecb_key_press), qp);

  {
    GtkWidget *notebook;

    notebook =  gtk_notebook_new();
    gd->notebook = notebook;
    g_object_set(G_OBJECT(notebook), "scrollable", TRUE, NULL);

    {
      /*************************************************************************
       *                   Configure Graph
       *************************************************************************/

      GtkWidget *vbox;
      GtkWidget *tab_label;
      GtkWidget *hbox;

      vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);

      gd->config_label = make_pretty_header_label("Configure Graph", GTK_BOX(vbox));

      {
        GtkWidget *scrollwin;
        scrollwin = gtk_scrolled_window_new(gtk_adjustment_new(0,0,0,0,0,0),
                    gtk_adjustment_new(0,0,0,0,0,0));
        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);
        
        {
          /*****************************************************************************
           *             Left VBox
           *****************************************************************************/
          GtkWidget *ivbox;
          ivbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
          gtk_box_set_homogeneous(GTK_BOX(ivbox), FALSE);
          gd->show_container = ivbox;

          {
            /******************************************
             *         Show check buttons
             ******************************************/
            create_show_check_button("Show Grid",
                cb_show_grid, qp, ivbox, gr->show_grid);
            create_show_check_button("Show Grid Numbers",
                cb_show_grid_numbers, qp, ivbox, gr->grid_numbers);
            create_show_check_button("Show Lines",
                cb_show_lines, qp, ivbox, 1);
            create_show_check_button("Show Points",
                cb_show_points, qp, ivbox, 1);
          }

          {
            /******************************************
             *         X and Y Scales
             ******************************************/
            GtkWidget *combo_box;
            gd->x_scale = combo_box = gtk_combo_box_text_new();
            gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combo_box), 0, "Different X Scales");
            gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combo_box), 1, "Same X Scales");
            g_signal_connect(G_OBJECT(combo_box), "changed", G_CALLBACK(cb_same_x_scale), qp);
            gtk_box_pack_start(GTK_BOX(ivbox), combo_box, FALSE, FALSE, 0);
            gtk_widget_show(combo_box);

            gd->y_scale = combo_box = gtk_combo_box_text_new();
            gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combo_box), 0, "Different Y Scales");
            gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combo_box), 1, "Same Y Scales");
            g_signal_connect(G_OBJECT(combo_box), "changed", G_CALLBACK(cb_same_y_scale), qp);
            gtk_box_pack_start(GTK_BOX(ivbox), combo_box, FALSE, FALSE, 0);
            gtk_widget_show(combo_box);
          }

          {
            /********************************************
             *           Color and Font
             ********************************************/
            GtkWidget *grid;
            grid = gtk_grid_new();
            gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
            gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
            {
              GtkWidget *label, *picker;
              label = gtk_label_new ("Background Color:");
              gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
              gd->background_color_picker = picker = gtk_color_button_new();
              gtk_color_button_set_title(GTK_COLOR_BUTTON(picker),
                  "Select the Graph Background Color");
              gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(picker), TRUE);
              gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
              gtk_grid_attach(GTK_GRID(grid), picker, 1, 0, 1, 1);
              g_signal_connect(G_OBJECT(picker), "color-set",
                  G_CALLBACK(cb_background_color), qp);
              gtk_widget_show(label);
              gtk_widget_show(picker);

              label = gtk_label_new("Grid Lines Color:");
              gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
              gd->grid_color_picker = picker = gtk_color_button_new();
              gtk_color_button_set_title(GTK_COLOR_BUTTON(picker),
                  "Select the Graph Grid Color");
              gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(picker), TRUE);
              gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
              gtk_grid_attach(GTK_GRID(grid), picker, 1, 1, 1, 1);
              g_signal_connect(G_OBJECT(picker), "color-set",
                  G_CALLBACK(cb_grid_color), qp);
              gtk_widget_show(label);
              gtk_widget_show(picker);

              label = gtk_label_new ("Grid Numbers Color:");
              gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
              gd->numbers_color_picker = picker = gtk_color_button_new();
              gtk_color_button_set_title(GTK_COLOR_BUTTON(picker),
                  "Select the Graph Grid Numbers Color");
              gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(picker), TRUE);
              gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
              gtk_grid_attach(GTK_GRID(grid), picker, 1, 2, 1, 1);
              g_signal_connect(G_OBJECT(picker), "color-set",
                  G_CALLBACK(cb_grid_numbers_color), qp);
              gtk_widget_show(label);
              gtk_widget_show(picker);

              label = gtk_label_new ("Grid Numbers Font:");
              gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
              gd->font_picker = picker = gtk_font_button_new ();
              gtk_font_button_set_title(GTK_FONT_BUTTON(picker),
                  "Select the Graph Grid Numbers Font");
              gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);
              gtk_grid_attach(GTK_GRID(grid), picker, 1, 3, 1, 1);
              g_signal_connect(G_OBJECT(picker), "font-set",
                  G_CALLBACK(cb_grid_font), qp);
              gtk_widget_show(label);
              gtk_widget_show(picker);
            }

            gtk_box_pack_start(GTK_BOX(ivbox), grid, FALSE, FALSE, 0);
            gtk_widget_show(grid);
          }


          gtk_box_pack_start(GTK_BOX(hbox), ivbox, FALSE, FALSE, 10);
          gtk_widget_show(ivbox);


          /*****************************************************************************
           *             Right VBox
           *****************************************************************************/
          ivbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
          gtk_box_set_homogeneous(GTK_BOX(ivbox), FALSE);
          {
            /***********************************************
             *          Int Number Sliders
             ***********************************************/
            gd->line_width_slider = create_slider_input(
                "Plots Line Width", &ivbox, 1, 20, 200);

            gd->point_size_slider = create_slider_input(
                "Plots Point Size", &ivbox, 1, 20, 200);

            gd->grid_line_width_slider = create_slider_input(
                "Grid Line Width", &ivbox, 1, 20, 200);

            gd->grid_x_line_space_slider = create_slider_input(
                "Grid X Line Space", &ivbox, 30, 600, 2000);

            gd->grid_y_line_space_slider = create_slider_input(
                "Grid Y Line Space", &ivbox, 30, 600, 2000);
          }

          gtk_box_pack_start(GTK_BOX(hbox), ivbox, FALSE, FALSE, 10);
          gtk_widget_show(ivbox);

          /*****************************************************************************/
        }

        gtk_container_add(GTK_CONTAINER(scrollwin), hbox);
        gtk_widget_show(hbox);
        gtk_box_pack_start(GTK_BOX(vbox), scrollwin, TRUE, TRUE, 8);
        gtk_widget_show(scrollwin);

        make_redraw_button(vbox, qp);
      }


      tab_label = gtk_label_new("Configure Graph");
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, tab_label);


      gtk_widget_show(tab_label);
      gtk_widget_show(vbox);
    }
    {
      /*************************************************************************
       *                   Select Plots
       *************************************************************************/

      GtkWidget *vbox, *tab_label, *scrollwin, *label, *hbox;

      vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);

      make_pretty_header_label("Select Channels to Plot or Unplot", GTK_BOX(vbox));


      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);
      label = gtk_label_new("X Channel");
      gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 20);
      gtk_widget_show(label);
      label = gtk_label_new("Y Channel");
      gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 30);
      gtk_widget_show(label);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
      gtk_widget_show(hbox);

      scrollwin = gtk_scrolled_window_new(gtk_adjustment_new(0,0,0,0,0,0),
                      gtk_adjustment_new(0,0,0,0,0,0));
      {
        GtkWidget *box;
        gd->selecter_hbox = box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_box_set_homogeneous(GTK_BOX(box), FALSE);
        plot_selecter_make(qp);
        gtk_container_add(GTK_CONTAINER(scrollwin), box);
        gtk_widget_show(box);
      }
      gtk_box_pack_start(GTK_BOX(vbox), scrollwin, TRUE, TRUE, 8);
      gtk_widget_show(scrollwin);

      //make_redraw_button(vbox, qp);

      gtk_widget_show(vbox);
      tab_label = gtk_label_new("Select Channels to Plot");
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, tab_label);
      gtk_widget_show(tab_label);

    }

    {
      /*************************************************************************
       *                   Plots List
       *************************************************************************/

      GtkWidget *vbox, *tab_label, *scrollwin;

      vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);

      {
        GtkWidget *combo_box, *hbox;
        hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);

        gd->plot_list_combo_box = combo_box = gtk_combo_box_text_new();
        gtk_widget_set_size_request(combo_box, 260, -1);
        gd->plot_list_modes = 0;
        gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combo_box), 0, "Pointer Value");
        g_signal_connect(G_OBJECT(combo_box), "changed", G_CALLBACK(cb_plot_list_mode), qp);
        gtk_box_pack_start(GTK_BOX(hbox), combo_box, FALSE, FALSE, 0);
        //gtk_widget_set_margin_top(combo_box, 26);
        //gtk_widget_set_margin_bottom(combo_box, 14);
        gtk_widget_show(combo_box);
        gtk_widget_show(hbox);
      }


      scrollwin = gtk_scrolled_window_new(gtk_adjustment_new(0,0,0,0,0,0),
                      gtk_adjustment_new(0,0,0,0,0,0));
      {
        GtkWidget *box;
        gd->plot_list_hbox = box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_set_homogeneous(GTK_BOX(box), FALSE);
        plot_list_make(qp);
        gtk_container_add(GTK_CONTAINER(scrollwin), box);
        gtk_widget_show(box);
      }
      gtk_box_pack_start(GTK_BOX(vbox), scrollwin, TRUE, TRUE, 8);
      gtk_widget_show(scrollwin);


      make_redraw_button(vbox, qp);

      tab_label = gtk_label_new("Plots List and Values");
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, tab_label);

      gtk_widget_show(tab_label);
      gtk_widget_show(vbox);
    }

    g_signal_connect(G_OBJECT(notebook), "switch-page",
        G_CALLBACK(cb_graph_detail_switch_page), qp);

    gtk_container_add(GTK_CONTAINER(qp->graph_detail->window), notebook);
    gtk_widget_show(notebook);
  }

  qp_win_graph_detail_init(qp);
}

static
void graph_detail_show(struct qp_win *qp)
{
  ASSERT(qp);

  if(!qp->graph_detail)
    graph_detail_create(qp);

  qp_win_graph_detail_init(qp);

  gtk_widget_show(qp->graph_detail->window);
  gtk_window_present(GTK_WINDOW(qp->graph_detail->window));
}

/* We let the view menu be the holder of the state of whether
 * or not the graph_detail should be showing or not, instead
 * of declaring another variable. */
void
cb_view_graph_detail(GtkWidget *w, gpointer data)
{
  struct qp_win *qp;
  qp = data;

  if(_cb_view_graph_detail_reenter)
    return;
    
  _cb_view_graph_detail_reenter = 1;

  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_graph_detail)))
    graph_detail_show(qp);
  else
    graph_detail_hide(qp);

  _cb_view_graph_detail_reenter = 0;
}

void
cb_graph_detail_show_hide(GtkWidget *w, gpointer data)
{
  struct qp_win *qp;
  qp = data;

  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_graph_detail)))
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(qp->view_graph_detail), FALSE);
  else
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(qp->view_graph_detail), TRUE);
}


