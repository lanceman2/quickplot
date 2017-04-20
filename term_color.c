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

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>

#include "term_color.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


/* for quickplot developer spew colors */
char *gry = "\033[030m";
char *red = "\033[031m";
char *grn = "\033[032m";
char *yel = "\033[033m";
char *blu = "\033[034m";
char *vil = "\033[035m";
char *tur = "\033[036m";
char *bld = "\033[1;037m";
char *bgry = "\033[1;030m";
char *bred = "\033[1;031m";
char *bgrn = "\033[1;032m";
char *byel = "\033[1;033m";
char *bblu = "\033[1;034m";
char *bvil = "\033[1;035m";
char *btur = "\033[1;036m";
char *rgry = "\033[1;7;030m";
char *rred = "\033[1;7;031m";
char *rgrn = "\033[1;7;032m";
char *ryel = "\033[1;7;033m";
char *rblu = "\033[1;7;034m";
char *rvil = "\033[1;7;035m";
char *rtur = "\033[1;7;036m";
char *rbld = "\033[1;7;037m";

char *trm = "\033[000m";
char *nul = "";


static int is_init_and_color = 0; /* 0 = not init
                                    -1 = init but no color
                                     1 = init with color
                                  */

int qp_term_color_on(void)
{
  return qp_term_color_init();
}


int qp_term_color_init(void)
{
  char *env;

  if(is_init_and_color)
    return ((is_init_and_color == -1)?0:1);

  env = getenv("QP_TERM_COLOR");
  if(env &&
      (strncasecmp(env,"off",2) == 0 ||
       strncasecmp(env,"none",2) == 0))
  {
    gry = red = grn = yel = blu = vil = tur =
     bld = bgry = bred = bgrn = byel = bblu = bvil = btur =
    rgry = rred = rgrn = ryel = rblu = rvil = rtur = rbld =
     trm = nul;
    is_init_and_color = -1;
    return 0; /* there will not be color */
  }
  return is_init_and_color = 1; /* there will be color */
}

