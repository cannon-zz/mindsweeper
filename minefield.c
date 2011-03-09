/*
 * Mindsweeper
 *
 * This implements the playing field as a GTK widget.  This is different
 * from the widget implemented in the gnomines game in that this widget
 * does not handle any game logic itself.  It is purely an I/O aparatus
 * that requires a full game-play back-end for operation.
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
 * Synopsis:
 *
 * #include "minefield.h"
 *
 * MineField *minefield_new(gint rows, gint columns);
 * void minefield_set_cell_size(MineField *minefield, gint size);
 * void minefield_set_board_size(MineField *minefield, gint rows, gint columns);
 * void minefield_set_state(MineField *minefield, gint row, gint column,
 *                          MineFieldState state);
 * MineFieldState minefield_get_state(MineField *minefield, gint row,
 *                                    gint column);
 *
 * Signals:
 *
 * "action" 
 *
 * Description:
 *
 * A call to minefield_new() returns a new MineField object of the given
 * size.  The cell size of the minefield can be adjusted via
 * minefield_set_cell_size().  A MineField's size can be adjusted by a call
 * to minefield_set_board_size() which will also reset all the cells to the
 * MINEFIELD_UNMARKED state.  The MineField widget does not implement any
 * game logic.  Rather, it emits the signal "action" whenever a relevent
 * mouse event occurs.  The game developer must write an appropriate
 * back-end to handle game logic and update the MineField's graphics
 * through calls to minefield_set_state().  The current state of a cell on
 * the MineField can be retrieved with a call to minefield_get_state().
 * Row and column co-ordinates start with 1,1 in the top-left corner of the
 * game board (this might change in the future).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "minefield.h"

/*
 * Cell graphics
 */

#include "mf_boom.xpm"
#include "mf_flagged.xpm"
#include "mf_mined.xpm"
#include "mf_wrong.xpm"

#define XDEMINEUR_COLOURS_RED   {       0,       0,      0, 0xffff, 0x3535, 0xcccc,      0, 0x9191, 0xc4c4};
#define XDEMINEUR_COLOURS_GREEN {       0,       0, 0xffff,      0,      0,      0, 0x9999,      0, 0xc4c4};
#define XDEMINEUR_COLOURS_BLUE  {       0,  0xffff,      0,      0, 0x9191, 0xcccc,      0,      0, 0xc4c4};

#define GNOMINES_COLOURS_RED    { 0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0xa0a0, 0x0000, 0xa0a0, 0x0000 }
#define GNOMINES_COLOURS_GREEN  { 0x0000, 0x0000, 0xa0a0, 0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000 }
#define GNOMINES_COLOURS_BLUE   { 0x0000, 0xffff, 0x0000, 0x0000, 0x7fff, 0x0000, 0xffff, 0xa0a0, 0x0000 }

#define COLOURS_RED	XDEMINEUR_COLOURS_RED
#define COLOURS_GREEN	XDEMINEUR_COLOURS_GREEN
#define COLOURS_BLUE	XDEMINEUR_COLOURS_BLUE
#define MAX_CELL_SIZE  40


/*
 * Data
 */

enum minefield_signals_t {
	MINEFIELD_ACTION = 0
};

static guint minefield_signal[1] = { 0 } ;      /* signal array */
static GdkPixmap *pixmap[4];                    /* pixmaps for playing field */
static GdkBitmap *mask[4];                      /* clip masks for pictures */
static GtkFixedClass *parent_class = NULL;	/* parent class */
gint MineFieldMinCellSize = 15;


/*
 * Misc functions
 */

static MineFieldState *cell_state(MineField *minefield, gint row, gint column)
{
	return &minefield->cell[row*minefield->columns + column];
}


static gfloat *cell_probability(MineField *minefield, gint row, gint column)
{
	return &minefield->probability[row*minefield->columns + column];
}


static PangoLayout **minefield_numeral(MineField *minefield, gint n)
{
	return &minefield->numeral[n - 1];
}


static void free_cells(MineField *minefield)
{
	free(minefield->cell);
	minefield->cell = NULL;
}


static void free_probabilities(MineField *minefield)
{
	free(minefield->probability);
	minefield->probability = NULL;
}


static void attr_list_insert(PangoAttrList *attrlist, PangoAttribute *attr)
{
	attr->start_index = 0;
	attr->end_index = G_MAXINT;
	pango_attr_list_insert(attrlist, attr);
}


/*
 * Widget graphics
 */

static void paint_numeral(MineField *minefield, gint n, gint x, gint y, gint width, gint height, GdkRectangle *area)
{
	GtkWidget  *widget = GTK_WIDGET(minefield);
	PangoLayout  *layout = *minefield_numeral(minefield, n);
	gint  text_width, text_height;

	pango_layout_get_pixel_size(layout, &text_width, &text_height);

	x += (width - text_width)/2;
	y += (height - text_height)/2;

	gdk_draw_layout(widget->window, widget->style->black_gc, x, y, layout);
}


static void paint_pixmap(MineField *minefield, GdkPixmap *pixmap, GdkBitmap *mask, gint x, gint y, gint width, gint height, GdkRectangle *area)
{
	GtkWidget  *widget = GTK_WIDGET(minefield);
	GdkGC  *gc = widget->style->black_gc;
	gint  graphic_width, graphic_height;

	gdk_drawable_get_size(GDK_DRAWABLE(pixmap), &graphic_width, &graphic_height);

	x += (width - graphic_width)/2;
	y += (height - graphic_height)/2;
	
	gdk_gc_set_clip_mask(gc, mask);
	gdk_gc_set_clip_origin(gc, x, y);
	gdk_draw_drawable(widget->window, gc, pixmap, 0, 0, x, y, graphic_width, graphic_height);
	gdk_gc_set_clip_mask(gc, NULL);
}


static void paint_cell(MineField *minefield, gint row, gint column, GdkRectangle *area)
{
	GtkWidget  *widget = GTK_WIDGET(minefield);
	MineFieldState  state = *cell_state(minefield, row, column);
	gint  width = minefield->cell_size;
	gint  height = minefield->cell_size;
	gint  x = column * width;
	gint  y = row * height;

	/* Draw the cell's outline */

	switch(state) {
	case MINEFIELD_MINES_0:
	case MINEFIELD_MINES_1:
	case MINEFIELD_MINES_2:
	case MINEFIELD_MINES_3:
	case MINEFIELD_MINES_4:
	case MINEFIELD_MINES_5:
	case MINEFIELD_MINES_6:
	case MINEFIELD_MINES_7:
	case MINEFIELD_MINES_8:
	case MINEFIELD_MINED:
	case MINEFIELD_BOOM:
	case MINEFIELD_WRONG:
		gtk_paint_box(widget->style, widget->window, GTK_WIDGET_STATE(widget), GTK_SHADOW_NONE, area, widget, NULL, x, y, width, height);
		break;

	case MINEFIELD_FLAGGED:
	case MINEFIELD_UNMARKED:
		gtk_paint_box(widget->style, widget->window, GTK_WIDGET_STATE(widget), GTK_SHADOW_OUT, area, widget, NULL, x, y, width, height);
		break;
	}

	/* Draw the cell's contents */

	switch(state) {
	case MINEFIELD_MINES_1:
	case MINEFIELD_MINES_2:
	case MINEFIELD_MINES_3:
	case MINEFIELD_MINES_4:
	case MINEFIELD_MINES_5:
	case MINEFIELD_MINES_6:
	case MINEFIELD_MINES_7:
	case MINEFIELD_MINES_8:
		paint_numeral(minefield, state - MINEFIELD_MINES_0, x, y, width, height, area);
		break;

	case MINEFIELD_BOOM:
	case MINEFIELD_FLAGGED:
	case MINEFIELD_MINED:
	case MINEFIELD_WRONG:
		paint_pixmap(minefield, pixmap[state - MINEFIELD_BOOM], mask[state - MINEFIELD_BOOM], x, y, width, height, area);
		break;

	default:
		break;
	}
}


static void paint_probability(MineField *minefield, gint row, gint column)
{
	gfloat  probability = *cell_probability(minefield, row, column);
	gchar  message[20];  /* should be big enough */

	if(!minefield->cell_tips->enabled)
		return;
	else if(*cell_state(minefield, row, column) != MINEFIELD_UNMARKED)
		sprintf(message, "Decided");
	else if(probability >= 0.0)
		sprintf(message, "%f", probability);
	else
		sprintf(message, "Unknown");

	gtk_tooltips_set_tip(minefield->cell_tips, GTK_WIDGET(minefield), message, NULL);
}


static void construct_numerals(MineField *minefield)
{
	PangoLayout  *layout;
	PangoAttrList  *attrlist;
	gint  red[]   = COLOURS_RED;
	gint  green[] = COLOURS_GREEN;
	gint  blue[]  = COLOURS_BLUE;
	gchar  *string[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8" };
	gint  i;

	for(i = 1; i < 9; i++) {
		attrlist = pango_attr_list_new();
		attr_list_insert(attrlist, pango_attr_size_new(minefield->cell_size * PANGO_SCALE * 0.75));
		attr_list_insert(attrlist, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
		attr_list_insert(attrlist, pango_attr_foreground_new(red[i], green[i], blue[i]));
		*minefield_numeral(minefield, i) = layout = gtk_widget_create_pango_layout(GTK_WIDGET(minefield), string[i]);
		pango_layout_set_attributes(layout, attrlist);
		pango_attr_list_unref(attrlist);
	}
}


static void free_numerals(MineField *minefield)
{
	gint  i;

	for(i = 1; i < 9; i++) {
		g_object_unref(*minefield_numeral(minefield, i));
		*minefield_numeral(minefield, i) = NULL;
	}
}


/*
 * ============================================================================
 *
 *                          Widget Method Overrides
 *
 * ============================================================================
 */


static void realize(GtkWidget *widget)
{
	GdkWindowAttr attributes;

	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	attributes = (GdkWindowAttr) {
		.window_type = GDK_WINDOW_CHILD,
		.x = widget->allocation.x,
		.y = widget->allocation.y,
		.width = widget->allocation.width,
		.height = widget->allocation.height,
		.wclass = GDK_INPUT_OUTPUT,
		.visual = gtk_widget_get_visual(widget),
		.colormap = gtk_widget_get_colormap(widget),
		.event_mask = gtk_widget_get_events(widget) | GDK_DESTROY | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK
	};

	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP);

	gdk_window_set_user_data(widget->window, widget);

	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);
}


static void size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	MineField  *minefield = MINEFIELD(widget);

	requisition->width = minefield->columns * minefield->cell_size;
	requisition->height = minefield->rows * minefield->cell_size;
}


static gboolean button_press_release_event(GtkWidget *widget, GdkEventButton *event)
{
	MineField  *minefield = MINEFIELD(widget);

	event->x = (event->x / minefield->cell_size) + 1;
	event->y = (event->y / minefield->cell_size) + 1;
	event->type = event->type == GDK_BUTTON_PRESS;

	gtk_signal_emit(GTK_OBJECT(widget), minefield_signal[MINEFIELD_ACTION], event);

	return FALSE;
}


static gboolean destroy_event(GtkWidget *widget, GdkEventAny *event)
{
	MineField  *minefield = MINEFIELD(widget);

	free_cells(minefield);
	free_probabilities(minefield);
	free_numerals(minefield);
	gtk_widget_destroy(GTK_WIDGET(minefield->cell_tips));

	/* FIXME: need to chain? */
	return GTK_WIDGET_CLASS(parent_class)->destroy_event(widget, event);
}


static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event)
{
	MineField  *minefield = MINEFIELD(widget);
	gint  cell_size = minefield->cell_size;
	gint  first_column = event->area.x / cell_size;
	gint  last_column = (event->area.x + event->area.width - 1) / cell_size;
	gint  first_row = event->area.y / cell_size;
	gint  last_row = (event->area.y + event->area.height - 1) / cell_size;
	gint  i, j;

	if(GTK_WIDGET_DRAWABLE(widget)) {
		for(i = first_row; i <= last_row; i++)
			for(j = first_column; j <= last_column; j++)
				paint_cell(minefield, i, j, NULL);
	}

	return FALSE;
}


static gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event)
{
	MineField  *minefield = MINEFIELD(widget);
	gint  row = event->y / minefield->cell_size;
	gint  column = event->x / minefield->cell_size;

	if(row != minefield->tip_row || column != minefield->tip_column) {
		minefield->tip_row = row;
		minefield->tip_column = column;
		paint_probability(minefield, row, column);
	}

	return FALSE;
}


/*
 * ============================================================================
 *
 *                                 Properties
 *
 * ============================================================================
 */


enum property {
	ARG_CELL_SIZE = 1
};


static void set_property(GObject *object, enum property id, const GValue *value, GParamSpec *pspec)
{
	MineField *minefield = MINEFIELD(object);

	switch(id) {
	case ARG_CELL_SIZE: {
		gint size = g_value_get_int(value);
		g_return_if_fail(size >= MineFieldMinCellSize);
		if(minefield->cell_size != size) {
			minefield->cell_size = size;
			free_numerals(minefield);
			construct_numerals(minefield);
			gtk_widget_queue_resize(GTK_WIDGET(minefield));
		}
		break;
	}

	}
}


static void get_property(GObject *object, enum property id, GValue *value, GParamSpec *pspec)
{
	MineField *minefield = MINEFIELD(object);

	switch(id) {
	case ARG_CELL_SIZE:
		g_value_set_int(value, minefield->cell_size);
		break;

	}
}


/*
 * ============================================================================
 *
 *                          Object Method Overrides
 *
 * ============================================================================
 */


static void class_init(MineFieldClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GdkColormap  *colourmap;
	gint  i, width, height;

	colourmap = gdk_colormap_get_system();

	pixmap[MINEFIELD_BOOM - MINEFIELD_BOOM]    = gdk_pixmap_colormap_create_from_xpm_d(NULL, colourmap, &mask[MINEFIELD_BOOM - MINEFIELD_BOOM], NULL, ms_boom_xpm);
	pixmap[MINEFIELD_FLAGGED - MINEFIELD_BOOM] = gdk_pixmap_colormap_create_from_xpm_d(NULL, colourmap, &mask[MINEFIELD_FLAGGED - MINEFIELD_BOOM], NULL, ms_flagged_xpm);
	pixmap[MINEFIELD_MINED - MINEFIELD_BOOM]   = gdk_pixmap_colormap_create_from_xpm_d(NULL, colourmap, &mask[MINEFIELD_MINED - MINEFIELD_BOOM], NULL, ms_mined_xpm);
	pixmap[MINEFIELD_WRONG - MINEFIELD_BOOM]   = gdk_pixmap_colormap_create_from_xpm_d(NULL, colourmap, &mask[MINEFIELD_WRONG - MINEFIELD_BOOM], NULL, ms_wrong_xpm);

	for(i = 0; i < 4; i++) {
		gdk_drawable_get_size(GDK_DRAWABLE(pixmap[i]), &width, &height);
		if(width > MineFieldMinCellSize)
			MineFieldMinCellSize = width;
		if(height > MineFieldMinCellSize)
			MineFieldMinCellSize = height;
	}

	/*
	 * Signals
	 */

	klass->action = NULL;

	minefield_signal[MINEFIELD_ACTION] = gtk_signal_new(
		"action",
		GTK_RUN_FIRST,
		GTK_CLASS_TYPE(GTK_OBJECT_CLASS(klass)),
		GTK_SIGNAL_OFFSET(MineFieldClass, action),
		gtk_marshal_NONE__POINTER,
		GTK_TYPE_NONE,
		1,
		GDK_TYPE_EVENT
	);

	/*
	 * Object methods
	 */

	object_class->set_property = set_property;
	object_class->get_property = get_property;

	g_object_class_install_property(
		object_class,
		ARG_CELL_SIZE,
		g_param_spec_int(
			"cell-size",
			"cell size",
			"Size of each minefield cell in pixels.",
			MineFieldMinCellSize, MAX_CELL_SIZE, MineFieldMinCellSize,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
		)
	);

	/*
	 * Widget methods
	 */

	parent_class = g_type_class_peek_parent(klass);

	widget_class->realize = realize;
	widget_class->size_request = size_request;
	widget_class->button_press_event = button_press_release_event;
	widget_class->button_release_event = button_press_release_event;
	widget_class->destroy_event = destroy_event;
	widget_class->expose_event = expose_event;
	widget_class->motion_notify_event = motion_notify_event;
}


static void object_init(MineField *minefield)
{
	GTK_WIDGET_UNSET_FLAGS(minefield, GTK_NO_WINDOW);

	minefield->cell_size = MineFieldMinCellSize;
	minefield->rows = 0;
	minefield->columns = 0;
	minefield->tip_row = -1;
	minefield->tip_column = -1;
	minefield->cell = NULL;
	minefield->probability = NULL;

	minefield->cell_tips = gtk_tooltips_new();
	gtk_tooltips_disable(minefield->cell_tips);

	construct_numerals(minefield);
}


/*
 * ============================================================================
 *
 *                                Entry Point
 *
 * ============================================================================
 */


guint minefield_get_type(void)
{
	static guint minefield_type = 0;
	GtkTypeInfo minefield_info;

	if(!minefield_type) {
		minefield_info = (GtkTypeInfo) {
			.type_name = "MineField",
			.object_size = sizeof(MineField),
			.class_size = sizeof(MineFieldClass),
			.class_init_func = (GtkClassInitFunc) class_init,
			.object_init_func = (GtkObjectInitFunc) object_init
		};
		minefield_type = gtk_type_unique(gtk_widget_get_type(), &minefield_info);
	}

	return minefield_type;
}


/*
 * ============================================================================
 *
 *                              Exported Methods
 *
 * ============================================================================
 */


static void set_state(MineField *minefield, gint row, gint column, MineFieldState state)
{
	MineFieldState  *current_state = cell_state(minefield, row, column);
	gint  cell_size = minefield->cell_size;
	gint  x = column * cell_size;
	gint  y = row * cell_size;

	if(*current_state != state) {
		*current_state = state;
		gtk_widget_queue_draw_area(GTK_WIDGET(minefield), x, y, cell_size, cell_size);
	}
}


void minefield_set_state(MineField *minefield, gint row, gint column, MineFieldState state)
{
	g_return_if_fail(IS_MINEFIELD(minefield) && row >= 1 && row <= minefield->rows && column >= 1 && column <= minefield->columns && state >= 0 && state <= MINEFIELD_WRONG);

	set_state(minefield, row - 1, column - 1, state);
}


MineFieldState minefield_get_state(MineField *minefield, gint row, gint column)
{
	g_return_val_if_fail(IS_MINEFIELD(minefield) && row >= 1 && row <= minefield->rows && column >= 1 && column <= minefield->columns, MINEFIELD_UNMARKED);

	return *cell_state(minefield, row - 1, column - 1);
}


/*
 * Application interface:  get/set mine probabilities
 */


static void set_probability(MineField *minefield, gint row, gint column, gfloat probability)
{
	*cell_probability(minefield, row, column) = probability;
}


void minefield_set_probability(MineField *minefield, gint row, gint column, gfloat probability)
{
	g_return_if_fail(IS_MINEFIELD(minefield) && row >= 1 && row <= minefield->rows && column >= 1 && column <= minefield->columns && probability <= 1.0);

	set_probability(minefield, row - 1, column - 1, probability);
}


gfloat minefield_get_probability(MineField *minefield, gint row, gint column)
{
	g_return_val_if_fail(IS_MINEFIELD(minefield) && row >= 1 && row <= minefield->rows && column >= 1 && column <= minefield->columns, -1.0);

	return *cell_probability(minefield, row - 1, column - 1);
}


void minefield_set_probabilities_visible(MineField *minefield, gint truefalse)
{
	g_return_if_fail(IS_MINEFIELD(minefield));

	if(truefalse)
		gtk_tooltips_enable(minefield->cell_tips);
	else
		gtk_tooltips_disable(minefield->cell_tips);
}


gint minefield_get_probabilities_visible(MineField *minefield)
{
	g_return_val_if_fail(IS_MINEFIELD(minefield), 0);

	return minefield->cell_tips->enabled;
}


/*
 * Application interface:  reset the game board
 */


static void reset(MineField *minefield)
{
	gint  row, col;

	for(row = 0; row < minefield->rows; row++)
		for(col = 0; col < minefield->columns; col++) {
			set_state(minefield, row, col, MINEFIELD_UNMARKED);
			set_probability(minefield, row, col, -1.0);
		}
}


void minefield_reset(MineField *minefield)
{
	g_return_if_fail(IS_MINEFIELD(minefield));

	reset(minefield);
}


/*
 * Application interface:  adjust the size of a minefield.
 */


static void set_board_size(MineField *minefield, gint rows, gint columns)
{
	if((minefield->rows != rows) || (minefield->columns != columns)) {
		minefield->rows = 0;
		minefield->columns = 0;
		free_cells(minefield);
		free_probabilities(minefield);
		minefield->cell = calloc(rows * columns, sizeof(*minefield->cell));
		g_return_if_fail(minefield->cell != NULL);
		minefield->probability = malloc(rows * columns * sizeof(*minefield->probability));
		minefield->rows = rows;
		minefield->columns = columns;

		gtk_widget_queue_resize(GTK_WIDGET(minefield));
	}

	reset(minefield);
}

void minefield_set_board_size(MineField *minefield, gint rows, gint columns)
{
	g_return_if_fail(IS_MINEFIELD(minefield));

	set_board_size(minefield, rows, columns);
}


/*
 * Application interface:  create a new minefield
 */


MineField *minefield_new(gint rows, gint columns)
{
	MineField  *minefield;

	minefield = gtk_type_new(minefield_get_type());
	if(minefield)
		set_board_size(minefield, rows, columns);

	return minefield;
}
