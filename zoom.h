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



/*************************************************************
 * All data pasted into zoom and read from zoom is normalized
 * to be a linear tranformation of a 0,0 to 1,1 box.
 *************************************************************/


/* private */
static inline
void _qp_zoom_push(struct qp_zoom **z, double xscale, 
    double xshift, double yscale, double yshift)
{
  struct qp_zoom *new_z;
  ASSERT(z);
  ASSERT(*z);
  new_z = (struct qp_zoom*)qp_malloc(sizeof(*new_z));
  new_z->xscale = xscale;
  new_z->xshift = xshift;
  new_z->yscale = yscale;
  new_z->yshift = yshift;
  new_z->next = *z;
  *z = new_z;
}


static inline
void qp_zoom_push(struct qp_zoom **z, double xscale, 
    double xshift, double yscale, double yshift)
{
  _qp_zoom_push(z, xscale*(*z)->xscale, xscale*(*z)->xshift + xshift,
                   yscale*(*z)->yscale, yscale*(*z)->yshift + yshift);
}

static inline
void qp_zoom_pop(struct qp_zoom **z)
{
  ASSERT(z);
  ASSERT(*z);
  if((*z)->next)
  {
    struct qp_zoom *next;
    next = (*z)->next;
    free(*z);
    *z = next;
  }
}

/* free up all but the top of the stack */
static inline
void qp_zoom_pop_all(struct qp_zoom **z)
{
  ASSERT(z);
  ASSERT(*z);

  while((*z)->next)
  {
    struct qp_zoom *next;
    next = (*z)->next;
    free(*z);
    *z = next;
  }
}

/* add a zoom that is a shift in the 0,0 to 1,1 space */
static inline
void qp_zoom_push_shift(struct qp_zoom **z, double dx, double dy)
{
  _qp_zoom_push(z, (*z)->xscale, (*z)->xshift + dx,
                   (*z)->yscale, (*z)->yshift + dy);
}


/* xmin, ymin and xmax, ymax are normalized "close to"
 * between 0,0 to 1,1
 *
 * You must be sure the xmin, ymin, xmax, ymax values
 * are good before calling this. */
static inline
void qp_zoom_push_mm(struct qp_zoom **z,
    double xmin, double ymin, double xmax, double ymax)
{
  double xscale, xshift, yscale, yshift;

  xscale = (*z)->xscale/(xmax - xmin);
  yscale = (*z)->yscale/(ymax - ymin);

  xshift = xscale*((*z)->xshift - xmin)/(*z)->xscale;
  yshift = yscale*((*z)->yshift - ymin)/(*z)->yscale;

  _qp_zoom_push(z, xscale, xshift, yscale, yshift);
}

static inline
struct qp_zoom *qp_zoom_create(double xscale, double xshift,
                               double yscale, double yshift)
{
  struct qp_zoom *z;
  z = (struct qp_zoom *)qp_malloc(sizeof(struct qp_zoom));
  z->xscale = xscale;
  z->xshift = xshift;
  z->yscale = yscale;
  z->yshift = yshift;
  z->next = NULL;
  return z;
}

/* We assume that there is one zoom from qp_zoom_create()
 * to start with. */
static inline
void qp_zoom_copy(struct qp_zoom *z, const struct qp_zoom *org_z)
{
  ASSERT(org_z);
  ASSERT(z);

  if(z->next)
  {
    /* Remove and free the old stack */
    struct qp_zoom *n;
    for(n=z->next;n;n=n->next)
      free(n);
    z->next = NULL;
  }

  /* Copy the current zoom */
  z->xscale = org_z->xscale;
  z->yscale = org_z->yscale;
  z->xshift = org_z->xshift;
  z->yshift = org_z->yshift;

  for(org_z = org_z->next; org_z; org_z = org_z->next)
  {
    /* copy the rest of the zoom stack */
    z->next = qp_zoom_create(org_z->xscale, org_z->xshift,
          org_z->yscale, org_z->yshift);
    z = z->next;
  }
}


static inline
void qp_zoom_destroy(struct qp_zoom *z)
{
  ASSERT(z);

  while(z)
  {
    struct qp_zoom *next;
    next = z->next;
    free(z);
    z = next;
  }
}

