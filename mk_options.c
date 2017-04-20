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

/* Quickplot is turning out to have so many command
 * line options that we are automating the coding and
 * documenting of them using this C code.
 * This code is not compiled into Quickplot.  The
 * executable from this code is used to generate code
 * that goes into Quickplot.
 * We will try to make this the only place with these
 * argument option strings are listed, so that we can
 * change them in one place, or better yet, not change
 * them.  
 *
 * One negative to this method is that many other source
 * files depend on this one file and so when you edit
 * this one file many other files will need to be
 * recompiled.  Reminds me of using the QT moc
 * preprocessor and why I do not like QT.  This is not
 * that bad. */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "config.h"
#include "debug.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif

struct qp_option
{
  /* 0 = do not call function    1 or 2 = call function    2 = function will exit */
  int pass[2]; /* pass[0] == 1st pass   pass[1] == 2nd pass */
  char *long_op; /* initialization code is generated based this string */
  char *short_op;
  char *arg; /* if arg == "[STRING]" then it is optional */
  char *description; /* you can put @CONFIG_STUFF@ in this string */
  /* Set these two to make code that initializes and declares a
   * struct qp_app variable. */
  char *op_init_value; /* like "0" for app->op_my_long_option = 0; */
  char *op_type;       /* like "int" for int op_my_long_option; in the struct  qp_app */
};


/*  description special sequences that make HTML markup
 *
 *    "**" = start list <ul> 
 *    "##"  = <li>
 *    "&&" = end list   </ul>
 *
 *    "  " = "&nbsp; "
 *
 *    "--*"  =  "<a href="#name">--*</a>"  where --* is a long_op
 *
 *    "::"   = "<span class=code>"  and will not add '\n' until @@
 *    "@@"   = "</span>"
 */


/* int tri values are   auto = -1   on or yes = 1    no or off = 0 */


static
struct qp_option options[] =
{
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,1}, "",                     0,    "FILE",    "read data from file FILE.  If FILE is - (dash) then "
                                                  "standard input will be read.  This is the same as "
                                                  "the ::--file@@ option.  See also ::--pipe@@.",             0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {2,0}, "--about",              "-a", 0,         "display introductory information about Quickplot in a "
                                                  "browser and exit",                                         0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--auto-scale",         "-A", 0,         "automatically select the X and Y scales for graphs "
                                                  "containing more than one plot.  This is the default.  "
                                                  "See also ::--same-x-scale@@, ::--same-y-scale@@, "
                                                  "::--same-scale@@ ::--different-scale@@.",                  0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,1}, "--background-color",   "-C", "RGBA",    "set the color of the graph background.  RGBA may be any "
                                                  "string that GTK+ can parse into a RGB or RGBA color.  "
                                                  "For examples ::--background-color='rgba(0,0,255,0.5)'@@ "
                                                  "will make translucent blue, and ::-C '#050'@@ will make "
                                                  "a dark green.",                                            0,          "struct "
                                                                                                                          "qp_colora" },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--border",             "-b", 0,         "add a border to main window.  This is the default.  See "
                                                  "also ::--no-border@@.",                                    "TRUE",     "gboolean"  },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--buttons",            0,    0,         "show the button bar in the main window.  This is the "
                                                  "default.  See also ::--no-buttons@@.",                     "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--cairo-draw",         "-c", 0,         "draw graphs using the Cairo API.  Cairo drawing may be "
                                                  "slower, but you get translucent colors and "
                                                  "anti-aliasing in all aspects of the graph and in saved "
                                                  "image files.  See also ::--x11-draw@@.",                   0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--default-graph",      "-D", 0,         "create the default graph for the current file and turn "
                                                  "default graphing for future files read.  "
                                                  "If you give a ::--graph@@ or ::--graph-file@@ "
                                                  "after this option you will generate an additional "
                                                  "graph.  A default graph will be made each "
                                                  "time this option is encountered, so this can be used to "
                                                  "control when, in the sequence of command line options, "
                                                  "graphs are made.  See also ::--no-default-graph@@.",       "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--different-scale",    "-d", 0,         "graphs with more than one plot will have different "
                                                  "scales if the extreme values in each plot are not all "
                                                  "the same.  See also ::--same-scale@@, ::--same-x-scale@@ "
                                                  "and ::--same-y-scale@@.",                                  0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--file",               "-f", "FILE",    "read data from file FILE.  If FILE is - (dash) then "
                                                  "standard input will be read.  See also ::--pipe@@.",       0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--fullscreen",         "-F", 0,         "make the main window fullscreen.  See also "
                                                  "::--no-fullscreen@@ and ::--maximize@@.",                  0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--gaps",               0,    0,         "interpret NAN, -NAN, INF, -INF, and double overflow "
                                                  "numbers as a gap in the plot, and don't draw a "
                                                  "connecting line to adjacent non-gap points.  "
                                                  "This is the default.  See also ::--no-gaps@@.",            "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/

{ {0,1}, "--geometry",           0,    "GEO",     "specify the position and size of the main window.  To "
                                                  "set the geometry back to the default just set GEO to "
                                                  "NONE.  Example ::--geometry=1000x300-0+30@@",              0,          "cairo_rec"
                                                                                                                          "tangle_in"
                                                                                                                          "t_t"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--graph",              "-g", "LIST",    "make a graph with plots ::LIST@@.  "
                                                  "The ::LIST@@ is of the form ::\"x0 y0 x1 y1 x2 "
                                                  "y2 ...\"@@.  For example: ::--graph \"0 1 3 4\"@@ will "
                                                  "make two plots in a graph.  It will plot channel 1 vs "
                                                  "channel 0 and channel 4 vs channel 3 in the same graph. "
                                                  " Data channels are numbered, starting at 0, in the "
                                                  "order that they are created as files are read.  A "
                                                  "separate graph tab will be created for each ::--graph@@ "
                                                  "option given.  This ::--graph@@ option must be after "
                                                  "the file loading options that load the channels that it "
                                                  "lists to plot.  See also ::--graph-file@@.",               0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--graph-file",         "-G", "LIST",    "make a graph with plots ::LIST@@.  "
                                                  "The ::LIST@@ is of the form ::\"x0 y0 x1 y1 x2 "
                                                  "y2 ...\"@@.  Example: ::--graph-file \"0 1 3 4\"@@ will "
                                                  "make two plots in a graph.  It will plot channel 1 vs "
                                                  "channel 0 and channel 4 vs channel 3 in the same graph. "
                                                  " A separate graph tab will be created for each "
                                                  "::--graph-file@@ option given.  This is like the "
                                                  "::--graph@@ option except that the channel numbers "
                                                  "start at zero for the last file read.  "
                                                  "They are relative channel numbers.  So channel numbers "
                                                  "for ::---graph-file@@ may be negative to refer to "
                                                  "channels that came from files before the last file.  "
                                                  "This is handy if you load a lots of files and lose count "
                                                  "of the number of channels loaded in each file.",           0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--grid",               0,    0,         "draw a grid with the graph.  This is the default.  See "
                                                  "also ::--no-grid@@.",                                      "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--grid-font",          "-T", "FONT",    "set the font used to in the grid label numbers.  "
                                                  "Example: ::--grid-font='Sans Bold 12'@@.  The default "
                                                  "grid font is \"Sans 10\".",                                "qp_strdu"
                                                                                                              "p(\"Sans"
                                                                                                              " 10\")",   "char *"    },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,1}, "--grid-line-color",    0,    "RGBA",    "set the graph grid lines color.  RGBA may be any string "
                                                  "that GTK+ can parse into a RGB or RGBA color.  For "
                                                  "example ::--grid_line_color='rgba(255,0,0,0.5)'@@ will "
                                                  "make a translucent red.",                                  0,          "struct "
                                                                                                                          "qp_colora" },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--grid-line-width",    "-W", "PIXELS",  "set the width of the grid lines if there are any",         "4",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--grid-numbers",       0,    0,         "show grid numbers.  This is the default.  The grid must "
                                                  "be showing to show grid numbers too.  See also "
                                                  "::--no-grid-numbers@@.",                                   "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,1}, "--grid-text-color",    0,    "RGBA",    "set the graph grid text color.  RGBA may be any string "
                                                  "that GTK+ can parse into a RGB or RGBA color.  For "
                                                  "example ::--grid_text_color='rgba(0,255,0,0.5)'@@ will "
                                                  "make translucent green.",                                  0,          "struct "
                                                                                                                          "qp_colora" },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--grid-x-space",       "-X", "PIXELS",  "set the maximum x space between vertical grid lines. "
                                                  "The minimum will be about half this.  This distance "
                                                  "varies as the scale changes due to zooming.  This "
                                                  "distance cannot be fixed due to the way Quickplot "
                                                  "scales your graphs and always picks reasonable grid "
                                                  "line spacing.  See also ::--grid-x-space@@.",              "220",      "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--grid-y-space",       "-Y", "PIXELS",  "set the maximum y space between horizontal grid lines.  "
                                                  "See also ::--grid-x-space@@ above.",                       "190",      "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {2,0}, "--gtk-version",        0,    0,         "print the version of GTK+ that Quickplot was built with "
                                                  "and then exit",                                            0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--gui",                0,    0,         "show the menu bar, button bar, tabs bar, and the status "
                                                  "bar.  This is the default.  See also ::--no-gui@@.",       0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {2,0}, "--help",               "-h", 0,         "display help in a browser and exit",                       0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--label-separator",    "-p", "STR",     "specifies the label separator string STR if labels are "
                                                  "read in from the top of a text data plot file.  The "
                                                  "default value of ::STR@@ is ::\" \"@@ (a single space). "
                                                  " See option: ::--labels@@.",                               "qp_strdup"
                                                                                                              "(\" \")",  "char *"    },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--labels",             "-L", 0,         "read labels from the first line of a text file that "
                                                  "is not skipped.  See also: ::--skip-lines@@, "
                                                  "::--label-separator@@ and ::--no-labels@@.",               "0",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {2,0}, "--libsndfile-version", 0,    0,         "print the version of libsndfile that Quickplot was "
                                                  "built with and then exit",                                 0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--line-width",         "-I", "PIXELS",  "specify the plot line widths in pixels.  May be set to "
                                                  "AUTO to let Quickplot select the line width based on "
                                                  "the plot point density.  AUTO is the default.",            "-1",       "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,1}, "--linear-channel",     "-l", "[OPTS]",  "::OPTS@@ are ::START|[STEP]@@.  "
                                                  "This option prepends a linear series channel to the "
                                                  "file being read. ** ## ::START@@  "
                                                  "set the first value in the sequence to ::START@@.  The "
                                                  "default ::START@@ value is ::0@@. ## ::STEP@@  "
                                                  "set the sequence step size to ::STEP@@.  "
                                                  "The default ::STEP@@ is 1. && "
                                                  "There must be a ::START@@ before ::STEP@@.  "
                                                  "For example: ::--linear-channel='100 0.2'@@ will make "
                                                  "a linear channel that starts at 100 and steps 0.2.  "
                                                  "Sound files will always have a "
                                                  "linear channel that contains the time prepended.  Using "
                                                  "this option with a sound file would prepend an "
                                                  "additional channel.  Any file loaded that contains just "
                                                  "a single channel will automatically have a channel "
                                                  "prepended.  Using this option with a single channel file "
                                                  "will not prepend an additional channel, but will let you "
                                                  "set the start and step values for that prepended "
                                                  "channel.  See also ::--no-linear-channel@@.",              "NULL",     "struct "
                                                                                                                          "qp_channe"
                                                                                                                          "l *"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--lines",              "-j", "Y|N|A",   " ** ## ::Y@@  yes show lines. ## ::N@@  no don't show lines. "
                                                  "Same as ::--no-lines@@. ## ::A@@  auto, be smart about "
                                                  "it.  This is the default. &&",                             "-1",       "int"       },

/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,0}, "--local-menubars",     0,    0,         "disable that darn Ubuntu Unity globel menu bar.  This "
                                                  "will do nothing if not running with Unity.",               0,          0           },                                      
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--maximize",           "-m", 0,         "maximize the main window.  See also ::--no-maximize@@ "
                                                  "and ::--fullscreen@@.",                                    "0",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--menubar",            0,    0,         "show the menu bar.  This is the default.  See also "
                                                  "::--no-menubar@@.  This will do nothing if not running "
                                                  "with the Ubuntu Unity window manager.",                    "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--new-window",         "-w", 0,         "make a new main window for each graph",                    "0",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-border",          "-B", 0,         "display graphs main windows with no borders",              0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-buttons",         0,    0,         "hide the button bar in the main window.  See also "
                                                  "::--buttons@@.",                                           0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-default-graph",   "-U", 0,         "stop making the default graph for each file loaded.  See "
                                                  "also ::--default-graph@@.",                                0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-fullscreen",      0,    0,         "don't make the main window fullscreen.  This is the "
                                                  "default.  See also ::--fullscreen@@.",                     0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-gaps",            "-J", 0,         "draw a line across NAN (-NAN, INF, -INF and overflow "
                                                  "double) values if there are finite values on both "
                                                  "sides.  See also ::--gaps@@.",                             0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/

{ {0,1}, "--no-grid",            "-H", 0,         "don't draw graph grid lines in the graph.  See also "
                                                  "::--grid@@.",                                              0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-grid-numbers",    0,    0,         "don't show grid numbers.  See also ::--grid-numbers@@.",   0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-gui",             "-z", 0,         "don't show the menu bar, button bar, tabs bar, and "
                                                  "status bar.  See also ::--gui@@.",                         0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-labels",          "-Q", 0,         "don't read channel labels from the file.  This is "
                                                  "the default.  See also ::--labels@@.",                     0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-linear-channel",  "-k", 0,         "turn off adding a linear channel for up coming files.  "
                                                  "See also ::--linear-channel@@.",                           0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-lines",           "-i", 0,         "plot without drawing lines in the graph.  See also "
                                                  "--lines.",                                                 0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-maximize",        0,    0,         "don't maximize the main window.  This is the default.  "
                                                  "See also ::--maximize@@.",                                 0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-menubar",         "-M", 0,         "don't display the menu bar in the main window.  See "
                                                  "also ::--menubar@@.  This will do nothing if not "
                                                  "running with the Ubuntu Unity window manager.",            0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-new-window",      "-Z", 0,         "don't make a new main window for the graph.  This is "
                                                  "the default.  See also ::--new-window@@.",                 0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,0}, "--no-pipe",            "-N", 0,         "don't read data in from standard input even if there is "
                                                  "input to read.  See also ::--pipe@@.",                     0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-points",          "-o", 0,         "plot without drawing points in the graph.  See also "
                                                  "--points.",                                                0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,0}, "--no-readline",        0,    0,         "don't use GNU readline with the Quickplot command "
                                                  "shell if you run with the ::--shell@@ option.  This "
                                                  "will disable the use of line editing, shell "
                                                  "history, and tab command completion.  This "
                                                  "option has no effect if Quickplot is not built with "
                                                  "GNU readline.",                                            "0",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/

{ {0,1}, "--no-shape",           0,    0,         "turn off the use of the X11 shape extension.  See also "
                                                  "::--shape@@.",                                             0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-statusbar",       0,    0,         "hide the status bar in the main window.  See also "
                                                  "::--statusbar@@.",                                         0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--no-tabs",            0,    0,         "don't show the graph tabs in the main window.  See also "
                                                  "::--tabs@@.",                                              0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--number-of-plots",    "-n", "NUM",     "set the default maximum number of plots for each graph "
                                                  "to NUM",                                                   0,          "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,1}, "--pipe",               "-P", 0,         "read graph data from standard input.  By default "
                                                  "Quickplot "
                                                  "looks for data from standard input and stops looking if "
                                                  "no data is found in some short amount of time.  This "
                                                  "option will cause Quickplot to wait for standard input "
                                                  "indefinitely.  If you would like to type data in from "
                                                  "the terminal use ::--pipe@@.  This option is the same "
                                                  "as ::--file=-@@.",                                         "-1",       "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--point-size",         "-O", "PIXELS",  "start Quickplot using plot point size ::PIXELS@@ wide "
                                                  "in pixels.  This may be set to ::AUTO@@ to have "
                                                  "quickplot automatically set the point size depending on "
                                                  "the point density that is in graph.  ::AUTO@@ is the "
                                                  "default.",                                                 "-1",       "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--points",             0,    0,         "show points in the plots in the graph.  This is the "
                                                  "default.",                                                 "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {2,0}, "--print-about",        0,    0,         "prints the About document to standard output andthen "
                                                  "exits.  Use option ::--about@@ to display an HTML "
                                                  "version of the Quickplot About information.",              0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {2,0}, "--print-help",         0,    0,         "prints this Help document as ASCII text to standard "
                                                  "output and then exits.  Use option ::--help@@ for "
                                                  "displaying an HTML version of this help.",                 0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,1}, "--read-pipe-here",     "-R", 0,         "this is a place holder that tells Quickplot when to "
                                                  "read the data from standard input.  This is intended to "
                                                  "give the option of telling Quickplot when to read "
                                                  "standard input when Quickplot automatically determines "
                                                  "whether to read standard input or not.  See options "
                                                  "::--file@@, ::--pipe@@ and ::--no-pipe@@.",                "0",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--same-scale",         "-s", 0,         "plot all plots in the same graph scale.  See also "
                                                  "::--different-scale@@, ::--same-x-scale@@ and "
                                                  "::--same-y-scale@@.",                                      0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--same-x-scale",       "-x", "Y|N|A",   "use in place of ::--same-scale@@ or ::--auto-scale@@ "
                                                  "for finer control over how the x values of the plots "
                                                  "are scaled when you have more than one plot on a graph "
                                                  "** ## ::Y@@  yes same x scale ## ::N@@  no different x "
                                                  "scales ## ::A@@  auto, be smart about it.  This is the "
                                                  "default. && See also ::--same-y-scale@@.",                 "-1",       "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--same-y-scale",       "-y", "Y|N|A",  "use in place of ::--same-scale@@ or ::--auto-scale@@ "
                                                  "for finer control over how the x values of the plots "
                                                  "are scaled when you have more than one plot on a graph "
                                                  "** ## ::Y@@  yes same y scale ## ::N@@  no different y "
                                                  "scales ## ::A@@  auto, be smart about it.  This is the "
                                                  "default. && See also ::--same-x-scale@@. ",                "-1",       "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--shape",              0,    0,         "make graphs see through.  It "
                                                  "uses the X11 shape extension which was made famous by "
                                                  "xeyes.  The X11 shape extension may be a little flashy "
                                                  "on some systems.  Try using ::--shape@@ with the "
                                                  "::--no-gui@@, ::--no-grid@@, and ::--no-border@@ "
                                                  "options to make a floating graph on your display.  "
                                                  "The use of the X11 shape extension is a property of the "
                                                  "main window, not each graph tab.  This option may not "
                                                  "work well with fullscreen view.  This will slow down "
                                                  "graph drawing considerably.  You can toggle this on and "
                                                  "off with the ::x@@ key.  See option ::--no-shape@@.",      "0",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--shell",              "-e", 0,         "run a Quickplot command shell that reads commands from "
                                                  "standard input and writes out to standard output.  The "
                                                  "default is no shell and standard input will be read as "
                                                  "graph data.  If Quickplot is reading standard input for "
                                                  "plot data Quickplot will read shell commands "
                                                  "from the controlling terminal (/dev/tty) after all the "
                                                  "standard input has been read.  "
                                                  "You may use ::--no-pipe@@ to stop standard "
                                                  "input from being read as graph data.  The shell can do "
                                                  "most all the things that command-line options can do and "
                                                  "a lot more.  Run an interactive shell with ::quickplot "
                                                  "--shell@@ and use the help and tab completion to see how "
                                                  "it works.  You can also connect a Quickplot command "
                                                  "shell to a running Quickplot program with the program "
                                                  "::quickplot_shell@@.",                                     "NULL",     "struct "
                                                                                                                          "qp_shell"
                                                                                                                          " *"        },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--signal",             0,    "PID",     "signal SIGUSR1 to process PID after Quickplot is "
                                                  "running.",                                                 "0",        "pid_t"     },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,0}, "--silent",             0,    0,         "don't spew even on error.  The ::--silent@@ option will "
                                                  "override the effect of the ::--verbose@@ option.",         0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--skip-lines",         "-S", "NUM",     "skip the first ::NUM@@ lines when reading the file.  "
                                                  "This applies of all types of files that quickplot can "
                                                  "read.  Set ::NUM@@ to zero to stop skipping lines.",
                                                               /* TODO: make skip list not just n lines */    "0",        "size_t"    },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--statusbar",          0,    0,         "show the status bar below the graph.  This is the "
                                                  "default.  See also ::--no-statusbar@@.",                   "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--tabs",               0,    0,         "show the graph tabs.  This is the default.  See also "
                                                  "::--no-tabs@@.",                                           "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {1,0}, "--verbose",            "-v", 0,         "spew more to standard output.  See also ::--silent@@.",    0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {2,0}, "--version",            "-V", 0,         "print the Quickplot version number and then exit "
                                                  "returning 0 exit status",                                  0,          0           },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,1}, "--x11-draw",           "-q", 0,         "draw points and lines using the X11 API.  This is the "
                                                  "default.  Drawing may be much faster than with Cairo, "
                                                  "but there will be no translucent colors and no "
                                                  "anti-aliasing in the drawing of the plot lines and "
                                                  "points. There will be translucent colors and "
                                                  "anti-aliasing in the background and grid.  Also, saved "
                                                  "images will not have translucent colors like they do with "
                                                  "the Cairo draw mode.  You can "
                                                  "start drawing with X11 and switch to drawing with Cairo "
                                                  "when you want to save an image.  Use the ::r@@ key or "
                                                  "the View menu to switch back and forth between drawing "
                                                  "with X11 and Cairo.  See also ::--cairo-draw@@.",          "1",        "int"       },
/*------------------------------------------------------------------------------------------------------------------------------------*/
{ {0,0}, 0,                      0,    0,         0,                                                          0,          0           }
};



/* the length of the array of options */
static size_t length = 0;
/* the sorted array of options */
static struct qp_option **opt;



static inline
void *Malloc(size_t s)
{
  void *p;
  errno = 0;
  p = malloc(s);
  if(!p)
  {
    printf("malloc(%zu)\n", s);
    perror("malloc()");
    while(1) sleep(100);
  }
  return p;
}

static const char *prefix[2] = { "parse_1st_", "parse_2nd_" };
static char *func = NULL;


static inline
char *get_func(const char *prefix, const char *str)
{
  char *s;
  if(!str || !str[0])
    str = "File";

  if(func)
    free(func);
  s = (char *) str;
  while(*s == '-') ++s;

  func = Malloc(strlen(prefix) + strlen(s) + 1);
 
  sprintf(func, "%s%s", prefix, s);

  s = func;
  while(*s)
  {
    if(*s == '-')
      *s = '_';
    ++s;
  }

  return func;
}

static
int not_short_op_switch_case(struct qp_option *op, int pass)
{
  if(pass == 1 && op->pass[0] == 2)
    /* We do not need to parse options that would
     * exit in the 1st pass */
    return 1;

  return (
              (op->arg && op->arg[0] != '[')
          || !op->short_op
          || (strlen(op->short_op) != 2)
      );
}

static
void print_arg_parsing_code(int p)
{
  size_t i;
  int got_one = 0;
  int got_two = 0;

  printf(
      "{\n"
      "  int i = 1;\n"
      "\n"
      "  while(i < argc)\n"
      "  {\n"
      "     char *s;\n"
      "\n"
  );



  for(i=1;i<length;++i)
  {
    if(!opt[i]->pass[p] || opt[i]->arg)
      continue;

    func = get_func(prefix[p], opt[i]->long_op);

    printf("     %sif(!strcmp(\"%s\", argv[i]))\n"
           "     {\n"
           "        %s();\n%s"
           "     }\n",
           (got_one)?"else ":"",
           opt[i]->long_op, func,
           (opt[i]->pass[p] == 1)?"        ++i;\n":"");
    ++got_one;
  }


  for(i=1;i<length;++i)
  {
    if(!opt[i]->arg)
      continue;

    func = get_func(prefix[p], opt[i]->long_op);

    printf(
           "     %sif((s = get_opt(",(got_one)?"else ":"");
    if(opt[i]->short_op)
      printf("\"%s\"", opt[i]->short_op);
    else
      printf("NULL");
    printf(",\"%s\", argc, argv, &i)))\n"
           "     {\n", opt[i]->long_op);

    if(opt[i]->pass[p])
      printf(
           "        %s(s, argc, argv, &i);\n", func);
    else
      printf(
           "        /* do nothing this pass */\n");

    printf(
           "     }\n");
    
    if(opt[i]->arg[0] == '[')
    {  /* again for case with no optional argument */
      printf(
          "     %sif(!strcmp(\"%s\",argv[i])",
          (got_one)?"else ":"", opt[i]->long_op);
      if(opt[i]->short_op)
        printf(" || !strcmp(\"%s\",argv[i])",
            opt[i]->short_op);
      printf(
          ")\n"
          "     {\n"
          "        /* this is to caught it when there is no optional arg */\n"
          );
      if(opt[i]->pass[p])
        printf(
          "        %s(NULL, argc, argv, &i);\n", func);
      else
        printf(
          "        /* do nothing this pass */\n"
          );
      printf(
          "        ++i;\n"
          "     }\n");
    }

    ++got_one;
  }

  
  for(i=1;i<length;++i)
  {
    if(!opt[i]->arg && !opt[i]->pass[p] &&
        (p == 0 || opt[i]->pass[0] != 2))
    {
      printf("     %sif(!strcmp(\"%s\", argv[i])\n",
             (got_one)?"else ":"",
             opt[i]->long_op);
      ++got_one;
      ++got_two;
      break;
    }
  }
  
  for(++i;i<length;++i)
  {
    if(!opt[i]->arg && !opt[i]->pass[p] &&
        (p == 0 || opt[i]->pass[0] != 2))
    {
      printf("          || !strcmp(\"%s\", argv[i])\n",
             opt[i]->long_op);
      continue;
    }
  }

  if(got_two)
    printf(  "          )\n"
             "     {\n"
             "         /* do nothing this pass */\n"
             "         ++i;\n"
             "     }\n");




  got_two = 0;



  for(i=1;i<length;++i)
  {
    if(not_short_op_switch_case(opt[i], p))
        continue;

    printf("     %sif(argv[i][0] == '-' && is_all_short_no_arg_options(&argv[i][1]))\n",
         (got_one)?"else ":"");
    ++got_two;
    ++got_one;
    break;
  }

  if(got_two)
  {
    printf(
        "     {\n"
        "       size_t j, len;\n"
        "       len = strlen(argv[i]);\n"
        "\n"
        "       for(j=1;j<len;++j)\n"
        "         switch(argv[i][j])\n"
        "         {\n"
        );



    for(i=1;i<length;++i)
    {
      if(not_short_op_switch_case(opt[i], p) || !opt[i]->pass[p])
        continue;

        printf(  "          case '%c':\n", opt[i]->short_op[1]);
        func = get_func(prefix[p], opt[i]->long_op);
        if(!opt[i]->arg)
          printf("            %s();\n", func);
        else
          printf("            %s(NULL, argc, argv, &i);\n", func);
        printf(
              "            break;\n");
    }


    /* reuse dummy flag */
    got_two = 0;

    for(i=1;i<length;++i)
    {
      if(not_short_op_switch_case(opt[i], p)  || opt[i]->pass[p])
        continue;

      printf(  "          case '%c':\n", opt[i]->short_op[1]);
      got_two = 1;
    }

    if(got_two)
      printf(
              "            /* do nothing this pass */\n"
              "            break;\n");

    printf("          }\n"
           "\n"
           "       ++i;\n"
           "\n"
           "    }\n"
        );
  }


  printf("    %s\n"
         "    {\n"
         "      /* option FILE */\n", (got_one)?"else":"");
  
  if(opt[0]->pass[p])
    printf("      %s(argv[i]);\n", get_func(prefix[p], opt[0]->long_op));
  else
    printf("      /* do nothing this pass */\n");
  printf(  "      ++i;\n");


  printf(
         "    }\n"
         "  }\n"
         "\n"
         "}\n"
         "\n");
}

static
void print_argument_parsing_code(void)
{
  char *s, *shorts;
  size_t shorts_len;
  int i;

  /* Get a string with all the short options char
   * with out a arg. */
  s = (shorts = Malloc(length));
  for(i=1;i<length;++i)
  {
    if(opt[i]->short_op && (!opt[i]->arg || opt[i]->arg[0] == '[') &&
        strlen(opt[i]->short_op) == 2 && opt[i]->short_op[0] == '-'
      )
    {
      *s = (opt[i]->short_op)[1];
      ++s;
    }
  }
  *s = '\0';
  shorts_len = strlen(shorts);

  printf("\n"
      "static inline\n"
      "int is_all_short_no_arg_options(const char *str)\n"
      "{\n"
      "  const char *shorts = \"%s\";\n"
      "  const size_t shorts_len = %zu;\n"
      "  size_t i;\n"
      "\n"
      "  if(!str || !*str)\n"
      "    return 0; /* none */\n"
      "\n"
      "  while(*str)\n"
      "  {\n"
      "    for(i=0;i<shorts_len;++i)\n"
      "      if(shorts[i] == *str)\n"
      "        break;\n"
      "    if(i == shorts_len)\n"
      "      return 0;/* no it is not just short options */\n"
      "    ++str;\n"
      "  }\n"
      "\n"
      "  return 1; /* yes it is all short options */\n"
      "}\n"
      "\n",
      shorts, shorts_len);

  free(shorts);



  printf("\nstatic\n"
      "void parse_args_1st_pass(int argc, char **argv)\n");

  print_arg_parsing_code(0);

  printf("\nstatic\n"
      "void parse_args_2nd_pass(int argc, char **argv)\n");

  print_arg_parsing_code(1);
}

static
void print_pass_functions(int p)
{
  int i;

  printf("\n");

  /* FILE is a argument with an long option flag */
  if(opt[0]->pass[p])
    printf("static inline\n"
        "void %s(const char *file)\n"
        "{\n"
        "\n"
        "}\n"
        "\n",
        get_func(prefix[p], opt[0]->long_op));

  for(i=1;i<length;++i)
  {
    if(!opt[i]->pass[p] || opt[i]->arg)
      continue;

    func = get_func(prefix[p], opt[i]->long_op);
    printf("static inline\n"
        "void %s(void)\n"
        "{\n"
        "\n"
        "}\n"
        "\n", func);
  }

  printf("\n");

  for(i=1;i<length;++i)
  {
    if(!opt[i]->pass[p] || !opt[i]->arg)
      continue;

    func = get_func(prefix[p], opt[i]->long_op);
    printf("static inline\n"
        "void %s(char *arg, int argc, char **argv, int *i)\n"
        "{\n"
        "\n"
        "}\n"
        "\n", func);
  }

  printf("\n");
}

static
void print_1st_pass_functions(void)
{
  print_pass_functions(0);
}

static
void print_2nd_pass_functions(void)
{
  print_pass_functions(1);
}

static
void print_app_ops_declare_code()
{
  int i;

  for(i=1;i<length;++i)
  {
    if(!opt[i]->op_type)
      continue;

    printf("%s %s;\n", opt[i]->op_type,
        get_func("op_", opt[i]->long_op));
  }
}

static
void print_app_ops_init_code()
{
  int i;

  for(i=1;i<length;++i)
  {
    if(!opt[i]->op_init_value)
      continue;

    printf("  app->%s = %s;\n",
        get_func("op_", opt[i]->long_op),
        opt[i]->op_init_value);
  }
}

static
int get_string_len(char *str)
{
  if(!str)
    return 1;

  {
    size_t len;
    char *s;
    len = strlen(str);
    for(s=str;*s;++s)
      if(*s == '"')
        ++len;

    return len;
  }
}

/* adds space to the end to make the size you print be n */
/* returns the number of chars printed not including '\0' */
static
int Nprintf(int n, const char *format, ...)
{
  va_list ap;
  char *fmt, *str;
  int i, j, len;

  j = strlen(format);
  len = n + j + 1;
  fmt = Malloc(len + 1);
  str = Malloc(len + 1);

  strcpy(fmt, format);

  for(i=j;i<len;++i)
    fmt[i] = ' ';
  fmt[i] = '\0';

  va_start(ap, format);
  vsnprintf(str, n+1, fmt, ap);
  va_end(ap);
  printf("%s", str);

  len = strlen(str);

  free(fmt);
  free(str);

  return n;
}

/* replaces " with \"
 * Returns allocated memory */
static
char *fix_quotes(char *str)
{
  size_t str_len, out_len, i;
  char *s, *ret;

  if(!str)
    return str;


  str_len = (out_len = strlen(str));
  for(s=str;*s;++s)
    if(*s == '"')
      ++out_len;

  ret = (s = Malloc(out_len + 1));

  for(i=0;i<str_len;++i)
  {
    if(str[i] == '"')
    {
      *s = '\\';
      ++s;
      *s = '"';
    }
    else
      *s = str[i];
    ++s;
  }
  *s = '\0';

  return ret;
}
  

/* a snprintf() wrapper that I can get my head around. */
static inline
int Snprintf(char *str, size_t size, const char *format, ...)
{
  va_list ap;
  int ret;
  va_start(ap, format);
  vsnprintf(str, size+1, format, ap);
  va_end(ap);

  ret = strlen(str);

  return ret;
}

/* the str="0"  or str="hi aldnfa sdfkl" and so on  */
/* str ="---------------------------------------------------"
 *                      start                   len  
 * prints
 *                       "-----------------------"
 *                       "-----------------------"
 *                       "-------------",        
 *                                                   */
static
void kick_it_print(char *str, int start, int len, int add_comma)
{
  int slen;
  char *comma;

  if(add_comma)
    add_comma = 1;

  if(add_comma)
    comma = ",";
  else
    comma = "";

  if(!str)
  {
    Nprintf(len, "0%s", comma);
    return;
  }

  slen = strlen(str);
  add_comma += 2;
  --len;


  while(1)
  {
    int i;

    if(slen <= len - add_comma)
    {
      Nprintf(len+1, "\"%s\"%s ", str, comma);
      return;
    }

    for(i = len - add_comma; i> len/2 ; --i)
      if(str[i-1] == ' ')
        break;
    if(i == len/2)
      i = len - add_comma;

    printf("\"");
    slen -= i;
    Nprintf(i, "%s", str);
    str += i;
    printf("\"\n");


    Nprintf(start, " ");
  }
}    


static
void print_tidy_struct(void)
{
  int i;
  int count[7] = {0,0,0,0,0,0,0};

  printf("static\n"
      "struct qp_option options[] =\n"
      "{\n");

  for(i=0;i<length;++i)
  {
    int len;

    len = get_string_len(opt[i]->long_op) + 4;
    if(len > count[1])
      count[1] = len;
    
    len = get_string_len(opt[i]->short_op) + 4;
    if(len > count[2])
      count[2] = len;

    len = get_string_len(opt[i]->arg) + 4;
    if(len > count[3])
      count[3] = len;

    len = get_string_len(opt[i]->op_init_value) + 4;
    if(len > count[5])
      count[5] = len;

    len = get_string_len(opt[i]->op_type) + 4;
    if(len > count[6])
      count[6] = len;
  }


  for(i=1;i<7;i++)
    if(count[i] > 26)
      count[i] = 26;

  if(count[3] > 15)
    count[3] = 15;
  if(count[5] > 12)
    count[5] = 12;
  if(count[6] > 12)
    count[6] = 12;

  
  count[0] = strlen("{ {1,1}, ");
  count[4] = 60;

  /* change the counts to absolute distance */
  for(i=1;i<7;i++)
    count[i] += count[i-1];


  printf("/*");
  for(i=0;i<count[6]-2;++i)
    putchar('-');
  printf("*/\n");


  for(i=0;i<length;++i)
  { 
    char *s[6];
    int j;
    s[0] = fix_quotes(opt[i]->long_op);
    s[1] = fix_quotes(opt[i]->short_op);
    s[2] = fix_quotes(opt[i]->arg);
    s[3] = fix_quotes(opt[i]->description);
    s[4] = fix_quotes(opt[i]->op_init_value);
    s[5] = fix_quotes(opt[i]->op_type);
    Nprintf(count[0], "{ {%d,%d},", opt[i]->pass[0], opt[i]->pass[1]);
    for(j=0;j<6;++j)
    {
      kick_it_print(s[j], count[j], count[j+1] - count[j], (j!=5)?1:0);
      if(s[j])
        free(s[j]);
    }
    printf("},\n/*");
    for(j=0;j<count[6]-2;++j)
      putchar('-');
    printf("*/\n");
  }

  Nprintf(count[0], "{ {0,0},");
  for(i=0;i<6;++i)
  {
    kick_it_print(NULL, count[i], count[i+1] - count[i], (i!=5)?1:0);
  }
  printf("}\n");

  printf("};\n");
}

static
void print_help_text(void)
{

}

static
void print_html_options_list(void)
{

}

/* returns length printed
 * adds <a href=#LONG_OPT_NAME> 
 * if the str have a --long-opt in it.
 * Or returns 0 is none is found. */
int check_print_long_option_link(const char *str, size_t skip_i)
{
  size_t i;
  for(i=1;i<length;++i)
  {
    size_t len_opt, len_str;
    if(i == skip_i)
      continue;

    len_str = strlen(str);
    len_opt = strlen(opt[i]->long_op);
    if(!strncmp(opt[i]->long_op, str, len_opt) && len_str >= len_opt + 2 &&
        (
         (str[len_opt] == '@' && str[len_opt+1] == '@') ||
          str[len_opt] == ' ' || str[len_opt] == '='
        )
      )
      return printf("<a class=opt href=\"#%s\">", get_func("op_", opt[i]->long_op));
  }
  return 0;
}

static
void print_text2html(const char *str, size_t opt_i)
{
  char *s, *t;
  int w_count = 0;

  if(!str || !(*str) || !*(str +1))
  {
    printf("%s\n", str);
    return;
  }

  s = (char *) str;
  t = s + 1;
  while(*s && *t)
  {
    if(*s == '*' && *t == '*') // <ul>
    {
      if(w_count)
        putchar('\n');
      printf("      <ul>\n");
      s += 2;
      while(*s == ' ')
        ++s;
      t = s + 1;
      w_count = 0;
      continue;
    }
    if(*s == '#' && *t == '#') // <li>
    {
      if(w_count)
        putchar('\n');
      printf("      <li>");
      s += 2;
      while(*s == ' ')
        ++s;
      t = s + 1;
      w_count = 10;
      continue;
    }
    if(*s == ' ' && *t == ' ')
    {
      w_count += printf("&nbsp; ");
      s += 2;
      t += 2;
      continue;
    }
    if(*s == ':' && *t == ':') // <span>
    {
      int do_ancher;

      if(w_count)
      {
        putchar('\n');
        w_count = 0;
      }
      w_count += printf("      <span class=code>");
      s += 2;
      t += 2;

      w_count += (do_ancher = check_print_long_option_link(s, opt_i));

      while(*s && !(*s == '@' && *t == '@'))
      {
        putchar(*(s++));
        t = s + 1;
        ++w_count;
      }
      if(do_ancher)
        w_count += printf("</a>");
      w_count += printf("</span>");
      s += 2;
      if(*s == ' ' && *(s+1) != ' ')
      {
        putchar('\n');
        w_count = 0;
        ++s;
      }
      t = s + 1;
      continue;
    }
    if(*s == '&' && *t == '&') // </ul>
    {
      if(w_count)
        putchar('\n');
      printf("      </ul>\n");
      s += 2;
      while(*s == ' ')
        ++s;
      t = s + 1;
      w_count = 0;
      continue;
    }

    if(w_count > 50 && *s == ' ')
    {
      putchar('\n');
      w_count = 0;
    }
    else
    {
      ++w_count;
      putchar(*s);
    }

    ++t;
    ++s;
  }
  if(*s)
    putchar(*s);
  putchar('\n');
}

static inline
void put_man_char(char c)
{
  if(c == '-')
    putchar('\\');
  putchar(c);
}

static inline
int check_for_span_man(char **str, int *w_count, char *last_char)
{
  char *s = *str;

  if(!(*s) || !(*(s+1)))
    return 0;

  if(*s == ':' && *(s+1) == ':') // <span>
  {
    s += 2;
    if(*last_char != '\n')
      /* This is what last_char was for:
       * to see if we need a '\n' printed here. */
      putchar('\n');
    while(isspace(*s)) ++s;
    *w_count = 0;
    printf("\\fB");
    while(*s && !(*s == '@' && *(s+1) == '@')) // </span>
    {
      put_man_char(*s);
      *last_char = *s;
      ++(*w_count);
      ++s;
    }
    if(*s == '@' && *(s+1) == '@')
      s +=2;
    printf("\\fR");

    *str = s;
    return 1;
  }
  return 0;
}


static
void print_text2man(char *str)
{
  char *s;
  int w_count = 0;
  char last_char = '\n';

  if(!str || !(*str) || !*(str+1))
  {
    printf("%s\n", str);
    return;
  }

  s = str;
  while(*s && *(s+1))
  {
    if(check_for_span_man(&s, &w_count, &last_char)) // <span>
      continue;
    if(*s == '*' && *(s+1) == '*') // <ul>
    {
      s += 2;
      while(isspace(*s)) ++s;
      while(*s && !(*s == '&' && *(s+1) == '&')) // </ul>
      {
        if(*s == '#' && *(s+1) == '#') // <li>
        {
          if(last_char != '\n')
            putchar('\n');
          printf(".sp\n");
          s += 2;
          while(isspace(*s)) ++s;
          last_char = '\n';
        }
        else if(check_for_span_man(&s, &w_count, &last_char)) // <span>
          continue;
        else
        {
          put_man_char(*s);
          last_char = *s;
          ++s;
        }
      }
      if(*s == '&' && *(s+1) == '&')
        s += 2;
      if(last_char != '\n')
        putchar('\n');
      last_char = '\n';
      w_count = 0;
      continue;
    }
    if(w_count > 60 && isspace(*s))
    {
      putchar('\n');
      while(isspace(*s)) ++s;
      last_char = '\n';
      w_count = 0;
      continue;
    }
    if(w_count == 0 && isspace(*s))
    {
      while(isspace(*s)) ++s;
      continue;
    }

    put_man_char(*s);
    last_char = *s;
    ++w_count;
    ++s;
  }

  if(*s)
  {
    putchar(*s);
    last_char = *s;
  }

  if(last_char != '\n')
    putchar('\n');
}

static inline
char *remove_dashes(char *str)
{
  char *s = str;

  while(*s == '-') ++s;
  return s;
}

static
void print_man_page_options(void)
{
  int i;

  printf(".SH OPTIONS\n.PP\n");

  printf(".TP\n\\fBFILE\\fR\n");
  print_text2man(opt[0]->description);

  for(i=1; i<length; ++i)
  {
    char arg[128];
    if(opt[i]->arg)
      snprintf(arg, 128, " \\fB%s\\fR", opt[i]->arg);
    else
      arg[0] = '\0';

    if(opt[i]->short_op && strlen(opt[i]->short_op) == 2)
      printf(".TP\n\\fB\\-\\-%s\\fR%s or \\fB\\-%s\\fR%s\n",
          remove_dashes(opt[i]->long_op), arg,
          remove_dashes(opt[i]->short_op), arg);
    else
      printf(".TP\n\\fB\\-\\-%s\\fR%s\n",
          remove_dashes(opt[i]->long_op), arg);
    print_text2man(opt[i]->description);
  }
}

static
void print_html_options_table(void)
{
  int i;
  char class[2] = { 'a', 'b' };

  printf(
    "<table class=options summary=\"Quickplot argument options\">\n"
    );


  printf(
    "  <tr>\n"
    "    <th class=opt>long option</th>\n"
    "    <th class=opt title=\"short options\" style=\"%s\">short</th>\n"
    "    <th class=opt title=\"option arguments\">arg</th>\n"
    "    <th class=opt>description</th>\n"
    "  </tr>\n"
    "\n", "font-size:50%;"
    );

  for(i=0;i<length;++i)
  {
    printf(
    "  <tr class=%c>\n",
    class[i%2]);

    printf(
    "    <td class=opt style=\"white-space:nowrap;\">\n"
    "      <a name=\"%s%s\">%s</a>\n"
    "    </td>\n", i?"":"FILE",
    get_func("op_", opt[i]->long_op), opt[i]->long_op);

    printf(
    "    <td class=opt style=\"white-space:nowrap;\">\n"
    "      %s\n"
    "    </td>\n",
    opt[i]->short_op?opt[i]->short_op:"");

    printf(
    "    <td class=opt>\n"
    "      %s\n"
    "    </td>\n",
    opt[i]->arg?opt[i]->arg:"");
    
    printf(
    "    <td class=opt>\n");
    print_text2html(opt[i]->description, i);
    printf(
    "    </td>\n");

        printf(
    "  </tr>\n");
  }

  printf("</table>\n");


}

int main(int argc, char **argv)
{
  {
    ssize_t n = 2;
    if(argc > 1)
      n = strlen(argv[1]);

    if(argc < 2 || argc > 3 || n != 2 ||
          argv[1][0] != '-' ||
          (argv[1][1] != 't' && argv[1][1] != 'l' && argv[1][1] != 'h' &&
           argv[1][1] != 'a' && argv[1][1] != '1' && argv[1][1] != '2' &&
           argv[1][1] != 'i' && argv[1][1] != 'I' && argv[1][1] != 'T' &&
           argv[1][1] != 'p' && argv[1][1] != 's' && argv[1][1] != 'm')
          )
    {
      printf("Usage: %s [ -a | -h | -i | -I | -l | -p | -t | -1 | -2 ]\n"
          "\n"
          "  Generate HTML and C code that is"
          " related to Quickplot command line options\n"
          "  Returns 0 on success and 1 on error.\n"
          "\n"
          "    -a  print C code for argument parsing\n"
          "    -h  print C code for OPTIONS help text\n"
          "    -i  print C code for app->op_ initialization\n"
          "    -I  print C code for declaring app\n"
          "    -l  print html options list\n"
          "    -m  print OPTIONS part of man page\n"
          "    -p  print the options\n"
          "    -s  print the short options sorted\n"
          "    -t  print html options table\n"
          "    -T  print C code of the big qp_options array\n"
          "    -1  print 1st pass parsing C code template\n"
          "    -2  print 2nd pass parsing C code template\n"
          "\n", argv[0]);
      return 1;
    }
  }

  {
    int err = 0;
    /* make an array that we can sort */
    struct qp_option *o;
    /* Test that the options do not have duplicates */
    for(o=(struct qp_option *)options;o->long_op;++o)
    {
      struct qp_option *oc;
      for(oc=(o+1);oc->long_op;++oc)
      {
        if(!strcmp(oc->long_op, o->long_op))
        {
          printf("There are at least two long options:  %s\n", o->long_op);
          err = 1;
        }
        if(oc->short_op && o->short_op && !strcmp(oc->short_op, o->short_op))
        {
          printf("There are at least two short options:  %s\n", o->short_op);
          printf("one for: %s\n", o->long_op);
          printf("one for: %s\n", oc->long_op);
          err = 1;
        }
      }
      ++length;
    }
    if(err)
      return 1;
  }

  {
    /* sort the array */
    ssize_t i, end;

    opt = Malloc(sizeof(*opt)*length);
    /* sort the options in order of long option */
    for(i=0;i<length;++i)
      opt[i] = (struct qp_option *) &options[i];

    /* bubble sort for this small list */
    end = length - 2;
    while(end >= 0)
    {
      for(i=0;i<end;++i)
      {
        if(strcmp(opt[i]->long_op, opt[i+1]->long_op) > 0)
        {
          struct qp_option *o;
          o = opt[i];
          opt[i] = opt[i+1];
          opt[i+1] = o;
        }
      }
      --end;
    }
  }

#if 0
  if(argv[1][1] == 'a')
  {
    size_t i;

    printf("---- OLD SORT -----\n");
    for(i=0;i<length;++i)
      printf("%s\n", options[i].long_op);

    printf("---- NEW SORT ----\n");
    for(i=0;i<length;++i)
      printf("%s\n", opt[i]->long_op);
  }
#endif

  if(opt[0]->long_op[0])
  {
    printf("FILE is not the first option listed\n");
    return 1;
  }

  if(argv[1][1] == 'p')
  {
    size_t i;
    for(i=0;i<length;++i)
      printf("%s %s %s\n", opt[i]->long_op, opt[i]->short_op, opt[i]->arg);
    return 0;
  }

  if(argv[1][1] == 'm')
  {
    printf(".\\\" This OPTIONS part of this file was generated by running: "
      "%s %s\n", argv[0], argv[1]);
    print_man_page_options();
    return 0;
  }

  if(argv[1][1] == 's')
  {
    int i, L = 1, end;
    char *shorts, *s;

    for(i=0;i<length;++i)
    {
      char *o;
      o = opt[i]->short_op;
      if(o)
      {
        if(strlen(o) == 2 && o[0] == '-')
          L += 1;
        else
          L += strlen(o);
      }
    }
    s = (shorts = Malloc(L));
    for(i=0;i<length;++i)
    {
      char *o;
      o = opt[i]->short_op;
      if(o)
      {
        int l;
        l = strlen(o);
        if(l == 2 && o[0] == '-')
        {
          *s = o[1];
          ++s;
        }
        else
        {
          snprintf(s, l+1, "%s", o);
          s += l;
        }
      }
    }
    *s = '\0';

    /* bubble sort for this small list */
    end = L - 2;
    while(end >= 0)
    {
      for(i=0;i<end;++i)
      {
        if(shorts[i] >  shorts[i+1])
        {
          char c;
          c = shorts[i];
          shorts[i] = shorts[i+1];
          shorts[i+1] = c;
        }
      }
      --end;
    }


    printf("%s\n", shorts);
    return 0;
  }



  if(argv[1][1] == 'T')
  { 
    print_tidy_struct();
    return 0;
  }

  if(argv[1][1] == 'I')
  {
    printf("/* This file was auto-generated by running: `%s %s' */\n",
        argv[0], argv[1]);
    print_app_ops_declare_code();
    return 0;
  }

  if(argv[1][1] == 'i')
  {
    printf("/* This file was auto-generated by running: `%s %s' */\n",
        argv[0], argv[1]);
    print_app_ops_init_code();
    return 0;
  }


  if(argv[1][1] == 'a')
  {
    printf("/* This file was auto-generated by running: `%s %s' */\n",
        argv[0], argv[1]);
    print_argument_parsing_code();
    return 0;
  }

  if(argv[1][1] == 'h')
  {
    print_help_text();
    return 0;
  }

  if(argv[1][1] == 'l')
  {
    print_html_options_list();
    return 0;
  }

  if(argv[1][1] == 't')
  {
    printf("<!-- This table was auto-generated by running: `%s %s' -->\n",
        argv[0], argv[1]);
    print_html_options_table();
    return 0;
  }

  if(argv[1][1] == '1')
  {
    printf("/* This file was auto-generated by running: `%s %s' */\n",
        argv[0], argv[1]);
    print_1st_pass_functions();
    return 0;
  }

  if(argv[1][1] == '2')
  {
    printf("/* This file was auto-generated by running: `%s %s' */\n",
        argv[0], argv[1]);
    print_2nd_pass_functions();
    return 0;
  }

  return 0;
}

