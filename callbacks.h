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

#ifdef CB
#error "Oh shit: CB() is defined already"
#endif
#ifdef ECB
#error "Oh shit: ECB() is defined already"
#endif


#define CB(X)     extern void cb_##X(GtkWidget *w, gpointer data)
#define ECB(X)  extern gboolean ecb_##X(GtkWidget *w,\
                      GdkEvent *event, gpointer data)

extern int _cairo_draw_ignore_event;


ECB(close);
CB(quit);
ECB(key_release);
ECB(key_press);
CB(zoom_out);
CB(zoom_out_all);
CB(view_menubar);
CB(view_graph_tabs);
CB(view_buttonbar);
CB(view_statusbar);
CB(view_border);
//#undef NO_BIG_WIN_COPY
/* Define NO_BIG_WIN_COPY to have the "copy window" disabled when the window
 * is in fullscreen or maximized.  */
#define NO_BIG_WIN_COPY
#ifdef NO_BIG_WIN_COPY
extern
gboolean ecb_window_state(GtkWidget *widget, GdkEventWindowState *event,
    struct qp_win *qp);
#endif
CB(view_fullscreen);
CB(view_cairo_draw);
CB(view_shape);
CB(close_tab);
CB(delete_window);
extern gboolean ecb_graph_draw(GtkWidget *w, cairo_t *cr, gpointer data);
CB(help);
CB(about);
ECB(graph_configure);
ECB(graph_scroll);
gboolean cb_switch_page(GtkNotebook *notebook, GtkWidget *page,
    guint page_num, gpointer data);
ECB(graph_button_press);
ECB(graph_button_release);
ECB(graph_pointer_motion);
CB(open_file);
CB(remove_source);
CB(new_window);
CB(copy_window);
CB(new_graph_tab);
CB(save_png_image_file);


// graph_details.c
CB(graph_detail_show_hide);
CB(view_graph_detail);

#undef ECB
#undef CB

