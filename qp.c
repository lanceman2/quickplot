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
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <dlfcn.h>

#include "quickplot.h"

#include "config.h"
#include "qp.h"
#include "debug.h"
#include "spew.h"
#include "list.h"
#include "channel.h"
#include "channel_double.h"
#include "term_color.h"
#include "plot.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


#if !(GTK_CHECK_VERSION(3, 0, 0))
# error "You need GTK+ version 3"
#endif



/* The default qp_win is always the last one
 * in the app qp list, which is the last one
 * created. */
struct qp_win *default_qp = NULL;

struct qp_app *app = NULL;

/* return 0 on success, -1 if it was initialized already
 * and 1 on failure. */
int qp_app_init(int *argc, char ***argv)
{
  if(argc == NULL || *argc == 0)
    argc = NULL;
  if(!argc || !argv || !*argv)
  {
    argc = NULL;
    argv = NULL;
  }

  if(!app->is_gtk_init && gtk_init_check(argc, argv) == FALSE)
  {
    QP_ERROR("gtk_init_check() failed\n");
    WARN("gtk_init_check() failed\n");
    VASSERT(0, "gtk_init_check() failed\n");
    return 1;
  }
  if(app->is_gtk_init)
    return -1;
  app->is_gtk_init = 1;
  app->argc = argc;
  app->argv = argv;



  {
    /* This stupid code is to fix Quickplot when running with
     * Ubuntu Unity window manager.  The libraries that
     * Quickplot runs with are not the same as `ldd PATHTOQUICKPLOT'
     * with tell you so the unity code that will run which I see from
     * /proc/PID/maps is libunity-gtk-module.so calling function
     * void gtk_module_init(void) which looks at environment variable
     * UBUNTU_MENUPROXY.  They appear to load that after the linker
     * is done.  I see no reasonable interface to the state of
     * the globel menu shit in the libunity-gtk-module.so source code.

     * We need this so that we know whether or not we can hide the
     * main menu, and in doing so will cause a redraw event,
     * which in turn tells us whether or not we need to set
     * the wait cursor when we try to hide the main menu, because
     * you can't set the cursor in the redraw code, because
     * it's too late then.  Ya what-ever!  Once again: Unity === bad design */

      const char *env;
      env = g_getenv("UBUNTU_MENUPROXY");
      if(dlopen("libunity-gtk-module.so", RTLD_NOLOAD | RTLD_LAZY) &&
            env && env[0] && (
                !g_ascii_strcasecmp(env, "0") ||
                !g_ascii_strcasecmp(env, "no") ||
                !g_ascii_strcasecmp(env, "off") ||
                !g_ascii_strcasecmp(env, "false")
                ))
        app->is_globel_menu = 0;
      else
        /* We will check after creating the menubar in win.c. */
        app->is_globel_menu = 3; // will be 0 or 1 later.
  }


  { /* The close buttons on the tabs were too big by default.
     * They made the tabs change height when the button
     * was added.  This appears to fix them. */
#ifdef QP_DEBUG
    GError *error = NULL;
#endif
    const char css[] =
      "GtkButton#tab_close_button {\n"
      "  -GtkButton-default-border: 0px;\n"
      "  -GtkButton-default-outside-border: 0px;\n"
      "  -GtkButton-inner-border: 0px;\n"
      "  -GtkWidget-focus-line-width: 0px;\n"
      "  -GtkWidget-focus-padding: 0px;\n"
      "   border-radius: 4px;\n" /* This looks prettier to me. */
      "  padding: 0px;\n"
      " }";
    GtkCssProvider *provider;
    provider = gtk_css_provider_new();
    if(gtk_css_provider_load_from_data(
          provider, css, strlen(css),
#ifdef QP_DEBUG
          &error
#else
          NULL
#endif
          ))
    {
      gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
#ifdef QP_DEBUG
    else
    {
      WARN("Error=%s\n", error->message);
    }
    if(error)
      g_error_free(error);
#endif
  }
  return 0;
}


struct qp_app *qp_app_create(void)
{
  ASSERT(!app);
  if(app) return app;

  app = qp_malloc(sizeof(*app));
  memset(app, 0, sizeof(*app));

  app->pid = getpid();
  app->argc = NULL;
  app->argv = NULL;

  app->sources = qp_sllist_create(NULL);
  app->shells = qp_sllist_create(NULL);

  app->op_grid_on_top = 1;
  app->op_number_of_plots = NUMBER_OF_PLOTS;


  /* Generated code */
# include "app_op_init.h"

  app->op_geometry.x = INT_MAX;
  app->op_geometry.y = INT_MAX;
  app->op_geometry.width = 800;
  app->op_geometry.height = 700;

  app->op_label_separator = qp_strdup(" ");

  /* default rgb(a) colors */
  app->op_background_color.r = 0.01;
  app->op_background_color.b = 0.06;
  app->op_background_color.g = 0.02;
  app->op_background_color.a = 0.4;

  app->op_grid_line_color.r = 0.76;
  app->op_grid_line_color.g = 0.76;
  app->op_grid_line_color.b = 0.76;
  app->op_grid_line_color.a = 0.6;

  app->op_grid_text_color.r = 0.76;
  app->op_grid_text_color.g = 0.76;
  app->op_grid_text_color.b = 0.76;
  app->op_grid_text_color.a = 0.9;

  app->gui_can_exit = 1;
  app->root_window_width = 0;
  app->root_window_height = 0;

  /* these get set later */
  app->grabCursor = NULL;
  app->pickCursor = NULL;
  app->zoomCursor = NULL;


  app->is_gtk_init = 0;
  app->qps = qp_sllist_create(NULL);
  app->main_window_count = 0;

  return app;
}

void qp_win_graph_remove(qp_win_t qp, qp_graph_t graph)
{
  qp = qp_win_check(qp);
  ASSERT(qp->graphs);
#ifdef QP_DEBUG
  ASSERT(qp_sllist_remove(qp->graphs, graph, 0) == 1);
#else
  qp_sllist_remove(qp->graphs, graph, 0);
#endif
}

void qp_app_source_remove(qp_source_t source)
{
  qp_app_check();
  ASSERT(app->sources);
#ifdef QP_DEBUG
  ASSERT(qp_sllist_remove(app->sources, source, 0) == 1);
#else
  qp_sllist_remove(app->sources, source, 0);
#endif
}

void qp_win_destroy(qp_win_t qp)
{
  qp = qp_win_check(qp);

  ASSERT(app);
  ASSERT(qp);
  ASSERT(qp->window);

  if(qp->ref_count != 1)
  {
    ASSERT(qp->ref_count > 1);
    qp->destroy_called = 1;
    /* We cannot use just a counter because there could be an
     * arbitary number of button destroying clicks that
     * we wish to delay acting on. */
    // The time of destruction will come later.
    return;
  }

  {
    struct qp_graph *gr;
    while((gr=qp_sllist_first(qp->graphs)))
    {
      /* We do not iterate here because qp_graph_destroy()
       * modifies the list. */
      qp_graph_destroy(gr);
    }
  }

  qp_sllist_destroy(qp->graphs, 0);
  qp_sllist_remove(app->qps, qp, 0);

  if(qp->graph_detail)
    qp_graph_detail_destory(qp);

  if(qp->window)
  {
    gtk_widget_destroy(qp->window);
    --(app->main_window_count);
    ASSERT(app->main_window_count >= 0);
  }

  free(qp);

  if(default_qp == qp)
    default_qp = qp_sllist_last(app->qps);

  if(qp->grid_font)
    free(qp->grid_font);

  if(app->main_window_count == 1)
  {
    /* We must make the other qp->delete_window_menu_item
     * have the correct sensitive state. */
    ASSERT(qp_sllist_length(app->qps) > 0);
    for(qp=qp_sllist_begin(app->qps);qp; qp=qp_sllist_next(app->qps))
      /* They do not all have windows */
      if(qp->window)
      {
        ASSERT(qp->delete_window_menu_item);
        gtk_widget_set_sensitive(qp->delete_window_menu_item, FALSE);
        break;
      }
    ASSERT(qp);
  }
}

void qp_app_set_window_titles(void)
{
  struct qp_win *qp;
  ASSERT(app);
  ASSERT(app->qps);
  for(qp = qp_sllist_begin(app->qps); qp; qp = qp_sllist_next(app->qps))
    if(qp->window)
      qp_win_set_window_title(qp);
}

/* We reset the window titles has files are
 * loaded and unloaded. */
void qp_win_set_window_title(struct qp_win *qp)
{
#define END_LEN  256
#define BEG_LEN  24
  char title_mem[BEG_LEN + END_LEN];
  char *title;

  ASSERT(qp);
  ASSERT(qp->window);
  if(!qp->window)
    return;

  title = &title_mem[BEG_LEN];

  if(qp_sllist_length(app->sources))
  {
    size_t len = END_LEN, l;
    struct qp_source *s;
    char *str;
    str = title;
    s = qp_sllist_begin(app->sources);
    ASSERT(s);
    snprintf(str, len, "Quickplot: %s", s->name);
    l = strlen(str);
    str += l;
    len -= l;
    for(s= qp_sllist_next(app->sources);
        s && len > 1;
        s= qp_sllist_next(app->sources))
    {
      snprintf(str, len, " %s", s->name);
      l = strlen(str);
      str += l;
      len -= l;
    }
    if(len == 1)
      snprintf(str-5, 5, " ...");
  }
  else
    sprintf(title, "Quickplot");

  if(qp->window_num > 1)
  {
    char beg[BEG_LEN];
    size_t len, l;
    snprintf(beg, BEG_LEN, "[%d] ", qp->window_num);
    l = len = strlen(beg);
    ASSERT(len);
    ASSERT(len<BEG_LEN);
    do
    {
      --l;
      title_mem[BEG_LEN-l-1] = beg[len-l-1];
    } while(l);

    title = &title_mem[BEG_LEN-len];
  }
    
  gtk_window_set_title(GTK_WINDOW(qp->window), title);

  DEBUG("Set main window title to: \"%s\"\n", title);

#undef BEG_LEN
#undef END_LEN
}

size_t qp_app_read(const char *filename)
{
  struct qp_source *s;
  s = qp_source_create(filename, QP_TYPE_UNKNOWN);

  if(!s) return 0;

  return s->num_channels;
}


static inline
size_t num_channels(struct qp_sllist *sources)
{
  size_t num_chan = 0;
  struct qp_source *s;
  for(s=qp_sllist_begin(sources);
      s; s=qp_sllist_next(sources))
    num_chan += s->num_channels;
  return num_chan;
}

static
struct qp_source *get_source(struct qp_sllist *sources,
    size_t chan_num,
    size_t *ret_source_chan_num)
{
  struct qp_source *s;
  size_t count = 0;

  for(s=qp_sllist_begin(sources);
      s; s=qp_sllist_next(sources))
  {
    count += s->num_channels;
    if(count > chan_num)
    {
      if(ret_source_chan_num)
        *ret_source_chan_num =
          chan_num -(count - s->num_channels);
      return s;
    }
  }
  VASSERT(0, "Cannot get channel %zu\n", chan_num);
  return NULL;
}

static inline
struct qp_source *get_source_channel_num(
    struct qp_sllist *sources,
    size_t chan_num,
    struct qp_channel **ret_channel,
    size_t *ret_source_chan_num)
{
  struct qp_source *s;
  struct qp_channel **c;
  size_t schan_num;
  s = get_source(sources, chan_num, &schan_num);
  if(ret_source_chan_num)
    *ret_source_chan_num = schan_num;

  c = s->channels;
  while(schan_num--) ++c;
  ASSERT(*c);
  *ret_channel = *c;
  
  return s;
}

static inline
void qp_append_channel_list(const char *xy, const ssize_t *a, size_t num)
{
  size_t i;
  QP_APPEND(" %s channels={", xy);
  for(i=0; i<num-1; ++i)
    QP_APPEND("%zd,", a[i]);
  QP_APPEND("%zd}\n", a[i]);
}

void qp_win_graph_default(qp_win_t qp)
{
  struct qp_source *s;

  s = qp_sllist_begin(app->sources);
  for(;s; s = qp_sllist_next(app->sources))
    qp_win_graph_default_source(qp, s, NULL);
}

/* Returns 1 on error 0 on success */
int qp_win_graph_default_source(qp_win_t qp,
    qp_source_t s, const char *name)
{
  ssize_t *x,*y, i, i_offset = 0;
  size_t num_plots;
  struct qp_source *ss;
  int ret;

  ASSERT(s);
  /* Lets make a rule that says that sources must have at
   * least 2 channnels. */
  ASSERT(s->num_channels >= 2);

  num_plots = app->op_number_of_plots;
  if(num_plots > s->num_channels - 1)
    num_plots = s->num_channels - 1;

  ss = qp_sllist_begin(app->sources);
  while(ss)
  {
    if(s == ss)
      break;
    i_offset += ss->num_channels;
    ss = qp_sllist_next(app->sources);
  }

  VASSERT(ss, "Source not found in list");
  if(!ss) return 1; /* source not found */

  x = qp_malloc(sizeof(ssize_t)*num_plots);
  y = qp_malloc(sizeof(ssize_t)*num_plots);

  for(i=0; i<num_plots; ++i)
    y[i] = (x[i] = i_offset) + i + 1;

  ASSERT(i >= 1);

  ret = qp_win_graph(qp, x, y, i, name);

  free(x);
  free(y);

  return ret;
}

/* Returns 1 on error
 * Returns 0 on success
 *
 * x and y are arrays of channel numbers starting at 0
 * channels numbers start at zero in the first channel in
 * the first source and from there number all channels in
 * all sources. */
int qp_win_graph(qp_win_t qp, const ssize_t *x, const ssize_t *y, size_t num,
    const char *name)
{
  size_t num_chan = 0, i;
  int same_extremes = 1;
  struct qp_channel *chan_0;
  struct qp_graph *gr, *last_graph;
  double dx_min = INFINITY, dy_min = INFINITY,
         xmin = INFINITY, xmax = -INFINITY,
         ymin = INFINITY, ymax = -INFINITY;

  ASSERT(x);
  ASSERT(y);
  ASSERT(num);

  if(!num)
  {
    WARN("Number of plots=0\n"); 
    return 1;
  }

  ASSERT(app->sources);
  ASSERT(x);
  ASSERT(y);
  ASSERT(num);

  num_chan += num_channels(app->sources);

  {
    size_t max_channel = 0;
    for(i=0; i<num; ++i)
    {
      if(x[i] > max_channel)
        max_channel = x[i];
      if(y[i] > max_channel)
        max_channel = y[i];
    }

    if(max_channel >= num_chan)
    {
      QP_WARN("Invalid plot list:\n");
      qp_append_channel_list("x", x, num);
      qp_append_channel_list("y", y, num);
      QP_APPEND(" There are only %zu channels not %zu\n",
          num_chan, max_channel + 1);
      QP_APPEND(" Channel numbers start at 0\n");
      return 1;
    }
  }

  if(!name || !name[0])
    name = get_source(app->sources, y[0], 0)->name;


  if(app->op_new_window && !qp)
  {
    if(!(qp = qp_win_create()))
      return 1;
  }
  else
    qp = qp_win_check(qp);


  last_graph = qp_sllist_last(qp->graphs);

  gr = qp_graph_create(qp, name);

  get_source_channel_num(app->sources, x[0], &chan_0, NULL);
  same_extremes = 1;

  for(i=0; i<num; ++i)
  {
    struct qp_channel *chan;
    get_source_channel_num(app->sources, x[i], &chan, NULL);
    if(chan->form == QP_CHANNEL_FORM_SERIES)
    {
      double dx;
      dx = chan->series.max - chan->series.min;
      if(xmin > chan->series.min)
        xmin = chan->series.min;
      if(xmax < chan->series.max)
        xmax = chan->series.max;
      if(dx > SMALL_DOUBLE && dx_min > dx)
        dx_min = dx;
      if(chan_0->series.max != chan->series.max ||
          chan_0->series.min != chan->series.min)
        same_extremes = 0;
    }
    else
      VASSERT(0, "More code needed here");
  }

  gr->same_x_scale = qp->same_x_scale;
  gr->same_x_limits = same_extremes;

  if(same_extremes)
  {
    /* same_extremes means they should be same scales */
    gr->same_x_scale = 1;
  }

  if(gr->same_x_scale == -1)
  {
    if(dx_min == INFINITY)
    {
      if(xmax == xmin)
      {
        /* It could be just one point so lets give it a
         * decent scale, not like the 1e-14 crap. */
        xmax += 1;
        xmin -= 1;
      }
      /* all the x channels were pretty constant */
      else if(xmax - xmin < SMALL_DOUBLE)
      {
        xmax += SMALL_DOUBLE;
        xmin -= SMALL_DOUBLE;
      }
    }
    else if(dx_min < (xmax - xmin)/SCALE_FACTOR)
    {
      /* use different scales */
      xmin = INFINITY;
      xmax = -INFINITY;
    }
  }
  else if(gr->same_x_scale)
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
  else
  {
    /* use different scales */
    xmin = INFINITY;
    xmax = -INFINITY;
  }


  get_source_channel_num(app->sources, y[0], &chan_0, NULL);
  same_extremes = 1;

  for(i=0; i<num; ++i)
  {
    struct qp_channel *chan;
    get_source_channel_num(app->sources, y[i], &chan, NULL);
      if(chan->form == QP_CHANNEL_FORM_SERIES)
    {
      double dy = chan->series.max - chan->series.min;
      if(ymin > chan->series.min)
        ymin = chan->series.min;
      if(ymax < chan->series.max)
        ymax = chan->series.max;
      if(dy > SMALL_DOUBLE && dy_min > dy)
        dy_min = dy;
      if(chan_0->series.max != chan->series.max ||
          chan_0->series.min != chan->series.min)
        same_extremes = 0;
    }
    else
      VASSERT(0, "More code needed here");
  }

  gr->same_y_scale = qp->same_y_scale;
  gr->same_y_limits = same_extremes;

  if(same_extremes)
  {
    /* same_extremes means they should be same scales */
    gr->same_y_scale = 1;
  }

  if(gr->same_y_scale == -1)
  {
    if(dy_min == INFINITY)
    {
      if(ymax == ymin)
      {
        /* It could be just one point so lets give it a
         * decent scale, not like the 1e-14 crap. */
        ymax += 1;
        ymin -= 1;
      }
      /* all the y channels were pretty constant */
      else if(ymax - ymin < SMALL_DOUBLE)
      {
        ymax += SMALL_DOUBLE;
        ymin -= SMALL_DOUBLE;
      }
    }
    else if(dy_min < (ymax - ymin)/SCALE_FACTOR)
    {
      /* use different scales */
      ymin = INFINITY;
      ymax = -INFINITY;
    }
  }
  else if(gr->same_y_scale)
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
  else
  {
    /* use different scales */
    ymin = INFINITY;
    ymax = -INFINITY;
  }


  /* Okay here is what the scaling really is */
  gr->same_x_scale = (xmax > xmin)?1:0;
  gr->same_y_scale = (ymax > ymin)?1:0;

  //DEBUG("xmax=%.30g xmin=%.30g\n", xmax, xmin);


  //DEBUG("x=[%g,%g] y=[%g,%g] dx_min=%g dy_min=%g\n",
  //    xmin, xmax, ymin, ymax, dx_min, dy_min);

  for(i=0; i<num; ++i)
  {
    char pname[128];
    struct qp_channel *chan_x, *chan_y;
    size_t cnx, cny;
    struct qp_source *sx, *sy;

    sx = get_source_channel_num(app->sources, x[i], &chan_x, &cnx);
    sy = get_source_channel_num(app->sources, y[i], &chan_y, &cny);

    qp_source_get_plot_name(pname, 128, sx, sy, cnx, cny);

    qp_plot_create(gr, chan_x, chan_y, pname, xmin, xmax, ymin, ymax);
  }

#if QP_DEBUG
  {
    struct qp_plot *p;
    size_t i = 0;
    INFO("Created graph \"%s\" with %zu plots:\n", gr->name, num);
    for(p=qp_sllist_begin(gr->plots);
        p; ++i, p=qp_sllist_next(gr->plots))
    {
      size_t len = (size_t) -1;
      if(p->x->form == QP_CHANNEL_FORM_SERIES)
        len = qp_channel_series_length(p->x);
      if(p->y->form == QP_CHANNEL_FORM_SERIES &&
          qp_channel_series_length(p->y) < len)
          len = qp_channel_series_length(p->y);
      APPEND("  plot %zu [%zu,%zu]: %s",
            i, x[i], y[i], p->name);
      if(len != (size_t) -1)
        APPEND(" with %zu points\n", len);
      else
        APPEND("\n");
    }
  }
#endif

  {
    struct qp_plot *p;
    size_t i = 0;
    QP_INFO("Created graph \"%s\" with %zu plots:\n", gr->name, num);
    for(p=qp_sllist_begin(gr->plots);
        p; ++i, p=qp_sllist_next(gr->plots))
    {
      size_t len = (size_t) -1;
      if(p->x->form == QP_CHANNEL_FORM_SERIES)
        len = qp_channel_series_length(p->x);
      if(p->y->form == QP_CHANNEL_FORM_SERIES &&
          qp_channel_series_length(p->y) < len)
          len = qp_channel_series_length(p->y);
      QP_APPEND("  plot %zu [%zu,%zu]: %s",
            i, x[i], y[i], p->name);
      if(len != (size_t) -1)
        QP_APPEND(" with %zu points\n", len);
      else
        QP_APPEND("\n");
    }
  }

  /* remove the last graph if it is empty */
  if(last_graph && qp_sllist_length(last_graph->plots) == 0)
    qp_graph_destroy(last_graph);

  qp_graph_detail_set_value_mode(gr);

  return 0;
}


#define STR_LEN  128
#define NUM_LEN  31
#define IMIN(x,y) (((x)<(y))?(x):(y))

/* if x == INT_MAX this will not use x,y */
void qp_win_set_status(struct qp_win *qp)
{
  char status[STR_LEN];
  struct qp_graph *gr;

  if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_statusbar)))
    return;

  gr = qp->current_graph;
  ASSERT(gr);

  if(qp_sllist_length(gr->plots) > 0)
  {
    char x_s[NUM_LEN], y_s[NUM_LEN];
    char *shift;
    struct qp_plot *p;
    p = qp_sllist_first(gr->plots);

    if(!p->xscale || !p->yscale)
      /* The plot has not been drawn yet. */
      return;

    if(!p->sig_fig_x || !p->sig_fig_y)
    {
      GtkAllocation a;
      gtk_widget_get_allocation(gr->drawing_area, &a);
      qp_plot_get_sig_fig(p, a.width, a.height);
    }

    /* we assume the plots have had the graph scales initialized */
    shift = (gr->grab_x || gr->grab_y)?"with shift":"";

    if(gr->same_x_scale && gr->qp->pointer_x >= 0)
      snprintf(x_s, IMIN(NUM_LEN, p->sig_fig_x + 8),
          "%+.*g                                  ",
          p->sig_fig_x,
          qp_plot_get_xval(p, qp->pointer_x + gr->pixbuf_x + gr->grab_x));
    else
      snprintf(x_s, IMIN(NUM_LEN, 8),
          "                                           ");

    if(gr->same_y_scale && gr->qp->pointer_y >= 0)
      snprintf(y_s, IMIN(NUM_LEN, p->sig_fig_y + 8),
          "%+.*g                                  ",
          p->sig_fig_y,
          qp_plot_get_yval(p, qp->pointer_y + gr->pixbuf_y + gr->grab_y));
    else
      snprintf(y_s, IMIN(NUM_LEN, 8),
          "                                           ");

    snprintf(status, STR_LEN, "%s  %s  {%s} %s %zu plot%s,"
        " Zoom Level %d %s", x_s, y_s, gr->name,
        (gr->x11)?"(x11 draw)":"(cairo draw)",
        qp_sllist_length(gr->plots),
        (qp_sllist_length(gr->plots) > 1)?"s":"",
        gr->zoom_level, shift);
  }
  else
    snprintf(status, STR_LEN, "%s no plots", gr->name);


  gtk_entry_set_text(GTK_ENTRY(qp->status_entry), status);
}
#undef NUM_LEN
#undef STR_LEN

