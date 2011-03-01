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

#ifndef _UI_H
#define _UI_H

#include <gdk/gdk.h>
#include "minefield.h"

/*
 * UI wrapper for MineField widget states.
 */

typedef MineFieldState ui_square_state_t;


/*
 * Functions
 */

void ui_init(void);
void ui_minefield_reset(void);
void ui_update_time_counter(int);
void ui_update_mine_counter(int);
void ui_update_status(enum status_t);
void ui_update_game_number(int);
void ui_update_square_state(int, int, ui_square_state_t);
void ui_update_square_probability(int, int, float);

static void ui_threads_enter(void)
{
	gdk_threads_enter();
}

static void ui_threads_leave(void)
{
	gdk_threads_leave();
}

#endif /* _UI_H */
