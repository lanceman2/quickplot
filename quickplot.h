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

/** installed library header file for libquickplot */


#ifndef _QUICKPLOT_H_
#define _QUICKPLOT_H_



#include <sys/types.h>
#include <gtk/gtk.h>


#define QP_CHANNEL_FORM_SERIES 0
#define QP_CHANNEL_FORM_FUNC   1


/* quickplots stored buffer data types.
 * We save memory space by using the
 * smallest type needed to hold the
 * values that are plotted.  We may
 * never use some of these. */
#define QP_TYPE_UNKNOWN            0
#define QP_TYPE_SHORT              1
#define QP_TYPE_INT                2
#define QP_TYPE_LONG               3
#define QP_TYPE_LONG_LONG          4
#define QP_TYPE_UNSIGNED_SHORT     5
#define QP_TYPE_UNSIGNED_INT       6
#define QP_TYPE_UNSIGNED_LONG      7
#define QP_TYPE_UNSIGNED_LONG_LONG 8
#define QP_TYPE_FLOAT              9
#define QP_TYPE_DOUBLE            10
#define QP_TYPE_LONG_DOUBLE       11
/* A source can have multiple types,
 * different types in different channels. */
#define QP_TYPE_MULTIPLE          12
/* Added to make coding easier */
#define QP_TYPE_MAX               12


/** \brief the object that holds all quickplot state data
 */
typedef struct qp_win *qp_win_t;

/** \brief holds x,y channels to plot
 */
typedef struct qp_plot *qp_plot_t;

/** \brief holds all loaded data or functions from a file or
 * other source
 */
typedef struct qp_source *qp_source_t;

/** \brief holds a series of values or function that can be plotted
 */
typedef struct qp_channel *qp_channel_t;

/** \brief hold a list of plots to put on a graph 
 */
typedef struct qp_graph *qp_graph_t;



#ifdef __cplusplus
extern "C"
{
#endif


extern
qp_win_t qp_win_create(void);

extern
void qp_win_destroy(qp_win_t qp);

/** /return the number of channels created from this */
extern
size_t qp_app_read(const char *filename);


/**
 * x and y are channel numbers starting at 0
 * \return 0 on success and 1 on failure */
extern
int qp_win_graph(qp_win_t qp, const ssize_t *x, const ssize_t *y,
    size_t num, const char *name);

extern
void qp_win_graph_default(qp_win_t qp);

extern
int qp_win_graph_default_source(qp_win_t qp, qp_source_t s, const char *name);


/** \return 0 on success, -1 if it was initialized already
 * and 1 on failure. */
extern
int qp_app_init(int *argc, char ***argv);


#ifdef  __cplusplus
}
#endif

#endif /* #ifndef _QUICKPLOT_H_ */


