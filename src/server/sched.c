/*
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-98 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
 *      Dick Balaska         <dick@xpilot.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include "version.h"
#include "xpconfig.h"
#include "debug.h"
#include "const.h"
#include "error.h"
#include "types.h"
#include "sched.h"
#include "global.h"
#include "server.h"
#include "portability.h"

char sched_version[] = VERSION;

int32_t sched_running = false;

volatile int32_t timer_ticks; /* SIGALRMs that have occurred */
#if 0
static int32_t timers_used; /* SIGALRMs that have been used */
static int32_t timer_freq; /* rate at which timer ticks. (in FPS) */
static void (*timer_handler)(void);
static time_t current_time;
#endif
typedef int32_t FDTYPE;
//static double sched_times[1000];
//static int32_t counter = 0;
static struct sigaction act;


/*
 * Block or unblock a single signal.
 */
static void sig_ok(int32_t signum, int32_t flag)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, signum);
	if (sigprocmask((flag) ? SIG_UNBLOCK : SIG_BLOCK, &sigset, NULL) == -1) {
		error("sigprocmask(%d,%d)", signum, flag);
		exit(1);
	}
}

/*
 * Prevent the real-time timer from interrupting system calls.
 * Globally accessible.
 */
void block_timer(void)
{
	sig_ok(SIGALRM, 0);
}

/*
 * Unblock the real-time timer.
 * Globally accessible.
 */
void allow_timer(void)
{
	sig_ok(SIGALRM, 1);
}

/*
 * Setup the handling of the SIGALRM signal
 * and setup the real-time interval timer.
 */
void setup_timer(int32_t timer_freq)
{

	struct itimerval itv;

	/*
	 * Prevent SIGALRMs from disturbing the initialization.
	 */
	block_timer();

	/*
	 * Install a signal handler for the alarm signal.
	 */
	act.sa_handler = Main_loop;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);
	if (sigaction(SIGALRM, &act, (struct sigaction *) NULL) == -1) {
		error("sigaction SIGALRM");
		exit(1);
	}

	/*
	 * Install a real-time timer.
	 */
	if (timer_freq <= 0 || timer_freq > 100) {
		error("illegal timer frequency: %ld", timer_freq);
		exit(1);
	}

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 1000000 / timer_freq;
	itv.it_value = itv.it_interval;
	if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
		error("setitimer");
		exit(1);
	}

	/*
	 * Allow the real-time timer to generate SIGALRM signals.
	 */
	allow_timer();
}

#define NUM_SELECT_FD		((int32_t)sizeof(int32_t) * 8)

struct io_handler {
	int32_t fd;
	void (*func)(int32_t, void *);
	void *arg;
};

static struct io_handler input_handlers[NUM_SELECT_FD];
static fd_set input_mask;
static int32_t max_fd, min_fd;
static int32_t input_inited = false;

static void io_dummy(int32_t fd, void *arg)
{
	xpprintf("io_dummy called!  (%d, %p)\n", fd, arg);
}

void install_input(void(*func)(int32_t, void *), int32_t fd, void *arg)
{
	int32_t i;

	if (input_inited == false) {
		input_inited = true;
		FD_ZERO(&input_mask);
		min_fd = fd;
		max_fd = fd;
		for (i = 0; i < NELEM(input_handlers); i++) {
			input_handlers[i].fd = -1;
			input_handlers[i].func = io_dummy;
			input_handlers[i].arg = 0;
		}
	}
	if (fd < min_fd || fd >= min_fd + NUM_SELECT_FD) {
		error("install illegal input handler fd %d (%d)", fd, min_fd);
		ServerExit();
	}
	if (FD_ISSET(fd, &input_mask)) {
		error("input handler %d busy", fd);
		ServerExit();
	}
	input_handlers[fd - min_fd].fd = fd;
	input_handlers[fd - min_fd].func = func;
	input_handlers[fd - min_fd].arg = arg;
	FD_SET(fd, &input_mask);
	if (fd > max_fd) {
		max_fd = fd;
	}
}

void remove_input(int32_t fd)
{
	if (fd < min_fd || fd >= min_fd + NUM_SELECT_FD) {
		error("remove illegal input handler fd %d (%d)", fd, min_fd);
		ServerExit();
	}
	if (FD_ISSET(fd, &input_mask)) {
		input_handlers[fd - min_fd].fd = -1;
		input_handlers[fd - min_fd].func = io_dummy;
		input_handlers[fd - min_fd].arg = 0;
		FD_CLR((FDTYPE)fd, &input_mask);
		if (fd == max_fd) {
			int32_t i = fd;
			max_fd = -1;
			while (--i >= min_fd) {
				if (FD_ISSET(i, &input_mask)) {
					max_fd = i;
					break;
				}
			}
		}
	}
}

void stop_sched(void)
{
	sched_running = 0;
}

void sched(void)
{
	int32_t i, n;
//	struct timeval tv /*, *tvp = &tv, tv1*/;
	fd_set readmask;
	if (sched_running) {
		error("sched already running");
		exit(1);
	}

	sched_running = 1;

	while (sched_running) {
//		tv.tv_sec = 0;
//		tv.tv_usec = 0;

		/*
		 gettimeofday(&tv1, NULL);
		 sched_times[counter] = timeval_to_seconds(tv1);
		 counter++;
		 if (counter > 600){
		 int32_t i;
		 for (i = 0; i < 600; i++){
		 printf("%e\n",sched_times[i]);

		 }
		 exit(1);
		 }
		 */

		readmask = input_mask;
		n = select(max_fd + 1, &readmask, 0, 0, NULL);
		if (n != -1) {
			sigprocmask(SIG_BLOCK, &act.sa_mask, NULL);
			for (i = max_fd; i >= min_fd; i--) {
				if (FD_ISSET(i, &readmask)) {
					struct io_handler *ioh;
					ioh = &input_handlers[i - min_fd];
					(*(ioh->func))(ioh->fd, ioh->arg);
				}
			}
			sigprocmask(SIG_UNBLOCK, &act.sa_mask, NULL);
		}
	}
}

