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

#include "config.h"
#include "debug.h"
#include "list.h"

#ifdef DMALLOC
#  include "dmalloc.h"
#endif



struct qp_sllist *qp_sllist_create(struct qp_sllist *orig)
{
  struct qp_sllist *l;
  l = (struct qp_sllist *) qp_malloc(sizeof(struct qp_sllist));
  if(orig)
  {
    ASSERT(*(orig->ref_count) > 0);

    l->first = orig->first;
    l->last = orig->last;
    l->length = orig->length;
    l->current = NULL;
    l->ref_count = orig->ref_count;
    ++(*(l->ref_count));
  }
  else
  {
    l->first = l->last = l->current = NULL;
    l->length = 0;
    l->ref_count = (int *)qp_malloc(sizeof(int));
    *(l->ref_count) = 1;
  }
  return l;
}

struct qp_dllist *qp_dllist_create(struct qp_dllist *orig)
{
  struct qp_dllist *l;
  l = (struct qp_dllist *) qp_malloc(sizeof(struct qp_dllist));
  if(orig)
  {
    ASSERT(*(orig->ref_count) > 0);

    l->first = orig->first;
    l->last = orig->last;
    l->length = orig->length;
    l->current = NULL;
    l->ref_count = orig->ref_count;
    ++(*(l->ref_count));
  }
  else
  {
    l->first = l->last = l->current = NULL;
    l->length = 0;
    l->ref_count = (int *)qp_malloc(sizeof(int));
    *(l->ref_count) = 1;
  }
  return l;
}

/** add val to the end */
void qp_sllist_append(struct qp_sllist *l, void *val)
{
  struct qp_sllist_entry *new_e;
  ASSERT(l);
  ASSERT(*(l->ref_count) == 1);
  new_e = (struct qp_sllist_entry *)
    qp_malloc(sizeof(struct qp_sllist_entry));
  new_e->next = NULL;
  new_e->val = val;

  if(l->last)
  {
    l->last->next = new_e;
    l->last = new_e;
  }
  else
  {
    l->current = l->first = l->last = new_e;
  }
  ++(l->length);
}

/** add val to the end */
void qp_dllist_append(struct qp_dllist *l, void *val)
{
  struct qp_dllist_entry *new_e;
  ASSERT(l);
  ASSERT(*(l->ref_count) == 1);
  new_e = (struct qp_dllist_entry *)
    qp_malloc(sizeof(struct qp_dllist_entry));
  new_e->next = NULL;
  new_e->prev = l->last;
  new_e->val = val;

  if(l->last)
  {
    l->last->next = new_e;
    l->last = new_e;
  }
  else
  {
    l->current = l->first = l->last = new_e;
  }
  ++(l->length);
}

void qp_sllist_destroy(struct qp_sllist *l, int free_vals)
{
  ASSERT(l);
  ASSERT(*(l->ref_count) > 0);

  if(!l) return;


  if(*(l->ref_count) == 1)
  {
    /* This is the last reader of this
     * list so we may free the entries */
    struct qp_sllist_entry *e;
    e = l->first;
    while(e)
    {
      struct qp_sllist_entry *prev_e;
      prev_e = e;
      e = e->next;
      if(free_vals && prev_e->val)
        free(prev_e->val);
      free(prev_e);
    }
    free(l->ref_count);
  }
  else
    --(*(l->ref_count));

  free(l);
}


void qp_dllist_destroy(struct qp_dllist *l, int free_vals)
{
  ASSERT(l);
  ASSERT(*(l->ref_count) > 0);

  if(!l) return;

  if(*(l->ref_count) == 1)
  {
    struct qp_dllist_entry *e;
    e = l->first;
    while(e)
    {
      struct qp_dllist_entry *prev_e;
      prev_e = e;
      e = e->next;
      if(free_vals && prev_e->val)
        free(prev_e->val);
      free(prev_e);
    }
    free(l->ref_count);
  }
  else
    --(*(l->ref_count));

  free(l);
}

/* returns the number found */
int qp_sllist_find(struct qp_sllist *l, void *val)
{
  void *v;
  int ret = 0;
  ASSERT(l);
  ASSERT(*(l->ref_count) > 0);

  for(v=qp_sllist_begin(l); v;
      v=qp_sllist_next(l))
    if(v == val)
      ++ret;
  return ret;
}

/* returns the number found */
int qp_dllist_find(struct qp_dllist *l, void *val)
{
  void *v;
  int ret = 0;
  ASSERT(l);
  ASSERT(*(l->ref_count) > 0);

  for(v=qp_dllist_begin(l); v;
      v=qp_dllist_next(l))
    if(v == val)
      ++ret;
  return ret;
}

/* remove by pointer */
int qp_sllist_remove(struct qp_sllist *l,
    const void *val, int free_val)
{
  struct qp_sllist_entry *e, *prev = NULL;
  int ret = 0;
  ASSERT(l);
  ASSERT(*(l->ref_count) == 1);

  for(e=l->first; e;)
  {
    struct qp_sllist_entry *next;
    
    next = e->next;
    if(e->val == val)
    {
      if(prev)
      {
        prev->next = e->next;
        if(l->last == e)
          l->last = prev;
      }
      else /* it is the first entry */
      {
        l->first = e->next;
        if(l->last == e)
          l->last = NULL;
      }
      if(l->current == e)
        /* reset iterators */
        l->current = NULL;

      /* free it once */
      if(free_val && !ret)
        free(e->val);
      free(e);
      --(l->length);
      ASSERT(l->length != (size_t) -1);
      ++ret;
    }
    else
      prev = e;

    e = next;
  }
  return ret;
}

/* remove by pointer */
int qp_dllist_remove(struct qp_dllist *l,
    const void *val, int free_val)
{
  struct qp_dllist_entry *e;
  int ret = 0;
  ASSERT(l);
  ASSERT(*(l->ref_count) == 1);
  for(e=l->first; e;)
  {
    struct qp_dllist_entry *next;
    next = e->next;
    if(e->val == val)
    {
      if(e->prev)
        e->prev->next = e->next;
      if(e->next)
        e->next->prev = e->prev;

      if(l->first == e)
        l->first = e->next;
      if(l->last == e)
        l->last = e->prev;

      if(l->current == e)
        /* reset iterators */
        l->current = NULL;

      /* free it once */
      if(free_val && !ret)
        free(e->val);
      free(e);
      --(l->length);
      ASSERT(l->length != (size_t) -1);
      ++ret;
    }
    e = next;
  }
  return ret;
}

