/* $Id: map.c,v 1.11 2008/09/02 19:08:51 rotunda_pk Exp $
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-2001 by
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#define SERVER
#include "version.h"
#include "xpconfig.h"
#include "serverconst.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "bit.h"
#include "error.h"
#include "commonproto.h"

int8_t map_version[] = VERSION;

/*
 * Globals.
 */
World_map World;

static void Init_map(void);
static void Alloc_map(void);

#ifdef DEBUG
static void Print_map(void) /* Debugging only. */
{
	int32_t x, y;

	for (y=World.y-1; y>=0; y--) {
		for (x=0; x<World.x; x++)
		switch (World.block[x][y]) {
			case SPACE:
			putchar(' ');
			break;
			case BASE:
			putchar('_');
			break;
			default:
			putchar('X');
			break;
		}
		putchar('\n');
	}
}
#endif

static void Init_map(void)
{
	World.x = 256;
	World.y = 256;
	World.diagonal = (int32_t) LENGTH(World.x, World.y);
	World.width = World.x * BLOCK_SZ;
	World.height = World.y * BLOCK_SZ;
	World.hypotenuse = (int32_t) LENGTH(World.width, World.height);
	World.NumFuels = 0;
	World.NumBases = 0;
	World.NumTreasures = 0;
}

void Free_map(void)
{
	if (World.block) {
		free(World.block);
		World.block = NULL;
	}
	if (World.itemID) {
		free(World.itemID);
		World.itemID = NULL;
	}
	if (World.base) {
		free(World.base);
		World.base = NULL;
	}
	if (World.fuel) {
		free(World.fuel);
		World.fuel = NULL;
	}
}

static void Alloc_map(void)
{
	int32_t x;

	if (World.block)
		Free_map();

	World.block = (uint8_t **) malloc(sizeof(uint8_t *)
			* World.x + World.x * sizeof(uint8_t) * World.y);
	World.itemID = (uint16_t **) malloc(sizeof(uint16_t *)
			* World.x + World.x * sizeof(uint16_t) * World.y);
	World.base = NULL;
	World.fuel = NULL;
	if (World.block == NULL || World.itemID == NULL) {
		Free_map();
		error("Couldn't allocate memory for map (%d bytes)", World.x
				* (World.y * (sizeof(uint8_t)
						+ sizeof(vector_t))
						+ sizeof(vector_t*)
						+ sizeof(uint8_t*)));
		exit(-1);
	}
	else {
		uint8_t *map_line;
		uint8_t **map_pointer;
		uint16_t *item_line;
		uint16_t **item_pointer;

		map_pointer = World.block;
		map_line = (uint8_t*) ((uint8_t**) map_pointer
				+ World.x);
		item_pointer = World.itemID;
		item_line = (uint16_t*) ((uint16_t**) item_pointer
				+ World.x);
		for (x = 0; x < World.x; x++) {
			*map_pointer = map_line;
			map_pointer += 1;
			map_line += World.y;
			*item_pointer = item_line;
			item_pointer += 1;
			item_line += World.y;
		}
	}
}

static void Map_extra_error(int32_t line_num)
{
#ifndef SILENT
	static int32_t prev_line_num, error_count;
	const int32_t max_error = 5;

	if (line_num > prev_line_num) {
		prev_line_num = line_num;
		if (++error_count <= max_error) {
			xpprintf(
					"Map file contains extranous characters on line %d\n",
					line_num);
		}
		else if (error_count - max_error == 1) {
			xpprintf("And so on...\n");
		}
	}
#endif
}

static void Map_missing_error(int32_t line_num)
{
#ifndef SILENT
	static int32_t prev_line_num, error_count;
	const int32_t max_error = 5;

	if (line_num > prev_line_num) {
		prev_line_num = line_num;
		if (++error_count <= max_error) {
			xpprintf("Not enough map data on map data line %d\n",
					line_num);
		}
		else if (error_count - max_error == 1) {
			xpprintf("And so on...\n");
		}
	}
#endif
}

bool Grok_map(void)
{
	int32_t i, x, y, c;
	int8_t *s;

	Init_map();

	if (mapWidth <= 0 || mapWidth > MAX_MAP_SIZE || mapHeight <= 0
			|| mapHeight > MAX_MAP_SIZE) {
		errno = 0;
		error("mapWidth or mapHeight exceeds map size limit [1, %d]",
				MAX_MAP_SIZE);
		free(mapData);
		mapData = NULL;
	}
	else {
		World.x = mapWidth;
		World.y = mapHeight;
	}
	if (extraBorder) {
		World.x += 2;
		World.y += 2;
	}
	World.diagonal = (int32_t) LENGTH(World.x, World.y);
	World.width = World.x * BLOCK_SZ;
	World.height = World.y * BLOCK_SZ;
	World.hypotenuse = (int32_t) LENGTH(World.width, World.height);
	strlcpy(World.name, mapName, sizeof(World.name));
	strlcpy(World.author, mapAuthor, sizeof(World.author));

	if (!mapData) {
		warn("No mapData.");
		return FALSE;
	}

	Alloc_map();

	x = -1;
	y = World.y - 1;

	Set_world_rules();
	Set_world_items();

	s = mapData;
	while (y >= 0) {

		x++;

		if (extraBorder && (x == 0 || x == World.x - 1 || y == 0 || y
				== World.y - 1)) {
			if (x >= World.x) {
				x = -1;
				y--;
				continue;
			}
			else {
				/* make extra border of solid rock */
				c = 'x';
			}
		}
		else {
			c = *s;
			if (c == '\0' || c == EOF) {
				if (x < World.x) {
					/* not enough map data on this line */
					Map_missing_error(World.y - y);
					c = ' ';
				}
				else {
					c = '\n';
				}
			}
			else {
				if (c == '\n' && x < World.x) {
					/* not enough map data on this line */
					Map_missing_error(World.y - y);
					c = ' ';
				}
				else {
					s++;
				}
			}
		}
		if (x >= World.x || c == '\n') {
			y--;
			x = -1;
			if (c != '\n') { /* Get rest of line */
				Map_extra_error(World.y - y);
				while (c != '\n' && c != EOF) {
					c = *s++;
				}
			}
			continue;
		}

		switch (World.block[x][y] = c) {
		case '*':
			if (BIT(World.rules->mode, TEAM_PLAY))
				World.NumTreasures++;
			else
				World.block[x][y] = ' ';
			break;
		case '#':
			World.NumFuels++;
			break;
		case '_':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			World.NumBases++;
			break;
		default:
			break;
		}
	}

	free(mapData);
	mapData = NULL;

	/*
	 * Get space for special objects.
	 */
	if (World.NumFuels > 0 && (World.fuel = (fuel_t *) malloc(
			World.NumFuels * sizeof(fuel_t))) == NULL) {
		error("Out of memory - fuel depots");
		exit(-1);
	}
	if (World.NumTreasures > 0 && (World.treasures = (treasure_t *) malloc(
			World.NumTreasures * sizeof(treasure_t))) == NULL) {
		error("Out of memory - treasures");
		exit(-1);
	}
	if (World.NumBases > 0) {
		if ((World.base = (base_t *) malloc(World.NumBases
				* sizeof(base_t))) == NULL) {
			error("Out of memory - bases");
			exit(-1);
		}
	}
	else {
		error("WARNING: map has no bases!");
	}

	/*
	 * Now reset all counters since we will recount everything
	 * and reuse these counters while inserting the objects
	 * into structures.
	 */
	World.NumFuels = 0;
	World.NumTreasures = 0;
	World.NumBases = 0;

	for (i = 0; i < MAX_TEAMS; i++) {
		World.teams[i].Num = i;
		World.teams[i].NumMembers = 0;
		World.teams[i].NumRobots = 0;
		World.teams[i].NumBases = 0;
		World.teams[i].NumTreasures = 0;
		World.teams[i].TreasuresDestroyed = 0;
		World.teams[i].TreasuresLeft = 0;
		World.teams[i].Swapper = NULL;
	}

	/*
	 * Change read tags to internal data, create objects
	 */
	{
		for (x = 0; x < World.x; x++) {
			uint8_t *line = World.block[x];
			uint16_t *itemID = World.itemID[x];

			for (y = 0; y < World.y; y++) {
				int8_t c = line[y];

				itemID[y] = (uint16_t) -1;

				switch (c) {
				case ' ':
				case '.':
				default:
					line[y] = SPACE;
					break;

				case 'x':
					line[y] = FILLED;
					break;
				case 's':
					line[y] = REC_LU;
					break;
				case 'a':
					line[y] = REC_RU;
					break;
				case 'w':
					line[y] = REC_LD;
					break;
				case 'q':
					line[y] = REC_RD;
					break;

				case '#':
					line[y] = FUEL;
					itemID[y] = World.NumFuels;
					World.fuel[World.NumFuels].blk_pos.x
							= x;
					World.fuel[World.NumFuels].blk_pos.y
							= y;
					World.fuel[World.NumFuels].pix_pos.x
							= (x + 0.5f) * BLOCK_SZ;
					World.fuel[World.NumFuels].pix_pos.y
							= (y + 0.5f) * BLOCK_SZ;
					World.fuel[World.NumFuels].fuel
							= START_STATION_FUEL;
					World.fuel[World.NumFuels].conn_mask
							= (uint32_t) -1;
					World.fuel[World.NumFuels].last_change
							= frame_loops;
					World.fuel[World.NumFuels].team
							= NULL;
					World.fuel[World.NumFuels].id = World.NumFuels;

					World.NumFuels++;
					break;

				case '*':
					line[y] = TREASURE;
					itemID[y] = World.NumTreasures;
					World.treasures[World.NumTreasures].pos.x
							= x;
					World.treasures[World.NumTreasures].pos.y
							= y;
					World.treasures[World.NumTreasures].have
							= false;
					World.treasures[World.NumTreasures].destroyed
							= 0;
					/*
					 * Determining which team it belongs to is done later,
					 * in Find_closest_team().
					 */
					World.treasures[World.NumTreasures].team
							= NULL;

					World.treasures[World.NumTreasures].id = World.NumTreasures;
					World.NumTreasures++;
					break;

				case '$':
					line[y] = BASE_ATTRACTOR;
					break;
				case '_':
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					line[y] = BASE;
					itemID[y] = World.NumBases;
					World.base[World.NumBases].id = World.NumBases;
					World.base[World.NumBases].pos.x = x;
					World.base[World.NumBases].pos.y = y;
					/*
					 * The direction of the base should be so that it points
					 * up with respect to the gravity in the region.  This
					 * is fixed in Find_base_dir() when the gravity has
					 * been computed.
					 */
					World.base[World.NumBases].dir = DIR_UP;
					if (BIT(World.rules->mode, TEAM_PLAY)) {
						if (c >= '0' && c <= '9') {
							World.base[World.NumBases].team = &World.teams[c - '0'];
						}
						else {
							World.base[World.NumBases].team = &World.teams[0];
						}
						World.base[World.NumBases].team->NumBases++;
						if (World.base[World.NumBases].team->NumBases
								== 1)
							World.NumTeamBases++;
					}
					else {
						World.base[World.NumBases].team
								= NULL;
					}
					World.NumBases++;
					break;
				}
			}
		}

		/*
		 * Determine which team a treasure belongs to.
		 */
		if (BIT(World.rules->mode, TEAM_PLAY)) {
			team_t *team = NULL;
			for (i = 0; i < World.NumTreasures; i++) {
				team = Find_closest_team(
						World.treasures[i].pos.x,
						World.treasures[i].pos.y);
				World.treasures[i].team = team;
				if (team == NULL) {
					error(
							"Couldn't find a matching team for the treasure.");
				}
				else {
					team->NumTreasures++;
					team->TreasuresLeft++;
				}
			}
			for (i = 0; i < World.NumFuels; i++) {
				team = Find_closest_team(
						World.fuel[i].blk_pos.x,
						World.fuel[i].blk_pos.y);
				if (team == NULL) {
					error(
							"Couldn't find a matching team for fuelstation.");
				}
				World.fuel[i].team = team;
			}
		}
	}

	if (maxRobots == -1) {
		maxRobots = World.NumBases;
	}

#ifndef	SILENT
	xpprintf(
			"World....: %s\nBases....: %d\nMapsize..: %dx%d\nTeam play: %s\n",
			World.name, World.NumBases, World.x, World.y, BIT(
					World.rules->mode, TEAM_PLAY) ? "on"
					: "off");
#endif

	D(Print_map(); )

	return TRUE;
}

/*
 * Find the correct direction of the base, according to the gravity in
 * the base region.
 *
 * If a base attractor is adjacent to a base then the base will point
 * to the attractor.
 */
void Find_base_direction(void)
{
	int32_t i;

	for (i = 0; i < World.NumBases; i++) {
		int32_t x = World.base[i].pos.x,
		y = World.base[i].pos.y,
		dir,
		att;

		dir = DIR_UP;
		att = -1;
		/*BASES SNAP TO UPWARDS ATTRACTOR FIRST*/
		if (y == World.y - 1 && World.block[x][0] == BASE_ATTRACTOR && BIT(World.rules->mode, WRAP_PLAY)) { /*check wrapped*/
			if (att == -1 || dir == DIR_UP) {
				att = DIR_UP;
			}
		}
		if (y < World.y - 1 && World.block[x][y + 1] == BASE_ATTRACTOR) {
			if (att == -1 || dir == DIR_UP) {
				att = DIR_UP;
			}
		}
		/*THEN DOWNWARDS ATTRACTORS*/
		if (y == 0 && World.block[x][World.y-1] == BASE_ATTRACTOR && BIT(World.rules->mode, WRAP_PLAY)) { /*check wrapped*/
			if (att == -1 || dir == DIR_DOWN) {
				att = DIR_DOWN;
			}
		}
		if (y> 0 && World.block[x][y - 1] == BASE_ATTRACTOR) {
			if (att == -1 || dir == DIR_DOWN) {
				att = DIR_DOWN;
			}
		}
		/*THEN RIGHTWARDS ATTRACTORS*/
		if (x == World.x - 1 && World.block[0][y] == BASE_ATTRACTOR && BIT(World.rules->mode, WRAP_PLAY)) { /*check wrapped*/
			if (att == -1 || dir == DIR_RIGHT) {
				att = DIR_RIGHT;
			}
		}
		if (x < World.x - 1 && World.block[x + 1][y] == BASE_ATTRACTOR) {
			if (att == -1 || dir == DIR_RIGHT) {
				att = DIR_RIGHT;
			}
		}
		/*THEN LEFTWARDS ATTRACTORS*/
		if (x == 0 && World.block[World.x-1][y] == BASE_ATTRACTOR && BIT(World.rules->mode, WRAP_PLAY)) { /*check wrapped*/
			if (att == -1 || dir == DIR_LEFT) {
				att = DIR_LEFT;
			}
		}
		if (x> 0 && World.block[x - 1][y] == BASE_ATTRACTOR) {
			if (att == -1 || dir == DIR_LEFT) {
				att = DIR_LEFT;
			}
		}
		if (att != -1) {
			dir = att;
		}
		World.base[i].dir = dir;
	}
	for (i = 0; i < World.x; i++) {
		int32_t j;
		for (j = 0; j < World.y; j++) {
			if (World.block[i][j] == BASE_ATTRACTOR) {
				World.block[i][j] = SPACE;
			}
		}
	}
}

/*
 * Return the team that is closest to this position.
 */
team_t *Find_closest_team(int32_t posx, int32_t posy)
{
	team_t *team = NULL;
	int32_t i;
	DFLOAT closest = FLT_MAX, l;

	for (i = 0; i < World.NumBases; i++) {
		if (World.base[i].team == NULL)
		continue;

		l = Wrap_length((posx - World.base[i].pos.x)*BLOCK_SZ,
		(posy - World.base[i].pos.y)*BLOCK_SZ);

		if (l < closest) {
			team = World.base[i].team;
			closest = l;
		}
	}

	return team;
}

DFLOAT Wrap_findDir(DFLOAT dx, DFLOAT dy)
{
	dx = WRAP_DX(dx);
	dy = WRAP_DY(dy);
	return findDir(dx, dy);
}

DFLOAT Wrap_length(DFLOAT dx, DFLOAT dy)
{
	dx = WRAP_DX(dx);
	dy = WRAP_DY(dy);
	return LENGTH(dx, dy);
}
