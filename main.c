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
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "solver.h"
#include "mindsweeper.h"
#include "ui.h"


/*
 * timer_tick()
 *
 * Once a second checks to see if the game is on.  If so then the time
 * counter is incremented and a redraw of the time indicator is requested.
 */

static void *timer_tick(void *nul)
{
	int     	signum;
	sigset_t	alarmsig;

	sigemptyset(&alarmsig);
	sigaddset(&alarmsig, SIGALRM);

	while(1) {
		alarm(1);
		sigwait(&alarmsig, &signum);
		if((signum == SIGALRM) && state.clock_started && (state.focus || !settings.pause_when_unfocused)) {
			ui_threads_enter();
			ui_update_time_counter(++state.time);
			ui_threads_leave();
		}
	}

	return NULL;
}


/*
 * Entry point
 */

int main(int argc, char *argv[])
{
	int			opt;
	pthread_t		autoplay_thread;
	pthread_t		timer_thread;
	FILE			*seedfile;

	/*
	 * Initialize.
	 */

	g_thread_init(NULL);
	gdk_threads_init();
	gtk_init(&argc, &argv);
	mindsweeper_init(&minefield);
	solver_init();

	/*
	 * Parse command line.
	 */

	while((opt = getopt(argc, argv,
#ifdef DIAGNOSTICS
					"ahlopc:g:m:n:r:"
#else
					"ahopc:m:n:r:"
#endif
					)) != EOF)
		switch(opt) {
			case 'a':
			settings.autoplay = TRUE;
			break;

#ifdef DIAGNOSTICS
			case 'l':
			settings.logmode = TRUE;
			break;
#endif

			case 'o':
			settings.open = TRUE;
			break;

			case 'p':
			settings.pause_when_unfocused = TRUE;
			break;

			case 'r':
			minefield.rows = atoi(optarg);
			break;

			case 'c':
			minefield.cols = atoi(optarg);
			break;

#ifdef DIAGNOSTICS
			case 'g':
			stats.total_games = atoi(optarg);
			break;
#endif

			case 'm':
			minefield.mines = atoi(optarg);
			break;

			case 'n':
			minefield.number = atoi(optarg);
			break;

			case 'h':
			default:
			printf("Usage: %s [options]\nOptions:\n"
			       "\t--diplay display   use specified display\n"
			       "\t-n number          play specified game\n"
			       "\t-r rows            set number of rows\n"
			       "\t-c columns         set number of columns\n"
			       "\t-m mines           set number of mines\n"
#ifdef DIAGNOSTICS
			       "\t-g games           set number of games to play\n"
#endif
			       "\t-o                 start with an open patch\n"
			       "\t-a                 automatically find mines\n"
			       "\t-p                 pause when window is not focused\n"
#ifdef DIAGNOSTICS
			       "\t-l                 disable graphics updating\n"
#endif
			       "\t-h                 display this message\n"
			       "\n", argv[0]);
			exit(1);
		}
	if(minefield.rows < MIN_ROWS || minefield.rows > MAX_ROWS ||
	   minefield.cols < MIN_COLS || minefield.cols > MAX_COLS ||
	   minefield.mines < minefield.rows*minefield.cols*MIN_DENSITY ||
	   minefield.mines > minefield.rows*minefield.cols*MAX_DENSITY) {
		minefield.rows = (int []) DEFAULT_ROWS[EXPERT];
		minefield.cols = (int []) DEFAULT_COLS[EXPERT];
		minefield.mines = (int []) DEFAULT_MINES[EXPERT];
	}
	if(stats.total_games < 1)
		stats.total_games = 1;

	/*
	 * If the game number wasn't specified, then get a seed from
	 * /dev/random or time() then take the absolute value modulo
	 * RAND_MAX to get a number in a nice range.
	 */

	if(minefield.number < 0) {
		if(!(seedfile = fopen("/dev/random", "r")))
			minefield.number = (int) time(NULL);
		else {
			fread(&minefield.number, sizeof(int), 1, seedfile);
			fclose(seedfile);
		}
	}
	minefield.number = (unsigned int) minefield.number % RAND_MAX;

	/*
	 * Block SIGALRM.  Other threads will inherit this mask.
	 */

	{
	sigset_t alarmsig;
	sigemptyset(&alarmsig);
	sigaddset(&alarmsig, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &alarmsig, NULL);
	}

	/*
	 * Create the timer and autoplay threads then start GTK.
	 */

	pthread_create(&autoplay_thread, NULL, autoplay, NULL);
	pthread_create(&timer_thread, NULL, timer_tick, NULL);

	ui_threads_enter();
	ui_init();
	gtk_main();
	ui_threads_leave();
	release_minefield(&minefield);

	pthread_cancel(timer_thread);
	pthread_join(timer_thread, NULL);
	pthread_cancel(autoplay_thread);
	pthread_join(autoplay_thread, NULL);

	exit(0);
}
