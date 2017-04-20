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

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>


#include "config.h"
#include "debug.h"
#include "list.h"
#include "spew.h"
#include "term_color.h"
#include "shell_common.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


#  ifdef HAVE_READLINE_HISTORY
#define HISTORY_FILENAME_POSTFIX  "/.quickplot_history"
char *history_filename = NULL;
int added_history_length = 0;
#  endif

#ifdef HAVE_READLINE_HISTORY
void Save_history(void)
{
  if(history_filename && added_history_length)
  {
    if(added_history_length < 0 || added_history_length > 2000)
      added_history_length = 2000;
    append_history(added_history_length, history_filename);
    history_truncate_file(history_filename, 2000);
    added_history_length = 0;
  }
}

static inline
int Has_Space(const char *s)
{
  for(;*s;++s)
    if(isspace(*s))
      return 1;
  return 0;
}

static inline
void Add_history(int argc, char **argv, size_t maxlen)
{
  int i;
  size_t len;
  char *buf = qp_malloc(maxlen + 1);
  /* We write packed version of user_line
   * into history */
  len = sprintf(buf, "%s", argv[0]);
  for(i=1; i<argc; ++i)
  {
    if(Has_Space(argv[i]))
      len += sprintf(&buf[len], " '%s'", argv[i]);
    else
      len += sprintf(&buf[len], " %s", argv[i]);
  }
  add_history(buf);
  ++added_history_length;
  free(buf);
}
#endif


static inline
void change_input(const char *filename, FILE *file_in, FILE *spew_out)
{
  FILE *new_file_in;
  errno = 0;
  new_file_in = fopen(filename, "r");
  if(new_file_in)
  {
    errno = 0;
    if(dup2(fileno(new_file_in), fileno(file_in)) == -1)
    {
      fprintf(spew_out,
          "command: input %s failed dup2(%d,%d) failed"
          " errno=%d: %s\n", filename, fileno(file_in),
        fileno(new_file_in), errno, strerror(errno));
      fflush(spew_out);
    }
    else
    {
      fclose(new_file_in);
      fprintf(spew_out,
          "Switched to reading input from \"%s\"\n",
          filename);
      fflush(spew_out);
    }
  }
  else
  {
    fprintf(spew_out,
        "command: input %s failed fopen(\"%s\",\"r\") failed"
        " errno=%d: %s\n", filename, filename, errno,
        strerror(errno));
    fflush(spew_out);
  }
}


struct command commands[] =
{
  { "app",      "PAR ...",  "get and set parameters app wide"       , 0},
  { "channels", 0,          "list channels"                         , 0},
  { "close",    "FILE ...", "close (delete) a buffer"               , 0},
  { "exit",     0,          "exit the shell"                        , 0},
  { "graph",    "PAR ...",  "get and set graph parameters"          , 0},
  { "help",     0,          "print this help"                       , 0},
  { "help",     "app",      "print app help"                        , 0},
  { "help",     "graph",    "print graph help"                      , 0},
  { "help",     "plot",     "print plot help"                       , 0},
  { "help",     "window",   "print window help"                     , 0},
#ifdef HAVE_READLINE_HISTORY
  { "history",  "NUM",      "list the command history"              , 0},
#endif
  { "input",    "IFILE",    "set shell to read input from IFILE"    , 0},
  { "open",     "FILE ...", "open and read data from FILEs"         , 0},
  { "plot",     "PAR ...",  "get and set plot pararmeters"          , 0},
  { "quit",     0,          "quit quickplot and exit the shell"     , 0},
  { "window",   "PAR ...",  "get and set window parameters"         , 0},
  { "?",        0,          "same as help"                          , 0},
#ifdef HAVE_READLINE_HISTORY
  { "!",        "NUM|COMM", "run command from history"              , 0},
#endif
  { 0,          0,          0                                       , 0}
};


struct command help_commands[] =
{
  { "app",      0,           0     , 0 },
  { "graph",    0,           0     , 0 },
  { "window",   0,           0     , 0 },
  { "plot",     0,           0     , 0 },
  { 0,          0,           0     , 0 }
};

struct command app_commands[] =
{
  { "default_graph",   "BOOL",       "create default graphs after"        , 0 },
  { "geometry",        "GEO",        "geometry of next window created"    , 0 },
  { "label_separator", "STR",        "read labels separator"              , 0 },
  { "labels",          "BOOL",       "read labels"                        , 0 },
  { "linear_channel",  "START STOP", "prepend a linear channel"           , 0 },
  { "new_window",      "BOOL",       "make new window for new graphs"     , 0 },
  { "number_of_plots", "NUM",        "number plots in default_graph"      , 0 },
  { "skip_lines",      "NUM",        "skip first NUM lines reading"       , 0 },
  { 0,                 0,           0                                     , 0 }
};

struct command window_commands[] =
{
  { "border",     "BOOL",      "show border"                                   , 1 },
  { "buttons",    "BOOL",      "show button bar"                               , 1 },
  { "copy",       0,           "copy the current window"                       , 0 },
  { "create",     0,           "create new window, set to current"             , 0 },
  { "destroy",    0,           "destroy the window"                            , 0 },
  { "fullscreen", "BOOL",      "make window fullscreen"                        , 0 },
  { "geometry",   "GEO",       "geometry of the window"                        , 0 },
  { "list",       0,           "list all window NUMs"                          , 0 },
  { "menubar",    "BOOL",      "show menu bar"                                 , 1 },
  { "shape",      "BOOL",      "draw graphs with X11 shape ext"                , 1 },
  { "statusbar",  "BOOL",      "show status bar"                               , 1 },
  { "tabs",       "BOOL",      "show graph tabs"                               , 1 },
  { 0,            0,           0                                               , 0 }
};

struct command graph_commands[] =
{
  { "bg",              "COLOR",     "graph background color"                      , 1 },
  { "cairo",           "BOOL",      "draw with Cairo API or use X11"              , 1 },
  { "create",          "X Y ...",   "create new graph, set to current"            , 0 },
  { "destroy",         0,           "destroy the current graph"                   , 0 },
  { "draw",            0,           "redraw the graph"                            , 1 },
  { "grid",            "BOOL",      "show the grid"                               , 1 },
  { "grid_font",       "STR",       "font of the grid numbers"                    , 1 },
  { "grid_line_color", "STR",       "grid line color"                             , 1 },
  { "grid_line_width", "NUM",       "grid line width"                             , 1 },
  { "grid_numbers",    "BOOL",      "show gird numbers"                           , 1 },
  { "grid_text_color", "COLOR",     "grid text color"                             , 1 },
  { "grid_x_space",    "NUM",       "maximum x grid spacing"                      , 1 },
  { "grid_y_space",    "NUM",       "maximum y grid spacing"                      , 1 },
  { "list",            0,           "list all graph NUMs in window"               , 0 },
  { "same_x_scale",    "BOOL",      "show plots on same x scale"                  , 1 },
  { "same_y_scale",    "BOOL",      "show plots on same y scale"                  , 1 },
  { "save",            "FILE",      "save a PNG image"                            , 0 },
  { "zoom",            "W H X Y",   "zoom normalized box, out or all"             , 0 },
  { 0,                 0,           0                                             , 0 }
};

struct command plot_commands[] =
{
  { "create",      "X Y",       "plot channels X Y, set current plot"       , 0 },
  { "destroy",     0,           "destroy the current plot"                  , 0 },
  { "gaps",        "BOOL",      "draw gaps for nan points"                  , 1 },
  { "line_color",  "COLOR",     "line color"                                , 0 },
  { "line_width",  "NUM",       "line width"                                , 1 },
  { "lines",       "BOOL",      "show lines"                                , 1 },
  { "list",        0,           "list all plot NUMs in current graph"       , 0 },
  { "point_color", "COLOR",     "point color"                               , 0 },
  { "point_size",  "NUM",       "point size"                                , 1 },
  { "points",      "BOOL",      "show points"                               , 1 },
  { 0,             0,           0                                           , 0 }
};

static
struct command* sub_commands[5];


#define MAX_NAME_LEN  128

static size_t max_name_len = 0;


static inline
void print_help(FILE *file)
{
  struct command *c;
  fprintf(file,
      " ****************************************************************\n"
      "                    Quickplot Shell Help\n"
      " ****************************************************************\n"
      "\n"
      "  In the Quickplot Shell all representative arguments, that are in\n"
      "  UPPER CASE, are optional.  If that argument is not given the\n"
      "  action will be to list possible or current values.  Consider\n"
      "  using ReadLine Tab Completion and History Up and Down Arrows.\n"
      "\n"
      "  The following commands are available:\n"
      "\n");
  for(c = commands; c->name; ++c)
  {
    size_t space_len;
    char space[MAX_NAME_LEN];
    char args[MAX_NAME_LEN];
    *args = '\0';
    space_len = max_name_len - strlen(c->name) + 2;
    if(c->args)
    {
      space_len -= (1 + strlen(c->args));
      snprintf(args, MAX_NAME_LEN,  " %s", c->args);
    }
    snprintf(space, space_len, "%s", "                            ");
    fprintf(file, "   %s%s %s%s\n", c->name, args, space, c->doc);
  }
  putc('\n', file);
}

static inline
void print_wgp_doc(FILE *file, char *sub_com)
{
  char spaces[64];
  ASSERT(strlen(sub_com)+1 < 64);
  snprintf(spaces, strlen(sub_com)+9, "                                  ");

  fprintf(file,
        "   %s%s%s NUM  set the %s you are acting on to NUM.  If NUM\n"
        "%s is not given this will list the current parameters\n"
        "%s of the current %s",
        tur, sub_com, trm, sub_com, spaces, spaces, sub_com);

  if(!strcmp(sub_com, "window"))
    fprintf(file,              ".\n\n");
  else if(!strcmp(sub_com, "graph"))
    fprintf(file,              " in the current window.\n\n");
  else if(!strcmp(sub_com, "plot"))
    fprintf(file,              " in the current graph.\n\n");
}

static inline
void print_node_help(FILE *file, char *node_title)
{
  struct command **scom;
  int child = 0;

  fprintf(file,
      " ****************************************************************\n"
      "                        %s help\n"
      " ****************************************************************\n"
      "\n",  node_title);

  if(!strcmp(node_title, "app"))
  {
    scom = sub_commands;
    fprintf(file,
      "  app commands set parameters for all plots in all graphs in all\n"
      "  windows.  If you need to set parameters for a specific window,\n"
      "  graph, or plot, use the window, graph, or plot commands.\n");
  }

  fprintf(file,
      "  The following %s commands are available:\n"
      "\n", node_title);

  if(!strcmp(node_title, "window"))
  {
    scom = &sub_commands[1];
    print_wgp_doc(file, "window");
  }
  else if(!strcmp(node_title, "graph"))
  {
    scom = &sub_commands[2];
    print_wgp_doc(file, "graph");
  }
  else if(!strcmp(node_title, "plot"))
  {
    scom = &sub_commands[3];
    print_wgp_doc(file, "plot");
  }

  for(;*scom;++scom)
  {
    struct command *com;
    size_t max_len = 0;
    for(com = *scom; com->name; ++com)
    {
      size_t nlen;
      if(child && !com->propagrate)
        continue;
      nlen = strlen(com->name) + 1;
      if(com->args)
        nlen += strlen(com->args) + 1;
      if(nlen > max_len)
        max_len = nlen;
    }
    if(!strcmp(node_title, "app"))
    {
      if(*scom == app_commands)
      {
        fprintf(file,
          "   %sapp%s    list all app parameter values\n\n",
          tur, trm);
        fprintf(file,
          "  When opening files with %sopen%s:\n\n",
          tur, trm);
      }
      else if(*scom == window_commands)
        fprintf(file, "  For all windows in the app:\n\n");
      else if(*scom == graph_commands)
        fprintf(file, "  For all graphs in the app:\n\n");
      else if(*scom == plot_commands)
        fprintf(file, "  For all plots in the app:\n\n");
    }
    else if(!strcmp(node_title, "window") && *scom == graph_commands)
      fprintf(file, "  For all graphs in the window:\n\n");
    else if(!strcmp(node_title, "window") && *scom == plot_commands)
      fprintf(file, "  For all plots in the window:\n\n");
    else if(!strcmp(node_title, "graph") && *scom == plot_commands)
      fprintf(file, "  For all plots in the graph:\n\n");

    for(com = *scom; com->name; ++com)
    {
      size_t space_len;
      char space[MAX_NAME_LEN];
      char args[MAX_NAME_LEN];
      if(child && !com->propagrate)
        continue;

      *args = '\0';
      space_len = max_len - strlen(com->name);
      if(com->args)
      {
        space_len -= (1 + strlen(com->args));
        snprintf(args, MAX_NAME_LEN,  " %s", com->args);
      }
      snprintf(space, space_len, "%s", "                            ");
      fprintf(file, "   %s%s%s %s%s%s%s  %s%s\n",
          tur, node_title, trm,
          blu, com->name, trm,
          args, space, com->doc);
    }
    putc('\n', file);
    child = 1;
  }
}

/* remove white space before and after
 * returns a pointer in string and modifies
 * the end of the string blanking white space */
static inline
char *strip_white_space(char *string)
{
  char *s, *t;
  for(s = string; isspace(*s); ++s);
  if(!*s) return s;
  t = s + strlen (s) - 1;
  while(t > s && isspace(*t)) --t;
  *++t = '\0';
  return s;
}

static
char *command_generator(const char *text, int state)
{
  static int list_index, len;
  char *name;

  /* If this is a new word to complete, initialize now.  This
     includes saving the length of TEXT for efficiency, and
     initializing the index variable to 0. */
  if(!state)
  {
    list_index = 0;
    len = strlen(text);
  }

  /* Return the next name which partially matches from the
     command list. */
  while((name = commands[list_index].name))
  {
    list_index++;
    if(strncmp(name, text, len) == 0)
      return qp_strdup(name);
  }

  if(list_index == 0)
    rl_attempted_completion_over = 1;

  /* If no names matched, then return NULL. */
  return NULL;
}

static char command[MAX_NAME_LEN];


static
char *sub_command_generator(const char *text, int state)
{
  static int len;
  static struct command *com = NULL;
  static int level = 0, child = 0;
  char *name;

  /* If this is a new word to complete, initialize now.  This
     includes saving the length of TEXT for efficiency, and
     initializing the index variable to 0. */
  if(!state || !com)
  {
    if(!strcmp(command, "help") || !strcmp(command, "?"))
      com = help_commands;
    else if(!strcmp(command, "graph"))
    {
      if(*text == '\0' || (*text >= '0' && *text <= '9'))
      {
        char *str;
        str = get_graph_nums(state);
        if(str)
          return str;
      }

      level = 'g';
      com = graph_commands;
    }
    else if(!strcmp(command, "app"))
    {
      level = 'a';
      com = app_commands;
    }
    else if(!strcmp(command, "window"))
    { 
      if(*text == '\0' || (*text >= '0' && *text <= '9'))
      {
        char *str;
        str = get_window_nums(state);
        if(str)
          return str;
      }

      level = 'w';
      com = window_commands;
    }
    else if(!strcmp(command, "plot"))
    {
      if(*text == '\0' || (*text >= '0' && *text <= '9'))
      {
        char *str;
        str = get_plot_nums(state);
        if(str)
          return str;
      }

      level = 'p';
      com = plot_commands;
    }
    else
      return NULL;
    len = strlen(text);
  }

  /* Return the next name which partially matches from the
     command list. */
  while((name = com->name))
  {
    int cmp;
    cmp = strncmp(name, text, len) == 0 && (!child || com->propagrate);
    ++com;

    if(cmp)
      return qp_strdup(name);
  }

  if(level == 'a')
  {
    child = 1;
    com = window_commands;
    level = 'w';
    return sub_command_generator(text, 1);
  }
  if(level == 'w')
  {
    /* graphs are children of windows */
    child = 1;
    com = graph_commands;
    level = 'g';
    return sub_command_generator(text, 1);
  }
  if(level == 'g')
  {
    /* plots are children of graphs */
    child = 1;
    com = plot_commands;
    level = 'p';
    return sub_command_generator(text, 1);
  }
  com = NULL;
  child = 0;
  rl_attempted_completion_over = 1;
  /* If no names matched, then return NULL. */
  return NULL;
}

/* set buf to the first word in rl_line_buffer */
static void find_command(char *buf)
{
  char *s, *e;
  size_t len;
  for(s = rl_line_buffer; isspace(*s); ++s);
  if(!*s)
  {
    buf[0] = '\0';
    return;
  }
  for(e = s; *e && !isspace(*e); ++e);
  len = e - s;
  if(len > MAX_NAME_LEN-1)
    len = MAX_NAME_LEN-1;
  strncpy(buf, s, len);
  buf[len] = '\0';
}

static
int has_sub_command(char *com)
{
  struct command *sub;
  if(!strcmp(com, "help") || !strcmp(com, "?"))
    return 1;
  for(sub = help_commands; sub->name; ++sub)
    if(!strcmp(com, sub->name))
      return 1;
  return 0;
}

static
int count_words(char *last_char)
{
  char *s;
  int count = 0;
  s = rl_line_buffer;
  while(*s)
  {
    for(; isspace(*s); ++s)
      *last_char = *s;
    if(*s) ++count;
    for(; *s && !isspace(*s); ++s)
      *last_char = *s;
  }
  return count;
}

static
char **qp_completion (const char *text, int start, int end)
{
  /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
  int words;
  char last_char = 0;
  words = count_words(&last_char);
  rl_attempted_completion_over = 0;

  if(words == 0 || (words == 1 && !isspace(last_char)))
    return rl_completion_matches(text, command_generator);

  find_command(command);

  if(!strcmp(command, "exit") || !strcmp(command, "quit"))
    /* We do not need command completion. */
    rl_attempted_completion_over = 1;
  else if(has_sub_command(command) &&
      (words == 1 || (words == 2 && !isspace(last_char))))
    return rl_completion_matches(text, sub_command_generator);
  else if(strcmp(command, "open")) // Not open
    rl_attempted_completion_over = 1;

  return NULL;
}


void qp_shell_initialize(int use_readline)
{
  struct command *c;

#ifdef HAVE_READLINE_HISTORY
  if(use_readline)
  {
    char *env;

    env = getenv("QUICKPLOT_HISTORY_FILE");
    if(env)
      history_filename = qp_strdup(env);
    else
    {
      env = getenv("HOME");
      if(env)
      {
        history_filename = qp_malloc(sizeof(char)*
            (strlen(env)+strlen(HISTORY_FILENAME_POSTFIX)+1));
        sprintf(history_filename, "%s" HISTORY_FILENAME_POSTFIX, env);
      }
    }

    if(history_filename)
      read_history(history_filename);
  }
#endif

  max_name_len = 0;
  for(c = commands; c->name; ++c)
  {
    size_t len;
    len = strlen(c->name);
    if(c->args)
      len += 1 + strlen(c->args);
    if(max_name_len < len)
      max_name_len = len;
  }

  sub_commands[0] = app_commands;
  sub_commands[1] = window_commands;
  sub_commands[2] = graph_commands;
  sub_commands[3] = plot_commands;
  sub_commands[4] = NULL;

  /* Check that coding assumption is good.
   * This check is not needed at non DEBUG time given that
   * the commands array is constant. */
  ASSERT(max_name_len < MAX_NAME_LEN);

#ifdef HAVE_LIBREADLINE
  if(use_readline)
  {
    /* Allow conditional parsing of the ~/.inputrc file. */
    rl_readline_name = "quickplot";

    /* Tell the completer that we want a crack first. */
    rl_attempted_completion_function = qp_completion;
  }
#endif
}



/* Returns 1 if there was a client_side_command
 * Returns 0 if there was no client_side_command found */

/* this does all client side commands except exit and quit. */
int process_client_side_commands(int *argc_, char ***argv_,
    FILE *in, FILE *out, size_t line_len, char **history_mem
#ifdef HAVE_LIBREADLINE
    , int no_readline, int *use_readline
#endif
    )
{
  int argc;
  char **argv;
  argc = *argc_;
  argv = *argv_;
  *history_mem = NULL;

#ifdef HAVE_READLINE_HISTORY
  /* We add to the history whether they are using
   * readline or not in case they switch to using
   * readline after not using it, by switching input
   * to a tty. */


#define DO_HISTORY()                                    \
  do {                                                  \
        char *new_line;                                 \
        *history_mem = new_line = qp_strdup(h->line);   \
        fprintf(out, "%s\n", new_line);                 \
        free(argv);                                     \
        line_len = strlen(new_line) + 1;                \
        argv = get_args(new_line, &argc);               \
        *argv_ = argv;                                  \
        *argc_ = argc;                                  \
  } while(0)


  if(argc > 0 && **argv == '!' &&
      ((argc == 1 && argv[0][1]) || argc > 1))
  {
    /* This block adds the !NUMBER  history stuff */
    char *num;
    char *endptr = NULL;
    int hist_num;
    if(argv[0][1])
      num = &argv[0][1];
    else
      num = argv[1];

    hist_num = strtol(num, &endptr, 10);
    if(*endptr == '\0')
    {
      HIST_ENTRY *h;
      if((argv[0][1] && argc > 1) || argc > 2)
      {
        /* We do not tolerate having extra arguments */
        fprintf(out, "Bad history command: ");
        spew_args(NULL, argc, argv, out);
        return 1;
      }

      h = history_get(hist_num);
      if(h)
      {
        DO_HISTORY();
      }
      else
      {
        fprintf(out, "Command number %d not found in history\n", hist_num);
        return 1;
      }
    }
    else if(argv[0][1] == '!' && argv[0][2] == '\0' && argc == 1)
    {
      HIST_ENTRY *h;
      h = current_history();
      if(h)
      {
        DO_HISTORY();
      }
      else
      {
        fprintf(out, "No history found for: ");
        spew_args(NULL, argc, argv, out);
      }
    }
    else
    {
      /* search history for match */
      char *line;
      size_t len;
      HIST_ENTRY *h;
      int i;
      /* fix is it !argv0 argv1 or ! argv0 argv1 */
      if(argv[0][1])
      {
        argv[0] = &argv[0][1];
      }
      else
      {
        int j;
        for(j=0;argv[j];++j)
          argv[j] = argv[j+1];
        --argc;
      }
      line = qp_malloc(line_len + 1);
      len = sprintf(line, "%s", argv[0]);
      for(i=1; i<argc; ++i)
      {
        if(Has_Space(argv[i]))
          len += sprintf(&line[len], " '%s'", argv[i]);
        else
          len += sprintf(&line[len], " %s", argv[i]);
      }
      if(history_search_prefix(line, -1) == 0 && (h = current_history()))
      {
        DO_HISTORY();
        history_set_pos(history_length);
      }
      else
      {
        fprintf(out, "Cannot find history matching to: ");
        spew_args(NULL, argc, argv, out);
        free(line);
        return 1;
      }
    }
  }

#undef DO_HISTORY


  Add_history(argc, argv, line_len);

#endif


  if(argc >= 1)
  {
    if(!strcmp("help", *argv) ||
       !strcmp("?", *argv)) // help
    {
      if(argc == 1)
        print_help(out);
      else if(argc == 2 && (!strcmp(argv[1], "app") ||
          !strcmp(argv[1], "window") ||
          !strcmp(argv[1], "graph") ||
          !strcmp(argv[1], "plot")))
         print_node_help(out, argv[1]);
      else
      {
        fprintf(out, "No help for: ");
        spew_args(NULL, argc-1, argv+1, out);
      }
      return 1;
    }
#ifdef HAVE_READLINE_HISTORY
    else if(!strcmp("history", *argv) &&
        (argc == 1 || argc == 2))
    {
      HIST_ENTRY **h;
      int i = 0;
      if(argc > 1)
      {
        char *end;
        int num;
        num = strtol(argv[1], &end, 10);
        if(*end == '\0')
        {
          if(num < history_length)
            i = history_length - num;
        }
      }
      h = history_list();
      h += i;
      for(;h && *h; ++h)
        fprintf(out, "%s  %s\n", space_number(++i), (*h)->line);
      return 1;
    }
#endif
    else if(!strcmp("input", *argv))
    {
      if(argc > 1)
      {
        change_input(argv[1], in, out);
#ifdef HAVE_LIBREADLINE
        if(isatty(fileno(in)) && !no_readline)
        {
          /* setup readline */
          *use_readline = 1;
          fprintf(out, "Using readline version: %d.%d\n",
              RL_VERSION_MAJOR, RL_VERSION_MINOR);
        }
        else
        {
#endif
          /* setup getline */
#ifdef HAVE_LIBREADLINE
          *use_readline = 0;
#endif
          fprintf(out, "Using getline()\n");
          fflush(out);
#ifdef HAVE_LIBREADLINE
        }
#endif
        return 1;
      }
      else
      {
        fprintf(out,
            "reading input from file descriptor %d which %s a tty device\n",
            fileno(in), isatty(fileno(in))?"is":"is not");
        fflush(out);
        return 1;
      }
    }
  }
  return 0;
}

