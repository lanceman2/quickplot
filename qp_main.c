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

#include "quickplot.h"

#include "config.h"
#include "debug.h"
#include "list.h"
#include "spew.h"
#include "qp.h"
#include "shell.h"
#include "term_color.h"
#include "shell_common.h"

#ifdef QP_DEBUG
# include <signal.h>
#endif

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


static
void usr1_sighandler(int sig_num, siginfo_t *info, void *ptr)
{
  char path_in[FIFO_PATH_LEN], path_out[FIFO_PATH_LEN];
  FILE *in = NULL, *out = NULL;

  NOTICE("Received signal %d from pid %d\n", sig_num, info->si_pid);

  in = fopen(set_to_qp_fifo_path(path_in, getpid(), info->si_pid), "r");
  if(!in)
  {
    QP_EERROR("Failed to open fifo %s\n", path_in);
    return;
  }

  out = fopen(set_from_qp_fifo_path(path_out, getpid(), info->si_pid), "w");
  if(!out)
  {
    fclose(in);
    QP_EERROR("Failed to open fifo %s\n", path_out);
    return;
  }

  qp_shell_create(in, out, 1, info->si_pid);
}

#ifdef QP_DEBUG
static
void error_sighandler(int sig_num)
{
  VASSERT(0, "We caught signal %d", sig_num);
}
#endif


int main (int argc, char **argv)
{
  struct qp_gtk_options *gtk_opt;
  struct sigaction action;

#ifdef QP_DEBUG
  signal(SIGSEGV, error_sighandler);
  signal(SIGABRT, error_sighandler);
#endif

  qp_app_create();

  gtk_opt = strip_gtk_options(&argc, &argv);

  qp_getargs_1st_pass(argc, argv);

  if(qp_gtk_init_check(gtk_opt)) return 1;

  qp_getargs_2nd_pass(argc, argv);

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = usr1_sighandler;
  action.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR1, &action, NULL);

  if(app->op_signal)
    kill(app->op_signal, SIGUSR1);
  
  DEBUG("Quickplot pid %d\n", getpid());

  gtk_main();

  if(app->op_shell)
	qp_shell_destroy(app->op_shell);

  return 0;
}
