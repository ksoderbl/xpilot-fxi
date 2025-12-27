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

#ifndef	MAP_H
#define	MAP_H

#include "types.h"
#include "structs.h"
#include "global.h"
#include "rules.h"
#include "item.h"
#include "objpos.h"
#include "bit.h"
#include "team.h"

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
#define DIR_UP			(ANGLE_RESOLUTION/4)
#define DIR_LEFT		(ANGLE_RESOLUTION/2)
#define DIR_DOWN		(3*ANGLE_RESOLUTION/4)

/* Properties of teams */
#define TEAM_INVALID		-1
#define TEAM_DEFAULT		(1 << 0)
#define TEAM_ONLY_ROBOTS	(1 << 1)
#define TEAM_ONLY_PAUSERS	(1 << 2)

#define Team_num_is_valid(team_num)	((team_num) >= 0 && (team_num) < MAX_TEAMS)

/* Let active/waiting players see with the eyes of this team's members */
#define TEAM_ALLOW_VIEWING	(1 << 16)

/* Let the pausers' team see with the eyes of this team's members */
#define TEAM_ALLOW_VIEWING_PAUSED	(1 << 17)

/* Show team talk messages of this team to members of pausers' team */
#define TEAM_SHOW_TALK_PAUSED		(1 << 18)

#define Block_is_base(x, y)		(World.block[x][y] == BASE)
#define Block_is_fuel(x, y)		(World.block[x][y] == FUEL)
#define Block_is_treasure(x, y)		(World.block[x][y] == TREASURE)

struct _fuel {
	int32_t id;
	objposition_t pos;
	int32_t fuel;
	uint32_t conn_mask;	/* mask marking player connections which already received an update of this fuel station */
	int32_t last_change;
	team_t *team;
};

typedef struct {
	int32_t initial; /* initial number of elements per player. */
	int32_t num; /* Number active right now */
} item_t;

struct _treasure {
	int32_t id; /* treasure id */
	objposition_t pos;
	bool have; /* true if this treasure has ball in it */
	team_t *team; /* team of this treasure */
	int32_t destroyed; /* number of times this treasure destroyed */
};

struct _base {
	int32_t id;
	objposition_t pos;
	int32_t dir;
	team_t *team;
};

struct World_map_ {
	int32_t x, y; /* Size of world in blocks */
	int32_t diagonal; /* Diagonal length in blocks */
	int32_t width, height; /* Size of world in pixels (optimization) */
	int32_t cwidth, cheight;/* Size of world in clicks (optimization) */
	int32_t hypotenuse; /* Diagonal length in pixels (optimization) */
	rules_t *rules;
	char name[MAX_CHARS];
	char author[MAX_CHARS];

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
};

//extern World_map World;

/** \brief Wraps a positive or negative value to fit it within range [0; limit)
 *
 * \param val		value to be wrapped
 * \param limit		upper bound (must be positive)
 *
 * \return	wrapped value
 */
static inline int32_t Map_wrap_base(int32_t val, int32_t limit)
{
	if (BIT(World.rules->mode, WRAP_PLAY)) {
		while (val >= limit) {
			val -= limit;
		}
		while (val < 0) {
			val += limit;
		}
	}
	else {
		if (val >= limit) {
			val = limit - 1;
		}
		if (val < 0) {
			val = 0;
		}
	}

	return val;
}

/** \brief Wrapping macro for X coordinate in pixels */
#define WRAP_XPIXEL(x_)		Map_wrap_base(x_, World.width)
/** \brief Wrapping macro for Y coordinate in pixels */
#define WRAP_YPIXEL(y_)		Map_wrap_base(y_, World.height)

/** \brief Wrapping macro for X coordinate in blocks */
#define WRAP_XBLOCK(x_)		Map_wrap_base(x_, World.x)
/** \brief Wrapping macro for Y coordinate in blocks */
#define WRAP_YBLOCK(y_)		Map_wrap_base(y_, World.y)

/** \brief Wrapping macro for X coordinate in clicks */
#define WRAP_XCLICK(x_)		Map_wrap_base(x_, World.cwidth)
/** \brief Wrapping macro for Y coordinate in clicks */
#define WRAP_YCLICK(y_)		Map_wrap_base(y_, World.cheight)

/*
 * Two macros for edge wrap of differences in position.
 * If the absolute value of a difference is bigger than
 * half the map size then it is wrapped.
 */
static inline int32_t WRAP_DX(int32_t dx)
{
	return (BIT(World.rules->mode, WRAP_PLAY) ? ((dx) < - (World.width >> 1) ? (dx) + World.width
			: ((dx) > (World.width >> 1) ? (dx) - World.width : (dx))) : (dx));
}

static inline int32_t WRAP_DY(int32_t dy)
{
	return (BIT(World.rules->mode, WRAP_PLAY) ? ((dy) < - (World.height >> 1) ? (dy) + World.height
			: ((dy) > (World.height >> 1) ? (dy) - World.height : (dy))) : (dy));
}

DFLOAT Map_get_discrete_angle(objposition_t *p1, objposition_t *p2);
DFLOAT Map_get_distance(objposition_t *p1, objposition_t *p2);

void Map_compute_base_direction(void);
base_t *Map_get_base_by_pos(objposition_t *pos);
treasure_t *Map_get_treasure_by_pos(objposition_t *pos);
team_t *Map_get_closest_team(objposition_t *pos);

void Fuelstation_add_fuel(fuel_t *fs, int32_t fuel);

void Map_free(void);
bool Map_parse(void);

#endif
