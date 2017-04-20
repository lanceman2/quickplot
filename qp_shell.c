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
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

#include "config.h"
#include "debug.h"
#include "spew.h"
#include "list.h"
#include "term_color.h"
#include "shell_common.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif

static char *prompt = NULL;
static FILE *file_to = NULL;
static int stdin_isatty;

static pid_t pid = -1; /* quickplot shell server pid */

#ifdef HAVE_LIBREADLINE
int use_readline = 0;
#endif


char *get_window_nums(int state)
{
  return NULL;
}

char *get_graph_nums(int state)
{
  return NULL;
}

char *get_plot_nums(int state)
{
  return NULL;
}


#ifdef QP_DEBUG
static
void debug_sighandler(int sig_num)
{
  VASSERT(0, "We caught signal %d", sig_num);
}
#endif

static
void signal_exit(void)
{
  fprintf(file_to, "exit\n");
}

static
void term_sighandler(int sig_num)
{
  printf("We caught signal %d\n", sig_num);
  signal_exit();
  printf("exiting\n");
  exit(1);
}

static inline
int Getline(char **line, size_t *len)
{
#ifdef HAVE_LIBREADLINE
  if(use_readline)
  {
    if(*line)
      free(*line);
    *line = readline(prompt);
    if(*line)
    {

      return 1;
    }
    else
      return 0;
  }
  else
  {
#endif
    size_t l;

    printf("%s", prompt);
    fflush(stdout);

    if(getline(line, len, stdin) == -1)
    {
      return 0;
    }

    l = strlen(*line);
    ASSERT(l > 0);
    /* remove newline '\n' */
    (*line)[l-1] = '\0';
    if(!stdin_isatty)
      printf("%s\n", *line);
    return 1;

#ifdef HAVE_LIBREADLINE
  }
#endif
}

void usage(void)
{
  printf(
      "  Usage: quickplot_shell [PID]|[-h|--help]|[QUICKPLOT_OPTIONS]\n"
      "\n"
      "  If PID is not given, this will launch quickplot as a new process and\n"
      "  connect to that new process.  In this case Quickplot will be started\n"
      "  with QUICKPLOT_OPTIONS if any are given.\n"
      "\n" 
      "  If PID is given, connect to a running Quickplot program with process ID\n"
      "  (pid) PID.\n"
      "\n");

  exit(1);
}

/* returns 1 on error
 * returns 0 on success */
static inline
int read_and_spew_until_end(char **reply, size_t *len, FILE *from, pid_t pid)
{
  do
  {
    size_t rlen;

    if(getline(reply, len, from) == -1)
    {
      printf("Quickplot pid %d: broke the connection.\n", pid);
      return 1;
    }
    rlen = strlen(*reply);
    if(rlen == 2 && **reply == END_REPLY && *((*reply)+1) == '\n')
    {
      DEBUG("END\n");
      return 0;
    }
    printf("%s", *reply);
  }
  while(1);

  return 0;
}

static int got_usr1 = 0;

void usr1_catcher(int sig_num)
{
  got_usr1 = 1;
}

int main (int argc, char **argv)
{
  char *user_line = NULL, *qp_reply = NULL;
  size_t qp_reply_len = 0, user_len = 0;
  int ret = 0;
  /* reading to the from Quickplot */
  FILE *file_from = NULL;
  char path_to[FIFO_PATH_LEN], path_from[FIFO_PATH_LEN];
  *path_to = '\0';
  *path_from = '\0';

  if(argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
    usage();

  if(argc == 2 && argv[1][0] > '0' && argv[1][0] < '9')
  {
    char *end;
    pid = strtoul(argv[1], &end, 10);
    if(*end != '\0')
    {
      printf("Cannot parse PID from \"%s\"\n\n", argv[1]);
      usage();
    }
  }

  if(pid == -1)
  {
    char *qp_path;
    size_t len;
    len = strlen(argv[0]);
    /* remove the _shell part of stuff/path_to/quickplot_shell */
    if(len >= 15)
    {
      qp_path = qp_strdup(argv[0]);
      qp_path[len-6] = '\0';
    }
    else
      qp_path = qp_strdup("quickplot");

    signal(SIGUSR1, usr1_catcher);

    errno = 0;
    pid = fork();
    if(pid == -1)
    {
      QP_EERROR("failed to fork()\n");
      return 1;
    }
    if(pid != 0)
    {
      // parent runs quickplot
      char **e_argv;
      int i;
      char last_arg[64];
      e_argv = qp_malloc((argc+2)*sizeof(*e_argv));
      e_argv[0] = qp_strdup("quickplot");
      for(i=1;i<argc;++i)
        e_argv[i] = argv[i];
      snprintf(last_arg, 64, "--signal=%d", pid);
      e_argv[i++] = last_arg;
      e_argv[i] = NULL;
      execv(qp_path, e_argv);
      QP_EERROR("failed to execute %s\n", qp_path);
      // we failed, kill my child.
      kill(pid, SIGINT);
      return 1; // failed
    }
    else
    {
      pid = getppid();
      /* wait for sig SIGCONT */
      DEBUG("pid=%d waiting for signal\n", getpid());
      while(!got_usr1)
        usleep(100000);
      DEBUG("pid=%d got signal\n", getpid());
    }
  }

  errno = 0;
  if(kill(pid, SIGCONT))
  {
    QP_EERROR("Failed to signal pid %d\n", pid);
    return 1;
  }

  errno = 0;
  if(mkfifo(set_to_qp_fifo_path(path_to, pid, getpid()),
        S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))
  {
    QP_EERROR("Failed to create fifo %s\n", path_to);
    *path_to = '\0';
    ret = 1;
    goto cleanup;
  }
  errno = 0;
  if(mkfifo(set_from_qp_fifo_path(path_from, pid, getpid()),
        S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))
  {
    QP_EERROR("Failed to create fifo %s\n", path_from);
    *path_from = '\0';
    ret = 1;
    goto cleanup;
  }
  /* Since the FIFO block on fopen(,"r") we will signal
   * before we call fopen(,"r").
   *
   * Looks like fopen(,"w") blocks too. */
  errno = 0;
  if(kill(pid, SIGUSR1))
  {
    QP_EERROR("Failed to signal pid %d\n", pid);
    return 1;
  }

  errno = 0;
  if(!(file_to = fopen(path_to, "w")))
  {
    QP_EERROR("fopen(\"%s\",\"a\") failed\n", path_to);
    ret = 1;
    goto cleanup_with_signal;
  }
  errno = 0;
  if(!(file_from = fopen(path_from, "r")))
  {
    QP_EERROR("fopen(\"%s\",\"r\") failed\n", path_from);
    ret = 1;
    goto cleanup_with_signal;
  }
  setlinebuf(file_from);

  prompt = allocate_prompt();

#ifdef QP_DEBUG
  signal(SIGSEGV, debug_sighandler);
  signal(SIGABRT, debug_sighandler);
#endif

  signal(SIGTERM, term_sighandler);
  signal(SIGQUIT, term_sighandler);
  signal(SIGINT, term_sighandler);

stdin_isatty = isatty(fileno(stdin));
qp_shell_initialize(1);

#ifdef HAVE_LIBREADLINE
  if(stdin_isatty)
  {
    use_readline = 1;
    printf("Using readline version: %d.%d\n",
        RL_VERSION_MAJOR, RL_VERSION_MINOR);
  }
  else
  {
#else
    printf("Using getline()\n");
#endif
#ifdef HAVE_LIBREADLINE
  }
#endif

  /* Exercise the pipe so it does not block the Quickplot
   * service on fopen(,"r"). */
  fprintf(file_to, "start\n");
  fflush(file_to);

  if(read_and_spew_until_end(&qp_reply, &qp_reply_len, file_from, pid))
    goto cleanup_with_signal;

  /* We can hide the FIFO files now that both parties are connected.
   * The OS will keep the FIFO files in existence so long as one of
   * the two processes have them open. */
  unlink(path_to);
  *path_to = '\0';
  unlink(path_from);
  *path_from = '\0';

  while((Getline(&user_line, &user_len)))
  {
    char **argv;
    int argc;
    size_t len;
    char *history_mem = NULL;
    len = strlen(user_line) + 1;
    argv = get_args(user_line, &argc);

    if(argc > 0)
    {
      if(!strcmp("exit", *argv)) // exit
        break;
      if(!strcmp("quit", *argv)) // quit
      {
        fprintf(file_to, "quit\n");
        fflush(file_to);
        break;
      }

      if(process_client_side_commands(&argc, &argv,
            stdin, stdout, len, &history_mem
#ifdef HAVE_LIBREADLINE
          , 0, &use_readline
#endif
          ))
      {
        if(!strcmp("input", *argv) && argc > 1)
        {
          stdin_isatty = isatty(fileno(stdin));
        }
      }
      else // command to server
      {
        int i;
#ifdef QP_DEBUG
        if(SPEW_LEVEL() <= 1)
        {
          printf("Read user line: \"%s\"", argv[0]);
          for(i=1; i<argc; ++i)
            printf(" %s", argv[i]);
          printf("\n");
        }
#endif
        fprintf(file_to, "%s", argv[0]);
        for(i=1; i<argc; ++i)
          fprintf(file_to, " %s", argv[i]);
        fprintf(file_to,"\n");
        fflush(file_to);

        // read and print reply
        if(read_and_spew_until_end(&qp_reply, &qp_reply_len, file_from, pid))
          goto cleanup_with_signal;
      }
    }
    if(history_mem)
      free(history_mem);
    free(argv);
  }

#ifdef HAVE_READLINE_HISTORY
  Save_history();
#endif

cleanup_with_signal:

  signal_exit();

cleanup:

  if(*path_to)
    unlink(path_to);
  if(*path_from)
    unlink(path_from);

  printf("exiting\n");
  return ret;
}
