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


#define GET_BUF_LEN  64
static char get_buf[GET_BUF_LEN];

static
void PrintRJ(FILE *out, int n, const char *str)
{
  int len;
  ASSERT(str);
  len = strlen(str);
  if(len < n)
  {
    n -= len;
    while(n--)
      putc(' ', out);
  }
#ifdef QP_DEBUG
  else
    VASSERT(0, "%s(,n=%d,\"%s\")\n", __func__, n, str);
#endif

  fprintf(out, "%s", str);
}

/* pad with spaces after */
static
void PrintLJ(FILE *out, int n, const char *format, ...)
{
  va_list ap;
  char *fmt, *str;
  int i, j, len;

  j = strlen(format);
  len = n + j + 2;
  fmt = qp_malloc(len + 1);
  str = qp_malloc(len + 1);

  strcpy(fmt, format);

  for(i=j;i<len;++i)
    fmt[i] = ' ';
  fmt[i] = '\0';

  va_start(ap, format);
  vsnprintf(str, n+1, fmt, ap);
  va_end(ap);

  VASSERT(str[n-1] == ' ',
      "maxlen=%d str=\"%s\" strlen=%zu\n",
      n, str, strlen(str));

  if(strlen(str) > n)
    str[n-1] = '\0';

  fprintf(out, "%s", str);

  free(fmt);
  free(str);
}


static inline
char *GeometryValue(cairo_rectangle_int_t *g)
{
  if(g->x == INT_MAX || g->y == INT_MAX)
    snprintf(get_buf, GET_BUF_LEN, "%dx%d", g->width, g->height);
  else
  {
    char x[32], y[32];
    snprintf(x, 32, "%+d", g->x);
    snprintf(y, 32, "%+d",  g->y);
    if(g->x == INT_MIN)
      snprintf(x, 32, "-0");
    if(g->y == INT_MIN)
      snprintf(y, 32, "-0");
    snprintf(get_buf, GET_BUF_LEN, "%dx%d%s%s",
        g->width, g->height, x, y);
  } 
  return get_buf;
}

static inline
char *ColorValue(struct qp_colora *c)
{
  snprintf(get_buf, GET_BUF_LEN,
      "'rgba(%d,%d,%d,%.2g)'",
          (int)(c->r * 255.0),
          (int)(c->g * 255.0),
          (int)(c->b * 255.0),
          c->a);
  return get_buf;
}

static inline
char *StringValue(const char *s)
{
  snprintf(get_buf, GET_BUF_LEN, "'%s'", s);
  return get_buf;
}

static inline
char *BoolValue(int val)
{
  snprintf(get_buf, GET_BUF_LEN, (val==-1)?"auto":(val?"on":"off"));
  return get_buf;
}

static inline
char *IntValue(int val)
{
  if(val == -1)
    snprintf(get_buf, GET_BUF_LEN, "auto");
  else
    snprintf(get_buf, GET_BUF_LEN, "%d", val);
  return get_buf;
}

static inline
char *DoubleValue(double val)
{
  if(val == -1.0)
    snprintf(get_buf, GET_BUF_LEN, "auto");
  else
    snprintf(get_buf, GET_BUF_LEN, "%g", val);
  return get_buf;
}

static inline
int GetBool(const char *arg, int with_auto)
{
  if(!strcasecmp(arg, "on") || arg[0] == 'y' || arg[0] == 'Y' ||
      !strcasecmp(arg, "true") || (arg[0] >= '1' && arg[0] <= '9') ||
      !strcasecmp(arg, "set"))
    return 1;
  if(with_auto && !strncasecmp(arg, "auto", 2))
    return -1;
  else
    return 0;
}

/* returns 1 on success. 0 on error */
static inline
int GetDouble(FILE *out, const char *arg, double min, double max, double *val)
{
  char *end;
  int ret;
  ret = strtod(arg, &end);
  if(*end != '\0')
  {
    fprintf(out, "bad double value: %s\n", arg);
    return 0;
  }
  else if(ret > max)
    ret = max;
  else if(ret < min)
    ret = min;
  *val = ret;
  return 1;
}


/* returns 1 on success. 0 on error */
static inline
int GetInt(FILE *out, const char *arg, int min, int max, int *val)
{
  char *end;
  int ret;
  ret = strtol(arg, &end, 10);
  if(*end != '\0')
  {
    fprintf(out, "bad int value: %s\n", arg);
    return 0;
  }
  else if(ret > max)
    ret = max;
  else if(ret < min)
    ret = min;
  *val = ret;
  return 1;
}

static inline
int GetSizeT(FILE *out, const char *arg, size_t min,
    size_t max, size_t *val)
{
  char *end;
  size_t ret;
  ret = strtoul(arg, &end, 10);
  if(*end != '\0')
  {
    fprintf(out, "bad size_t value: %s\n", arg);
    return 0;
  }
  else if(ret > max)
    ret = max;
  else if(ret < min)
    ret = min;
  *val = ret;
  return 1;
}

static inline
int End_Reply(struct qp_shell *sh, int argc, char **argv)
{
#ifdef QP_DEBUG
  if(SPEW_LEVEL() <= 1)
    spew_args("Processed command", argc, argv, sh->file_out);
#endif

  if(sh->pid != app->pid)
  {
    /* The protocol is: the user writes a one line and then
     * the server responds with any number of lines and ends
     * with END_REPLY"\n" 
     * meaning this is the end of the returned data from
     * the last command. */
    fprintf(sh->file_out, "%c\n", END_REPLY);
  }
  fflush(sh->file_out);
  return 1;
}

static
char *ArgsToString(char **argv)
{
  size_t len = 1;
  char **s, *ret;
  for(s=argv;*s;++s)
    len += strlen(*s)+1;
  ret = qp_malloc(sizeof(char)*len);
  len = 0;
  for(s=argv;*s;++s)
  {
    strcpy(&ret[len], *s);
    len += strlen(*s);
    ret[len++] = ' ';
  }
  ret[len-1] = '\0';
  return ret;
}

/* return 1 on success 0 on error */
static inline
int ParseColor(struct qp_colora *c, char **argv)
{
  GdkRGBA rgba;
  char *arg;
  arg = ArgsToString(argv);
  rgba.alpha = 1.0;
  if(!gdk_rgba_parse(&rgba, arg))
  {
    free(arg);
    return 0;
  }

  c->r = rgba.red;
  c->g = rgba.green;
  c->b = rgba.blue;
  c->a = rgba.alpha;
  free(arg);

  return 1;
}

/* returns 1 on success, 0 on error */
static
int GetPlotNums(char **argv, ssize_t **X, ssize_t **Y, size_t *num)
{
  char *arg;
  ssize_t max_chan = -1;
  struct qp_source *s;
  int ret = 0;
  for(s=qp_sllist_begin(app->sources);s;s=qp_sllist_next(app->sources))
    max_chan += s->num_channels;

  arg = ArgsToString(argv);

  printf("arg=\"%s\"\n", arg);

  if(max_chan > -1 && get_plot_option(arg, X, Y, num, NULL, 0, max_chan))
    ret = 1;

  free(arg);
  return ret;
}

static inline
void BadCommand(const char *prefix, size_t argc, char **argv, FILE *file)
{
  spew_args(prefix, argc, argv, file);
  fprintf(file, "Try: help\n");
}

static inline
void BadCommand2(FILE *file, int argc, char **argv)
{
  fprintf(file, "Bad %s %s command: ", argv[0], argv[1]);
  spew_args(NULL, argc, argv, file);
  fprintf(file, "Try: help\n");
}

static inline
void graph_switch_draw_mode(struct qp_graph *gr)
{
  qp_graph_switch_draw_mode(gr);
  ecb_graph_configure(NULL, NULL, gr);
  gtk_widget_queue_draw(gr->drawing_area);
}


#define APP_FORALLGRAPHS(QP, GR)                                          \
  do {                                                                    \
    struct qp_win *qp;                                                    \
    for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))      \
    {                                                                     \
      struct qp_graph *gr;                                                \
      QP;                                                                 \
      for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))\
      {                                                                   \
        GR;                                                               \
        gr->pixbuf_needs_draw = 1;                                        \
      }                                                                   \
      if(qp->graph_detail) qp_win_graph_detail_init(qp);                  \
    }                                                                     \
  } while(0)

#define APP_FORALLPLOTS(QP, GR, P)                                        \
  do {                                                                    \
    struct qp_win *qp;                                                    \
    for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))      \
    {                                                                     \
      struct qp_graph *gr;                                                \
      QP;                                                                 \
      for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))\
      {                                                                   \
        struct qp_plot *p;                                                \
        GR;                                                               \
        gr->pixbuf_needs_draw = 1;                                        \
        for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))   \
        {                                                                 \
          P;                                                              \
        }                                                                 \
      }                                                                   \
      if(qp->graph_detail) qp_win_graph_detail_init(qp);                  \
    }                                                                     \
  } while(0)



#define P_LEN   19
#define V_LEN   28

static inline
void PrintValueDesc(FILE *out,
    const char *par, const char *val, const char *_class, const char *desc)
{
  PrintRJ(out, P_LEN, par);
  fprintf(out, "   ");
  PrintLJ(out, V_LEN, "%s", val);
  fprintf(out, "   %s%s%s %s\n", tur, _class, trm, desc);
}

static inline
void PrintParValDescHeader(FILE *out)
{
  fprintf(out,
  "\n"
  "      parameter                 value                %sobject%s   description\n"
  " ------------------- ------------------------------ -------------------------------------\n", tur, trm);
}

static
char *app_get_value(const char *name)
{
  if(!strcmp(name, "default_graph"))
    return BoolValue(app->op_default_graph);
  if(!strcmp(name, "geometry"))
    return GeometryValue(&app->op_geometry);
  if(!strcmp(name, "label_separator"))
    return StringValue(app->op_label_separator);
  if(!strcmp(name, "labels"))
    return BoolValue(app->op_labels);
  if(!strcmp(name, "linear_channel"))
  {
    if(app->op_linear_channel)
      snprintf(get_buf, GET_BUF_LEN,
            "'%g %g'",
            ((double*)(app->op_linear_channel->data))[0],
            ((double*)(app->op_linear_channel->data))[1]);
    else
      snprintf(get_buf, GET_BUF_LEN, "off");
    return get_buf;
  }
  if(!strcmp(name, "new_window"))
    return BoolValue(app->op_new_window);
  if(!strcmp(name, "number_of_plots"))
    return IntValue(app->op_number_of_plots);
  if(!strcmp(name, "skip_lines"))
    return IntValue(app->op_skip_lines);
  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *app_window_get_value(const char *name)
{
  if(!strcmp(name, "border"))
    return BoolValue(app->op_border);
  if(!strcmp(name, "buttons"))
    return BoolValue(app->op_buttons);
  if(!strcmp(name, "menubar"))
    return BoolValue(app->op_menubar);
  if(!strcmp(name, "shape"))
    return BoolValue(app->op_shape);
  if(!strcmp(name, "statusbar"))
    return BoolValue(app->op_statusbar);
  if(!strcmp(name, "tabs"))
    return BoolValue(app->op_tabs);
  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *app_graph_get_value(const char *name)
{
  if(!strcmp(name, "bg"))
    return ColorValue(&app->op_background_color);
  if(!strcmp(name, "grid"))
    return BoolValue(app->op_grid);
  if(!strcmp(name, "grid_font"))
    return StringValue(app->op_grid_font);
  if(!strcmp(name, "grid_line_color"))
    return ColorValue(&app->op_grid_line_color);
  if(!strcmp(name, "grid_line_width"))
    return IntValue(app->op_grid_line_width);
  if(!strcmp(name, "grid_numbers"))
    return BoolValue(app->op_grid_numbers);
  if(!strcmp(name, "grid_text_color"))
    return ColorValue(&app->op_grid_text_color);
  if(!strcmp(name, "grid_x_space"))
    return IntValue(app->op_grid_x_space);
  if(!strcmp(name, "grid_y_space"))
    return IntValue(app->op_grid_y_space);
  if(!strcmp(name, "cairo"))
    return BoolValue(app->op_x11_draw?0:1);
  if(!strcmp(name, "same_x_scale"))
    return BoolValue(app->op_same_x_scale);
  if(!strcmp(name, "same_y_scale"))
    return BoolValue(app->op_same_y_scale);
  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *app_plot_get_value(const char *name)
{
  if(!strcmp(name, "gaps"))
    return BoolValue(app->op_gaps);
  if(!strcmp(name, "line_width"))
    return IntValue(app->op_line_width);
  if(!strcmp(name, "lines"))
    return BoolValue(app->op_lines);
  if(!strcmp(name, "point_size"))
    return IntValue(app->op_point_size);
  if(!strcmp(name, "points"))
    return BoolValue(app->op_points);
  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *window_get_value(struct qp_win *qp, const char *name)
{
  if(!strcmp(name, "border"))
    return BoolValue(qp->border);
  if(!strcmp(name, "buttons"))
    return BoolValue(gtk_check_menu_item_get_active(
          GTK_CHECK_MENU_ITEM(qp->view_buttonbar))?1:0);
  if(!strcmp(name, "fullscreen"))
    return BoolValue(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_fullscreen)));
  if(!strcmp(name, "geometry"))
  {
    int x, y, w, h;
    gtk_window_get_size(GTK_WINDOW(qp->window), &w, &h);
    gtk_window_get_position(GTK_WINDOW(qp->window), &x, &y);
    snprintf(get_buf, GET_BUF_LEN, "%dx%d%+d%+d", w, h, x, y);
    return get_buf;
  }
  if(!strcmp(name, "menubar"))
  {
      if(!app->is_globel_menu)
        return BoolValue(
          gtk_check_menu_item_get_active(
            GTK_CHECK_MENU_ITEM(qp->view_menubar))?1:0);
      else
        return BoolValue(1);
  }
  if(!strcmp(name, "shape"))
    return BoolValue(qp->shape);
  if(!strcmp(name, "statusbar"))
    return BoolValue(gtk_check_menu_item_get_active(
          GTK_CHECK_MENU_ITEM(qp->view_statusbar))?1:0);
  if(!strcmp(name, "tabs"))
    return BoolValue(gtk_check_menu_item_get_active(
          GTK_CHECK_MENU_ITEM(qp->view_graph_tabs))?1:0);

  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *window_graph_get_value(struct qp_win *qp, const char *name)
{
  if(!strcmp(name, "bg"))
    return ColorValue(&qp->background_color);
  if(!strcmp(name, "grid"))
    return BoolValue(qp->grid);
  if(!strcmp(name, "grid_font"))
    return StringValue(qp->grid_font);
  if(!strcmp(name, "grid_line_color"))
    return ColorValue(&qp->grid_line_color);
  if(!strcmp(name, "grid_line_width"))
    return IntValue(qp->grid_line_width);
  if(!strcmp(name, "grid_numbers"))
    return BoolValue(qp->grid_numbers);
  if(!strcmp(name, "grid_text_color"))
    return ColorValue(&qp->grid_text_color);
  if(!strcmp(name, "grid_x_space"))
    return IntValue(qp->grid_x_space);
  if(!strcmp(name, "grid_y_space"))
    return IntValue(qp->grid_y_space);
  if(!strcmp(name, "cairo"))
    return BoolValue(qp->x11_draw?0:1);
  if(!strcmp(name, "same_x_scale"))
    return BoolValue(qp->same_x_scale);
  if(!strcmp(name, "same_y_scale"))
    return BoolValue(qp->same_y_scale);
    
  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *window_plot_get_value(struct qp_win *qp, const char *name)
{
  if(!strcmp(name, "gaps"))
    return BoolValue(qp->gaps);
  if(!strcmp(name, "line_width"))
    return IntValue(qp->line_width);
  if(!strcmp(name, "lines"))
    return BoolValue(qp->lines);
  if(!strcmp(name, "point_size"))
    return IntValue(qp->point_size);
  if(!strcmp(name, "points"))
    return BoolValue(qp->points);

  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *graph_get_value(struct qp_graph *gr, const char *name)
{
  if(!strcmp(name, "bg"))
    return ColorValue(&gr->background_color);
  if(!strcmp(name, "grid"))
    return BoolValue(gr->show_grid);
  if(!strcmp(name, "grid_font"))
    return StringValue(gr->grid_font);
  if(!strcmp(name, "grid_line_color"))
    return ColorValue(&gr->grid_line_color);
  if(!strcmp(name, "grid_line_width"))
    return IntValue(gr->grid_line_width);
  if(!strcmp(name, "grid_numbers"))
    return BoolValue(gr->grid_numbers);
  if(!strcmp(name, "grid_text_color"))
    return ColorValue(&gr->grid_text_color);
  if(!strcmp(name, "grid_x_space"))
    return IntValue(gr->grid_x_space);
  if(!strcmp(name, "grid_y_space"))
    return IntValue(gr->grid_y_space);
  if(!strcmp(name, "cairo"))
    return BoolValue((gr->x11)?0:1);
  if(!strcmp(name, "same_x_scale"))
    return BoolValue(gr->same_x_scale);
  if(!strcmp(name, "same_y_scale"))
    return BoolValue(gr->same_y_scale);

  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *graph_plot_get_value(struct qp_graph *gr, const char *name)
{
  if(!strcmp(name, "gaps"))
    return BoolValue(gr->gaps);
  if(!strcmp(name, "line_width"))
    return DoubleValue(gr->line_width);
  if(!strcmp(name, "lines"))
    return BoolValue(gr->lines);
  if(!strcmp(name, "point_size"))
    return DoubleValue(gr->point_size);
  if(!strcmp(name, "points"))
    return BoolValue(gr->points);
  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

static
char *plot_get_value(struct qp_plot *p, const char *name)
{
  if(!strcmp(name, "gaps"))
    return BoolValue(p->gaps);
  if(!strcmp(name, "line_color"))
    return ColorValue((struct qp_colora *)&p->l);
  if(!strcmp(name, "line_width"))
    return DoubleValue(p->line_width);
  if(!strcmp(name, "lines"))
    return BoolValue(p->lines);
  if(!strcmp(name, "point_color"))
    return ColorValue((struct qp_colora *)&p->p);
  if(!strcmp(name, "point_size"))
    return DoubleValue(p->point_size);
  if(!strcmp(name, "points"))
    return BoolValue(p->points);

  VASSERT(0, "name=\"%s\" not found\n", name);
  return NULL;
}

