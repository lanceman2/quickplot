/* A template of this file was generated with `./mk_options -1' */
/* Clearly this will edited too. */

static inline
void parse_1st_File(const char *file)
{
  if(!strcmp("-", file))
  {
    if(app->op_pipe != 0)
      app->op_pipe = 1;
  }
}

static inline
void parse_1st_about(void)
{
  cb_about(NULL, (void *)1);
}

static inline
void parse_1st_gtk_version(void)
{
  printf("%d.%d.%d\n",
      GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
  exit(0);
}

static inline
void parse_1st_help(void)
{
  cb_help(NULL, (void *) 1);
}

static inline
void parse_1st_libsndfile_version(void)
{
  char *str;
  str = (char *) sf_version_string();
  while(*str && (*str < '0' || *str > '9'))
    ++str;
  printf("%s\n", str);
  exit(1);
}

static inline
void parse_1st_no_pipe(void)
{
  app->op_pipe = 0;
}

static inline
void parse_1st_no_readline(void)
{
  app->op_no_readline = 1;
}

static inline
void parse_1st_pipe(void)
{
  if(app->op_pipe != 0)
    app->op_pipe = 1;
}

static inline
void parse_1st_print_about(void)
{
  print_text("about.txt");
}

static inline
void parse_1st_print_help(void)
{
  print_text("help.txt");
}

static inline
void parse_1st_read_pipe_here(void)
{
  /* We will read the pipe when this options comes up in
   * parse_2nd_read_pipe_here(). We will not read the pipe
   * before of after that. */
  app->op_read_pipe_here = 1;
}

static inline
void parse_1st_silent(void)
{
  qp_spew_init(5);
  parser->p1.silent = 1;
}

static inline
void parse_1st_verbose(void)
{
  if(!parser->p1.silent)
    qp_spew_init(1);
}

static inline
void parse_1st_version(void)
{
  printf("%s\n", VERSION);
  exit(0);
}

static inline
void parse_1st_background_color(char *arg, int argc, char **argv, int *i)
{
  check_color_opt("--background-color", arg);
}

static inline
void parse_1st_grid_line_color(char *arg, int argc, char **argv, int *i)
{
  check_color_opt("--grid-line-color", arg);
}

static inline
void parse_1st_grid_text_color(char *arg, int argc, char **argv, int *i)
{
  check_color_opt("--grid-text-color", arg);
}

static inline
void parse_1st_linear_channel(char *arg, int argc, char **argv, int *i)
{
  parse_linear_channel(0, arg, argc, argv, i, NULL, NULL);
}

static inline
void parse_1st_local_menubars(void)
{
    setenv("UBUNTU_MENUPROXY", "0", 1);
}

