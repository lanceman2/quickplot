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

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else /* !defined(HAVE_READLINE_H) */
#    undef HAVE_LIBREADLINE
#  endif
#endif

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else /* !defined(HAVE_HISTORY_H) */
#    undef HAVE_READLINE_HISTORY
#  endif
#endif /* HAVE_READLINE_HISTORY */

#ifndef HAVE_LIBREADLINE
/* We do not use history without readline */
#  ifdef HAVE_READLINE_HISTORY
#    undef HAVE_READLINE_HISTORY
#  endif
#endif


#define FIFO_PATH_LEN (128)

extern char *get_window_nums(int state);
extern char *get_graph_nums(int state);
extern char *get_plot_nums(int state);

struct command
{
  char *name;
  char *args;
  char *doc;
  int propagrate;
};

extern
struct command commands[], app_commands[],
               window_commands[], graph_commands[],
               plot_commands[], help_commands[];

#ifdef HAVE_READLINE_HISTORY
extern
void Save_history(void);
#endif

static inline
char *space_number(size_t i)
{
  static char text[64];
  const char space[] = "                  ";
  int nspace;
  if(i<10)
    nspace = 5;
  else if(i<100)
    nspace = 4;
  else if(i>1000)
    nspace = 3;
  else if(i>10000)
    nspace = 2;
  else if(i> 100000)
    nspace = 1;
  else // 10000000
    nspace = 0;

  snprintf(text, 64, "%.*s%zu", nspace, space, i);
  return text;
}


/* To client */
#define END_REPLY   'E'  // "E\n"  End of reply from server


/* returns 1 if there is data to read
 * returns 0 if not */
static inline
int check_file_in(FILE *file, long sec, long usec)
{
  fd_set rfds;
  struct timeval tv;
  int ret;
  tv.tv_sec = sec;
  tv.tv_usec = usec;

  FD_ZERO(&rfds);
  FD_SET(fileno(file), &rfds);
  ret = select(fileno(file)+1, &rfds, NULL, NULL, &tv);
  //DEBUG("select(,fd=%d,)=%d\n", fileno(file), ret);
  if(ret > 0)
    return 1;

  VASSERT(ret != -1, "select(,fd=%d,,) failed\n", fileno(file));

  if(ret == -1)
    QP_EERROR("reading input failed\n");

  return 0;
}

static inline
char *allocate_prompt(void)
{
  char *prompt;
    prompt = getenv("QP_PROMPT");
  if(!prompt)
    prompt = getenv("PS2");
  if(!prompt)
    return qp_strdup("QP> ");
  return qp_strdup(prompt);
}


static inline
char *set_to_qp_fifo_path(char *str,
    pid_t qp_pid, pid_t shell_pid)
{
  snprintf(str, FIFO_PATH_LEN, "/tmp/quickplot_to_%d_%d",
      qp_pid, shell_pid);
  return str;
}

static inline
char *set_from_qp_fifo_path(char *str,
    pid_t qp_pid, pid_t shell_pid)
{
  snprintf(str, FIFO_PATH_LEN, "/tmp/quickplot_from_%d_%d",
      qp_pid, shell_pid);
  return str;
}

extern
void qp_shell_initialize(int use_readline);

/* returns allocated pointers to white space seperated args in the line
 * with \" and ' escaping just on the edges */
static inline
char **get_args(char *line, int *argc)
{
  /* this is a single process, single thread server
   * so we can use one allocated arg array */
  char **arg = NULL;
  size_t arg_len = 0;
  size_t count = 0;
  int sq_escape = 0, dq_escape = 0;
  char *s;

  ASSERT(line);

  arg = qp_malloc((arg_len = 4)*sizeof(char *));

  for(s = line; *s; )
  {
    while(isspace(*s)) ++s;
    if(*s == '\'')
    {
      sq_escape = 1;
      ++s;
    }
    if(*s == '"')
    {
      dq_escape = 1;
      ++s;
    }
    if(!*s) break;
    ++count;
    if(count+1 > arg_len)
      arg = qp_realloc(arg, (arg_len += 4)*sizeof(char *));
    arg[count-1] = s;
    while(*s && ((!isspace(*s) && !sq_escape && !dq_escape)
                 || (*s != '\'' && sq_escape)
                 || (*s != '"' && dq_escape))) ++s;
    sq_escape = dq_escape = 0;
    if(*s)
    {
      *s = '\0';
      ++s;
    }
  }
  arg[count] = NULL;

  *argc = count;
  return arg;
}

static inline
void spew_args(const char *prefix, size_t argc, char **argv, FILE *file)
{
  size_t i;
  if(prefix)
    fprintf(file, "%s(%zu args): ", prefix, argc);
  if(argc)
    fprintf(file, "%s", argv[0]);
  for(i=1; i<argc; ++i)
    fprintf(file, " %s", argv[i]);
  putc('\n', file);
}

extern
int process_client_side_commands(int *argc_, char ***argv_,
    FILE *in, FILE *out, size_t line_len, char **history_mem
#ifdef HAVE_LIBREADLINE
    , int no_readline, int *use_readline
#endif
    );

