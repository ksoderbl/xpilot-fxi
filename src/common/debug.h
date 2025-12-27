/*
 * debug.h
 *
 *  Created on: Jan 14, 2009
 *      Author: rotunda
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <execinfo.h>

#include "config.h"

#define MAX_NUM_BACKTRACE_ITEMS		10

//! Specifies a condition that must be met. If it is not, the program terminates.
#if (DEBUG > 0)
#define ASSERT(cond)	if (!(cond)) {\
	printf("Assertion failed (file: %s, line: %d)\n", __FILE__, __LINE__);\
	*(double *) (-1) = 4321.0; \
	abort(); \
	}
#else
#define ASSERT(cond)	do { } while (0);
#endif

#if (DEBUG > 0)
#    define D(x)	x
#else
#    define D(x)
#endif

#define	xpprintf	printf

void DEBUG_PrintBacktrace(void);
char *showtime(void);

#endif /* DEBUG_H_ */
