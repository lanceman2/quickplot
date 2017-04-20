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

static inline
long get_plot_num(const char *s, char **endptr)
{
  while(*s && *s < '0' && *s > '9')
    ++s;
  if(!(*s))
    return INT_MAX; /* got nothing more */

  long val;
  errno = 0;
  val = strtol(s, endptr, 10);

  if(*endptr == s)
    return INT_MAX;

  if((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
      || (errno != 0 && val == 0))
    return INT_MAX; /* got error */

  return val;
}

/* Returns 1 on success 0 on failure
 *
 * x and y must be freed
 *
 * parsing  --plot  '0 1 0 2 0 3'
 * or --plot-file '0 1 0 2 0 3'
 *
 * str="0 1 0 2 0 3" */
static inline
int get_plot_option(const char *str, ssize_t **x, ssize_t **y,
    size_t *num_plots, const char *opt, ssize_t min_chan, ssize_t max_chan)
{
  char *endptr;
  char *s;
  long nx, ny;
  int i = 1;

  ASSERT(x);
  ASSERT(y);
  ASSERT(num_plots);
  
  if(!qp_sllist_last(app->sources))
  {
    if(!opt) return 0;

    QP_ERROR("No files loaded yet, bad option %s='%s'\n",
         opt, str);
    exit(1);
  }

  s = (char *) str;

  nx = get_plot_num(s, &endptr);
  if(nx < min_chan || nx > max_chan) goto fail;
  s = endptr;
  ny = get_plot_num(s, &endptr);
  if(ny < min_chan || ny > max_chan) goto fail;
  s = endptr;

  if(x && y)
  {
    *x = qp_malloc(sizeof(ssize_t)*(strlen(s)+1)/2);
    *y = qp_malloc(sizeof(ssize_t)*(strlen(s)+1)/2);
    (*x)[0] = nx;
    (*y)[0] = ny;
  }

  while(*s)
  {
    nx = get_plot_num(s, &endptr);
    if(nx < min_chan || nx > max_chan) goto fail;
    (*x)[i] = nx;
    s = endptr;
    ny = get_plot_num(s, &endptr);
    if(ny < min_chan || ny > max_chan) goto fail;
    s = endptr;
    (*y)[i++] = ny;
  }

  *num_plots = i;

#ifdef QP_DEBUG
  if(opt)
  {
    DEBUG("got %s='", opt);
    if(*num_plots > 0)
      APPEND("%zu %zu", (*x)[0], (*y)[0]);
    for(i=1; i<(*num_plots); ++i)
      APPEND(" %zu %zu", (*x)[i], (*y)[i]);
    APPEND("\n");
  }
#endif

  return 1;

fail:

  if(opt)
  {
    ERROR("got bad option %s='%s'\n", opt, str);
    QP_ERROR("got bad option %s='%s'\n", opt, str);
    exit(1);
  }
  
  if(x && *x)
    free(*x);
  if(y && *y)
    free(*y);

  return 0;
}

/* returns 0 on success */
static inline
int parse_linear_channel(int pass, const char *arg,
    int argc, char **argv, int *i, double *start, double *step)
{
  char *s;
  int err = 0, j = 0;
  double val[2];

  if(start)
    *start = 0;
  if(step)
    *step = 1;

  if(arg == NULL || arg[0] == '\0')
    goto ret;

  s = (char *) arg;

  while(1)
  {
    char *endptr = NULL;
    struct lconv *lc;

    errno = 0;
    val[j] = strtod(s, &endptr);
    if(errno || s == endptr)
    {
      if(argv[*i - 1] != arg)
      {
        /* The argument is part of the option like:
         * '--linear-channel=0 1' or -l'0 3'.
         * arg is not part of another option so
         * this is an error */
        err = 1;
        break;
      }
      else
      {
        /* The optional arg is not ours */
        *i -= 1;
        break;
      }
    }

    if(j == 1)
    {
      if(start)
        *start = val[0];
      if(step)
        *step = val[1];
      break;
    }

    lc = localeconv();

    s = endptr;
    /* go to the next number */
    while(*s && (*s < '0' || *s > '9')
        && *s != '+' && *s != '-' && *s != 'e'
        && *s != 'E' && strcmp(s, lc->decimal_point))
      ++s;
  
    if(!(*s))
    {
      if(start)
        *start = val[0];
      break;
    }

    ++j;
  }
  

  if(err && pass == 0)
    return 1;

  /* we should not get an error on the 2nd pass */
  ASSERT(!err);

  if(err)
    return 1;

ret:

#ifdef QP_DEBUG
  if(start && step)
    DEBUG("got option --linear-channel with start=%.22g step=%.22g\n",
        *start, *step);
#endif

  return 0;
}

/* returns 0 on success and sets maximize
 * if maximize=0 is is a regular window size
 * if maximize=1 is is maximized ??
 * if maximize=2 it is fullscreen
 *
 * returns 1 on error and sets err */
static inline
int GetGeometry(char *arg, int *X, int *Y, int *W, int *H, int *maximize)
{
  int n[4], count = 0, x_count = -1;
  int w, h;
  char *endptr, *s;
  endptr = s = arg;

  while(1)
  {
    long val;

    val = strtol(s, &endptr, 10);
    if(s == endptr || !endptr ||
      val == LONG_MAX || val == LONG_MIN)
      return 1;
    if(*s == '-' && *(s+1) == '0')
      /* mark a "-0" position as in --geometry=600x700-0+0 */
      n[count++] = INT_MIN;
    else
      n[count++] = val;

    if(!(*endptr))
      break;

    s = endptr;
    while(*s && (*s < '0' || *s > '9') && *s != '-' && *s != '+')
    {
      if(*s == 'x' || *s == 'X')
        /* this is where the 'x' is */
        x_count = count;
      ++s;
    }
    if(!(*s))
      break;
  }
  
  if(
      !(count == 2 && (x_count == 1 || x_count == -1))
       &&
      !(count == 4 && (x_count == 1 || x_count == 3))
    )
    return 1;

  if(x_count == 1)
    {
      w = n[0];
      h = n[1];
    }
  else if(x_count == 3)
    {
      w = n[2];
      h = n[3];
    }

  if(w < 1 || h < 1)
    return 1;

  if(x_count == -1 || x_count == 3)
    {
      *X = n[0];
      *Y = n[1];
    }
  else if(count == 4)
    {
      *X = n[2];
      *Y = n[3];
    }

  if(app->root_window_width < 1)
    qp_get_root_window_size();

  if(w > app->root_window_width)
    w = app->root_window_width;
  if(h > app->root_window_height)
    h = app->root_window_height;

  if(w == app->root_window_width && h == app->root_window_height)
    /* We do not want to set app->op_geometry if it is
     * full screen so that they can still toggle out of
     * full screen if they choose to. */
    *maximize = 2; /* Fullscreen */
  else
  {
    *maximize = 0;
    *W = w;
    *H = h;
  }
  return 0;
}

