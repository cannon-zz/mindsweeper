/*
 * Mindsweeper
 *
 * Copyright (C) 2000 Kipp C. Cannon
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/*
 * Game board widget description
 */


#ifndef _MINEFIELD_H_
#define _MINEFIELD_H_


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktooltips.h>


#define MINEFIELD(obj)		GTK_CHECK_CAST(obj, minefield_get_type(), MineField)
#define MINEFIELD_CLASS(klass)	GTK_CHECK_CLASS_CAST(klass, minefield_get_type(), MineFieldClass)
#define IS_MINEFIELD(obj)	GTK_CHECK_TYPE(obj, minefield_get_type())


typedef struct {
	GtkWidgetClass  parent_class;

	void (*action)  (GdkEventButton *);	/* FIXME: make custom struct */
} MineFieldClass;


typedef enum {
	MINEFIELD_MINES_0 = 0,
	MINEFIELD_MINES_1,
	MINEFIELD_MINES_2,
	MINEFIELD_MINES_3,
	MINEFIELD_MINES_4,
	MINEFIELD_MINES_5,
	MINEFIELD_MINES_6,
	MINEFIELD_MINES_7,
	MINEFIELD_MINES_8,
	MINEFIELD_UNMARKED,
	MINEFIELD_BOOM,
	MINEFIELD_FLAGGED,
	MINEFIELD_MINED,
	MINEFIELD_WRONG
} MineFieldState;


typedef struct {
	GtkWidget  widget;

	gint  cell_size;
	gint  rows;
	gint  columns;

	gint  tip_row;
	gint  tip_column;
	GtkTooltips  *cell_tips;

	PangoLayout  *numeral[8];

	MineFieldState *cell;
	gfloat  *probability;
} MineField;


guint minefield_get_type(void);
MineField *minefield_new(gint, gint);
void minefield_reset(MineField *);
void minefield_set_cell_size(MineField *, gint);
gint minefield_get_cell_size(MineField *);
void minefield_set_board_size(MineField *, gint, gint);
void minefield_set_state(MineField *, gint, gint, MineFieldState);
void minefield_set_probability(MineField *, gint, gint, gfloat);
gfloat minefield_get_probability(MineField *, gint, gint);
void minefield_set_probabilities_visible(MineField *, gint);
gint minefield_get_probabilities_visible(MineField *);
MineFieldState minefield_get_state(MineField *, gint, gint);


extern gint MineFieldMinCellSize;


#endif /* _MINEFIELD_H_ */
