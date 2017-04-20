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
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <math.h>


#include "color_gen.h"
#include "config.h"
#include "debug.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif


#define DELTA_HUE  (6.0/4.6575324428)
#define DELTA_SAT  (0.18255);
#define DELTA_VAL  (0.160324/6.0);

#if 1
/* constants for hue_func() */
#define HM0  (1.0/1.3)
#define HM1  (2.0/1.3)
#define HA1  (HM0 * 0.2 - 0.2 * HM1)
#define HM2  HM0
#define HA2  (HA1 + HM1 * 0.5 - 0.5 * HM2)

static
double hue_func(double x)
{
  /* This makes the colors tend to be less green. */
  double y;

  while(x < 0.0)
    x += 1.0;
  while(x >= 1.0)
    x -= 1.0;

  if(x < 0.2)
    return HM0 * x;
  if(x < 0.5)
    return HA1 + HM1 * x;
  /* x < 1.0 */
  y = HA2 + HM2 * x;

  if(y >= 1.0)
    y -= 1.0;
  return y;
}
#else
static
double hue_func(double x)
{ 
  return x;
}
#endif

#define SAT_MAX  0.85
#define VAL_MAX  0.92
#define SAT_MIN  0.40
#define VAL_MIN  0.50



static
void GetNextHue(struct qp_color_gen *cg)
{
  cg->hue_x += DELTA_HUE;

  if(cg->hue_x  > 1.0)
  {
    cg->hue_x -= 1.0;

//#define SINES

#ifndef SINES
    /* This looks better */
    cg->value -= DELTA_VAL;
    if(cg->value < VAL_MIN)
    {
      cg->value += VAL_MAX - VAL_MIN;
      cg->saturation += DELTA_SAT;
      if(cg->saturation > SAT_MAX)
        cg->saturation += SAT_MIN - SAT_MAX;
    }
#endif
  }

#ifdef SINES
  cg->saturation_x += 0.2121212*2*M_PI;
  cg->value_x = 0.02233233124*2*M_PI;
  
  cg->saturation = 0.5*(SAT_MAX + SAT_MIN) +
                   0.5*(SAT_MAX - SAT_MIN) * cos(cg->saturation_x);

  cg->value = 0.5*(VAL_MAX + VAL_MIN) +
              0.5*(VAL_MAX - VAL_MIN) * sin(cg->value_x);
#endif

  cg->hue = hue_func(cg->hue_x);
}

struct qp_color_gen *qp_color_gen_create(void)
{
  struct qp_color_gen *cg;
  
  cg = qp_malloc(sizeof(*cg));

  cg->hue = 0;
  cg->hue_x = -DELTA_HUE + 0.0001;

  cg->saturation = SAT_MAX;
  cg->value = VAL_MAX;

#ifdef SINES
  cg->saturation_x = 0;
  cg->value_x = 0;
#endif

  return cg;
}

void qp_color_gen_destroy(struct qp_color_gen *cg)
{
  ASSERT(cg);
  free(cg);
}


double qp_color_gen_next(struct qp_color_gen *cg,
        double *red,
        double *green,
        double *blue,
        double hue)
/* Returns red, blue, and green as determined from the
 * value, saturation, and hue which are all bounded by [0,1).
 */

#if 1
/* See reference: http://en.wikipedia.org/wiki/HSL_color_space
 * 
 * We use difference normalization than that reference does. */
{
  double max, lower, slope;

  if(hue < 0.0 || hue >= 1.0)
  {
    GetNextHue(cg);
    hue = cg->hue;
  }

  lower = cg->value*(1.0 - cg->saturation);
  max = cg->value;
  slope = 6*(max-lower);

  /* One color chances linearly with hue in the space of hue
   * that is divided into 6 parts. */

  if(hue < 1 * 1.0/6.0)
  {
    *red = max;
    *blue = lower;
    *green = lower + hue*slope;
  }
  else if(hue < 2 * 1.0/6.0)
  {
    *red = - lower + 2*max - hue*slope;
    *blue = lower;
    *green = max;
  }
  else if(hue < 3 * 1.0/6.0)
  { 
    *red = lower;
    *blue = hue*slope - 2*max + 3*lower;
    *green = max;
  }
  else if(hue < 4 * 1.0/6.0)
  {
    *red = lower;
    *blue = max;
    *green = 4*max - 3*lower - hue*slope;
  }
  else if(hue < 5 * 1.0/6.0) 
  {
    *red = hue*slope - 4*max + 5*lower;
    *blue = max;
    *green = lower;
  }
  else if(hue < 6 * 1.0/6.0)
  {
    *red = max;
    *blue = 6*max - 5*lower - hue*slope;
    *green = lower;
  }

  return hue;
}

#else

{
  /* Lets try three shifted and scaled sine waves in place of the
   * piece-wise linear stuff above.   The piece-wise linear method
   * incases the sine waves when plotted together.  Okay that was fun.
   */
  double amp, shift;

  if(hue < 0.0 || hue >= 1.0)
  {
    GetNextHue(cg);
    hue = cg->hue;
  }

  amp = cg->value*cg->saturation/2.0;
  shift = cg->value*(1.0 - cg->saturation) + amp;

  *red   = shift + amp*cos(hue*2.0F*M_PI);
  *blue  = shift + amp*cos(hue*2.0F*M_PI - 2.0F*M_PI*2/3);
  *green = shift + amp*cos(hue*2.0F*M_PI - 2.0F*M_PI/3);

  return hue;
}
#endif

