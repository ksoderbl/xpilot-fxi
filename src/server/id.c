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
#include <stdlib.h>
#include <stdio.h>

#include "version.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "error.h"

char id_version[] = VERSION;

static int32_t ID_queue[NUM_IDS];
static int32_t ID_inuse[NUM_IDS + 1];
static int32_t ID_inited = 0;
static uint32_t get_ID;
static uint32_t put_ID;

static void init_ID(void)
{
	int32_t i, id;

	if (ID_inited == 0) {
		ID_inited = 1;
		for (i = 0, id = 1; i < NUM_IDS; i++, id++) {
			ID_queue[i] = id;
			ID_inuse[id] = 0;
		}
		get_ID = 0;
		put_ID = NUM_IDS;
	}
	if (put_ID - get_ID > NUM_IDS) {
		error("ID queue corruption (%u,%u,%d)", get_ID, put_ID, NUM_IDS);
		exit(1);
	}
}

int32_t peek_ID(void)
{
	int32_t id;

	init_ID();

	if (get_ID == put_ID) {
		id = 0;
	}
	else {
		id = ID_queue[get_ID % NUM_IDS];
	}
	return id;
}

int32_t request_ID(void)
{
	int32_t id;

	id = peek_ID();
	if (id != 0) {
		get_ID++;
		ID_inuse[id] = 1;
	}

	return id;
}

void release_ID(int32_t id)
{
	init_ID();

	if (put_ID - get_ID == NUM_IDS || id <= 0 || id > NUM_IDS
			|| ID_inuse[id] != 1) {
		error("Illegal ID (%u,%u,%d,%d)", get_ID, put_ID, id,
				ID_inuse[id % (NUM_IDS + 1)]);
		exit(1);
	}
	ID_queue[put_ID++ % NUM_IDS] = id;
	ID_inuse[id] = 0;
}

