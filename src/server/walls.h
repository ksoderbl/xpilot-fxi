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

#ifndef WALLS_H
#define WALLS_H

extern char walls_version[];

#include "objpos.h"

/*
 * Wall collision detection and bouncing.
 *
 * The wall collision detection routines depend on repeatability
 * (getting the same result even after some "neutral" calculations)
 * and an exact determination whether a point is in space,
 * inside the wall (crash!) or on the edge.
 * This will be hard to achieve if only floating point would be used.
 * However, a resolution of a pixel is a bit rough and ugly.
 * Therefore a fixed point sub-pixel resolution is used called clicks.
 */

#define FLOAT_TO_INT32(F)	((F) < 0 ? -(int32_t)(0.5f-(F)) : (int32_t)((F)+0.5f))
#define DOUBLE_TO_INT32(D)	((D) < 0 ? -(int32_t)(0.5-(D)) : (int32_t)((D)+0.5))

typedef enum {
	NotACrash = 0,
	CrashUniverse = 0x01,
	CrashWall = 0x02,
	CrashTreasure = 0x08,
	CrashUnknown = 0x20,
	CrashWallSpeed = 0x80,
	CrashWallNoFuel = 0x100
} move_crash_t;

typedef enum {
	NotABounce = 0,
	BounceHorLo = 0x01,
	BounceHorHi = 0x02,
	BounceVerLo = 0x04,
	BounceVerHi = 0x08,
	BounceLeftDown = 0x10,
	BounceLeftUp = 0x20,
	BounceRightDown = 0x40,
	BounceRightUp = 0x80,
	BounceEdge = 0x0100
} move_bounce_t;

typedef struct {
	int32_t wall_bounce;
	int32_t treasure_crashes;
	object_t *obj;
	player_t *pl;
} move_info_t;

typedef struct {
	const move_info_t *mip;
	move_crash_t crash;
	move_bounce_t bounce;
	clpos_t pos;
	vector_t vel;
	clvec_t todo;
	clvec_t done;
	int32_t dir;
	treasure_t *treasure;
} move_state_t;

struct move_parameters {
	uint32_t obj_bounce_mask; /* which objects bounce? */
	uint32_t obj_treasure_mask; /* objects treasure crash? */
};

void Walls_init(void);

void Move_init(void);

void Move_object(object_t *obj);
void Move_player(player_t *pl);

void Turn_player(player_t *pl);
void Turn_player_old(player_t *pl);

void Collision_object_wall_check(move_state_t *ms);

#endif
