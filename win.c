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

#include "imgGrabCursor.xpm"
#include "imgHoldCursor.xpm"
#include "quickplot.xpm"
#include "imgNewWindow.xpm"
#include "imgDeleteWindow.xpm"
#include "imgCopyWindow.xpm"
#include "imgSaveImage.xpm"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif

static int main_window_create_count = 0;


static inline
GtkWidget *create_check_menu_item(GtkWidget *menu, const char *label,
    guint key, gboolean is_checked,
    void (*callback)(GtkWidget*, void*),
    gpointer data)
{
  GtkWidget *mi;
  mi = gtk_check_menu_item_new_with_mnemonic(label);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), is_checked);

  if(key)
    gtk_widget_add_accelerator(mi, "activate",
      gtk_menu_get_accel_group(GTK_MENU(menu)),
      key, (GdkModifierType) 0x0, GTK_ACCEL_VISIBLE);

  g_signal_connect(G_OBJECT(mi), "toggled", G_CALLBACK(callback), data);

  gtk_widget_show(mi);
  return mi;
}

    
static inline
void create_menu_item_seperator(GtkWidget *menu)
{
  GtkWidget *mi;
  mi = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
  gtk_widget_show(mi);
}

static inline
GtkWidget *create_menu(GtkWidget *menubar,
    GtkAccelGroup *accelGroup, const char *label)
{
  GtkWidget *menuitem, *menu;
  menuitem = gtk_menu_item_new_with_label(label);
  menu = gtk_menu_new();
  gtk_menu_set_accel_group(GTK_MENU(menu), accelGroup);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitem);
  gtk_widget_show(menuitem);
  /* The menu is shown when the user activates it. */
  return menu;
}

static inline
GtkWidget *create_menu_item(GtkWidget *menu,
	 const char *label,
	 const char *pixmap[], const gchar *STOCK,
	 guint key, gboolean with_mnemonic,
	 void (*callback)(GtkWidget*, void*),
	 gpointer data, gboolean is_sensitive)
{
  GtkWidget *mi;

  if(with_mnemonic)
    mi = gtk_image_menu_item_new_with_mnemonic(label);
  else
    mi =  gtk_image_menu_item_new_with_label(label);

  if(pixmap)
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),
      gtk_image_new_from_pixbuf(
	gdk_pixbuf_new_from_xpm_data(pixmap)));
  else if(STOCK)
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),
      gtk_image_new_from_stock(STOCK, GTK_ICON_SIZE_MENU));

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

  if(callback)
    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(callback), data);

  if(key)
    gtk_widget_add_accelerator(mi, "activate",
      gtk_menu_get_accel_group(GTK_MENU(menu)),
      key, (GdkModifierType) 0x0, GTK_ACCEL_VISIBLE);

  gtk_widget_set_sensitive(mi, is_sensitive);
  gtk_widget_show(mi);
  return mi;
}

static inline
void add_source_buffer_remove_menu(struct qp_win *qp, struct qp_source *s)
{
  GtkWidget *mi;
  const size_t MLEN = 64;
  char str[MLEN];
  char *name;
  size_t len;
  len = strlen(s->name);
  if(len > MLEN-1)
  {
    snprintf(str, MLEN, "... %s", &s->name[len-59]);
    name = str;
  }
  else
    name = s->name;

  mi = create_menu_item(qp->file_menu, name, NULL, GTK_STOCK_DELETE,
            0, FALSE, cb_remove_source, s, TRUE);
  ASSERT(g_object_get_data(G_OBJECT(mi), "quickplot-source") == NULL);
  g_object_set_data(G_OBJECT(mi), "quickplot-source", s);
  gtk_widget_set_tooltip_text(mi, "Remove this buffer, its channels,"
      " and all its plots");
}


void add_source_buffer_remove_menus(struct qp_source *s)
{
  struct qp_win *qp;
  for(qp=(struct qp_win*)qp_sllist_begin(app->qps);
      qp; qp=(struct qp_win*)qp_sllist_next(app->qps))
    if(qp->window)
      add_source_buffer_remove_menu(qp, s);
}


static inline
GtkWidget *create_button(GtkWidget *hbox, GtkAccelGroup *accelGroup,
       const char *label, guint key,
       void (*callback)(GtkWidget*, void*),
       gpointer data)
{
  GtkWidget *b = gtk_button_new_with_mnemonic(label);
  gtk_box_pack_start(GTK_BOX(hbox), b, FALSE, TRUE, 0);
  gtk_widget_add_accelerator(b, "activate", accelGroup, key,
			     (GdkModifierType) 0x0,
			     GTK_ACCEL_VISIBLE);
  gtk_widget_show(b);
  g_signal_connect(G_OBJECT(b),"clicked", G_CALLBACK(callback), data);
  return b;
}


void qp_get_root_window_size(void)
{
  GdkWindow *root;
  root = gdk_get_default_root_window();
  ASSERT(root);
  app->root_window_width = gdk_window_get_width(root);
  app->root_window_height = gdk_window_get_height(root);
  ASSERT(app->root_window_width > 0);
  ASSERT(app->root_window_height > 0);
}

/* We flip back the tab and wait for the drawing to
 * finish and then flip back to the next tab and
 * so on, until all tabs have been flipped. */
static
gboolean qp_startup_idle_callback(gpointer data)
{
  struct qp_win *qp = NULL;
  qp = (struct qp_win *) data;
  ASSERT(qp);
  ASSERT(qp->notebook);

  if(qp->destroy_called)
  {
    --qp->ref_count;
    qp_win_destroy(qp);
    return FALSE;
  }

//DEBUG("qp->init_front_page_num=%d  page=%d\n", qp->init_front_page_num,
//    gtk_notebook_get_current_page(GTK_NOTEBOOK(qp->notebook)));
  
  if(qp->init_front_page_num == -1)
  {
    int n;

    if((n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(qp->notebook))) == 1)
    {
      qp->init_front_page_num = -1;
      --qp->ref_count;
      DEBUG("removing callback\n");
      return FALSE; /* remove this idle callback */
    }

    qp->init_front_page_num =
      gtk_notebook_get_current_page(GTK_NOTEBOOK(qp->notebook));

    if(qp->init_front_page_num != 0)
      gtk_notebook_set_current_page(GTK_NOTEBOOK(qp->notebook), 0);
    else
      gtk_notebook_set_current_page(GTK_NOTEBOOK(qp->notebook), 1);
    return TRUE;
  }
  else
  {
    int n, c;
    gtk_notebook_next_page(GTK_NOTEBOOK(qp->notebook));
    n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(qp->notebook));
    c = gtk_notebook_get_current_page(GTK_NOTEBOOK(qp->notebook));
    if(c == qp->init_front_page_num)
    {
      gtk_notebook_next_page(GTK_NOTEBOOK(qp->notebook));
      c = gtk_notebook_get_current_page(GTK_NOTEBOOK(qp->notebook));
    }

    if(c == n-1)
    {
      gtk_notebook_set_current_page(GTK_NOTEBOOK(qp->notebook), qp->init_front_page_num);
      qp->init_front_page_num = -1;
      --qp->ref_count;
      DEBUG("removing callback\n");
      return FALSE; /* remove this idle callback */
    }
    return TRUE;
  }
}

struct qp_win_config
{
  int border, x11_draw, width, height, menubar, buttonbar, tabs, statusbar;
};

/* The "gtk-shell-shows-menubar" property of the menubar
 * seem to be what tells Unity Globel menubar to be
 * used.  It would be nice if we could tell this before
 * we start making widgets.
 *
 * Returns 1 if the "gtk-shell-shows-menubar" property is set
 * else returns 0 */
static int
_shell_shows_menubar(GtkWidget *widget)
{
  GtkSettings *settings;
  GParamSpec *pspec;
  gboolean shell_shows_menubar;

  g_return_val_if_fail(GTK_IS_WIDGET(widget), 0);
  settings = gtk_widget_get_settings(widget);
  g_return_val_if_fail(GTK_IS_SETTINGS(settings), 0);
  pspec = g_object_class_find_property(G_OBJECT_GET_CLASS (settings),
          "gtk-shell-shows-menubar");
  g_return_val_if_fail(G_IS_PARAM_SPEC(pspec), 0);
  g_return_val_if_fail(pspec->value_type == G_TYPE_BOOLEAN, 0);
  g_object_get(settings, "gtk-shell-shows-menubar",
          &shell_shows_menubar, NULL);

  return shell_shows_menubar?1:0;
}

/* the qp_win_config thing will over-ride the app->op_* stuff
 * so that we may have this make a copy of a qp that exists
 * and may have a different state than the app->op_* stuff */
static
qp_win_t _qp_win_create(const struct qp_win_config *c)
{
  struct qp_win *qp;
  GtkWidget *vbox;
  GtkAccelGroup *accelGroup;
  GdkPixbuf *pixbuf;

  if(qp_app_init_check())
    return NULL;

  qp = qp_malloc(sizeof(*qp));
  memset(qp,0,sizeof(*qp));
  qp->ref_count = 1;
  qp->graphs = qp_sllist_create(NULL);
  qp_sllist_append(app->qps, qp);
  default_qp = qp;

  qp->gaps = app->op_gaps;
  qp->lines = app->op_lines;
  qp->points = app->op_points;
  qp->grid = app->op_grid;
  qp->grid_font = qp_strdup(app->op_grid_font);
  qp->same_x_scale = app->op_same_x_scale;
  qp->same_y_scale = app->op_same_y_scale;
  qp->grid_numbers = app->op_grid_numbers;
  qp->grid_line_width = app->op_grid_line_width;
  qp->grid_x_space = app->op_grid_x_space;
  qp->grid_y_space = app->op_grid_y_space;
  qp->point_size = app->op_point_size;
  qp->line_width = app->op_line_width;

  memcpy(&qp->background_color, &app->op_background_color,
      sizeof(qp->background_color));
  memcpy(&qp->grid_line_color, &app->op_grid_line_color,
      sizeof(qp->grid_line_color));
  memcpy(&qp->grid_text_color, &app->op_grid_text_color,
      sizeof(qp->grid_text_color));

  qp->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  ASSERT(qp->window);

  qp->border = (c)?(c->border):(app->op_border);
  gtk_window_set_decorated(GTK_WINDOW(qp->window), qp->border);
 

  qp->shape = (c)?0:(app->op_shape);
  qp->x11_draw = (c)?(c->x11_draw):(app->op_x11_draw);

  gtk_container_set_border_width(GTK_CONTAINER(qp->window), 0);

#ifdef NO_BIG_WIN_COPY
  gtk_widget_set_events(qp->window,     /* for window-state-event */
      gtk_widget_get_events(qp->window) | GDK_STRUCTURE_MASK);
#endif

  ++main_window_create_count;
  ++(app->main_window_count);
  ASSERT(app->main_window_count > 0);

  qp->window_num = main_window_create_count;
  qp_win_set_window_title(qp);

  //gtk_widget_set_events(qp->window, gtk_widget_get_events(qp->window));

  if(app->root_window_width < 1)
    qp_get_root_window_size();

  {
    int width, height;
    width = (c)?(c->width):(app->op_geometry.width);
    height = (c)?(c->height):(app->op_geometry.height);
    gtk_window_set_default_size(GTK_WINDOW(qp->window), width, height);
  }

  if(app->op_geometry.x != INT_MAX && app->op_geometry.y != INT_MAX && !c)
  {
    int x, y;
    x = app->op_geometry.x;
    y = app->op_geometry.y;

    if(x >= app->root_window_width)
      x = app->root_window_width - 1;
    else if(x <= -app->root_window_width && x != INT_MIN)
      x = -app->root_window_width - 1;

    if(y >= app->root_window_height)
      y = app->root_window_height - 1;
    else if(y <= -app->root_window_height && y != INT_MIN)
      y = -app->root_window_height - 1;


    if(x == INT_MIN) /* like example: --geometry=600x600-0+0 */
      x = app->root_window_width - app->op_geometry.width;
    else if(x < 0)
      x = app->root_window_width - app->op_geometry.width + x;
  
    if(y == INT_MIN) /* like example: --geometry=600x600+0-0 */
      y = app->root_window_height - app->op_geometry.height;
    else if(y < 0)
      y = app->root_window_height - app->op_geometry.height + y;

    DEBUG("move WINDOW to (%d,%d)\n", x, y);

    gtk_window_move(GTK_WINDOW(qp->window), x, y);
  }

  gtk_window_set_icon(GTK_WINDOW(qp->window),
		      pixbuf = gdk_pixbuf_new_from_xpm_data(quickplot));
  
  g_object_unref(G_OBJECT(pixbuf));

  accelGroup = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(qp->window), accelGroup);


  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
  ASSERT(vbox);

  gtk_container_add(GTK_CONTAINER(qp->window), vbox);
  {
    /*********************************************************************
     *                           top menu bar
     *********************************************************************/
    GtkWidget *menubar;

    qp->menubar =
    menubar = gtk_menu_bar_new();
    gtk_menu_bar_set_pack_direction(GTK_MENU_BAR(menubar),
        GTK_PACK_DIRECTION_LTR);

    if(app->is_globel_menu == 3)
        // set to 0 or 1
        app->is_globel_menu = _shell_shows_menubar(menubar);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    
    {
      GtkWidget *menu;
      /*******************************************************************
       *                   File menu
       *******************************************************************/
      qp->file_menu =
      menu = create_menu(menubar, accelGroup, "File");
      create_menu_item(menu, "_Open File ...", NULL, GTK_STOCK_OPEN,
          GDK_KEY_O, TRUE, cb_open_file, qp, TRUE);
      create_menu_item(menu, "_New Graph Tab", NULL, GTK_STOCK_NEW,
          GDK_KEY_N, TRUE, cb_new_graph_tab, qp, TRUE);
      create_menu_item(menu, "New _Window (Empty)", imgNewWindow, NULL,
          GDK_KEY_W, TRUE, cb_new_window, NULL, TRUE);
      qp->copy_window_menu_item =
      create_menu_item(menu, "_Copy Window", imgCopyWindow, NULL,
          GDK_KEY_C, TRUE, cb_copy_window, qp,
          (app->op_maximize && !c)?FALSE:TRUE);
      qp->delete_window_menu_item =
      create_menu_item(menu, "_Delete Window", imgDeleteWindow, NULL,
          GDK_KEY_D, TRUE, cb_delete_window, qp, app->main_window_count != 1);
      if(app->main_window_count == 2)
      {
        /* We must make the other qp->delete_window_menu_item
         * have the correct sensitive state. */
        struct qp_win *oqp;
        ASSERT(qp_sllist_length(app->qps) > 0);
        for(oqp=qp_sllist_begin(app->qps);
          oqp; oqp=qp_sllist_next(app->qps))
          if(oqp->window)
          {
            gtk_widget_set_sensitive(oqp->delete_window_menu_item, TRUE);
            break;
          }
          ASSERT(oqp);
      }
      create_menu_item(menu, "Save PNG _Image File", imgSaveImage, NULL,
          GDK_KEY_I, TRUE, cb_save_png_image_file, qp, TRUE);
      create_menu_item_seperator(menu);
      create_menu_item(menu, "_Quit", NULL, GTK_STOCK_QUIT,
          GDK_KEY_Q, TRUE, cb_quit, qp->window, TRUE);
      create_menu_item_seperator(menu);
      create_menu_item_seperator(menu);
      {
        struct qp_source *s;
        for(s=(struct qp_source*)qp_sllist_begin(app->sources);
            s;s=(struct qp_source*)qp_sllist_next(app->sources))
          add_source_buffer_remove_menu(qp, s);
      }

      /*******************************************************************
       *                   View menu
       *******************************************************************/
      menu = create_menu(menubar, accelGroup, "View");
      if(!app->is_globel_menu)
        qp->view_menubar =
          create_check_menu_item(menu, "_Menu Bar", GDK_KEY_M,
            (c)?(c->menubar):(app->op_menubar), cb_view_menubar, qp);
      else
        qp->view_menubar = NULL;
      qp->view_buttonbar =
      create_check_menu_item(menu, "_Button Bar", GDK_KEY_B,
          (c)?(c->buttonbar):(app->op_buttons), cb_view_buttonbar, qp);
      qp->view_graph_tabs =
      create_check_menu_item(menu, "Graph _Tabs", GDK_KEY_T,
          (c)?(c->tabs):(app->op_tabs), cb_view_graph_tabs, qp);
      qp->view_statusbar =
      create_check_menu_item(menu, "_Status Bar", GDK_KEY_S,
          (c)?(c->statusbar):(app->op_statusbar), cb_view_statusbar, qp);

      create_menu_item_seperator(menu);

      qp->view_cairo_draw =
        create_check_menu_item(menu, "D_raw with Cairo", GDK_KEY_R,
          !qp->x11_draw, cb_view_cairo_draw, qp);
      qp->view_border =
        create_check_menu_item(menu, "Window Bord_er", GDK_KEY_E,
          qp->border, cb_view_border, qp);
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(qp->view_border), qp->border);
      qp->view_fullscreen =
      create_check_menu_item(menu, "_Full Screen", GDK_KEY_F,
          (c)?0:(app->op_maximize == 2), cb_view_fullscreen, qp);
      qp->view_shape =
      create_check_menu_item(menu, "Shape _X11 Extension", GDK_KEY_X,
          qp->shape, cb_view_shape, qp);
      create_menu_item(menu, "_zoom Out", NULL, NULL,
          0, TRUE, cb_zoom_out, qp, TRUE);
      create_menu_item(menu, "_Zoom Out All", NULL, NULL,
          GDK_KEY_Z, TRUE, cb_zoom_out_all, qp, TRUE);
      
      create_menu_item_seperator(menu);
     
      qp->view_graph_detail =
      create_check_menu_item(menu, "_Graph Details", GDK_KEY_G,
          FALSE, cb_view_graph_detail, qp);
      /*******************************************************************
       *                   Help menu
       *******************************************************************/
      menu = create_menu(menubar, accelGroup, "Help");
      create_menu_item(menu, "_About", NULL, GTK_STOCK_ABOUT,
          GDK_KEY_A, TRUE, cb_about, NULL, TRUE);
      create_menu_item(menu, "_Help", NULL, GTK_STOCK_HELP,
          GDK_KEY_H, TRUE, cb_help, NULL, TRUE);
    }
    if((c)?(c->menubar):(app->op_menubar))
      gtk_widget_show(menubar);  

    /*******************************************************************
     *                   Button Bar
     *******************************************************************/
    qp->buttonbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_set_homogeneous(GTK_BOX(qp->buttonbar), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), qp->buttonbar, FALSE, FALSE, 0);
    
    create_button(qp->buttonbar, accelGroup, "_Open File ...",
        GDK_KEY_O, cb_open_file, qp);
    create_button(qp->buttonbar, accelGroup, "_New Graph Tab ...",
        GDK_KEY_N, cb_new_graph_tab, qp);
    qp->button_graph_detail = 
      create_button(qp->buttonbar, accelGroup, "_Graph Details",
        GDK_KEY_G, cb_graph_detail_show_hide, qp);
     create_button(qp->buttonbar, accelGroup, "Save PNG _Image ...",
        GDK_KEY_I, cb_save_png_image_file, qp);

    if((c)?(c->buttonbar):(app->op_buttons))
	gtk_widget_show(qp->buttonbar);

    /*******************************************************************
     *                   Graph Tabs
     *******************************************************************/
    qp->notebook = gtk_notebook_new();
    
    gtk_notebook_set_show_border(GTK_NOTEBOOK(qp->notebook), FALSE);

    gtk_box_pack_start(GTK_BOX(vbox), qp->notebook, TRUE, TRUE, 0);
    g_object_set(G_OBJECT(qp->notebook), "scrollable", TRUE, NULL);
      
    if((c)?(!c->tabs):(!app->op_tabs))
      gtk_notebook_set_show_tabs(GTK_NOTEBOOK(qp->notebook), FALSE);

    /* We need at least one graph tab to start with. */
    qp->current_graph = qp_graph_create(qp, NULL);

    gtk_widget_show(qp->notebook);

    /*******************************************************************
     *                   Status Bar
     *******************************************************************/
    qp->statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_set_homogeneous(GTK_BOX(qp->statusbar), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), qp->statusbar, FALSE, FALSE, 0);

    qp->status_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(qp->statusbar), qp->status_entry,
        TRUE, TRUE, 0);
    {
      PangoFontDescription *pfd;
      pfd = pango_font_description_from_string("Monospace Bold 11");
      if(pfd)
      {
        gtk_widget_override_font(qp->status_entry, pfd);
        pango_font_description_free(pfd);
      }
    }
    gtk_entry_set_text(GTK_ENTRY(qp->status_entry), "status bar");
    // This "editable" does not work.
    //g_object_set_property(G_OBJECT(qp->status_entry), "editable", FALSE);
    gtk_widget_show(qp->status_entry);

    if((c)?(c->statusbar):(app->op_statusbar))
	gtk_widget_show(qp->statusbar);
    
    /*******************************************************************
     *******************************************************************/
  }

  g_signal_connect(G_OBJECT(qp->window), "key-press-event",
      G_CALLBACK(ecb_key_press), qp);

  g_signal_connect(G_OBJECT(qp->window), "key-release-event",
      G_CALLBACK(ecb_key_release), qp);

#ifdef NO_BIG_WIN_COPY
  g_signal_connect(G_OBJECT(qp->window), "window-state-event",
      G_CALLBACK(ecb_window_state), qp);
#endif

  g_signal_connect(G_OBJECT(qp->window), "delete_event",
      G_CALLBACK(ecb_close), qp);

  g_signal_connect(G_OBJECT(qp->notebook), "switch-page",
      G_CALLBACK(cb_switch_page), NULL);


 
  if(!app->grabCursor)
  {
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_new_from_xpm_data(imgGrabCursor);
    ASSERT(pixbuf);
    app->grabCursor = gdk_cursor_new_from_pixbuf(
        gdk_display_get_default(), pixbuf, 12, 16);
    ASSERT(app->grabCursor);
    
    pixbuf = gdk_pixbuf_new_from_xpm_data(imgHoldCursor);
    ASSERT(pixbuf);
    app->holdCursor = gdk_cursor_new_from_pixbuf(
        gdk_display_get_default(), pixbuf, 12, 16);
    ASSERT(app->holdCursor);
    
    //app->grabCursor = gdk_cursor_new(GDK_HAND2);
    //app->holdCursor = gdk_cursor_new(GDK_HAND1);
    
    app->pickCursor = gdk_cursor_new(GDK_CROSSHAIR);
    app->zoomCursor = gdk_cursor_new(GDK_DRAFT_LARGE);
    app->waitCursor = gdk_cursor_new(GDK_WATCH);
  }

  qp->last_shape_region = NULL;
 

  if(app->op_maximize == 1 && !c)
  {
    gtk_window_maximize(GTK_WINDOW(qp->window));
  }
  else if(app->op_maximize == 2 && !c)
  {
    gtk_window_fullscreen(GTK_WINDOW(qp->window));
  }

  gtk_widget_show(vbox);
  gtk_widget_show(qp->window);

  /* Setup/draw the plots in the tabs */
  qp->init_front_page_num = -1;
  g_idle_add_full(G_PRIORITY_LOW + 10, qp_startup_idle_callback, qp, NULL);
  ++qp->ref_count;

  return qp; /* success */
}


qp_win_t qp_win_create(void)
{
  return _qp_win_create(NULL);
}


struct qp_win *qp_win_copy_create(struct qp_win *old_qp)
{
  struct qp_win *qp = NULL;
  struct qp_graph *gr, *old_gr;
  int width, height;
  struct qp_win_config config;

  gtk_window_get_size(GTK_WINDOW(old_qp->window), &width, &height);

  config.border = old_qp->border;
  config.x11_draw = old_qp->x11_draw;
  config.width = width;
  config.height = height;
  if(!app->is_globel_menu)
    config.menubar = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(old_qp->view_menubar));
  else
    config.menubar = 0;
  config.buttonbar = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(old_qp->view_buttonbar));
  config.tabs = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(old_qp->view_graph_tabs));
  config.statusbar = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(old_qp->view_statusbar));

  qp = _qp_win_create(&config);

  ASSERT(qp_sllist_length(old_qp->graphs) == 
      gtk_notebook_get_n_pages(GTK_NOTEBOOK(old_qp->notebook)));

  /* Copy all the graphs */
  old_gr = qp_sllist_begin(old_qp->graphs);
  gr = qp_sllist_first(qp->graphs);
  ASSERT(gr);
  qp_graph_copy(gr, old_gr);

  for(old_gr=qp_sllist_next(old_qp->graphs);
      old_gr; old_gr=qp_sllist_next(old_qp->graphs))
  {
    gr = qp_graph_create(qp, old_gr->name);
    qp_graph_copy(gr, old_gr);
  }

  gtk_notebook_set_current_page(GTK_NOTEBOOK(qp->notebook),
    gtk_notebook_get_current_page(GTK_NOTEBOOK(old_qp->notebook)));

  qp->pointer_x = old_qp->pointer_x;
  qp->pointer_y = old_qp->pointer_y;

  

  return qp;
}

