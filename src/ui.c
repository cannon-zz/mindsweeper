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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "solver.h"
#include "mindsweeper.h"
#include "ui.h"
#include "minefield.h"


/*
 * Pixmap files
 */

/* the const hack changes the delcaration inside each .xpm from
 *	static char *blah[]
 * to
 *	const static char *blah[] 
 * which silences an "incompatible pointer" warning */
const
#include "pixmaps/about.xpm"
const
#include "digit_0.xpm"
const
#include "digit_1.xpm"
const
#include "digit_2.xpm"
const
#include "digit_3.xpm"
const
#include "digit_4.xpm"
const
#include "digit_5.xpm"
const
#include "digit_6.xpm"
const
#include "digit_7.xpm"
const
#include "digit_8.xpm"
const
#include "digit_9.xpm"
const
#include "gameover.xpm"
const
#include "guess.xpm"
const
#include "noguess.xpm"
const
#include "noanalysis.xpm"
const
#include "searching.xpm"
const
#include "stopped.xpm"


/*
 * Global variables
 */

#define  MINE_DIGITS  4
#define  TIME_DIGITS  4
#define  SPACING  5

static MineField  *gameboard;
static GdkPixbuf  *digit_pixbuf[10];
static GdkPixbuf  *gameover_pixbuf, *guess_pixbuf, *noguess_pixbuf;
static GdkPixbuf  *noanalysis_pixbuf, *searching_pixbuf, *stopped_pixbuf;
static GdkPixbuf  *about_pixbuf;
static GtkWidget  *game_number_label;
static GtkWidget  *status_img;
static GtkWidget  *mine_digits[MINE_DIGITS];
static GtkWidget  *time_digits[TIME_DIGITS];


/*
 * local_get_minefield() local_release_minefield()
 *
 * wrapper for the get_minefield() / release_minefield functions
 */

static void local_get_minefield(struct minefield_t *mf)
{
	ui_threads_leave();
	get_minefield(mf);
	ui_threads_enter();
}

static void local_release_minefield(struct minefield_t *mf)
{
	release_minefield(mf);
}


/*
 * focus_change()
 *
 * Updates the focus flag (used to start and stop the clock)
 */

static gint focus_change(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	state.focus = event->focus_change.in;
	if(state.clock_started && !state.focus && settings.pause_when_unfocused)
		gtk_widget_hide(GTK_WIDGET(gameboard));
	else
		gtk_widget_show(GTK_WIDGET(gameboard));
	return FALSE;
}


/*
 * ui_minefield_clear()
 *
 * Reset the minefield
 */

void ui_minefield_reset(void)
{
	minefield_reset(gameboard);
}


/*
 * ui_update_counter()
 *
 * updates a counter
 */

static void ui_update_counter(GtkWidget **digit, int digits, int count)
{
	int  i;

	if(settings.logmode)
		return;

	if(count < 0)
		count = 0;

	for(i = 0; i < digits; i++) {
		gtk_image_set_from_pixbuf(GTK_IMAGE(digit[i]), digit_pixbuf[count%10]);
		count /= 10;
	}
}

void ui_update_time_counter(int count)
{
	ui_update_counter(time_digits, TIME_DIGITS, count);
}

void ui_update_mine_counter(int count)
{
	ui_update_counter(mine_digits, MINE_DIGITS, count);
}


/*
 * ui_update_square_*()
 *
 * Update the data for a square.
 */

void ui_update_square_state(int row, int col, ui_square_state_t square_state)
{
	if(!settings.logmode)
		minefield_set_state(gameboard, row, col, square_state);
}

void ui_update_square_probability(int row, int col, float prob)
{
	minefield_set_probability(gameboard, row, col, prob);
}


/*
 * ui_update_game_number()
 *
 * Updates the game number indicator
 */

void ui_update_game_number(int number)
{
	char  buff[15];

	if(settings.logmode)
		return;

	sprintf(buff, "  %010d  ", number);
	gtk_label_set_text(GTK_LABEL(game_number_label), buff);
}


/*
 * ui_update_status()
 *
 * Updates the status indicator
 */

void ui_update_status(enum status_t status)
{
	state.search_status = status;

	if(settings.logmode)
		return;

	if(status == gameover)
		gtk_image_set_from_pixbuf(GTK_IMAGE(status_img), gameover_pixbuf);
	else if(!settings.analysis)
		gtk_image_set_from_pixbuf(GTK_IMAGE(status_img), noanalysis_pixbuf);
	else if(!state.won && !state.lost)
		switch(status) {
		case stopped:
			gtk_image_set_from_pixbuf(GTK_IMAGE(status_img), stopped_pixbuf);
			break;

		case searching:
			gtk_image_set_from_pixbuf(GTK_IMAGE(status_img), searching_pixbuf);
			break;

		case guess:
			gtk_image_set_from_pixbuf(GTK_IMAGE(status_img), guess_pixbuf);
			break;

		case noguess:
			gtk_image_set_from_pixbuf(GTK_IMAGE(status_img), noguess_pixbuf);
			break;

		case gameover:
			break;
		}
}


/*
 * read_pixmaps()
 *
 * reads the xpm data for all the images
 */

static gboolean read_pixmaps(void)
{
	digit_pixbuf[0] = gdk_pixbuf_new_from_xpm_data(digit_0_xpm);
	digit_pixbuf[1] = gdk_pixbuf_new_from_xpm_data(digit_1_xpm);
	digit_pixbuf[2] = gdk_pixbuf_new_from_xpm_data(digit_2_xpm);
	digit_pixbuf[3] = gdk_pixbuf_new_from_xpm_data(digit_3_xpm);
	digit_pixbuf[4] = gdk_pixbuf_new_from_xpm_data(digit_4_xpm);
	digit_pixbuf[5] = gdk_pixbuf_new_from_xpm_data(digit_5_xpm);
	digit_pixbuf[6] = gdk_pixbuf_new_from_xpm_data(digit_6_xpm);
	digit_pixbuf[7] = gdk_pixbuf_new_from_xpm_data(digit_7_xpm);
	digit_pixbuf[8] = gdk_pixbuf_new_from_xpm_data(digit_8_xpm);
	digit_pixbuf[9] = gdk_pixbuf_new_from_xpm_data(digit_9_xpm);
	gameover_pixbuf   = gdk_pixbuf_new_from_xpm_data(gameover_xpm);
	guess_pixbuf      = gdk_pixbuf_new_from_xpm_data(guess_xpm);
	noguess_pixbuf    = gdk_pixbuf_new_from_xpm_data(noguess_xpm);
	noanalysis_pixbuf = gdk_pixbuf_new_from_xpm_data(noanalysis_xpm);
	searching_pixbuf  = gdk_pixbuf_new_from_xpm_data(searching_xpm);
	stopped_pixbuf    = gdk_pixbuf_new_from_xpm_data(stopped_xpm);
	about_pixbuf    = gdk_pixbuf_new_from_xpm_data(about_xpm);
	return TRUE;
}


/*
 * local_pre_game()
 *
 * Local wrapper for pre_game().  Also acts as an event handler for the New
 * Game button.
 */

static void local_pre_game(GtkWidget *widget, gpointer data)
{
	local_get_minefield(&minefield);
	stats.played = stats.won = stats.lost = stats.guessed = 0;
	if(data)
		pre_game(&minefield, *(int *) data);
	else
		pre_game(&minefield, rand());
	local_release_minefield(&minefield);

	return;
}


/*
 * board_handler()
 *
 * Handles mouse clicks on the game board.
 */

static void board_handler(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	struct minefield_t  *mf = (struct minefield_t *) data;
	int  result;

	local_get_minefield(mf);
	result = make_move(mf, (int) event->x, (int) event->y, event->button, event->type);
	local_release_minefield(mf);

	if(result && !state.won && !state.lost)
		start_search();

	return;
}


/*
 * exit_handler()
 *
 * Handles the Exit menu item and any other close requests
 */

static void exit_handler(GtkObject *object, gpointer data)
{
	local_get_minefield(&minefield);
	gtk_main_quit();

	return;
}


/*
 * settings_handler()
 *
 * Handles everything to do with the settings window.
 */

enum settings_action_t {
	settings_act_create,
	settings_act_destroy,
	settings_act_ok,
	settings_act_cancel,
	settings_act_beg,
	settings_act_int,
	settings_act_adv,
	settings_act_bob,
	settings_act_density,
	settings_game_analysis
};

static void display_mine_density(GtkWidget *label, float density)
{
	char  buff[16];

	sprintf(buff, "Mines (%4.2f%%)", density);
	gtk_label_set_text(GTK_LABEL(label), buff);
}

static void board_size_presets(GtkObject *rows, GtkObject *cols, GtkObject *mines, gint level)
{
	int  default_rows[] = DEFAULT_ROWS;
	int  default_cols[] = DEFAULT_COLS;
	int  default_mines[] = DEFAULT_MINES;

	gtk_adjustment_set_value(GTK_ADJUSTMENT(rows), default_rows[level]);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(cols), default_cols[level]);
	gtk_adjustment_set_value(GTK_ADJUSTMENT(mines), default_mines[level]);
}

static gint settings_handler(GtkObject *object, gpointer action)
{
	static GtkWidget  *window_settings = NULL;
	static GtkObject  *rows, *cols, *mines, *cell_size;
	static GtkWidget  *number, *open, *analysis, *probabilities, *autoplay, *pause;
	static GtkWidget  *mines_label;
	GtkWidget  *widget, *hbox, *table;
	char  buff[14];
#ifdef DIAGNOSTICS
	static GtkWidget  *logmode;
#endif

	switch((enum settings_action_t) action) {
		/*
		 * Create the window
		 */

	case settings_act_create:
		if(window_settings) {
			gdk_window_raise(window_settings->window);
			break;
		}

		window_settings = gtk_dialog_new();
		gtk_window_set_title(GTK_WINDOW(window_settings), "Settings");
		gtk_container_set_border_width(GTK_CONTAINER(window_settings), SPACING);
		g_signal_connect(GTK_OBJECT(window_settings), "destroy", G_CALLBACK(settings_handler), (gpointer) settings_act_destroy);


		/* Default settings buttons */

		hbox = gtk_hbutton_box_new();
		gtk_box_set_spacing(GTK_BOX(hbox), SPACING);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->vbox), hbox, FALSE, FALSE, 0);

		widget = gtk_button_new_with_label("Beginner");
		g_signal_connect(GTK_OBJECT(widget), "clicked", G_CALLBACK(settings_handler), (gpointer) settings_act_beg);
		gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

		widget = gtk_button_new_with_label("Intermediate");
		g_signal_connect(GTK_OBJECT(widget), "clicked", G_CALLBACK(settings_handler), (gpointer) settings_act_int);
		gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

		widget = gtk_button_new_with_label("Advanced");
		g_signal_connect(GTK_OBJECT(widget), "clicked", G_CALLBACK(settings_handler), (gpointer) settings_act_adv);
		gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

		widget = gtk_button_new_with_label("Bobby Fischer");
		g_signal_connect(GTK_OBJECT(widget), "clicked", G_CALLBACK(settings_handler), (gpointer) settings_act_bob);
		gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);


		/* Board settings sliders and game number */

		table = gtk_table_new(5, 2, FALSE);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->vbox), table, FALSE, FALSE, 0);

		widget = gtk_label_new("Rows");
		gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 0, 1);

		rows = gtk_adjustment_new(minefield.rows, MIN_ROWS, MAX_ROWS, 1.0, 1.0, 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(rows));
		gtk_scale_set_digits(GTK_SCALE(widget), 0);
		gtk_range_set_update_policy(GTK_RANGE(widget), GTK_UPDATE_DISCONTINUOUS);
		gtk_widget_set_usize(GTK_WIDGET(widget), 150, 40);
		g_signal_connect(GTK_OBJECT(rows), "value_changed", G_CALLBACK(settings_handler), (gpointer) settings_act_density);
		gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, 0, 1);


		widget = gtk_label_new("Columns");
		gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 1, 2);

		cols = gtk_adjustment_new(minefield.cols, MIN_COLS, MAX_COLS, 1.0, 1.0, 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(cols));
		gtk_scale_set_digits(GTK_SCALE(widget), 0);
		gtk_range_set_update_policy(GTK_RANGE(widget), GTK_UPDATE_DISCONTINUOUS);
		gtk_widget_set_usize(GTK_WIDGET(widget), 150, 40);
		g_signal_connect(GTK_OBJECT(cols), "value_changed", G_CALLBACK(settings_handler), (gpointer) settings_act_density);
		gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, 1, 2);


		mines_label = gtk_label_new(NULL);
		display_mine_density(mines_label, 100.0*minefield.mines/minefield.rows/minefield.cols);
		gtk_table_attach_defaults(GTK_TABLE(table), mines_label, 0, 1, 2, 3);

		mines = gtk_adjustment_new(minefield.mines, 1.0, minefield.rows*minefield.cols*MAX_DENSITY, 1.0, 1.0, 0.0);
		widget = gtk_hscale_new(GTK_ADJUSTMENT(mines));
		gtk_scale_set_digits(GTK_SCALE(widget), 0);
		gtk_range_set_update_policy(GTK_RANGE(widget), GTK_UPDATE_DISCONTINUOUS);
		gtk_widget_set_usize(GTK_WIDGET(widget), 150, 40);
		g_signal_connect(GTK_OBJECT(mines), "value_changed", G_CALLBACK(settings_handler), (gpointer) settings_act_density);
		gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, 2, 3);


		widget = gtk_label_new("Cell Size");
		gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 3, 4);

		{
		GParamSpecInt *cell_size_param = G_PARAM_SPEC_INT(g_object_class_find_property(G_OBJECT_GET_CLASS(gameboard), "cell-size"));
		gint minefield_cell_size;
		g_object_get(G_OBJECT(gameboard), "cell-size", &minefield_cell_size, NULL);
		cell_size = gtk_adjustment_new(minefield_cell_size, cell_size_param->minimum, cell_size_param->maximum, 1.0, 1.0, 0.0);
		/* FIXME:  g_param_spec_unref(cell_size_param); ? */
		}
		widget = gtk_hscale_new(GTK_ADJUSTMENT(cell_size));
		gtk_scale_set_digits(GTK_SCALE(widget), 0);
		gtk_range_set_update_policy(GTK_RANGE(widget), GTK_UPDATE_DISCONTINUOUS);
		gtk_widget_set_usize(GTK_WIDGET(widget), 150, 40);
		gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, 3, 4);


		widget = gtk_label_new("Game number");
		gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 4, 5);

		number = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(number), 10);
		sprintf(buff, "%010d", minefield.number);
		gtk_entry_set_text(GTK_ENTRY(number), buff);
		gtk_table_attach_defaults(GTK_TABLE(table), number, 1, 2, 4, 5);


		/* Option buttons */

		open = gtk_check_button_new_with_label("Start with open region");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(open), settings.open);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->vbox), open, FALSE, FALSE, 0);

		analysis = gtk_check_button_new_with_label("Perform game board analysis");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(analysis), settings.analysis);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->vbox), analysis, FALSE, FALSE, 0);
		g_signal_connect(GTK_OBJECT(analysis), "toggled", G_CALLBACK(settings_handler), (gpointer) settings_game_analysis);

		probabilities = gtk_check_button_new_with_label("Show mine probabilities");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(probabilities), settings.show_probability && settings.analysis);
		gtk_widget_set_sensitive(GTK_WIDGET(probabilities), settings.analysis);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->vbox), probabilities, FALSE, FALSE, 0);

		autoplay = gtk_check_button_new_with_label("Autoplay");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoplay), settings.autoplay && settings.analysis);
		gtk_widget_set_sensitive(GTK_WIDGET(autoplay), settings.analysis);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->vbox), autoplay, FALSE, FALSE, 0);

		pause = gtk_check_button_new_with_label("Pause when window is not focused");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pause), settings.pause_when_unfocused);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->vbox), pause, FALSE, FALSE, 0);

#ifdef DIAGNOSTICS
		logmode = gtk_check_button_new_with_label("Disable graphics");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(logmode), settings.logmode);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->vbox), logmode, FALSE, FALSE, 0);
#endif /* DIAGNOSTICS */


		/* OK and Cancel buttons */

		widget = gtk_button_new_with_label("OK");
		g_signal_connect(GTK_OBJECT(widget), "clicked", G_CALLBACK(settings_handler), (gpointer) settings_act_ok);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->action_area), widget, TRUE, TRUE, 0);

		widget = gtk_button_new_with_label("Cancel");
		g_signal_connect(GTK_OBJECT(widget), "clicked", G_CALLBACK(settings_handler), (gpointer) settings_act_cancel);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window_settings)->action_area), widget, TRUE, TRUE, 0);


		/* Done creating window */

		gtk_widget_show_all(window_settings);
		break;


		/*
		 * Window being destroyed
		 */

	case settings_act_destroy:
		window_settings = NULL;
		break;


		/*
		 * Ok/Cancel buttons pressed
		 */

	case settings_act_ok:
		settings.open = GTK_TOGGLE_BUTTON(open)->active;
		settings.analysis = GTK_TOGGLE_BUTTON(analysis)->active;
		settings.show_probability = GTK_TOGGLE_BUTTON(probabilities)->active;
		settings.autoplay = GTK_TOGGLE_BUTTON(autoplay)->active;
		settings.pause_when_unfocused = GTK_TOGGLE_BUTTON(pause)->active;
#ifdef DIAGNOSTICS
		settings.logmode = GTK_TOGGLE_BUTTON(logmode)->active;
#endif /* DIAGNOSTICS */
		if(minefield.rows != GTK_ADJUSTMENT(rows)->value ||
		   minefield.cols != GTK_ADJUSTMENT(cols)->value) {
			minefield.rows = GTK_ADJUSTMENT(rows)->value;
			minefield.cols = GTK_ADJUSTMENT(cols)->value;
		}
		minefield.mines = GTK_ADJUSTMENT(mines)->value;
		minefield.number = atoi(gtk_entry_get_text(GTK_ENTRY(number)));
		g_object_set(
			G_OBJECT(gameboard),
			"cell-size", (gint) GTK_ADJUSTMENT(cell_size)->value,
			"rows", (gint) minefield.rows,
			"columns", (gint) minefield.cols,
			"has-tooltip", (gboolean) settings.analysis && settings.show_probability,
			NULL
		);
		local_pre_game(NULL, &minefield.number);

	case settings_act_cancel:
		gtk_widget_destroy(GTK_WIDGET(window_settings));
		break;


		/*
		 * Preset layout selected
		 */

	case settings_act_beg:
		board_size_presets(rows, cols, mines, BEGINNER);
		break;

	case settings_act_int:
		board_size_presets(rows, cols, mines, INTERMEDIATE);
		break;

	case settings_act_adv:
		board_size_presets(rows, cols, mines, EXPERT);
		break;

	case settings_act_bob:
		board_size_presets(rows, cols, mines, BOBBY);
		break;


		/*
		 * Mine density changed
		 */

	case settings_act_density:
		GTK_ADJUSTMENT(mines)->upper = GTK_ADJUSTMENT(rows)->value* GTK_ADJUSTMENT(cols)->value*MAX_DENSITY;
		gtk_adjustment_changed(GTK_ADJUSTMENT(mines));
		display_mine_density(mines_label, 100.0*GTK_ADJUSTMENT(mines)->value/(GTK_ADJUSTMENT(rows)->value*GTK_ADJUSTMENT(cols)->value));
		break;


		/*
		 * Game board analysis mode changed
		 */

	case settings_game_analysis:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(probabilities), settings.show_probability && GTK_TOGGLE_BUTTON(analysis)->active);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoplay), settings.autoplay && GTK_TOGGLE_BUTTON(analysis)->active);
		gtk_widget_set_sensitive(GTK_WIDGET(probabilities), GTK_TOGGLE_BUTTON(analysis)->active);
		gtk_widget_set_sensitive(GTK_WIDGET(autoplay), GTK_TOGGLE_BUTTON(analysis)->active);
		break;
	}

	return FALSE;
}


/*
 * About message
 */

static void about_message(GtkWidget *widget, gpointer data)
{
	static const gchar *authors[] = {
		"Kipp C. Cannon",
		NULL
	};
	gtk_show_about_dialog(
		NULL,
		"program-name", "MindSweeper",
		"version", PACKAGE_VERSION,
		"logo", about_pixbuf,
		"authors", authors,
		"copyright", "2002--2011",
		"website", "http://mindsweeper.sf.net",
		"license", "This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.",
		"wrap-license", TRUE,
		NULL
	);
}


/*
 * ui_init()
 *
 * Creates the main window and hooks up all the signal handlers
 */

void ui_init(void)
{
	int	i;
	GtkWidget	*window_main;
	GtkWidget	*frame;
	GtkWidget	*vbox, *hbox, *hbox1;
	GtkWidget	*menubar;
	GtkWidget	*widget;

	/*
	 * Initialize the pixbufs
	 */

	read_pixmaps();

	/*
	 * Create main window and set it's characteristics
	 */

	window_main = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(window_main), FALSE, FALSE, TRUE);
	gtk_window_set_title(GTK_WINDOW(window_main), "MindSweeper");
	gtk_container_set_border_width(GTK_CONTAINER(window_main), 0);
	g_signal_connect(GTK_OBJECT(window_main), "destroy", G_CALLBACK(exit_handler), NULL);
	g_signal_connect(GTK_OBJECT(window_main), "focus_in_event", G_CALLBACK(focus_change), NULL);
	g_signal_connect(GTK_OBJECT(window_main), "focus_out_event", G_CALLBACK(focus_change), NULL);

	gtk_hbutton_box_set_spacing_default(SPACING);

	/*
	 * Window structure is:
	 *    vbox:
	 *       menubar:
	 *          exit
	 *          settings
	 *          about
	 *       hbox:
	 *          frame:
	 *             hbox:
	 *                mine digit x 3
	 *          frame:
	 *             game number indicator
	 *          new game button
	 *          frame:
	 *             status indicator
	 *          frame:
	 *             hbox:
	 *                time digit x 3
	 *       hbox:
	 *          frame:
	 *             gameboard
	 */
	/* vbox for whole window */
	vbox = gtk_vbox_new(FALSE, SPACING);
	gtk_container_add(GTK_CONTAINER(window_main), vbox);

	/* menu bar */
	menubar = gtk_menu_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	widget = gtk_menu_item_new_with_mnemonic("E_xit!");
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), widget);
	g_signal_connect(GTK_OBJECT(widget), "activate", G_CALLBACK(exit_handler), NULL);

	widget = gtk_menu_item_new_with_mnemonic("_Settings");
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), widget);
	g_signal_connect(GTK_OBJECT(widget), "activate", G_CALLBACK(settings_handler), (gpointer) settings_act_create);

	widget = gtk_menu_item_new_with_mnemonic("_About");
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), widget);
	g_signal_connect(GTK_OBJECT(widget), "activate", G_CALLBACK(about_message), NULL);

	/* hbox for status row */
	hbox = gtk_hbox_new(FALSE, 2*SPACING);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), SPACING);

	/* mine counter */
	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, FALSE, 0);
	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);
	for(i = MINE_DIGITS-1; i >= 0; i--) {
		mine_digits[i] = gtk_image_new_from_pixbuf(digit_pixbuf[0]);
		gtk_box_pack_start(GTK_BOX(hbox1), mine_digits[i], FALSE, FALSE, 0);
	}

	/* status indicator */
	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, FALSE, 0);
	status_img = gtk_image_new_from_pixbuf(stopped_pixbuf);
	gtk_container_add(GTK_CONTAINER(frame), status_img);

	/* new game button */
	widget = gtk_button_new_with_label("New Game");
	g_signal_connect(GTK_OBJECT(widget), "clicked", G_CALLBACK(local_pre_game), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, FALSE, 0);

	/* game number indicator */
	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, FALSE, 0);
	game_number_label = gtk_label_new(NULL);
	gtk_container_add(GTK_CONTAINER(frame), game_number_label);

	/* timer */
	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, FALSE, 0);
	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);
	for(i = TIME_DIGITS-1; i >= 0; i--) {
		time_digits[i] = gtk_image_new_from_pixbuf(digit_pixbuf[0]);
		gtk_box_pack_start(GTK_BOX(hbox1), time_digits[i], FALSE, FALSE, 0);
	}

	/* minefield */
	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, TRUE, FALSE, 0);

	widget = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(widget), GTK_SHADOW_IN);
	gtk_container_set_border_width(GTK_CONTAINER(widget), SPACING);
	gtk_box_pack_start(GTK_BOX(hbox1), widget, TRUE, FALSE, 0);

	gameboard = g_object_new(
		MINEFIELD_TYPE,
		"rows", (gint) minefield.rows,
		"columns", (gint) minefield.cols,
		NULL
	);
	gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(gameboard));
	g_signal_connect(GTK_OBJECT(gameboard), "action", G_CALLBACK(board_handler), &minefield);

	/*
	 * Finally make the window contents visible (do this last to avoid
	 * redraws while constructing it).
	 */

	gtk_widget_show_all(window_main);

	local_pre_game(NULL, &minefield.number);
}
