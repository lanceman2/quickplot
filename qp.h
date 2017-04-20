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


#ifndef PACKAGE_TARNAME
#  error  "You must include config.h before this file"
#endif

#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <cairo/cairo.h>


#define GRAB_BUTTON (1)
#define PICK_BUTTON (2)
#define ZOOM_BUTTON (3)


/* Given we just have one X server connection we just need
 * one saved pointer and mouse flag thingy. */
extern
int save_x, save_y,
    mouse_num, got_motion;



#ifdef INT
#error "INT is already defined"
#endif

/* ABS was defined already */
#define ABSVAL(x) (((x)>0)?(x):(-(x)))


#ifndef WINDOZ /* TODO: change to the windows defined thingy */
#define DIR_CHAR '/'
#define DIR_STR  "/"
#else
#define DIR_CHAR '\\'
#define DIR_STR  "\\"
#endif

#define QP_DA_BG_RGBA     { 1.0, 1.0, 1.0, 1.0 }


/* converting the values to ints before plotting
 * increases Cairo draw rate by an order of magnitude!! */
#define INT(x)  ((int)(((x)>0.0)?(x)+0.5:(x)-0.5))


struct qp_zoom
{
  double xscale, xshift, yscale, yshift;
  struct qp_zoom *next;
};


/* color with alpha */
struct qp_colora
{
  double r, g, b, a;
};


struct qp_color
{
  struct qp_colora c; /* cairo colors */
  unsigned long x;    /* x11 color */
};



/* The plot class precomputes all the linear transformations
 * and reduces them to just one mulitple and one add for
 * each point value read in the tight plot loops where it counts. */

struct qp_plot
{
  struct qp_channel *x, *y;
 
  /* For value picking */
  struct qp_channel *x_picker, *y_picker;
  size_t num_points, picker_i; /* current picker index */
  double xval, yval; /* last values read from x_picker and y_picker */


  struct qp_graph *gr;
  char *name;
  int plot_num; /* gr->plot_create_count after this was made */

  /* point and line colors */
  struct qp_color p, l;

  /* boolean to show lines, points and gaps */
  int lines, points, gaps;

  /* these are changed at the begining of each reading/plotting loop
   * They change with the current zoom that is passed in from the
   * graph */
  double xscale, yscale, xshift, yshift;
  /* these store the plot scales for the life of the plot
   * They never change after the plot is created */
  double xscale0, yscale0, xshift0, yshift0;

  /* This is used when we know how to limit the number
   * of values that we need to read from the channels
   * based on the nature of the data. */
  size_t num_read;

  double line_width, point_size;

  /* These show the middle mouse picker plot values */
  GtkWidget *x_entry, *y_entry;
  int sig_fig_x, sig_fig_y;

  /* Virtual functions in C */

  /* This gives us a common interface to reading the
   * data whether the data be stored in floats, doubles
   * ints, and etc. */

  int (* x_is_reading)(struct qp_channel *c);
  int (* y_is_reading)(struct qp_channel *c);

  double (* channel_x_begin)(struct qp_channel *c);
  double (* channel_y_begin)(struct qp_channel *c);
  double (* channel_x_end)  (struct qp_channel *c);
  double (* channel_y_end)  (struct qp_channel *c);
  double (* channel_x_next) (struct qp_channel *c);
  double (* channel_y_next) (struct qp_channel *c);
  double (* channel_x_prev) (struct qp_channel *c);
  double (* channel_y_prev) (struct qp_channel *c);


  double (* channel_series_x_index)(struct qp_channel *c, size_t i);
  double (* channel_series_y_index)(struct qp_channel *c, size_t i);

  size_t (* channel_series_x_get_index)(struct qp_channel *c);

  //size_t (* num_vals)(struct qp_plot *p);
};


struct qp_source
{
  char *name;
  char **labels;
  size_t num_labels;

  size_t num_values;
  int value_type; /* can be multi-type */

  size_t num_channels;

  /* An array of channels (pointers) */
  struct qp_channel **channels;
};



/* graphs do not exist unless they are
 * in a qp_win with a gtk window. */
struct qp_graph
{
  int ref_count; // flags to keep from destroying at the wrong time
  int destroy_called;

  char *name;

  int graph_num; /* win->graph_create_count after this was made */
  struct qp_plot *current_plot;

  struct qp_color_gen *color_gen;

  struct qp_sllist *plots;
  struct qp_win *qp;

  GtkWidget *drawing_area,
            *tab_label,
            *tab_label_hbox,
            *close_button;

  /* xscale, xshift, yscale, yshift provide an
   * additional drawing area transformation that maps
   * to a sub area of the drawing area and flips Y
   * so it is positively changing in the up direction. */
  int xscale, xshift, yscale, yshift;


  /* The first zoom in the stack is the identity transformation.
   * Zooms are normalized to a 0,0 to 1,1 box */
  struct qp_zoom *z;
 
  int zoom_level; /* starting at 0 */
  
  
  /* bool value == do all the plots have the same scale? */
  int same_x_scale, same_y_scale;
  /* do all the plots have the same min and max */
  int same_x_limits, same_y_limits;

  int show_grid, grid_numbers;
  /* maximum pixel space between grid lines
   * the smallest it could be is 1/3 of this */
  int grid_x_space, grid_y_space;

  /* default plot parameters in the graph */
  int line_width, point_size;
  int points, lines, gaps;

  int grid_line_width;
  int grid_on_top;


  char *grid_font;
  PangoLayout *pangolayout;

  struct qp_colora background_color, grid_line_color, grid_text_color;

  /* A place to store the background color is we
   * switch to using X11 shape extension */
  double bg_alpha_preshape;

  int pixbuf_width, pixbuf_height,
      pixbuf_x, pixbuf_y;
  /* grab x,y needs to be a double so we may scale it on
   * window resize */
  double grab_x, grab_y;

  /* Defining relative positions of things:
   *
 ------------------------------------------------------------------------
 position            |position in  |position       |position
 in pixbuf           |drawing area |in scaled plots|in plot values
   (int)             |   (int)     |   (int)       |  (double)
 --------------------|-------------|---------------|----------------------
 (xi+pixbuf_x+grab_x,|(xi+grab_x,  |  (xi,yi)      |(qp_plot_get_xval(xi),
  yi+pixbuf_y+grab_y)| yi+grab_y)  |               | qp_plot_get_xval(xi))
 -------------------------------------------------------------------------

     scaling takes place in the qp_plot_get_*val() in the qp_plot objects
   */


  /* since we have our own drawing buffer which we double buffer with,
   * we do not need to redraw it at every draw event, hence this flag */
  int pixbuf_needs_draw;

  int draw_zoom_box;      // flag to tell to draw a zoom box
  int z_x, z_y, z_w, z_h; // zoom box geometry in drawing_area pixels
  int draw_value_pick;
  /* Value pick mode
   * First two bits:
   * Pointer Value          = 0,
   * Interpolate Plot Value = 1,
   * Pick Plot Value        = 2,
   * third and forth bits are available modes:
   * 0 = 0, 1 = 0 or 1, or 2 = 0, 1 or 2 */
  int value_mode,
      /* The pick x y values depend on the mode
       * if it's mode=2 Pick Plot Value then
       * value_pick_x (y) will not necessarily be the
       * same as qp->pointer_x (y). */
      value_pick_x, value_pick_y;


  int waiting_to_resize_draw;

  /* our back drawing buffer which may be larger than the drawing_area */
  cairo_surface_t *pixbuf_surface;
  cairo_t *cr; /* just tmp variable for drawing */
  void (*DrawLine)(struct qp_graph *gr, int *new_line,
      double from_x, double from_y, double to_x, double to_y);

  int font_antialias_set; /* bool */
  
  /* is NULL if not drawing with X11 */
  struct qp_graph_x11 *x11;
  int plot_create_count;
};

struct qp_graph_x11
{
  GC gc;
  Pixmap pixmap; /* sits under the pixbuf_surface */
  Display *dsp;

  /* background is used to store the background color
   * in raw format for the X11 shape extension draw
   * mode. */
  uint32_t background;
  int background_set;
};

/* there can be only one app object */
struct qp_app
{
  int *argc;
  char ***argv;

  int is_globel_menu;
  int is_gtk_init;
  int main_window_count;
  pid_t pid;

  struct qp_sllist *qps;
  struct qp_sllist *sources;
  struct qp_sllist *shells;


/* This include declares op_ variables which are long
 * options like for example:
 * --grid-line-width ==> op_grip_line_width
 *
 *  app_op_declare.h is a generated file. */
# include "app_op_declare.h"

  /* TODO: an option like grid draw position. */
  int op_grid_on_top;

  GdkCursor *waitCursor,
            *grabCursor,
            *holdCursor,
            *pickCursor,
            *zoomCursor;

 
  int root_window_width, root_window_height;

  int gui_can_exit;
};

struct qp_slider
{
  int min, max, extended_max;
  GtkWidget *entry, *scale;
  void (*callback)(struct qp_slider *s);
  int *val;
  double *dval;
  int is_plot_line_width;
  struct qp_graph *gr;
};


#define PL_IS_SHOWING (1<<4)

struct qp_graph_detail
{
  /* see graph::valuemode
   * first 4 bits for modes
   * and fifth bit for is showing */
  int plot_list_modes;

  GtkWidget *window,
            *notebook,
            *config_label,
            *show_container,
            *background_color_picker,
            *grid_color_picker,
            *numbers_color_picker,
            *font_picker,
            *x_scale,
            *y_scale,
            *selecter_x_vbox,
            *selecter_y_vbox,
            *selecter_drawing_area,
            *selecter_hbox,
            *plot_list_hbox,
            *plot_list_combo_box,
            *plot_list_drawing_area,
            **plot_list_button_lines,
            **plot_list_button_points;

  struct qp_slider *line_width_slider,
                   *point_size_slider,
                   *grid_line_width_slider,
                   *grid_x_line_space_slider,
                   *grid_y_line_space_slider,
                   **plot_line_width_slider,
                   **plot_point_size_slider;
};

/* There is one of these per quickplot main window
 * The window does not have to be made. */
struct qp_win
{
  int ref_count; // flags to keep from destroying at the wrong time
  int destroy_called;

  struct qp_sllist *graphs;
  struct qp_graph *current_graph;

  GtkWidget *window,
            *view_buttonbar,
            *view_menubar,
            *view_graph_tabs,
            *view_statusbar,
            *view_border,
            *view_fullscreen,
            *view_shape,
            *view_x11_draw,
            *view_cairo_draw,
            *view_graph_detail,
            *copy_window_menu_item,
            *delete_window_menu_item,
            *menubar,
            *file_menu,
            *buttonbar,
            *button_graph_detail,
            *notebook, /* graph tabs */
            *statusbar,
            *status_entry;

  
  struct qp_graph_detail *graph_detail;



  //int wait_warning_showing;

  /* this is the last known pointer position, in pixels,
   * in the drawing_area, all drawing areas being in the
   * same position and size for a given qp_win. */
  int pointer_x, pointer_y;

  /* A flag to update the graph_detail after
   * a graph draw */
  int update_graph_detail;

  int wait_cursor_showing;

  /* boolean  draw graphs with X11 API */
  int x11_draw;

  /* Each qp window that is made has a number
   * assigned. The window title uses this. */
  int window_num;

  int border;
  int shape; /* use X11 shape extention */

  /* This is just when we use the X shape extension.
   * We save the last shape that was applied and
   * do not apply a new shape region unless it
   * changes.  Otherwise we can end up with many
   * unnecessary redraw events. */
  cairo_region_t *last_shape_region;

  /* The front page after qp_startup_idle_callback() */
  int init_front_page_num;
  int graph_create_count;

  /* graph and plot default parameter values */
  int gaps, lines, points, grid, grid_numbers, same_x_scale, same_y_scale;
  int grid_x_space, grid_y_space, grid_line_width;
  int line_width, point_size;
  char *grid_font;
  struct qp_colora background_color, grid_line_color, grid_text_color;
};


/* this is the one and only gtk app object. */
extern
struct qp_app *app;

extern
struct qp_app *qp_app_create(void);

extern
void set_value_pick_entries(struct qp_graph *gr, int x, int y);

extern
void qp_getargs_1st_pass(int argc, char **argv);

extern
void qp_getargs_2nd_pass(int argc, char **argv);

extern
void qp_graph_detail_set_value_mode(struct qp_graph *gr);
extern
void qp_graph_detail_plot_list_remake(struct qp_win *qp);

extern
void qp_graph_detail_destory(struct qp_win *qp);

extern
struct qp_win *qp_win_copy_create(struct qp_win *old_qp);

extern
int qp_find_doc_file(const char *fileName, char **fullpath_ret);

extern
int qp_launch_browser(const char *fileName);

extern
void qp_get_root_window_size(void);

/* Setup the widget for a particular graph */
extern
void qp_win_graph_detail_init(struct qp_win *qp);

extern
void qp_graph_copy(struct qp_graph *gr, struct qp_graph *old_gr);

extern
void qp_win_set_status(struct qp_win *qp);

extern
void qp_win_set_window_title(struct qp_win *qp);
extern
void qp_app_set_window_titles(void);

extern
void add_source_buffer_remove_menus(struct qp_source *source);

extern
void qp_app_graph_detail_source_remake(void);

extern
void qp_plot_set_X11_color(struct qp_plot *p, struct qp_color *c);

#ifdef QP_DEBUG
extern
void qp_source_debug_print(struct qp_source *s);
#endif

extern
void qp_plot_destroy(struct qp_plot *plot, struct qp_graph *gr);


extern
int qp_source_parse_doubles(struct qp_source *source, char *line_in);

extern
void qp_graph_zoom_out(struct qp_graph *gr, int all);

extern
void qp_graph_switch_draw_mode(struct qp_graph *gr);

extern
void qp_graph_set_grid_font(struct qp_graph *gr);

extern
void qp_win_graph_remove(qp_win_t qp, qp_graph_t graph);

/* plots belong to the creating graph */
extern
struct qp_plot *qp_plot_copy_create(qp_graph_t gr,
    struct qp_plot *old_p);

extern
qp_plot_t qp_plot_create(struct qp_graph *graph,
    qp_channel_t x, qp_channel_t y, const char *name,
    double xmin, double xmax, double ymin, double ymax);

extern
qp_graph_t qp_graph_create(qp_win_t qp, const char *name);

extern
void qp_graph_grid_draw(struct qp_graph *gr,
    struct qp_plot *p,
    cairo_t *cr, int width, int height);

extern
void qp_graph_destroy(qp_graph_t graph);

extern
void qp_graph_append(qp_graph_t graph, qp_plot_t  plot);


extern
void qp_graph_draw(struct qp_graph *gr, cairo_t *cr);


extern
int qp_win_save_png(struct qp_win *qp,
    struct qp_graph *gr, const char *filename);

/** 
 * Sources can be shared between qps
 *
 * value_type will be ignored if the value_type can
 * be easily determined from the file contents. */
extern
qp_source_t qp_source_create(
    const char *filename, int value_type);

extern
qp_source_t qp_source_create_from_func(
    const char *name, int value_type,
    void (* func)(const void *));
  
extern
void qp_source_destroy(qp_source_t source);


extern
struct qp_gtk_options *strip_gtk_options(int *argc, char ***argv);

extern
int qp_gtk_init_check(struct qp_gtk_options *opt);


/* default_qp is the last qp created during startup
 * After that it is set by the shells.
 * TODO: Add list of structs to hold attibutes
 * of connected shells, since more than one shell
 * can connect at a time. */
extern
struct qp_win *default_qp;

static inline
struct qp_win *qp_win_check(struct qp_win *qp)
{
  if(qp)
    return qp;
  if(default_qp)
    return default_qp;
  return default_qp = qp_win_create();
}

static inline
char *qp_source_get_label(struct qp_source *s, size_t num, char *buf, size_t buf_len)
{
  if(num >= s->num_labels)
    snprintf(buf, buf_len, "%s[%zu]", s->name, num);
  else
    snprintf(buf, buf_len, "%s", s->labels[num]);
  return buf;
}

static inline
void qp_app_check(void)
{
  if(!app)
    qp_app_create();
}

/* Checks for app and the gtk_init() was called. */
static inline
int qp_app_init_check(void)
{
  if(!app)
    qp_app_create();
  if(!app->is_gtk_init && qp_app_init(NULL, NULL))
    return 1; /* failure */
  return 0; /* success */
}

static inline
void qp_source_get_plot_name(char *pname, size_t plen,
    struct qp_source *sx, struct qp_source *sy,
    size_t channel_num_x, size_t channel_num_y)
{
  size_t len = 0;
      
  if(channel_num_y >= sy->num_labels)
    snprintf(pname, plen, "%s[%zu] VS ",
        sy->name, channel_num_y);
  else
    snprintf(pname, plen, "%s VS ",
        sy->labels[channel_num_y]);
  
  len = strlen(pname);

  if(channel_num_x >= sx->num_labels)
    snprintf(&pname[len], plen-len, "%s[%zu]",
        sx->name, channel_num_x);
  else
    snprintf(&pname[len], plen-len, "%s",
        sx->labels[channel_num_x]);
}

extern
void qp_graph_remove_plot(struct qp_graph *gr, struct qp_plot *p);
extern
void qp_graph_add_plot(struct qp_graph *gr,
    struct qp_source *sx, struct qp_source *sy,
    size_t x_channel_num, size_t y_channel_num);
extern
void qp_graph_same_x_scale(struct qp_graph *gr, int same_x_scale);
extern
void qp_graph_same_y_scale(struct qp_graph *gr, int same_y_scale);

