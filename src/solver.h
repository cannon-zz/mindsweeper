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

#ifndef _SOLVER_H
#define _SOLVER_H

#include <pthread.h>

/*
 * Search status
 */

enum  status_t  { stopped, searching, guess, noguess, gameover };


/*
 * start_search()
 *
 * Signals the need to start a new search.
 */

extern pthread_mutex_t  solver_start_mutex;
extern pthread_cond_t   solver_start_cond;
extern int  solver_start_requested;

static void start_search(void)
{
	pthread_mutex_lock(&solver_start_mutex);
	pthread_cond_signal(&solver_start_cond);
	solver_start_requested = 1;
	pthread_mutex_unlock(&solver_start_mutex);
}


/*
 * Function prototypes
 */

void *autoplay(void *);

static void solver_init(void)
{
	solver_start_requested = 0;
	pthread_mutex_init(&solver_start_mutex, NULL);
	pthread_cond_init(&solver_start_cond, NULL);
}

#endif /* _SOLVER_H */
