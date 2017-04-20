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

#ifndef _QP_TERM_COLOR_H_
#define _QP_TERM_COLOR_H_

/* for quickplot developer spew colors */
extern char *gry;
extern char *red;
extern char *grn;
extern char *yel;
extern char *blu;
extern char *vil;
extern char *tur;
extern char *bld;
extern char *bgry;
extern char *bred;
extern char *bgrn;
extern char *byel;
extern char *bblu;
extern char *bvil;
extern char *btur;
extern char *rgry;
extern char *rred;
extern char *rgrn;
extern char *ryel;
extern char *rblu;
extern char *rvil;
extern char *rtur;
extern char *rbld;
extern char *trm;
extern char *nul;


/* on returns 1,  off returns 0 */
extern int qp_term_color_on(void); 

extern int qp_term_color_init(void);

#else /* #ifndef _QP_TERM_COLOR_H_ */

#error "You included this file twice."

#endif /* #ifndef _QP_TERM_COLOR_H_ */
