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

#ifndef _MINDSWEEPER_H
#define _MINDSWEEPER_H

#include <pthread.h>

/*
 * Misc
 */

#ifndef TRUE
#define TRUE		1
#endif
#ifndef FALSE
#define FALSE		0
#endif


/*
 * Preset layouts
 */

#define BEGINNER 0
#define INTERMEDIATE 1
#define EXPERT 2
#define BOBBY 3

#define MINDSWEEPER_ROWS  {  8, 16,  16,  38 }
#define MINDSWEEPER_COLS  {  8, 16,  30,  66 }
#define MINDSWEEPER_MINES { 10, 43, 100, 552 }

#define GNOMINES_ROWS  {  8, 16, 16,  38 }
#define GNOMINES_COLS  {  8, 16, 30,  66 }
#define GNOMINES_MINES { 10, 40, 99, 552 }

#define KDEMINES_ROWS  GNOMINES_ROWS	/* they're the same ... */
#define KDEMINES_COLS  GNOMINES_COLS
#define KDEMINES_MINES GNOMINES_MINES

#define XDEMINEUR_ROWS  MINDSWEEPER_ROWS	/* they're the same ... */
#define XDEMINEUR_COLS  MINDSWEEPER_COLS
#define XDEMINEUR_MINES MINDSWEEPER_MINES

#define DEFAULT_ROWS  MINDSWEEPER_ROWS	/* choose what we'll use ... */
#define DEFAULT_COLS  MINDSWEEPER_COLS
#define DEFAULT_MINES MINDSWEEPER_MINES

#define MIN_ROWS        5
#define MAX_ROWS        38
#define MIN_COLS        5
#define MAX_COLS        66
#define MIN_DENSITY     0.00
#define MAX_DENSITY     0.50


/*
 * Type definitions
 */

enum  action_t  { release = 0, press = 1 };

struct state {
	int  won;
	int  lost;
	int  clock_started;
	int  focus;
	int  search_status;
	int  time;
};

struct mf_cell_t;
struct minefield_t {
	int  number;
	int  rows;
	int  cols;
	int  mines;
	struct mf_cell_t **field;
	int  flags;
	int  unmarked;
	pthread_mutex_t  lock;
	int  needed;
};

struct settings {
	int  logmode;
	int  open;
	int  analysis;
	int  show_probability;
	int  autoplay;
	int  pause_when_unfocused;
};

struct stats {
	int  total_games;
	int  played;
	int  won;
	int  lost;
	int  guessed;
};

#define STATE_INITIALIZER (struct state) { \
	.won = FALSE, \
	.lost = FALSE, \
	.clock_started = FALSE, \
	.focus = FALSE, \
	.search_status = stopped, \
	.time = 0 \
}

#define MINEFIELD_INITIALIZER (struct minefield_t) { \
	.number = -1, \
	.rows = -1, \
	.cols = -1, \
	.mines = -1, \
	.field = NULL, \
	.flags = 0, \
	.unmarked = 0, \
	.lock = PTHREAD_MUTEX_INITIALIZER, \
	.needed = 0 \
}

#define SETTINGS_INITIALIZER (struct settings) { \
	.logmode = FALSE, \
	.open = FALSE, \
	.analysis = TRUE, \
	.show_probability = FALSE, \
	.autoplay = FALSE, \
	.pause_when_unfocused = FALSE \
}

#define STATS_INITIALIZER (struct stats) { \
	.total_games = 1, \
	.played = 0, \
	.won = 0, \
	.lost = 0, \
	.guessed = 0 \
}


/*
 * Global variables
 */

extern struct state  state;
extern struct minefield_t  minefield;
extern struct settings  settings;
extern struct stats  stats;


/*
 * Setting and retrieving minefield cell state
 */

struct mf_cell_t {
	unsigned int  minesaround : 4;
	unsigned int  is_mine : 1;
	unsigned int  is_flagged : 1;
	unsigned int  is_cleared : 1;
	unsigned int  is_pressed : 1;
	unsigned int  is_unavailable : 1;
};

#define MF_CELL_INITIALIZER (struct mf_cell_t) { \
	.minesaround = 0, \
	.is_mine = 0, \
	.is_flagged = 0, \
	.is_cleared = 0, \
	.is_pressed = 0, \
	.is_unavailable = 0 \
}

static inline struct mf_cell_t *minefield_cell(struct minefield_t *mf, int col, int row)
{
	return(&mf->field[col][row]);
}

#define IS_MINE(mf,col,row)		(minefield_cell(mf,col,row)->is_mine)
#define	IS_CLEARED(mf,col,row)		(minefield_cell(mf,col,row)->is_cleared)
#define	IS_AVAILABLE(mf,col,row)	(!(minefield_cell(mf,col,row)->is_cleared || minefield_cell(mf,col,row)->is_unavailable))
#define IS_UNMARKED(mf,col,row)		(!(minefield_cell(mf,col,row)->is_flagged || minefield_cell(mf,col,row)->is_cleared))


/*
 * get_minefield()
 *
 * Stops the autoplay thread and gets control of the minefield.
 *
 * release_minefield()
 *
 * Releases control of the minefield.
 */

static void get_minefield(struct minefield_t *mf)
{
	mf->needed++;
	pthread_mutex_lock(&mf->lock);
	mf->needed--;
}

static void release_minefield(struct minefield_t *mf)
{
	pthread_mutex_unlock(&mf->lock);
}


/*
 * for_each_neighbour()
 *
 * Macro for iterating a function call over each of a cell's neighbours.
 */

#define for_each_neighbour(mf,i,j,f,op) \
	f(mf,i-1,j-1) op \
	f(mf,i-1,j  ) op \
	f(mf,i-1,j+1) op \
	f(mf,i  ,j-1) op \
	f(mf,i  ,j+1) op \
	f(mf,i+1,j-1) op \
	f(mf,i+1,j  ) op \
	f(mf,i+1,j+1)


/*
 * FLAGSAROUND(i, j)
 *
 * Returns the number of (i,j)'s neighbours that are flagged.
 */

#define	IS_FLAGGED(mf,col,row)	minefield_cell(mf,col,row)->is_flagged

static int FLAGSAROUND(struct minefield_t *mf, int i, int j)
{
	return(for_each_neighbour(&minefield, i, j, IS_FLAGGED, +));
}


/*
 * Function prototypes
 */

void	pre_game(struct minefield_t *, unsigned int);
int	make_move(struct minefield_t *, int, int, int, enum action_t);
void	mindsweeper_init(struct minefield_t *);

#endif /* _MINDSWEEPER_H */
