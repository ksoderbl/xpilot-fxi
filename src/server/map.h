/* $Id: map.h,v 1.11 2008/09/02 19:08:51 rotunda_pk Exp $
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

#ifndef	MAP_H
#define	MAP_H

#include "types.h"
#include "structs.h"

#include "rules.h"
#include "item.h"
#include "object.h"


#define SPACE			0
#define BASE			1
#define FILLED			2
#define REC_LU			3
#define REC_LD			4
#define REC_RU			5
#define REC_RD			6
#define FUEL			7
#define TREASURE		15
#define BASE_ATTRACTOR		127

#define SPACE_BIT		(1 << SPACE)
#define BASE_BIT		(1 << BASE)
#define FILLED_BIT		(1 << FILLED)
#define REC_LU_BIT		(1 << REC_LU)
#define REC_LD_BIT		(1 << REC_LD)
#define REC_RU_BIT		(1 << REC_RU)
#define REC_RD_BIT		(1 << REC_RD)
#define FUEL_BIT		(1 << FUEL)
#define TREASURE_BIT		(1 << TREASURE)

#define DIR_RIGHT		0
#define DIR_UP			(RES/4)
#define DIR_LEFT		(RES/2)
#define DIR_DOWN		(3*RES/4)

/* Properties of teams */
#define TEAM_DEFAULT		(1 << 0)
#define TEAM_ONLY_ROBOTS	(1 << 1)
#define TEAM_ONLY_PAUSERS	(1 << 2)

/* Let active/waiting players see with the eyes of this team's members */
#define TEAM_ALLOW_VIEWING	(1 << 16)

/* Let the pausers' team see with the eyes of this team's members */
#define TEAM_ALLOW_VIEWING_PAUSED	(1 << 17)

/* Show team talk messages of this team to members of pausers' team */
#define TEAM_SHOW_TALK_PAUSED		(1 << 18)


struct _fuel {
	int32_t id;
	ipos_t blk_pos;
	position_t pix_pos;
	int32_t fuel;
	uint32_t conn_mask;
	int32_t last_change;
	team_t *team;
};

struct _base {
	int32_t id;
	ipos_t pos;
	int32_t dir;
	team_t *team;
};

typedef struct {
	int32_t initial; /* initial number of elements per player. */
	int32_t num; /* Number active right now */
} item_t;

struct _treasure {
	ipos_t pos;
	int32_t id; /* treasure id */
	bool have; /* true if this treasure has ball in it */
	team_t *team; /* team of this treasure */
	int32_t destroyed; /* number of times this treasure destroyed */
};

struct _team {
	int32_t Num; /* Number of team */
	int32_t NumMembers; /* Number of current members */
	int32_t NumRobots; /* Number of robot players */
	int32_t NumBases; /* Number of bases owned */
	int32_t NumTreasures; /* Number of treasures owned */
	int32_t TreasuresDestroyed; /* Number of destroyed treasures */
	int32_t TreasuresLeft; /* Number of treasures left */
	player_t *Swapper; /* Player swapping to this full team */
	int32_t flags; /* Properties of the team */
};

typedef struct {
	int32_t x, y; /* Size of world in blocks */
	int32_t diagonal; /* Diagonal length in blocks */
	int32_t width, height; /* Size of world in pixels (optimization) */
	int32_t cwidth, cheight;/* Size of world in clicks (optimization) */
	int32_t hypotenuse; /* Diagonal length in pixels (optimization) */
	rules_t *rules;
	int8_t name[MAX_CHARS];
	int8_t author[MAX_CHARS];

	uint8_t **block; /* type of item in each block */
	uint16_t **itemID; /* index into cannon/fuel/targets/treasure/itemConcentrator/bases/grav/wormhole, depending on value of corresponding block, -1 for space, walls, etc */

	item_t items[NUM_ITEMS];

	team_t teams[MAX_TEAMS];

	int32_t NumTeamBases; /* How many 'different' teams are allowed */
	int32_t NumBases;
	base_t *base;
	int32_t NumFuels;
	fuel_t *fuel;
	int32_t NumTreasures;
	treasure_t *treasures;
	int32_t nextEvent;
} World_map;

#endif
