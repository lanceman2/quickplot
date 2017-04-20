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


/* This is just to process commands on the server side.
 * The program quickplot is a server to shells and the
 * program quickplot can also be a shell. */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>


#include <X11/Xlib.h>

#include <gtk/gtk.h>

#include "quickplot.h"

#include "shell.h"
#include "config.h"
#include "qp.h"
#include "debug.h"
#include "list.h"
#include "spew.h"
#include "term_color.h"
#include "shell_common.h"
#include "callbacks.h"
#include "channel.h"
#include "utils.h"
#include "shell_get_set_values.h"
#include "zoom.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


/* TODO: clearly this straight follow approach could be
 * better optimized.   For one thing strcmp() looks at the
 * whole string and calculates a total diff, when we just need
 * to know at the first character that differs.  The command
 * strings could be in an ordered tree and/or etc ...
 *
 * Need to look into flex and bison???*/

/* returns 1 if it can keep running
 * returns 0 if the qp_shell is destroyed */
int do_server_commands(size_t argc, char **argv, struct qp_shell *sh)
{
  FILE *out = sh->file_out;

  ASSERT(argc > 0);
  ASSERT(argv[0]);
  ASSERT(default_qp);
  ASSERT(default_qp->current_graph);

  if(argc == 1)
  {
    /********************************/
    /*   single argument commands   */
    /********************************/

    if(!strcmp(argv[0], "app"))
    {
      struct command *c;
      fprintf(out,
          " You have the following app default value settings.  They are not necessarily\n"
          " the values for any particular window, graph or plot.  Setting them at the\n"
          " app level will set the parameter for all existing windows, graphs and plots.\n");
      if(qp_sllist_length(app->qps) == 1)
        fprintf(out, " There is 1 window in this app.\n");
      else
        fprintf(out, " There are %zu windows in this app.\n", qp_sllist_length(app->qps));
      PrintParValDescHeader(out);
      
      for(c = app_commands; c->name; ++c)
        PrintValueDesc(out, c->name, app_get_value(c->name), "open", c->doc);
      for(c = window_commands; c->name; ++c)
        if(c->propagrate)
          PrintValueDesc(out, c->name, app_window_get_value(c->name), "window", c->doc);
      for(c = graph_commands; c->name; ++c)
        if(c->propagrate && strcmp(c->name, "draw"))
          PrintValueDesc(out, c->name, app_graph_get_value(c->name), "graph", c->doc);
      for(c = plot_commands; c->name; ++c)
        if(c->propagrate)
          PrintValueDesc(out, c->name, app_plot_get_value(c->name), "plot", c->doc);
    }
    else if(!strcmp(argv[0], "channels") || !strcmp(argv[0], "open") || !strcmp(argv[0], "close"))
    {
      struct qp_source *s;
      size_t count = 0;
      fprintf(out,
          "    FileName        Number      Name\n"
          " ----------------   ------ ------------------\n");
      for(s=qp_sllist_begin(app->sources);s;s=qp_sllist_next(app->sources))
      {
        size_t i;
        fprintf(out,"  %s\n", s->name);
        for(i=0;i<s->num_channels;++i)
        {
          char text[128];
          fprintf(out,
              "                   %s   %s\n", space_number(count++),
              qp_source_get_label(s, i, text, 128));
        }
      }
    }
    else if(!strcmp(argv[0], "exit"))
    {
      qp_shell_destroy(sh);
      /* We cannot do anything more since
       * the sh is now destroyed. */
      return 0;
    }
    else if(!strcmp(argv[0],"graph"))
    {
      if(default_qp && default_qp->current_graph)
      {
        struct command *c;
        struct qp_graph *gr;
        fprintf(out,
          "  The current window %d  graph %d with tab name \"%s\"\n"
          "  There are %zu plot(s) in this graph.\n"
          "\n"
          "  Possible graph numbers in window %d are:\n",
          default_qp->window_num, default_qp->current_graph->graph_num,
          default_qp->current_graph->name,
          qp_sllist_length(default_qp->current_graph->plots), default_qp->window_num);
        for(gr=qp_sllist_begin(default_qp->graphs);gr;gr=qp_sllist_next(default_qp->graphs))
          fprintf(out, "           %d \"%s\"\n", gr->graph_num, gr->name);
        gr = default_qp->current_graph;

        PrintParValDescHeader(out);

        for(c = graph_commands; c->name; ++c)
          if(strcmp(c->name, "create") && strcmp(c->name, "destroy") &&
              strcmp(c->name, "list") && strcmp(c->name, "draw") &&
              strcmp(c->name, "save") && strcmp(c->name, "zoom"))
            PrintValueDesc(out, c->name, graph_get_value(gr, c->name), "graph", c->doc);
        for(c = plot_commands; c->name; ++c)
          if(c->propagrate)
            PrintValueDesc(out, c->name, graph_plot_get_value(gr, c->name), "plot", c->doc);
      }
      else if(default_qp)
        fprintf(out, "There are no graphs in the current window %d\n", default_qp->window_num);
      else
        fprintf(out, "There are no graphs, or windows\n");
    }
    else if(!strcmp(argv[0],"plot"))
    {
      if(default_qp && default_qp->current_graph && default_qp->current_graph->current_plot)
      {
        struct command *c;
        struct qp_graph *gr;
        struct qp_plot *p;
        gr = default_qp->current_graph;
        fprintf(out,
          "  The current window %d graph %d \"%s\"  plot %d \"%s\"\n"
          "  There are %zu plots in the current graph.\n"
          "\n"
          "  Possible plot numbers in graph %d \"%s\" are:\n",
          default_qp->window_num, gr->graph_num, gr->name,
          gr->current_plot->plot_num,
          gr->current_plot->name,
          qp_sllist_length(gr->plots), gr->graph_num, gr->name);
        for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
          fprintf(out, "            %d \"%s\"\n", p->plot_num, p->name);
        p = gr->current_plot;

        PrintParValDescHeader(out);

        for(c = plot_commands; c->name; ++c)
          if(strcmp(c->name, "create") && strcmp(c->name, "destroy") && strcmp(c->name, "list"))
            PrintValueDesc(out, c->name, plot_get_value(p, c->name), "plot", c->doc);
      }
      else if(default_qp && default_qp->current_graph)
        fprintf(out, "There are no plots in the current graph %d of the current window %d\n",
            default_qp->current_graph->graph_num, default_qp->window_num);
      else if(default_qp)
        fprintf(out, "There are no plots or graph in the current window %d\n",
            default_qp->window_num);
      else
        fprintf(out, "There are no plots, graphs, or windows\n");
    }
    else if(!strcmp(argv[0], "quit"))
    {
      qp_shell_destroy(sh);
      cb_quit(NULL, NULL);
      /* The app may still be running.
       * See qp_main.c */
      /* We cannot do anything more since
       * the sh is now destroyed. */
      return 0;
    }
    else if(!strcmp(argv[0], "start"))
    {
      /* This just writes back.  This is just like a ping
       * to get the pipes flowing.  This seems to be needed.
       * This is not intended to be a user command, but
       * they may use it. */
    }
    else if(!strcmp(argv[0],"window"))
    {
      if(default_qp)
      {
        struct command *c;
        struct qp_win *qp;
        fprintf(out,
          "  The current window number is %d\n"
          "\n"
          "  Possible window numbers are:",
          default_qp->window_num);
        for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
          fprintf(out, " %d", qp->window_num);
        putc('\n', out);
        qp = default_qp;
        PrintParValDescHeader(out);

        for(c = window_commands; c->name; ++c)
          if(strcmp(c->name, "create") && strcmp(c->name, "destroy") &&
              strcmp(c->name, "list") && strcmp(c->name, "copy"))
            PrintValueDesc(out, c->name, window_get_value(qp, c->name), "window", c->doc);
        for(c = graph_commands; c->name; ++c)
          if(c->propagrate && strcmp(c->name, "draw"))
            PrintValueDesc(out, c->name, window_graph_get_value(qp, c->name), "graph", c->doc);
        for(c = plot_commands; c->name; ++c)
          if(c->propagrate)
            PrintValueDesc(out, c->name, window_plot_get_value(qp, c->name), "plot", c->doc);
      }
      else
        fprintf(out, "There are no windows\n");
    }
    else
      BadCommand("Unknown command", argc, argv, out);
  
    /********************************/
  }
  else if(argc > 1)
  {
    /********************************/
    /*   multi argument commands    */
    /********************************/

    if(!strcmp(argv[0], "app"))
    {
      if(!strcmp(argv[1], "default_graph"))
      {
        if(argc == 2)
          fprintf(out, "%s\n", BoolValue(app->op_default_graph));
        else if(argc == 3)
        {
          app->op_default_graph = GetBool(argv[2], 0);
          fprintf(out, "%s\n", BoolValue(app->op_default_graph));
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "geometry"))
      {
        if(argc == 2)
          fprintf(out, "%s\n", app_get_value("geometry"));
        else if(argc == 3)
        {
          if(GetGeometry(argv[2], &(app->op_geometry.x), &(app->op_geometry.y),
              &(app->op_geometry.width), &(app->op_geometry.height),
              &(app->op_maximize)))
              fprintf(out, "Invalid geometry\n");
          else
            fprintf(out, "%s\n", app_get_value("geometry"));
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "label_separator"))
      {
        if(argc == 2)
          fprintf(out, "%s\n", app_get_value("label_separator"));
        else if(argc == 3)
        {
          if(app->op_label_separator)
            free(app->op_label_separator);
          app->op_label_separator = qp_strdup(argv[2]);
          fprintf(out, "%s\n", app_get_value("label_separator"));
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "labels"))
      {
        if(argc == 2)
          fprintf(out, "%s\n", app_get_value("labels"));
        else if(argc == 3)
        {
          app->op_labels = GetBool(argv[2], 0);
          fprintf(out, "%s\n", app_get_value("labels"));
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "linear_channel"))
      {
        if(argc == 2)
          fprintf(out, "%s\n", app_get_value("linear_channel"));
        else if(argc == 3 || argc == 4)
        {
          if(app->op_linear_channel)
          {
            qp_channel_destroy(app->op_linear_channel);
            app->op_linear_channel = NULL;
          }

          if(!(argv[2][0] >= 'A' && argv[2][0] <= 'Z') &&
              !(argv[2][0] >= 'a' && argv[2][0] <= 'z'))
          {
            double start = 0.0, step = 1.0;
            char *s, *end;
            char *arg;
            arg = ArgsToString(&argv[2]);
            s = arg;
            start = strtod(s, &end);
            if(s == end || start == NAN || start == -NAN ||
                start == HUGE_VAL || start == -HUGE_VAL)
              goto linear_channel_err;
            s = end;
            while(isspace(*s)) ++s;
            if(*s)
            {
              step = strtod(s, &end);
              if(s == end || step == NAN || step == -NAN ||
                step == HUGE_VAL || step == -HUGE_VAL)
              goto linear_channel_err;
            }

            free(arg);
            app->op_linear_channel = qp_channel_linear_create(start, step);
          }
          else if(GetBool(argv[2], 0))
            app->op_linear_channel = qp_channel_linear_create(0, 1);

          goto linear_channel_end;

      linear_channel_err:
          BadCommand2(out, argc, argv);
      linear_channel_end:
          fprintf(out, "%s\n", app_get_value("linear_channel"));
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "new_window"))
      {
        if(argc == 3)
          app->op_new_window = GetBool(argv[2], 0);
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_get_value("new_window"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "number_of_plots"))
      {
        if(argc == 3)
          GetInt(out, argv[2], 1, 10000, &app->op_number_of_plots);
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_get_value("number_of_plots"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "skip_lines"))
      {
        if(argc == 3)
          GetSizeT(out, argv[2], 0, INT_MAX, &app->op_skip_lines);
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_get_value("skip_lines"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "border"))
      {
        if(argc == 3)
        {
          struct qp_win *qp;
          app->op_border = GetBool(argv[2], 0);
          for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
            if((app->op_border &&
                !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_border))) ||
              (!app->op_border &&
                  gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_border))))
                gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_border));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_window_get_value("border"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "buttons"))
      {
        if(argc == 3)
        {
          struct qp_win *qp;
          app->op_buttons = GetBool(argv[2], 0);
          for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
          {
            int buttons;
            buttons = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_buttonbar));
            if((app->op_buttons && !buttons) || (!app->op_buttons && buttons))
                gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_buttonbar));
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_window_get_value("buttons"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "menubar"))
      {
        if(argc == 3 && !app->is_globel_menu)
        {
          struct qp_win *qp;
          app->op_menubar = GetBool(argv[2], 0);
          for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
          {
            int menubar;
            menubar = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_menubar));
            if((app->op_menubar && !menubar) || (!app->op_menubar && menubar))
              gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_menubar));
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_window_get_value("menubar"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "shape"))
      {
        if(argc == 3)
        {
          struct qp_win *qp;
          app->op_shape = GetBool(argv[2], 0);
          for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
            if((app->op_shape && !qp->shape) || (!app->op_shape && qp->shape))
              gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_shape));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_window_get_value("shape"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "statusbar"))
      {
        if(argc == 3)
        {
          struct qp_win *qp;
          app->op_statusbar = GetBool(argv[2], 0);
          for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
          {
            int statusbar;
            statusbar = !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_statusbar));
            if((app->op_statusbar && !statusbar) || (!app->op_statusbar && statusbar))
                gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_statusbar));
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_window_get_value("statusbar"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "tabs"))
      {
        if(argc == 3)
        {
          struct qp_win *qp;
          app->op_tabs = GetBool(argv[2], 0);
          for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
          {
            int tabs;
            tabs = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_graph_tabs));
            if((app->op_tabs && !tabs) || (!app->op_tabs && tabs))
                gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_graph_tabs));
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_window_get_value("tabs"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "bg"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&app->op_background_color, &argv[2]))
            APP_FORALLGRAPHS(\
                memcpy(&qp->background_color, &app->op_background_color, \
                        sizeof(qp->background_color)),\
                memcpy(&gr->background_color, &app->op_background_color, \
                        sizeof(gr->background_color)));
        }
        fprintf(out, "%s\n", app_graph_get_value("bg"));
      }
      else if(!strcmp(argv[1], "grid"))
      {
        if(argc == 3)
        {
          app->op_grid = GetBool(argv[2], 0);
          APP_FORALLGRAPHS(qp->grid = app->op_grid, gr->show_grid = app->op_grid);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_graph_get_value("grid"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_font"))
      {
        if(argc >= 3)
        {
          if(app->op_grid_font)
            free(app->op_grid_font);
          app->op_grid_font = ArgsToString(&argv[2]);
          APP_FORALLGRAPHS(\
              if(qp->grid_font) free(qp->grid_font);\
                qp->grid_font = qp_strdup(app->op_grid_font),\
              if(gr->grid_font) free(gr->grid_font);\
              gr->grid_font = qp_strdup(app->op_grid_font);\
              if(gr->pangolayout)\
                qp_graph_set_grid_font(gr)\
          );
        }
        fprintf(out, "%s\n", app_graph_get_value("grid_font"));
      }
      else if(!strcmp(argv[1], "grid_line_color"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&app->op_grid_line_color, &argv[2]))
          {
             APP_FORALLGRAPHS(\
                memcpy(&qp->grid_line_color, &app->op_grid_line_color, \
                        sizeof(qp->grid_line_color)),\
                memcpy(&gr->grid_line_color, &app->op_grid_line_color, \
                        sizeof(gr->grid_line_color)));
          }
        }
        fprintf(out, "%s\n", app_graph_get_value("grid_line_color"));
      }
      else if(!strcmp(argv[1], "grid_line_width"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 1000, &app->op_grid_line_width))
          APP_FORALLGRAPHS(qp->grid_line_width = app->op_grid_line_width,\
              gr->grid_line_width = app->op_grid_line_width);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_graph_get_value("grid_line_width"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_numbers"))
      {
        if(argc == 3)
        {
          app->op_grid_numbers = GetBool(argv[2], 0);
          APP_FORALLGRAPHS(qp->grid_numbers = app->op_grid_numbers,\
              gr->grid_numbers = qp->grid_numbers);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_graph_get_value("grid_numbers"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_text_color"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&app->op_grid_text_color, &argv[2]))
          {
             APP_FORALLGRAPHS(\
                memcpy(&qp->grid_text_color, &app->op_grid_text_color, \
                        sizeof(qp->grid_text_color)),\
                memcpy(&gr->grid_text_color, &app->op_grid_text_color, \
                        sizeof(gr->grid_text_color)));
          }
        }
        fprintf(out, "%s\n", app_graph_get_value("grid_text_color"));
      }
      else if(!strcmp(argv[1], "grid_x_space"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 100000, &app->op_grid_x_space))
            APP_FORALLGRAPHS(qp->grid_x_space = app->op_grid_x_space,\
                gr->grid_x_space = app->op_grid_x_space);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_graph_get_value("grid_x_space"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_y_space"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 100000, &app->op_grid_y_space))
            APP_FORALLGRAPHS(qp->grid_y_space = app->op_grid_y_space,\
                gr->grid_y_space = app->op_grid_y_space);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_graph_get_value("grid_y_space"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "cairo"))
      {
        if(argc == 3)
        {
          /* TODO: Clean this shit up */
          app->op_x11_draw = GetBool(argv[2], 0)?0:1;
          APP_FORALLGRAPHS(\
              _cairo_draw_ignore_event = 1;\
              if(qp->x11_draw != app->op_x11_draw) \
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(qp->view_cairo_draw),\
                  (app->op_x11_draw)?FALSE:TRUE);\
              _cairo_draw_ignore_event = 0;\
              qp->x11_draw = app->op_x11_draw,\
              if((qp->x11_draw && !gr->x11) || (!qp->x11_draw && gr->x11))\
                graph_switch_draw_mode(gr)\
              );
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_graph_get_value("cairo"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "draw"))
      {
        if(argc == 2)
        {
          struct qp_win *qp;
          for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
          {
            gtk_widget_queue_draw(qp->current_graph->drawing_area);
            qp->current_graph->pixbuf_needs_draw = 1;
            gdk_window_set_cursor(gtk_widget_get_window(qp->window), app->waitCursor);
          }
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "same_x_scale"))
      {
        if(argc == 3)
        {
          app->op_same_x_scale = GetBool(argv[2], 0);
          APP_FORALLGRAPHS(qp->same_x_scale = app->op_same_x_scale,\
              if(gr->same_x_scale != app->op_same_x_scale)\
                qp_graph_same_x_scale(gr, app->op_same_x_scale));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_graph_get_value("same_x_scale"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "same_y_scale"))
      {
        if(argc == 3)
        {
          app->op_same_y_scale = GetBool(argv[2], 0);
          APP_FORALLGRAPHS(qp->same_y_scale = app->op_same_y_scale,\
              if(gr->same_y_scale != app->op_same_y_scale)\
                qp_graph_same_y_scale(gr, app->op_same_y_scale));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_graph_get_value("same_y_scale"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "gaps"))
      {
        if(argc == 3)
        {
          app->op_gaps = GetBool(argv[2], 0);
          APP_FORALLPLOTS(qp->gaps = app->op_gaps,\
              gr->gaps = qp->gaps,\
              if(p->gaps != gr->gaps)\
                gr->pixbuf_needs_draw = 1;\
              p->gaps = gr->gaps);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_plot_get_value("gaps"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "line_width"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 1000, &app->op_line_width))
            APP_FORALLPLOTS(qp->line_width = app->op_line_width,\
                gr->line_width = qp->line_width,\
                if(p->line_width != gr->line_width)\
                  gr->pixbuf_needs_draw = 1;\
                p->line_width = gr->line_width);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_plot_get_value("line_width"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "lines"))
      {
        if(argc == 3)
        {
          app->op_lines = GetBool(argv[2], 0);
          APP_FORALLPLOTS(qp->lines = app->op_lines,\
              gr->lines = qp->lines,\
              if(p->lines != gr->lines)\
                gr->pixbuf_needs_draw = 1;\
              p->lines = gr->lines);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_plot_get_value("lines"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "point_size"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 1000, &app->op_point_size))
            APP_FORALLPLOTS(qp->point_size = app->op_point_size,\
                gr->point_size = qp->point_size,\
                if(p->point_size != gr->point_size)\
                  gr->pixbuf_needs_draw = 1;\
                p->point_size = gr->point_size);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_plot_get_value("point_size"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "points"))
      {
        if(argc == 3)
        {
          app->op_points = GetBool(argv[2], 0);
          APP_FORALLPLOTS(qp->points = app->op_points,\
              gr->points = qp->points,\
              if(p->points != gr->points)\
                gr->pixbuf_needs_draw = 1;\
              p->points = gr->points);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", app_plot_get_value("points"));
        else
          BadCommand2(out, argc, argv);
      }
      else
        BadCommand("Unknown command", argc, argv, out);
    }
    else if(!strcmp(argv[0], "close"))
    {
      int i;
      for(i=1;i<argc;++i)
      {
        struct qp_source *s;
        for(s=qp_sllist_begin(app->sources);s;s=qp_sllist_next(app->sources))
          if(!strcmp(s->name, argv[i]))
          {
            qp_source_destroy(s);
            break;
          }
        if(s)
          fprintf(out, "removed source \"%s\"\n", argv[i]);
        else
          fprintf(out, "source named \"%s\" was not found\n", argv[i]);
      }
    }
    else if(!strcmp(argv[0], "graph"))
    {
      struct qp_graph *gr = NULL;

      if(argc == 2 && *argv[1] >= '0' && *argv[1] <= '9')
      {
        /* Set the current graph */
        char *end = NULL;
        int gr_num;
        gr_num = strtol(argv[1], &end, 10);
        if(end && *end == '\0')
        {
          for(gr=qp_sllist_begin(default_qp->graphs);gr;
              gr=qp_sllist_next(default_qp->graphs))
            if(gr_num == gr->graph_num)
            {
              if(gr != default_qp->current_graph)
                gtk_notebook_set_current_page(GTK_NOTEBOOK(default_qp->notebook),
                  gtk_notebook_page_num(GTK_NOTEBOOK(default_qp->notebook),
                    gr->drawing_area));
              break;
            }
          if(!gr)
          {
            fprintf(out, "possible graph numbers are:");
              for(gr=qp_sllist_begin(default_qp->graphs);gr;
              gr=qp_sllist_next(default_qp->graphs))
                fprintf(out, " %d \"%s\"", gr->graph_num, gr->name);
            putc('\n', out);
          }
          else
            fprintf(out, "%d \"%s\"\n", gr->graph_num, gr->name);
        }
        return End_Reply(sh, argc, argv);
      }
      if(!strcmp(argv[1], "create"))
      {
        if(argc == 2)
          qp_graph_create(NULL, NULL);
        else
        {
          ssize_t *x = NULL, *y = NULL;
          size_t num;
          if(GetPlotNums(&argv[2], &x, &y, &num))
          {
            if(qp_win_graph(NULL, x, y, num, NULL))
              fprintf(out, "failed\n");
            else
              fprintf(out, "made graph number %d with %zu plots\n",
                  default_qp->current_graph->graph_num,
                  qp_sllist_length(default_qp->current_graph->plots));
            free(x);
            free(y);
          }
          else
            BadCommand2(out, argc, argv);
        }
        return End_Reply(sh, argc, argv);
      }

      if(default_qp && default_qp->current_graph)
        gr = default_qp->current_graph;
      else
      {
        /* There should always be a default graph */
        VASSERT(0, "there is not current graph");
        fprintf(out, "there is no graph\n");
        return End_Reply(sh, argc, argv);
      }

      if(!strcmp(argv[1], "bg"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&gr->background_color, &argv[2]))
          {
            if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
            gr->pixbuf_needs_draw = 1;
          }
        }
        fprintf(out, "%s\n", graph_get_value(gr, "bg"));
      }
      else if(argc == 2 && !strcmp(argv[1], "destroy"))
      {
        qp_graph_destroy(gr);
      }
      else if(!strcmp(argv[1], "grid"))
      {
        if(argc == 3)
        {
          gr->show_grid = GetBool(argv[2], 0);
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_get_value(gr, "grid"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_font"))
      {
        if(argc >= 3)
        {
          if(gr->grid_font)
            free(gr->grid_font);
          gr->grid_font = ArgsToString(&argv[2]);
          if(gr->pangolayout)
            qp_graph_set_grid_font(gr);
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        fprintf(out, "%s\n", graph_get_value(gr, "grid_font"));
      }
      else if(!strcmp(argv[1], "grid_line_color"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&gr->grid_line_color, &argv[2]))
          {
            if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
            gr->pixbuf_needs_draw = 1;
          }
        }
        fprintf(out, "%s\n", graph_get_value(gr, "grid_line_color"));
      }
      else if(!strcmp(argv[1], "grid_line_width"))
      {
        if(argc == 3)
        {
          GetInt(out, argv[2], 1, 1000, &gr->grid_line_width);
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_get_value(gr, "grid_line_width"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_numbers"))
      {
        if(argc == 3)
        {
          gr->grid_numbers = GetBool(argv[2], 0);
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_get_value(gr, "grid_numbers"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_text_color"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&gr->grid_text_color, &argv[2]))
          {
            if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
            gr->pixbuf_needs_draw = 1;
          }
        }
        fprintf(out, "%s\n", graph_get_value(gr, "grid_text_color"));
      }
      else if(!strcmp(argv[1], "grid_x_space"))
      {
        if(argc == 3)
        {
          GetInt(out, argv[2], 1, 100000, &gr->grid_x_space);
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_get_value(gr, "grid_x_space"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_y_space"))
      {
        if(argc == 3)
        {
          GetInt(out, argv[2], 1, 100000, &gr->grid_y_space);
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_get_value(gr, "grid_y_space"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "cairo"))
      {
        if(argc == 3)
        {
          int x11_draw;
          x11_draw = GetBool(argv[2], 0)?0:1;
          if((x11_draw && !gr->x11) || (!x11_draw && gr->x11))
          {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(default_qp->view_cairo_draw),
                x11_draw?FALSE:TRUE);
            graph_switch_draw_mode(gr);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_get_value(gr, "cairo"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "draw"))
      {
        if(argc == 2)
        {
          gtk_widget_queue_draw(gr->drawing_area);
          gr->pixbuf_needs_draw = 1;
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "list"))
      {
        if(argc == 2)
        {
          for(gr=qp_sllist_begin(default_qp->graphs);gr;
              gr=qp_sllist_next(default_qp->graphs))
            fprintf(out, "%d \"%s\"\n", gr->graph_num, gr->name);

          gr=qp_sllist_begin(default_qp->graphs);
          fprintf(out, "%d", gr->graph_num);
          for(gr=qp_sllist_next(default_qp->graphs);gr;
              gr=qp_sllist_next(default_qp->graphs))
            fprintf(out, " %d", gr->graph_num);
          putc('\n', out);
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "same_x_scale"))
      {
        if(argc == 3)
        {
          int new_same_x_scale;
          new_same_x_scale = GetBool(argv[2], 0);
          if(gr->same_x_scale != new_same_x_scale)
            qp_graph_same_x_scale(gr, new_same_x_scale);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_get_value(gr, "same_x_scale"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "same_y_scale"))
      {
        if(argc == 3)
        {
          int new_same_y_scale;
          new_same_y_scale = GetBool(argv[2], 0);
          if(gr->same_y_scale != new_same_y_scale)
            qp_graph_same_y_scale(gr, new_same_y_scale);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_get_value(gr, "same_y_scale"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "gaps"))
      {
        if(argc == 3)
        {
          struct qp_plot *p;
          gr->gaps = GetBool(argv[2], 0);
          for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
            p->gaps = gr->gaps;
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_plot_get_value(gr, "gaps"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "line_width"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 1000, &gr->line_width))
          {
            struct qp_plot *p;
            for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
              p->line_width = gr->line_width;
            if(default_qp->graph_detail)
              qp_win_graph_detail_init(default_qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_plot_get_value(gr, "line_width"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "lines"))
      {
        if(argc == 3)
        {
          struct qp_plot *p;
          gr->lines = GetBool(argv[2], 0);
          for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
            p->lines = gr->lines;
          if(default_qp->graph_detail)
            qp_win_graph_detail_init(default_qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_plot_get_value(gr, "lines"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "point_size"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 1000, &gr->point_size))
          {
            struct qp_plot *p;
            for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
              p->point_size = gr->point_size;
            if(default_qp->graph_detail)
              qp_win_graph_detail_init(default_qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_plot_get_value(gr, "point_size"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "points"))
      {
        if(argc == 3)
        {
          struct qp_plot *p;
          gr->points = GetBool(argv[2], 0);
          for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
            p->points = gr->points;
          if(default_qp->graph_detail)
            qp_win_graph_detail_init(default_qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", graph_plot_get_value(gr, "points"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "save"))
      {
        if(argc == 3)
        {
          int spew_level;
          spew_level = qp_set_spew_level(5); /* spew off */
          if(qp_win_save_png(default_qp, gr, argv[2]))
            fprintf(out, "failed to save file \"%s\" errno=%d:%s\n",
                argv[2], errno, strerror(errno));
          else
            fprintf(out, "saved file %s\n", argv[2]);
          qp_set_spew_level(spew_level); /* spew reset */
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "zoom"))
      {
        if(argc == 3 && !strcmp(argv[2], "out"))
          qp_graph_zoom_out(gr, 0);
        else if(argc == 3 && !strcmp(argv[2], "all"))
          qp_graph_zoom_out(gr, 1);
        else if(argc >= 3)
        {
          double val[4];
          int i = 0;
          char *s, *end;
          val[3] = HUGE_VAL;
          s = ArgsToString(&argv[2]);
          while(*s)
          {
            double d;
            while(isspace(*s))
              ++s;
            if(!*s) break;
            d = strtod(s, &end);
            if(end == s || d == HUGE_VAL || d == -HUGE_VAL)
              break;
            val[i++] = d;
            if(i == 4) break;
            s = end;
          }
          if(val[3] == HUGE_VAL ||
              ABSVAL(val[0]) <= SMALL_DOUBLE ||
              ABSVAL(val[1]) <= SMALL_DOUBLE)
            goto zoom_err;

          /* See ecb_graph_button_release() callbacks.c */

          if(gr->grab_x || gr->grab_y)
          {
            qp_zoom_push_shift(&(gr->z), - gr->grab_x/gr->xscale,
                                         - gr->grab_y/gr->yscale);
            ++gr->zoom_level;
            gr->grab_x = 0;
            gr->grab_y = 0;
          }
          //fprintf(out, "w=%g h=%g x=%g y=%g\n", val[0], val[1], val[2], val[3]);
          /* We zoom with XScale=val[0] normalized to 1 about the center
           *              XShift in the scaled normalized coordinates
           *              YScale=val[2] normalized to 1 about the center
           *              YShift in the scaled normalized coordinates 
           *  We figure that will be intuitive. */
          qp_zoom_push(&(gr->z), val[0], val[2]+0.5-0.5*val[0],
                                 val[1], val[3]+0.5-0.5*val[1]); 
          ++gr->zoom_level;
          gdk_window_set_cursor(gtk_widget_get_window(gr->qp->window),
            app->waitCursor);
          gr->pixbuf_needs_draw = 1;
          qp_win_set_status(gr->qp);
          gtk_widget_queue_draw(gr->drawing_area);
        }
        fprintf(out, "level %d%s\n", gr->zoom_level,
            (gr->grab_x || gr->grab_y)?" with shift":"");
        return End_Reply(sh, argc, argv);

      zoom_err:
          BadCommand2(out, argc, argv);
          fprintf(out, " Try:\ngraph zoom W H X Y\n For example:\n"
              "graph zoom .5 .5 0 0\n to zoom to half size\n and:\ngraph zoom 1 1 0 .25\n"
              " to move the graph up 1/4 of the view area\n");
          return End_Reply(sh, argc, argv);

      }
      else
        BadCommand("Unknown command", argc, argv, out);
    }
    else if(!strcmp(argv[0], "open"))
    {
      /* open files */
      char **filename;
      for(filename = &argv[1]; *filename; ++filename)
      {
        struct qp_source *s;
        s = qp_source_create(*filename, QP_TYPE_UNKNOWN);
        if(s && app->op_default_graph)
          qp_win_graph_default_source(NULL, s, NULL);
      }
    }
    else if(!strcmp(argv[0], "plot"))
    {
      struct qp_plot *p = NULL;
      struct qp_graph *gr = NULL;

      ASSERT(default_qp);
      ASSERT(default_qp->current_graph);

      gr = default_qp->current_graph;

      if(!strcmp(argv[1], "create") && (argc == 3 || argc == 4))
      {
        size_t x, y;

        if(argc == 3)
        {
          char *end, *s;
          s = argv[2];
          while(*s && (*s < '0' || *s > '9'))
            ++s;
          if(!*s)
            goto invalid;
          x = strtoul(s, &end, 10);
          if(!*end)
            goto invalid;
          s = end;
          while(*s && (*s < '0' || *s > '9'))
            ++s;
          if(!*s)
            goto invalid;
          y = strtoul(s, &end, 10);
          if(!*end)
            goto plot;

          invalid:
            fprintf(out, "invalid channel numbers\n");
            return End_Reply(sh, argc, argv);
        }
        else
          if(!GetSizeT(out, argv[2], 1, INT_MAX, &x) ||
             !GetSizeT(out, argv[3], 1, INT_MAX, &y))
            return End_Reply(sh, argc, argv);

        plot:
        {
          /* Find the x and y channels */
          struct qp_source *s, *xs = NULL, *ys = NULL;
          for(s=qp_sllist_begin(app->sources);s;s=qp_sllist_next(app->sources))
          {
            if(!xs && x < s->num_channels)
              xs = s;
            else if(!xs)
              x -= s->num_channels;
            if(!ys && y < s->num_channels)
              ys = s;
            else if(!ys)
              y -= s->num_channels;
          }
          if(!xs || !ys)
          {
            fprintf(out, "channel numbers to high");
            return End_Reply(sh, argc, argv);
          }
          qp_graph_add_plot(gr, xs, ys, x, y);
          gr->pixbuf_needs_draw = 1;
          return End_Reply(sh, argc, argv);
        }
      }

      if(!default_qp->current_graph->current_plot)
      {
        fprintf(out, "there is no current plot: try plot create\n");
        return End_Reply(sh, argc, argv);
      }

      p = default_qp->current_graph->current_plot;

      if(argc == 2 && argv[1][0] >= '0' && argv[1][0] <= '9')
      {
        int plot_num;
        if(GetInt(out, argv[1], 1, INT_MAX, &plot_num))
        {
          struct qp_plot *p;
          for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
            if(p->plot_num == plot_num)
              break;
          if(p)
          {
            gr->current_plot = p;
            fprintf(out, "%d\n", plot_num);
          }
          else
            fprintf(out, "plot numbered %d not found\n", plot_num);
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "destroy"))
      {
        if(argc == 2)
        {
          qp_graph_remove_plot(gr, p);
          gr->pixbuf_needs_draw = 1;
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "gaps"))
      {
        if(argc == 3)
        {
          p->gaps = GetBool(argv[2], 0);
          gr->pixbuf_needs_draw = 1;
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", plot_get_value(p, "gaps"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "line_color"))
      {
        if(argc >= 3)
        {
          if(ParseColor((struct qp_colora *) &p->l, &argv[2]))
          {
            gr->pixbuf_needs_draw = 1;
            if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
          }
        }
        fprintf(out, "%s\n", plot_get_value(p, "line_color"));
      }
      else if(!strcmp(argv[1], "line_width"))
      {
        if(argc == 3)
        {
          if(GetDouble(out, argv[2], 1, 1000, &p->line_width))
          {
            gr->pixbuf_needs_draw = 1;
            if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", plot_get_value(p, "line_width"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "lines"))
      {
        if(argc == 3)
        {
          p->lines = GetBool(argv[2], 0);
          gr->pixbuf_needs_draw = 1;
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", plot_get_value(p, "lines"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "list") && argc == 2)
      {
        for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
          fprintf(out, "%d \"%s\"\n", p->plot_num, p->name);

        p=qp_sllist_begin(gr->plots);
        fprintf(out, "%d", p->plot_num);
        for(p=qp_sllist_next(gr->plots);p;p=qp_sllist_next(gr->plots))
          fprintf(out, " %d", p->plot_num);
        putc('\n', out);
      }
      else if(!strcmp(argv[1], "point_color"))
      {
        if(argc >= 3)
        {
          if(ParseColor((struct qp_colora *) &p->p, &argv[2]))
          {
            gr->pixbuf_needs_draw = 1;
            if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
          }
        }
        fprintf(out, "%s\n", plot_get_value(p, "point_color"));
      }
      else if(!strcmp(argv[1], "point_size"))
      {
        if(argc == 3)
        {
          if(GetDouble(out, argv[2], 1, 1000, &p->point_size))
          {
            gr->pixbuf_needs_draw = 1;
            if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", plot_get_value(p, "point_size"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "points"))
      {
        if(argc == 3)
        {
          p->points = GetBool(argv[2], 0);
          gr->pixbuf_needs_draw = 1;
          if(gr->qp->graph_detail) qp_win_graph_detail_init(gr->qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", plot_get_value(p, "points"));
        else
          BadCommand2(out, argc, argv);
      }
      else
        BadCommand("Unknown command", argc, argv, out);
    }
    else if(!strcmp(argv[0], "window"))
    {
      struct qp_win *qp;
      if(!strcmp(argv[1], "create"))
      {
        if(argc == 2)
          qp_win_create();
        else
          BadCommand2(out, argc, argv);
        return End_Reply(sh, argc, argv);
      }
      qp = default_qp;

      if(argc == 2 && argv[1][0] >= '0' && argv[1][0] <= '9')
      {
        int window_num;
        if(GetInt(out, argv[1], 1, 10000, &window_num))
        {
          for(qp=qp_sllist_begin(app->qps);qp;qp=qp_sllist_next(app->qps))
            if(window_num == qp->window_num)
              break;
          if(qp)
            default_qp = qp;
          else
            fprintf(out, "window number %d not found\n", window_num);
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "copy"))
      {
        if(argc == 2)
          qp_win_copy_create(qp);
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "destroy"))
      {
        if(argc == 2 && qp_sllist_length(app->qps) > 1)
          qp_win_destroy(qp);
        else if(argc == 2)
          fprintf(out, "there is only one window left, try quit\n");
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "border"))
      {
        if(argc == 3)
        {
          int border, current;
          border = GetBool(argv[2], 0);
          current = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_border));
          if((border && !current) || (!border && current))
            gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_border));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_get_value(qp, "border"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "buttons"))
      {
        if(argc == 3)
        {
          int buttons;
          buttons = GetBool(argv[2], 0);
          if((buttons &&
              !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_buttonbar))) ||
              (!buttons &&
              gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_buttonbar))))
            gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_buttonbar));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_get_value(qp, "buttons"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "fullscreen"))
      {
        if(argc == 3)
        {
          int fullscreen, current;
          fullscreen = GetBool(argv[2], 0);
          current = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_fullscreen));
          if((current && !fullscreen) || (!current && fullscreen))
            gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_fullscreen));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_get_value(qp, "fullscreen"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "geometry"))
      {
        if(argc == 3)
        {
          int x=-1, y=-1, w=-1, h=-1, maximize=0;
          if(!GetGeometry(argv[2], &x, &y, &w, &h, &maximize))
          {
            if(maximize)
            {
              gtk_window_maximize(GTK_WINDOW(qp->window));
              x = y = w = h = -1;
            }
            if(x != -1 && y != -1)
              gtk_window_move(GTK_WINDOW(qp->window), x, y);
            if(w != -1 && h != -1)
              gtk_window_resize(GTK_WINDOW(qp->window), w, h);

          }
          else
          {
            fprintf(out, "Invalid geometry\n");
            return End_Reply(sh, argc, argv);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_get_value(qp, "geometry"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "list"))
      {
        if(argc == 2)
        {
          struct qp_win *qp;
          qp=qp_sllist_begin(app->qps);
          if(qp)
            fprintf(out, "%d", qp->window_num);
          for(qp=qp_sllist_next(app->qps);qp;qp=qp_sllist_next(app->qps))
            fprintf(out, " %d", qp->window_num);
          putc('\n', out);
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "menubar"))
      {
        if(argc == 3 && !app->is_globel_menu)
        {
          int menubar, current;
          menubar = GetBool(argv[2], 0);
          current = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_menubar));
          if((!menubar && current) || (menubar && !current))
            gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_menubar));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_get_value(qp, "menubar"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "shape"))
      {
        if(argc == 3)
        {
          int shape;
          shape = GetBool(argv[2], 0);
          if((shape && !qp->shape) || (!shape && qp->shape))
            gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_shape));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_get_value(qp, "shape"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "statusbar"))
      {
        if(argc == 3)
        {
          int statusbar, current;
          statusbar = GetBool(argv[2], 0);
          current = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_statusbar));
          if((!statusbar && current) || (statusbar && !current))
            gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_statusbar));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_get_value(qp, "statusbar"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "tabs"))
      {
        if(argc == 3)
        {
          int tabs, current;
          tabs = GetBool(argv[2], 0);
          current = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(qp->view_graph_tabs));
          if((!tabs && current) || (tabs && !current))
            gtk_menu_item_activate(GTK_MENU_ITEM(qp->view_graph_tabs));
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_get_value(qp, "tabs"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "bg"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&qp->background_color, &argv[2]))
          {
            struct qp_graph *gr;
            for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
            {
              memcpy(&gr->background_color, &qp->background_color,
                  sizeof(gr->background_color));
              gr->pixbuf_needs_draw = 1;
            }
            if(qp->graph_detail) qp_win_graph_detail_init(qp);
          }
        }
        fprintf(out, "%s\n", window_graph_get_value(qp, "bg"));
      }
      else if(!strcmp(argv[1], "cairo"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          qp->x11_draw = GetBool(argv[2], 0)?0:1;
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            if((!gr->x11 && qp->x11_draw) || (gr->x11 && !qp->x11_draw))
            {
              if(gr == qp->current_graph)
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(qp->view_cairo_draw),
                  (qp->x11_draw)?FALSE:TRUE);
              else
                graph_switch_draw_mode(gr);
            }
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_graph_get_value(qp, "cairo"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "draw"))
      {
        if(argc == 2)
        {
          gtk_widget_queue_draw(qp->current_graph->drawing_area);
          qp->current_graph->pixbuf_needs_draw = 1;
          gdk_window_set_cursor(gtk_widget_get_window(qp->window), app->waitCursor);
        }
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          qp->grid = GetBool(argv[2], 0);
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            if(gr == qp->current_graph)
            {
              if(gr->show_grid != qp->grid)
              {
                gr->show_grid = qp->grid;
                gr->pixbuf_needs_draw = 1;
                if(qp->graph_detail) qp_win_graph_detail_init(qp);
              }
              continue;
            }
            gr->show_grid = qp->grid;
            gr->pixbuf_needs_draw = 1;
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_graph_get_value(qp, "grid"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_font"))
      {
        if(argc >= 3)
        {
          struct qp_graph *gr;
          if(qp->grid_font) free(qp->grid_font);
          qp->grid_font = ArgsToString(&argv[2]);
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            if(gr->grid_font) free(gr->grid_font);
            gr->grid_font = qp_strdup(qp->grid_font);
            if(gr->pangolayout) qp_graph_set_grid_font(gr);
            gr->pixbuf_needs_draw = 1;
          }
          if(qp->graph_detail) qp_win_graph_detail_init(qp);
        }
        fprintf(out, "%s\n", window_graph_get_value(qp, "grid_font"));
      }
      else if(!strcmp(argv[1], "grid_line_color"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&qp->grid_line_color, &argv[2]))
          {
            struct qp_graph *gr;
            for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
            {
              memcpy(&gr->grid_line_color, &qp->grid_line_color,
                        sizeof(gr->grid_line_color));
              gr->pixbuf_needs_draw = 1;
            }
            if(qp->graph_detail) qp_win_graph_detail_init(qp);
          }
        }
        fprintf(out, "%s\n", window_graph_get_value(qp, "grid_line_color"));
      }
      else if(!strcmp(argv[1], "grid_line_width"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          if(GetInt(out, argv[2], 1, 1000, &qp->grid_line_width))
          {
            for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
            {
              gr->grid_line_width = qp->grid_line_width;
              gr->pixbuf_needs_draw = 1;
            }
            if(qp->graph_detail) qp_win_graph_detail_init(qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_graph_get_value(qp, "grid_line_width"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_numbers"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          qp->grid_numbers = GetBool(argv[2], 0);
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            if(gr == qp->current_graph)
            {
              if(gr->grid_numbers != qp->grid_numbers)
              {
                gr->grid_numbers = qp->grid_numbers;
                gr->pixbuf_needs_draw = 1;
                if(qp->graph_detail)
                  qp_win_graph_detail_init(qp);
              }
              continue;
            }
            gr->grid_numbers = qp->grid_numbers;
            gr->pixbuf_needs_draw = 1;
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_graph_get_value(qp, "grid_numbers"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_text_color"))
      {
        if(argc >= 3)
        {
          if(ParseColor(&qp->grid_text_color, &argv[2]))
          {
            struct qp_graph *gr;
            for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
            {
              memcpy(&gr->grid_text_color, &qp->grid_text_color,
                        sizeof(gr->grid_text_color));
              gr->pixbuf_needs_draw = 1;
            }
            if(qp->graph_detail) qp_win_graph_detail_init(qp);
          }
        }
        fprintf(out, "%s\n", window_graph_get_value(qp, "grid_text_color"));
      }
      else if(!strcmp(argv[1], "grid_x_space"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          if(GetInt(out, argv[2], 1, 1000, &qp->grid_x_space))
          {
            for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
            {
              gr->grid_x_space = qp->grid_x_space;
              gr->pixbuf_needs_draw = 1;
            }
            if(qp->graph_detail) qp_win_graph_detail_init(qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_graph_get_value(qp, "grid_x_space"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "grid_y_space"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          if(GetInt(out, argv[2], 1, 1000, &qp->grid_y_space))
          {
            for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
            {
              gr->grid_y_space = qp->grid_y_space;
              gr->pixbuf_needs_draw = 1;
            }
            if(qp->graph_detail) qp_win_graph_detail_init(qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_graph_get_value(qp, "grid_y_space"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "same_x_scale"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          qp->same_x_scale = GetBool(argv[2], 0);
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            if(gr->same_x_scale != qp->same_x_scale)
            {
              qp_graph_same_x_scale(gr, qp->same_x_scale);
              if(gr == qp->current_graph && qp->graph_detail)
                qp_win_graph_detail_init(qp);
            }
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_graph_get_value(qp, "same_x_scale"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "same_y_scale"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          qp->same_y_scale = GetBool(argv[2], 0);
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            if(gr->same_y_scale != qp->same_y_scale)
            {
              qp_graph_same_y_scale(gr, qp->same_y_scale);
              if(gr == qp->current_graph && qp->graph_detail)
                qp_win_graph_detail_init(qp);
            }
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_graph_get_value(qp, "same_y_scale"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "gaps"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          struct qp_plot *p;
          qp->gaps = GetBool(argv[2], 0);
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            gr->gaps = qp->gaps;
            for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
              p->gaps = gr->gaps;
            gr->pixbuf_needs_draw = 1;
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_plot_get_value(qp, "gaps"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "line_width"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 1000, &qp->line_width))
          {
            struct qp_graph *gr;
            struct qp_plot *p;
            for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
            {
              gr->line_width = qp->line_width;
              for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
                p->line_width = gr->line_width;
              gr->pixbuf_needs_draw = 1;
            }
            if(qp->graph_detail)
              qp_win_graph_detail_init(qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_plot_get_value(qp, "line_width"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "lines"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          struct qp_plot *p;
          qp->lines = GetBool(argv[2], 0);
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            gr->lines = qp->lines;
            for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
              p->lines = gr->lines;
            gr->pixbuf_needs_draw = 1;
          }
          if(qp->graph_detail)
            qp_win_graph_detail_init(qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_plot_get_value(qp, "lines"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "point_size"))
      {
        if(argc == 3)
        {
          if(GetInt(out, argv[2], 1, 1000, &qp->point_size))
          {
            struct qp_graph *gr;
            struct qp_plot *p;
            for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
            {
              gr->point_size = qp->point_size;
              for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
                p->point_size = gr->point_size;
              gr->pixbuf_needs_draw = 1;
            }
            if(qp->graph_detail)
              qp_win_graph_detail_init(qp);
          }
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_plot_get_value(qp, "point_size"));
        else
          BadCommand2(out, argc, argv);
      }
      else if(!strcmp(argv[1], "points"))
      {
        if(argc == 3)
        {
          struct qp_graph *gr;
          struct qp_plot *p;
          qp->points = GetBool(argv[2], 0);
          for(gr=qp_sllist_begin(qp->graphs);gr;gr=qp_sllist_next(qp->graphs))
          {
            gr->points = qp->points;
            for(p=qp_sllist_begin(gr->plots);p;p=qp_sllist_next(gr->plots))
              p->points = gr->points;
            gr->pixbuf_needs_draw = 1;
          }
          if(qp->graph_detail)
            qp_win_graph_detail_init(qp);
        }
        if(argc == 2 || argc == 3)
          fprintf(out, "%s\n", window_plot_get_value(qp, "points"));
        else
          BadCommand2(out, argc, argv);
      }
      else
        BadCommand("Unknown window command", argc, argv, out);
    }

    else
      BadCommand("Unknown command", argc, argv, out);

    /********************************/
  }

  return End_Reply(sh, argc, argv);
}

