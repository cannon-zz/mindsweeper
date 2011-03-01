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
 * A brief note on how this works.  This is a "brute-force" style solver.
 * It begins by identifying "possible mines" --- unmarked board locations
 * for which information exists --- and then exhaustively searches all
 * arrangements of mines in those sites that are consistent with the
 * information available.  After all consistent arrangements have been
 * found, any sites that needed to be cleared in all arrangements are
 * cleared and any that needed to be flagged in all arrangements are
 * flagged.  The process then repeats.
 *
 * There are subtleties, of course, that are used to speed the process up.
 * Firstly, many sites can be flagged or cleared based solely on the
 * information in their immediate neighbours (for example, a corner site
 * with a 1 beside it).  A special routine is included for identifying and
 * processing those.  The solver primarily iterates on this routine and
 * only resorts to the brute-force search when it fails.  Finally, the
 * brute-force routine performs a preliminary separation of the possible
 * mine sites into logically independent lists --- groups of sites that do
 * not influence each other.  This greatly reduces the volume of arragement
 * space that needs to be searched.
 */

#include <limits.h>
#include <pthread.h>
#ifdef DIAGNOSTICS
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "solver.h"
#include "mindsweeper.h"
#include "ui.h"


/*
 * Some types
 */

enum moves_t {
	nothing,
	flag,
	clear,
	cleararound
};

struct possible_mine_t {
	int  col, row;
	int  flag_placements;
};

struct mine_list_t {
	int  arrangements;
	int  min_flags, max_flags;
	int  sum_flags;
	int  length;
	struct possible_mine_t  *cell;
	struct mine_list_t  *prev;
	struct mine_list_t  *next;
};

struct mine_lists_t {
	int  min_flags, max_flags;
	int  length;
	struct mine_list_t  *first;
};

#define MINE_LISTS_INITIALIZER (struct mine_lists_t) { \
	.min_flags = 0, \
	.max_flags = 0, \
	.length = 0, \
	.first = NULL \
}


/*
 * Global data
 */

pthread_mutex_t  solver_start_mutex;
pthread_cond_t   solver_start_cond;
int  solver_start_requested;


/*
 * update_probability()
 *
 * Local wrapper for ui_update_square_probability().
 */

static void update_probability(int col, int row, float prob)
{
	ui_threads_enter();
	ui_update_square_probability(row, col, prob);
	ui_threads_leave();
}


/*
 * computer_move()
 *
 * Simulates the user making a move.
 */

static void zero_probability_if_available(struct minefield_t *mf, int col, int row)
{
	if(IS_AVAILABLE(mf, col, row) && !minefield_cell(mf, col, row)->is_flagged)
		ui_update_square_probability(row, col, 0.0);
}

static void computer_move(struct minefield_t *mf, int col, int row, enum moves_t cmd)
{
	ui_threads_enter();
	switch(cmd) {
		case nothing:
		break;

		case flag:
		ui_update_square_probability(row, col, 1.0);
		if(settings.autoplay) {
			make_move(mf, col, row, 3, press);
			make_move(mf, col, row, 3, release);
		}
		break;

		case clear:
		ui_update_square_probability(row, col, 0.0);
		if(settings.autoplay) {
			make_move(mf, col, row, 1, press);
			make_move(mf, col, row, 1, release);
		}
		break;

		case cleararound:
		for_each_neighbour(mf, col, row, zero_probability_if_available, ;);
		if(settings.autoplay) {
			make_move(mf, col, row, 1, press);
			make_move(mf, col, row, 3, press);
			make_move(mf, col, row, 3, release);
			make_move(mf, col, row, 1, release);
		}
		break;
	}
	ui_threads_leave();
}


/*
 * AVAILAROUND()
 *
 * Returns the number of sites surrounding the site at (col, row) that are
 * AVAILABLE (are allowed to hold a flag).  This complements FLAGSAROUND()
 * but because it is only required by the solver, it is here.
 */

static int AVAILAROUND(struct minefield_t *mf, int col, int row)
{
	return(for_each_neighbour(mf, col, row, IS_AVAILABLE, +));
}


/*
 * are_dependant()
 *
 * Returns FALSE if the two locations passed to it do not influence each
 * other i.e. the site at (i1,j1) can be flagged or unflagged without
 * (immediately) affecting the site at (i2,j2).  This is the case if the
 * two sites do not share a cleared neighbour.  Used internally by
 * add_possible_mine().
 */

static int is_cleared_neighbour_of(struct minefield_t *mf, int c1, int r1, int c2, int r2)
{
	return(minefield_cell(mf, c1, r1)->is_cleared && (abs(c1 - c2) < 2) && (abs(r1 - r2) < 2));
}

static int are_dependant(struct minefield_t *mf, int c1, int r1, int c2, int r2)
{
	return((abs(c1 - c2) <= 2) && (abs(r1 - r2) <= 2) &&
	       (is_cleared_neighbour_of(mf, c1-1, r1-1, c2, r2) ||
	        is_cleared_neighbour_of(mf, c1  , r1-1, c2, r2) ||
	        is_cleared_neighbour_of(mf, c1+1, r1-1, c2, r2) ||
	        is_cleared_neighbour_of(mf, c1+1, r1  , c2, r2) ||
	        is_cleared_neighbour_of(mf, c1+1, r1+1, c2, r2) ||
	        is_cleared_neighbour_of(mf, c1  , r1+1, c2, r2) ||
	        is_cleared_neighbour_of(mf, c1-1, r1+1, c2, r2) ||
	        is_cleared_neighbour_of(mf, c1-1, r1  , c2, r2)));
}


/*
 * free_mine_lists(), add_possible_mine(), is_possible_mine(),
 * join_mine_lists()
 *
 * Functions for manipulating the list of possible mines.  The "clear"
 * function deletes the entire possible_mine chain.  The "add" function
 * inserts a new possible_mine into the chain, correctly linking all
 * possible_mine lists that the new possible_mine makes dependant on
 * one-another.  The "is" function returns TRUE if the specified
 * co-ordinates correspond to a possible_mine in the chain.  The "join"
 * function joins list_b to list_a and removes list_b from the chain; the
 * return value can be used to continue scaning the chain without accessing
 * freed data.
 */

static void free_mine_lists(struct mine_lists_t *mine_lists)
{
	struct mine_list_t  *list;

	while(mine_lists->first) {
		free(mine_lists->first->cell);
		list = mine_lists->first->next;
		free(mine_lists->first);
		mine_lists->first = list;
	}

	/* omitted for performance purposes
	*mine_lists = MINE_LIST_INITIALIZER;
	*/
}

static struct mine_list_t *join_mine_lists(struct mine_list_t *list_a, struct mine_list_t *list_b)
{
	struct mine_list_t  *prev = list_b->prev;

	list_a->cell = realloc(list_a->cell, (list_a->length + list_b->length) * sizeof(*list_a->cell));
	memcpy(&list_a->cell[list_a->length], list_b->cell, list_b->length * sizeof(*list_b->cell));
	list_a->length += list_b->length;
	free(list_b->cell);

	prev->next = list_b->next;
	if(list_b->next)
		list_b->next->prev = prev;
	free(list_b);

	return(prev);
}

static void add_possible_mine(struct minefield_t *mf, int col, int row, struct mine_lists_t *mine_lists)
{
	struct mine_list_t  *list;
	struct possible_mine_t  *cell;
	int  i;

	/*
	 * Add the new possible_mine as its own list
	 */

	list = malloc(sizeof(*list));
	*list = (struct mine_list_t) {
		.arrangements = 0,
		.length = 1,
		.min_flags = INT_MAX,
		.max_flags = 0,
		.sum_flags = 0,
		.prev = NULL,
		.next = mine_lists->first
	};
	if(mine_lists->first)
		mine_lists->first->prev = list;
	mine_lists->first = list;
	mine_lists->length++;

	list->cell = malloc(sizeof(*list->cell));
	list->cell[0] = (struct possible_mine_t) {
		.col = col,
		.row = row,
		.flag_placements = 0,
	};

	/*
	 * Search the existing lists for sites that share information with
	 * the new one and join all such lists together.
	 */

	for(list = list->next; list; list = list->next)
		for(i = 0, cell = list->cell; i < list->length; i++, cell++)
			if(are_dependant(mf, col, row, cell->col, cell->row)) {
				list = join_mine_lists(mine_lists->first, list);
				break;
			}
}

static int is_possible_mine(int col, int row, struct mine_lists_t *mine_lists)
{
	struct mine_list_t  *list;
	struct possible_mine_t  *cell;
	int  i;

	for(list = mine_lists->first; list; list = list->next)
		for(i = 0, cell = list->cell; i < list->length; i++, cell++)
			if(cell->col == col && cell->row == row)
				return(TRUE);
	return(FALSE);
}


/*
 * find_possible_mines()
 *
 * Assembles a linked list of sites that are unmarked and have neighbours
 * which are cleared.  Such sites are called possible mines.  Returns TRUE
 * if any were found.  The possible_mines are actually assembled into a
 * chain of lists where sites in separate lists do not influence each other
 * and so can be analyzed independantly (maybe... see
 * analyze_possible_mines).
 */

static int find_possible_mines(struct minefield_t *mf, struct mine_lists_t *mine_lists)
{
	int  col, row;

	for(col = 1; col <= mf->cols; col++)
		for(row = 1; row <= mf->rows; row++)
			if(IS_UNMARKED(mf, col, row) && (for_each_neighbour(mf, col, row, IS_CLEARED, ||)))
				add_possible_mine(mf, col, row, mine_lists);
	return(mine_lists->first != NULL);
}


/*
 * optimize_list()
 *
 * Attempts to improve the performance of the brute-force search by
 * optimizing the order of cells in each list.  Cells that can be cleared
 * or flagged based on their immediate neighbours are moved to the front of
 * the list to avoid them being tested repeatedly.  Returns true if any
 * such cells were found in the given list.
 */

static int unmarked_neighbours_must_be_cleared(struct minefield_t *mf, int col, int row)
{
	return(minefield_cell(mf, col, row)->is_cleared && (FLAGSAROUND(mf, col, row) == minefield_cell(mf, col, row)->minesaround));
}

static int unmarked_neighbours_must_be_flagged(struct minefield_t *mf, int col, int row)
{
	return(minefield_cell(mf, col, row)->is_cleared && (AVAILAROUND(mf, col, row) == minefield_cell(mf, col, row)->minesaround));
}

static int optimize_list(struct minefield_t *mf, struct mine_list_t *list)
{
	struct possible_mine_t  *head, *tail, temp;
	int  i, found = 0;

	for(head = tail = list->cell, i = 0; i < list->length; head++, i++) {
		if(for_each_neighbour(mf, head->col, head->row, unmarked_neighbours_must_be_cleared, ||) || for_each_neighbour(mf, head->col, head->row, unmarked_neighbours_must_be_flagged, ||)) {
			temp = *head;
			memmove(tail + 1, tail, (void *) head - (void *) tail);
			*(tail++) = temp;
			found = 1;
		}
	}

	return(found);
}


/*
 * analyze_possible_mines(), analyze_list()
 *
 * The analyze_possible_mines() function uses analyze_list() to find
 * moves that can be made.  Returns >= 0 on success or < 0 on failure (see
 * analyze_list() for meaning of return codes).
 *
 * analyze_list() searches the decision tree for mine placement in
 * the specified list of possible mines finding arrangements which are
 * consistent with the information available in the board's cleared sites.
 * The total number of consistent arrangements found is returned or a value
 * < 0 if the search terminated prior to completion.  Upon completion of
 * the search, the flag_placement field in each possible mine indicates the
 * number of times a valid arrangement had a flag in that location.  If
 * this field equals the return value (the total number of arrangements)
 * then every arrangement required a flag in that location while if this
 * field equals 0 then every arrangement required that location to be
 * clear.
 */

static int not_enough_flagged(struct minefield_t *mf, int col, int row)
{
	return(!minefield_cell(mf, col, row)->is_cleared || (FLAGSAROUND(mf, col, row) < minefield_cell(mf, col, row)->minesaround));
}

static int not_enough_cleared(struct minefield_t *mf, int col, int row)
{
	return(!minefield_cell(mf, col, row)->is_cleared || (AVAILAROUND(mf, col, row) > minefield_cell(mf, col, row)->minesaround));
}

static int _analyze_list(struct minefield_t *mf, struct mine_list_t *list, struct possible_mine_t *cell, int flags_used)
{
	int  result;
	int  arrangements = 0;

	/*
	 * If we've run out of possible mine locations then we've assembled
	 * a valid arrangment.  If the minefield is needed, abandon the
	 * search.  Otherwise, make sure there are enough sites left over
	 * for the remaining flags and report a single successful
	 * arrangement if there are.
	 */

	if(cell - list->cell == list->length) {
		if(mf->needed)
			return(-1);
		if(mf->mines - mf->flags - flags_used > mf->unmarked - list->length)
			return(0);
		if(flags_used < list->min_flags)
			list->min_flags = flags_used;
		if(flags_used > list->max_flags)
			list->max_flags = flags_used;
		list->sum_flags += flags_used;
		return(1);
	}

	/*
	 * Check to see if all of the current cell's neighbours can
	 * tolerate it being flagged.  If they can, do so and move on to
	 * the next.  If a return value of < 0 is received, it means
	 * something is wrong so we should give up and pass the < 0 along
	 * to whoever called us.  Otherwise we can keep going.  In either
	 * case, the current possible mine's state must be restored.
	 */

	if((mf->flags + flags_used+1 <= mf->mines) && for_each_neighbour(mf, cell->col, cell->row, not_enough_flagged, &&)) {
		minefield_cell(mf, cell->col, cell->row)->is_flagged = 1;
		result = _analyze_list(mf, list, cell+1, flags_used+1);
		minefield_cell(mf, cell->col, cell->row)->is_flagged = 0;
		if(result < 0)
			return(result);
		cell->flag_placements += result;
		arrangements += result;
	}

	/*
	 * Same idea as above but now we're checking to see if the
	 * neighbours can tolerate it being cleared.  Rather than actually
	 * clearing the site, we use the UNAVAILABLE flag.  This way the
	 * site will no longer be considered available for holding a mine
	 * but the information that would be revealed if it actually was
	 * cleared is still kept secret.
	 */

	if(for_each_neighbour(mf, cell->col, cell->row, not_enough_cleared, &&)) {
		minefield_cell(mf, cell->col, cell->row)->is_unavailable = 1;
		result = _analyze_list(mf, list, cell+1, flags_used);
		minefield_cell(mf, cell->col, cell->row)->is_unavailable = 0;
		if(result < 0)
			return(result);
		arrangements += result;
	}

	return(arrangements);
}

static int analyze_list(struct minefield_t *mf, struct mine_list_t *list)
{
	return(_analyze_list(mf, list, list->cell, 0));
}


static int analyze_possible_mines(struct minefield_t *mf, struct mine_lists_t *mine_lists)
{
	struct mine_list_t  *list, *retry_list;
	struct possible_mine_t  *cell;
	int  i;

	/*
	 * Find the arrangements for each independant chain.  If the maximum
	 * number of flags that could possibly be required is less than or
	 * equal to the remaining mines then we're done.
	 */

	for(list = mine_lists->first; list; list = list->next) {
		optimize_list(mf, list);
		list->arrangements = analyze_list(mf, list);
		if(list->arrangements < 0)
			return(list->arrangements);
		mine_lists->min_flags += list->min_flags;
		mine_lists->max_flags += list->max_flags;
	}

	if(mf->flags + mine_lists->max_flags <= mf->mines)
		return(1);

	/*
	 * Some of the arrangements found would result in more flags being
	 * placed on the board than there are mines.  Merge all the lists
	 * that require an indeterminate number of flags into a single
	 * list.  Count the flags used by lists with fixed requirements
	 * while we're at it.
	 */

	mine_lists->min_flags = 0;
	mine_lists->max_flags = 0;
	for(retry_list = mine_lists->first; retry_list->min_flags == retry_list->max_flags; retry_list = retry_list->next) {
		mine_lists->min_flags += retry_list->min_flags;
		mine_lists->max_flags += retry_list->max_flags;
	}
	for(list = retry_list->next; list; list = list->next) {
		if(list->min_flags != list->max_flags)
			list = join_mine_lists(retry_list, list);
		else {
			mine_lists->min_flags += list->min_flags;
			mine_lists->max_flags += list->max_flags;
		}
	}
	retry_list->min_flags = INT_MAX;
	retry_list->max_flags = 0;
	retry_list->sum_flags = 0;
	for(i = 0, cell = retry_list->cell; i < retry_list->length; i++, cell++)
		cell->flag_placements = 0;

	/*
	 * Do a new search, but temporarily increase mf->flags by the
	 * number known to be required by the other lists to impose the
	 * proper mine limit constraint on the arrangements.
	 */

	mf->flags += mine_lists->max_flags;
	optimize_list(mf, retry_list);
	retry_list->arrangements = analyze_list(mf, retry_list);
	mf->flags -= mine_lists->max_flags;
	mine_lists->min_flags += retry_list->min_flags;
	mine_lists->max_flags += retry_list->max_flags;

	return(retry_list->arrangements);
}


/*
 * check_nonborder_sites()
 *
 * Checks to see if sites not included in the mine_lists can be played.
 * This is possible if (i) the sites in the mine_lists require all
 * remaining flags thus allowing all other sites to be cleared or (ii) the
 * sites in the mine_lists require sufficiently few flags that all other
 * sites must be flagged.  If either is the case then all remaining sites
 * are cleared or flagged respectively.  Returns TRUE if any moves were
 * made.
 */

static int check_nonborder_sites(struct minefield_t *mf, struct mine_lists_t *mine_lists)
{
	int  col, row;
	enum moves_t  action;
	int  made_moves = FALSE;

	if(mf->flags + mine_lists->min_flags == mf->mines)
		action = clear;
	else if(mf->mines - mf->flags - mine_lists->max_flags == mf->unmarked - mine_lists->length)
		action = flag;
	else
		return(FALSE);

	for(col = 1; col <= mf->cols; col++)
		for(row = 1; row <= mf->rows; row++)
			if(IS_UNMARKED(mf, col, row) && !is_possible_mine(col, row, mine_lists)) {
				computer_move(mf, col, row, action);
				made_moves = TRUE;
			}
	return(made_moves);
}


/*
 * play_move()
 *
 * Uses the information compiled by analyze_possible_mines() to play any
 * possible_mines that were found to require flagging or clearing.
 *
 * Returns TRUE if any moves could be made.
 */

static int play_moves(struct minefield_t *mf, struct mine_lists_t *mine_lists)
{
	struct mine_list_t  *list;
	struct possible_mine_t  *cell;
	int  i;
	int  made_moves = FALSE;

	for(list = mine_lists->first; list; list = list->next) {
		for(i = 0, cell = list->cell; i < list->length; i++, cell++) {
			if(cell->flag_placements == 0) {
				computer_move(mf, cell->col, cell->row, clear);
				made_moves = TRUE;
			} else if(cell->flag_placements == list->arrangements) {
				computer_move(mf, cell->col, cell->row, flag);
				made_moves = TRUE;
			} else
				update_probability(cell->col, cell->row, (float) cell->flag_placements/list->arrangements);
		}
	}

	return(made_moves);
}


/*
 * do_nonborder_probabilities()
 *
 * Compute the mean mine density in nonborder cells.
 */

static void do_nonborder_probabilities(struct minefield_t *mf, struct mine_lists_t *mine_lists)
{
	struct mine_list_t  *list;
	int  border_cells = 0;
	float  mean_flags = (float) (mf->mines - mf->flags);
	float  prob;
	int  row, col;

	for(list = mine_lists->first; list; list = list->next) {
		border_cells += list->length;
		/* ignore lists with no valid arrangements */
		if(list->arrangements > 0)
			mean_flags -= (float) list->sum_flags / list->arrangements;
	}

	prob = mean_flags / (mf->unmarked - border_cells);

	/* can happen if the board is in an inconsistent state */
	if(prob < 0.0)
		prob = 0.0;
	if(prob > 1.0)
		prob = 1.0;

	for(col = 1; col <= mf->cols; col++)
		for(row = 1; row <= mf->rows; row++)
			if(IS_UNMARKED(mf, col, row) && !is_possible_mine(col, row, mine_lists))
				update_probability(col, row, prob);
}


/*
 * show_stats()
 *
 * Used to display statistics for automated play.  The statistics reported
 * can be used to check the playability of game settings eg. to see what
 * percentage of games are actually playable for a given board size and
 * mine density.
 */

#ifdef DIAGNOSTICS
static void show_stats(void)
{
	if(stats.played == 1) {
		printf("\nPercent of test complete:\n" \
		"   10   20   30   40   50   60   70   80   90  100\n" \
		"....|....|....|....|....|....|....|....|....|....|\n");
		return;
	}
	if(!(stats.played*50 % stats.total_games)) {
		printf("%*c\r", 50*stats.played/stats.total_games, '*');
		fflush(stdout);
	}
	if((stats.played >= stats.total_games) && (stats.total_games > 1)) {
		printf("\nGames Played: %d\n" \
		"Won: %d (%f%%)\n" \
		"Lost: %d (%f%%)\n" \
		"Requiring Guess: %d (%f%%)\n" \
		"Unfinished: %d\n", stats.played,
		stats.won, 100.0*stats.won/stats.played,
		stats.lost, 100.0*stats.lost/stats.played,
		stats.guessed, 100.0*stats.guessed/stats.played,
		stats.played-stats.won-stats.lost-stats.guessed);
	}
}
#endif


/*
 * solve_minefield()
 *
 * This function analyses the minefield, makes any moves that can be made
 * and returns the result of its analysis.  If do_full_analysis == TRUE
 * then a full flag placement analysis will be performed regardless of the
 * availability of 0th order moves.
 */

static enum status_t solve_minefield(struct minefield_t *mf, int do_full_analysis)
{
	struct mine_lists_t  mine_lists = MINE_LISTS_INITIALIZER;
	enum status_t  result;

	if(!find_possible_mines(mf, &mine_lists)) {
		/*
		 * No sites have information available for them ...
		 */
		if(check_nonborder_sites(mf, &mine_lists)) {
			/*
			 * but all non-border sites must be either cleared
			 * or mined.
			 */
			result = noguess;
		} else {
			/*
			 * and there are still choices to make.
			 */
			result = guess;
		}
	} else if(analyze_possible_mines(mf, &mine_lists) < 0) {
		/*
		 * minefield analysis was terminated prematurely
		 */
		result = stopped;
	} else if(play_moves(mf, &mine_lists)) {
		/*
		 * The valid arrangements uniquely identify at least one
		 * move
		 */
		result = noguess;
	} else if(check_nonborder_sites(mf, &mine_lists)) {
		/*
		 * We can play squares outside the border region.
		 */
		result = noguess;
	} else {
		/*
		 * The valid arrangements do not uniquely identify any
		 * moves
		 */
		result = guess;
	}

	if(do_full_analysis)
		do_nonborder_probabilities(mf, &mine_lists);

	free_mine_lists(&mine_lists);

	return(result);
}


/*
 * update_status(), local_pre_game()
 *
 * Wrappers for the ui_update_status() and pre_game() functions.
 */

static void update_status(enum status_t status)
{
	ui_threads_enter();
	ui_update_status(status);
	ui_threads_leave();
}

#ifdef DIAGNOSTICS
static void local_pre_game(struct minefield_t *mf, unsigned int number)
{
	ui_threads_enter();
	pre_game(mf, number);
	ui_threads_leave();
}
#endif


/*
 * autoplay()
 *
 * This is the autoplay control loop.
 */

static int searching_must_stop(struct minefield_t *mf)
{
	return(state.won || state.lost || (state.search_status == guess) || (state.search_status == stopped) || mf->needed);
}

void *autoplay(void *nul)
{
	/*
	 * Start by nicing this thread so we won't take too many cycles
	 * from the user interface.
	 */

	setpriority(PRIO_PROCESS, 0, PRIO_MAX);

	/*
	 * Perform a search when the search conditional is signaled or if
	 * we are in autplay mode.
	 */

	get_minefield(&minefield);
	while(1) {
		if(!settings.autoplay || searching_must_stop(&minefield)) {
			release_minefield(&minefield);
			pthread_mutex_lock(&solver_start_mutex);
			if(!solver_start_requested)
				pthread_cond_wait(&solver_start_cond, &solver_start_mutex);
			solver_start_requested = 0;
			pthread_mutex_unlock(&solver_start_mutex);
			get_minefield(&minefield);
		}

		if(!settings.analysis)
			continue;

		update_status(searching);
		update_status(solve_minefield(&minefield, settings.show_probability));

#ifdef DIAGNOSTICS
		if(searching_must_stop(&minefield)) {
			stats.guessed += state.search_status == guess;
			if(state.search_status != stopped)
				show_stats();
			if((stats.played < stats.total_games) && !minefield.needed)
				local_pre_game(&minefield, rand());
		}
#endif
	}

	return NULL;
}
