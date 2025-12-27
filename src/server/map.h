/* $Id: map.h,v 4.4 1998/09/04 15:04:22 dick Exp $
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-98 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
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

#ifndef TYPES_H
/* need position */
#include "types.h"
#endif
#ifndef RULES_H
/* need rules_t */
#include "rules.h"
#endif
#ifndef ITEM_H
/* need NUM_ITEMS */
#include "item.h"
#endif

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

typedef struct {
    ipos	blk_pos;
    position	pix_pos;
    long	fuel;
    unsigned	conn_mask;
    long	last_change;
    int		team;
} fuel_t;

typedef struct {
    ipos	pos;
    int		dir;
    u_short	team;
} base_t;

typedef struct {
    int		initial;	/* initial number of elements per player. */
    int		num;		/* Number active right now */
} item_t;

typedef struct {
    ipos	pos;
    bool	have;		/* true if this treasure has ball in it */
    u_short	team;		/* team of this treasure */
    int 	destroyed;	/* number of times this treasure destroyed */
} treasure_t;

typedef struct {
    int		NumMembers;		/* Number of current members */
    int		NumRobots;		/* Number of robot players */
    int		NumBases;		/* Number of bases owned */
    int		NumTreasures;		/* Number of treasures owned */
    int		TreasuresDestroyed;	/* Number of destroyed treasures */
    int		TreasuresLeft;		/* Number of treasures left */
} team_t;

typedef struct {
    int		x, y;		/* Size of world in blocks */
    int		diagonal;	/* Diagonal length in blocks */
    int		width, height;	/* Size of world in pixels (optimization) */
    int		hypotenuse;	/* Diagonal length in pixels (optimization) */
    rules_t	*rules;
    char	name[MAX_CHARS];
    char	author[MAX_CHARS];

    u_byte	**block;        /* type of item in each block */
    u_short	**itemID;       /* index into cannon/fuel/targets/treasure/itemConcentrator/bases/grav/wormhole, depending on value of corresponding block, -1 for space, walls, etc */

    item_t	items[NUM_ITEMS];

    team_t	teams[MAX_TEAMS];

    int		NumTeamBases;      /* How many 'different' teams are allowed */
    int		NumBases;
    base_t	*base;
    int		NumFuels;
    fuel_t	*fuel;
    int		NumTreasures;
    treasure_t	*treasures;
    long	nextEvent;
} World_map;

#endif
