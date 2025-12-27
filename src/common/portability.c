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

/*
 * This file contains function wrappers around OS specific services.
 */

#include <unistd.h>
#include <pwd.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef PLOCKSERVER
# if defined(__linux__)
#  include <sys/mman.h>
# else
#  include <sys/lock.h>
# endif
#endif

#include "version.h"
#include "config.h"
#include "portability.h"

char portability_version[] = VERSION;

int32_t Get_process_id(void)
{
	return getpid();
}

void Get_login_name(char *buf, int32_t size)
{
	/* Unix */
	struct passwd *pwent = getpwuid(geteuid());
	strncpy(buf, pwent->pw_name, size);
	buf[size - 1] = '\0';
}
