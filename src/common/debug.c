/*
 * debug.c
 *
 *  Created on: Feb 2, 2009
 *      Author: rotunda
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "debug.h"

void *DEBUG_Backtrace[MAX_NUM_BACKTRACE_ITEMS];

void DEBUG_PrintBacktrace(void)
{
	backtrace(DEBUG_Backtrace, MAX_NUM_BACKTRACE_ITEMS);
	printf("Printing backtrace:\n");
	backtrace_symbols_fd(DEBUG_Backtrace, MAX_NUM_BACKTRACE_ITEMS, STDOUT_FILENO);
}

char *showtime(void)
{
	time_t now;
	struct tm *tmp;
	static char month_names[13][4] =
		{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
				"Sep", "Oct", "Nov", "Dec", "Bug" };
	static char buf[80];

	time(&now);
	tmp = localtime(&now);
	sprintf(buf, "%02d %s %02d:%02d:%02d", tmp->tm_mday,
			month_names[tmp->tm_mon], tmp->tm_hour, tmp->tm_min,
			tmp->tm_sec);
	return buf;
}
