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

struct qp_shell
{
  /* C says that the order of a struct is the order of the types
   * in it.  And there is no padding at the top. */
  GSource gsource; /* We inherit GSource. */
  GPollFD fd;
  FILE *file_in, *file_out;
  char *line;
  size_t len;
  char *prompt;
  guint tag;
  int close_on_exit;
  pid_t pid; /* process that is connected */
  int file_in_isatty;
};


extern
struct qp_shell *qp_shell_create(FILE *file_in, FILE *file_out,
    int close_on_exit, pid_t pid);

extern
void qp_shell_destroy(struct qp_shell *sh);

extern
int do_server_commands(size_t argc, char **argv, struct qp_shell *sh);

