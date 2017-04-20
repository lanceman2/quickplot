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


struct qp_color_gen
{
  double hue, saturation, value,
         hue_x, saturation_x, value_x;
};


extern
struct qp_color_gen *qp_color_gen_create(void);

extern
void qp_color_gen_destroy(struct qp_color_gen *cg);

/* returns the hue which is in [0,1) */
extern
double qp_color_gen_next(struct qp_color_gen *cg,
                      double *red,
		      double *green,
		      double *blue,
                      double hue);
