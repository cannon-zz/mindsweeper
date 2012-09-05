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
#include <stdlib.h>
#include "solver.h"
#include "mindsweeper.h"
#include "ui.h"

struct state  state;
struct minefield_t  minefield;
struct settings  settings;
struct stats  stats;


/*
 * update_square()
 *
 * tells the UI to set the graphic for the (row,col)th square.
 */

static void update_square(struct minefield_t *mf, int col, int row)
{
	ui_square_state_t square_state;

	if(minefield_cell(mf, col, row)->is_cleared) {
		if(minefield_cell(mf, col, row)->is_mine)
			square_state = MINEFIELD_BOOM;
		else
			square_state = minefield_cell(mf, col, row)->minesaround;
	} else if(minefield_cell(mf, col, row)->is_flagged) {
		if(state.lost && !minefield_cell(mf, col, row)->is_mine)
			square_state = MINEFIELD_WRONG;
		else
			square_state = MINEFIELD_FLAGGED;
	} else if(minefield_cell(mf, col, row)->is_pressed)
		square_state = MINEFIELD_MINES_0;
	else if(state.lost && minefield_cell(mf, col, row)->is_mine)
		square_state = MINEFIELD_MINED;
	else
		square_state = MINEFIELD_UNMARKED;

	ui_update_square_state(row, col, square_state);
}


/*
 * press_square() unpress_square()
 *
 * Gets a square into/out of the pressed state.  OK to unpress a square
 * without having previously pressed it.
 *
 * clear_square()
 *
 * Clears the square at (i,j) and opens up any regions of 0's that are
 * exposed.  Flagged squares are not cleared, nor are any squares in the
 * border region.  If the square being cleared is a mine, then the game has
 * been lost.  Returns the total number of squares cleared or < 0 if the
 * game was lost.
 *
 * clear_around()
 *
 * This function clears all of the square at (i,j)'s neighbours if it has
 * exactly as many flags around it as it does mines.  This doesn't check if
 * (i,j) is actually cleared or not.  Returns the number of squares
 * cleared.
 *
 * toggle_flag()
 *
 * This flags and unflags squares.  It is a separate function because,
 * unlike other properites of squares, the user interface requires some
 * stuff to be done when the FLAGGED property is changed.
 */

static void press_square(struct minefield_t *mf, int col, int row)
{
	if(IS_AVAILABLE(mf, col, row)) {
		minefield_cell(mf, col, row)->is_pressed = 1;
		update_square(mf, col, row);
	}
}

static void unpress_square(struct minefield_t *mf, int col, int row)
{
	if(IS_AVAILABLE(mf, col, row)) {
		minefield_cell(mf, col, row)->is_pressed = 0;
		update_square(mf, col, row);
	}
}

static int clear_square(struct minefield_t *mf, int col, int row)
{
	if(!IS_AVAILABLE(mf, col, row) || minefield_cell(mf, col, row)->is_flagged)
		return 0;

	mf->unmarked--;
	minefield_cell(mf, col, row)->is_cleared = 1;
	update_square(mf, col, row);

	if(minefield_cell(mf, col, row)->is_mine)
		return -mf->rows*mf->cols;
	if(minefield_cell(mf, col, row)->minesaround == 0)
		return 1 + for_each_neighbour(mf, col, row, clear_square, +);
	return 1;
}

static int clear_around(struct minefield_t *mf, int col, int row)
{
	if(FLAGSAROUND(mf, col, row) != minefield_cell(mf, col, row)->minesaround)
		return 0;

	return for_each_neighbour(mf, col, row, clear_square, +);
}

static void toggle_flag(struct minefield_t *mf, int col, int row)
{
	if(minefield_cell(mf, col, row)->is_flagged ^= 1) {
		mf->flags++;
		mf->unmarked--;
	} else {
		mf->flags--;
		mf->unmarked++;
	}
	update_square(mf, col, row);
	ui_update_mine_counter(mf->mines - mf->flags);
}


/*
 * pre_game()
 *
 * Initializes things for one game of mindsweeper.  Must be called before
 * each game.
 */

void pre_game(struct minefield_t *mf, unsigned int game_number)
{
	int col, row;

	/*
	 * Reset miscellaneous stuff.
	 */

	if(mf->field) {
		free(mf->field[0]);
		free(mf->field);
		mf->field = NULL;
	}

	state.clock_started = state.won = state.lost = FALSE;
	state.time = 0;
	mf->unmarked = mf->rows * mf->cols;
	stats.played++;

	ui_update_mine_counter(mf->mines);
	ui_update_time_counter(state.time);
	ui_update_status(stopped);

	/*
	 * Seed random number generator (allows a specific game to be
	 * replayed) and display the seed used.
	 */

	mf->number = game_number;
	srand(mf->number);
	ui_update_game_number(mf->number);

	/*
	 * Create and reset the mine field
	 */

	mf->field = malloc((mf->cols+2) * sizeof(*mf->field));
	mf->field[0] = malloc((mf->rows+2)*(mf->cols+2) * sizeof(*mf->field[0]));
	for(col = 1; col < mf->cols+2; col++)
		mf->field[col] = mf->field[col-1] + (mf->rows+2);

	for(row = 0; row < (mf->rows+2)*(mf->cols+2); row++)
		mf->field[0][row] = MF_CELL_INITIALIZER;

	/*
	 * Put border into the UNAVAILABLE state to prevent it from being
	 * considered for mine/flag placement.
	 */

	for(col = 0; col < mf->cols+2; col++)
		minefield_cell(mf, col, 0)->is_unavailable = minefield_cell(mf, col, mf->rows+1)->is_unavailable = 1;
	for(row = 0; row < mf->rows+2; row++)
		minefield_cell(mf, 0, row)->is_unavailable = minefield_cell(mf, mf->cols+1, row)->is_unavailable = 1;

	/*
	 * Place mines using mf->flags to keep track of the number to be
	 * placed (this also resets mf->flags to 0).
	 */

	mf->flags = mf->mines;
	for(col = 1; col <= mf->cols; col++)
		for(row = 1; row <= mf->rows; row++)
			if(rand() < RAND_MAX / (mf->rows*mf->cols - (col-1)*mf->rows - (row-1)) * mf->flags) {
				minefield_cell(mf, col, row)->is_mine = 1;
				mf->flags--;
			}

	/*
	 * Record the number of mines around each square and update the
	 * graphics while we're at it
	 */

	for(col = 1; col <= mf->cols; col++)
		for(row = 1; row <= mf->rows; row++)
			minefield_cell(mf, col, row)->minesaround = for_each_neighbour(mf, col, row, IS_MINE, +);

	ui_minefield_reset();

	/*
	 * Find an open patch if needed
	 */

	while(settings.open) {
		col = (rand() % mf->cols)+1;
		row = (rand() % mf->rows)+1;
		if(!minefield_cell(mf, col, row)->is_mine && !minefield_cell(mf, col, row)->minesaround) {
			clear_square(mf, col, row);
			break;
		}
	}

	/*
	 * If autoplay is selected then start a search
	 */

	if(settings.autoplay)
		start_search();
}


/*
 * make_move()
 *
 * Administers the rules of the game.  This function receives information
 * about an event that has occured (location on board, button that changed,
 * whether button was pressed or released) and performs any actions that
 * are appropriate.
 *
 * Returns:  TRUE if a move has been made, FALSE otherwise.
 */

int make_move(struct minefield_t *mf, int col, int row, int button, enum action_t action)
{
	static int  buttons_down = 0;
	static int  last_col, last_row;
	static int  made_move;

	if(state.lost || state.won) {
		buttons_down = 0;
		return FALSE;
	}

	switch(action) {
	case press:
		switch(++buttons_down) {
		case 1:
			last_col = col;
			last_row = row;
			made_move = FALSE;
			if(!minefield_cell(mf, col, row)->is_cleared && button == 1)
				press_square(mf, col, row);
			break;

		case 2:
			if(col != last_col || row != last_row)
				break;
			for_each_neighbour(mf, col, row, press_square, ;);
			break;
		}
		break;

	case release:
		switch(buttons_down--) {
		case 1:
			unpress_square(mf, last_col, last_row);
			if(minefield_cell(mf, last_col, last_row)->is_cleared || col != last_col || row != last_row)
				break;
			switch(button) {
			case 1:
				if(clear_square(mf, col, row) < 0)
					state.lost = TRUE;
				break;

			default:
				toggle_flag(mf, col, row);
				break;
			}
			made_move = TRUE;
			state.clock_started = TRUE;
			break;

		case 2:
			for_each_neighbour(mf, last_col, last_row, unpress_square, ;);
			if(col != last_col || row != last_row)
				break;
			made_move = clear_around(mf, col, row);
			if(made_move < 0)
				state.lost = TRUE;
			break;

		default:
			buttons_down = 0;
			break;
		}
		if(!made_move)
			break;
		if(mf->unmarked == 0 && mf->flags == mf->mines)
			state.won = TRUE;
		if(state.won || state.lost) {
			state.clock_started = FALSE;
			stats.won += state.won;
			stats.lost += state.lost;
			ui_update_status(gameover);
			for(col = 1; col <= mf->cols; col++)
				for(row = 1; row <= mf->rows; row++)
					update_square(mf, col, row);
		}
		break;
	}

	return (buttons_down == 0) && made_move;
}
