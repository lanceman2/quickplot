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

#ifndef _QP_DEBUG_H_
#error "You must include qp_debug.h before this file."
#endif


#define QP_FOREACH_SLLIST(list, func)             \
  do {                                            \
    struct qp_sllist_entry *e;                    \
    ASSERT(list);                                 \
    e = (list)->first;                            \
    while(e) {                                    \
      (func)(e->val);                             \
      e = e->next;                                \
    }                                             \
  } while(0)

#define QP_FOREACH_DLLIST(list, func)             \
  do {                                            \
    struct qp_dllist_entry *e;                    \
    ASSERT(list);                                 \
    e = (list)->first;                            \
    while(e) {                                    \
      (func)(e->val);                             \
      e = e->next;                                \
    }                                             \
  } while(0)

#define QP_FOREACH_DLLIST_R(list, func)           \
  do {                                            \
    struct qp_dllist_entry *e;                    \
    ASSERT(list);                                 \
    e = (list)->last;                             \
    while(e) {                                    \
      (func)(e->val);                             \
      e = e->prev;                                \
    }                                             \
  } while(0)


/* singly linked list */
struct qp_sllist_entry
{
  struct qp_sllist_entry *next;
  void *val; /* cast to what you like */
};


/* doubly linked list */
struct qp_dllist_entry
{
  struct qp_dllist_entry *next;
  struct qp_dllist_entry *prev;
  void *val; /* cast to what you like */
};


/* singly linked list managing object */
struct qp_sllist
{
  struct qp_sllist_entry *first, *last, *current;
  size_t length;
  /* if *ref_count > 1 copies have been made and we cannot edit
   * (add or remove entries) the list until *ref_count gets to
   * one by destroying copies.  When *ref_count == 0 the list
   * entries are destroyed. In our use case it is more efficient
   * to stop writing when there is more than one copy then to
   * add another flag, counter, or pointer thing to the data
   * structures.  We do not need the extra functionality of
   * multi-object editing. We need speed and multi-object
   * reading. */
  int *ref_count;
};


/* doubly linked list managing object */
struct qp_dllist
{
  struct qp_dllist_entry *first, *last, *current;
  size_t length;
  /* if *ref_count > 1 copies have been made and we cannot edit
   * (add or remove entries) the list until *ref_count gets to
   * one by destroying copies.  When *ref_count == 0 the list
   * entries are destroyed. In our use case it is more efficient
   * to stop writing when there is more than one copy then to
   * add another flag, counter, or pointer thing to the data
   * structures.  We do not need the extra functionality of
   * multi-object editing. We need speed and multi-object
   * reading. */
  int *ref_count;
};


extern
struct qp_sllist *qp_sllist_create(struct qp_sllist *l);


extern
struct qp_dllist *qp_dllist_create(struct qp_dllist *l);

/** add val to the end */
extern
void qp_sllist_append(struct qp_sllist *l, void *val);

/** add val to the end */
extern
void qp_dllist_append(struct qp_dllist *l, void *val);

/** if free_vals is non zero all the elements will have
 * free(element) called on them. */
extern
void qp_sllist_destroy(struct qp_sllist *l, int free_vals);

/** if free_vals is non zero all the elements will have
 * free(element) called on them. */
extern
void qp_dllist_destroy(struct qp_dllist *l, int free_vals);

static inline
size_t qp_sllist_length(struct qp_sllist *l)
{
  return l->length;
}

static inline
size_t qp_dllist_length(struct qp_dllist *l)
{
  return l->length;
}

static inline
void *qp_sllist_val(struct qp_sllist *l)
{
  ASSERT(l);
  if(l->current)
    return l->current->val;
  return NULL;
}

static inline
void *qp_dllist_val(struct qp_dllist *l)
{
  ASSERT(l);
  if(l->current)
    return l->current->val;
  return NULL;
}

/** moves current forward and then get val
 * \return val or NULL if at the end
 */
static inline
void *qp_sllist_next(struct qp_sllist *l)
{
  ASSERT(l);
  if(l->current && l->current->next)
  {
    l->current = l->current->next;
    return l->current->val;
  }
  else if(l->current)
    l->current = NULL;
  return NULL;
}


/** moves current forward and then get current val
 * \return val or NULL if at the end
 */
static inline
void *qp_dllist_next(struct qp_dllist *l)
{
  ASSERT(l);
  if(l->current && l->current->next)
  {
    l->current = l->current->next;
    return l->current->val;
  }
  else if(l->current)
    l->current = NULL;
  return NULL;
}


/** moves current back and then get current val 
 * \return val or NULL if at the begining
 */
static inline
void *qp_dllist_prev(struct qp_dllist *l)
{
  ASSERT(l);
  if(l->current && l->current->prev)
  {
    l->current = l->current->prev;
    return l->current->val;
  }
  else if(l->current)
    l->current = NULL;
  return NULL;
}

/** move to the begining
 * \return the first val
 */
static inline
void *qp_sllist_begin(struct qp_sllist *l)
{
  ASSERT(l);
  l->current = l->first;
  if(l->current)
    return l->current->val;
  return NULL;
}

/** move to the begining
 * \return the first val
 */
static inline
void *qp_dllist_begin(struct qp_dllist *l)
{
  ASSERT(l);
  l->current = l->first;
  if(l->current)
    return l->current->val;
  return NULL;
}

/** move to the ending
 * \return the last val
 */
static inline
void *qp_dllist_end(struct qp_dllist *l)
{
  ASSERT(l);
  l->current = l->last;
  if(l->current)
    return l->current->val;
  return NULL;
}

/**
 * \return the first val
 */
static inline
void *qp_sllist_first(struct qp_sllist *l)
{
  ASSERT(l);
  if(l->first)
    return l->first->val;
  return NULL;
}

/**
 * \return the first val
 */
static inline
void *qp_dllist_first(struct qp_dllist *l)
{
  ASSERT(l);
  if(l->first)
    return l->first->val;
  return NULL;
}

/**
 * \return the last val
 */
static inline
void *qp_sllist_last(struct qp_sllist *l)
{
  ASSERT(l);
  if(l->last)
    return l->last->val;
  return NULL;
}

/**
 * \return the last val
 */
static inline
void *qp_dllist_last(struct qp_dllist *l)
{
  ASSERT(l);
  if(l->last)
    return l->last->val;
  return NULL;
}

/* returns the number found */
extern
int qp_sllist_find(struct qp_sllist *l, void *val);

/* returns the number found */
extern
int qp_dllist_find(struct qp_dllist *l, void *val);


/** remove by pointer
 * This will remove all found.
 *
 * If you are sure that the current value last returned
 * from qp_sllist_next() is not the one you are removing
 * you can continue iterating with qp_sllist_next(),
 * otherwise the interator will be reset and 
 * qp_sllist_next() will return after this call.
 *
 * If the value is in the list more than once
 * it will be freed only once.

 * \return the number of values found and removed. */
extern
int qp_sllist_remove(struct qp_sllist *l,
    const void *val, int free_val);

/** remove by pointer
 * This will remove all found.
 * 
 * If you are sure that the current value last returned
 * from qp_sllist_next() is not the one you are removing
 * you can continue iterating with qp_sllist_next(),
 * otherwise the interator will be reset and 
 * qp_sllist_next() will return after this call.
 *
 * If the value is in the list more than once
 * it will be freed only once.
 * \return the number of values found and removed. */
extern
int qp_dllist_remove(struct qp_dllist *l,
    const void *val, int free_val);

