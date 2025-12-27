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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "version.h"
#include "commonproto.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "score.h"
#include "item.h"
#include "netserver.h"
#include "pack.h"
#include "error.h"
#include "walls.h"
#include "objpos.h"
#include "rank.h"
#include "frame.h"
#include "player_inline.h"

char walls_version[] = VERSION;

#define WALLDIST_MASK	\
	(FILLED_BIT | REC_LU_BIT | REC_LD_BIT | REC_RU_BIT | REC_RD_BIT \
	| FUEL_BIT | TREASURE_BIT )

uint32_t SPACE_BLOCKS = (SPACE_BIT | BASE_BIT);

static struct move_parameters mp;

/*
 * Two dimensional array giving for each point the distance
 * to the nearest wall, including the block containing the point itself.
 * Measured in blocks times 2.
 *
 * Structure:
 * 	(World.x) integers containing pointers to subsequent "lines" of length (World.y)
 * 	(World.x) subsequent "lines" of (World.y) integers, storing distances
 */
static uint8_t **walldist;

/*
 * Allocate memory for the two dimensional "walldist" array.
 */
static void Walldist_alloc(void)
{
	int32_t x;
	uint8_t *wall_line;
	uint8_t **wall_ptr;

	walldist = (uint8_t **) malloc(World.x * sizeof(uint8_t *)
			+ World.x * World.y);
	if (!walldist) {
		error("No memory for walldist");
		exit(1);
	}
	wall_ptr = walldist;
	wall_line = (uint8_t *) (wall_ptr + World.x);
	for (x = 0; x < World.x; x++) {
		*wall_ptr = wall_line;
		wall_ptr += 1;
		wall_line += World.y;
	}
}

static void Walldist_init(void)
{
	int32_t x, y, dx, dy, wx, wy;
	int32_t dist;
	int32_t mindist;
	int32_t maxdist = 2 * MIN(World.x, World.y);
	int32_t newdist;

	typedef struct Qelmt {
		int16_t x, y;
	} Qelmt_t;
	Qelmt_t *q;
	int32_t qfront = 0, qback = 0;

	if (maxdist > 255) {
		maxdist = 255;
	}
	q = (Qelmt_t *) malloc(World.x * World.y * sizeof(Qelmt_t));
	if (!q) {
		error("No memory for walldist init");
		exit(1);
	}
	for (x = 0; x < World.x; x++) {
		for (y = 0; y < World.y; y++) {
			if (BIT((1 << World.block[x][y]), WALLDIST_MASK)) {
				walldist[x][y] = 0;
				q[qback].x = x;
				q[qback].y = y;
				qback++;
			}
			else {
				walldist[x][y] = maxdist;
			}
		}
	}
	if (!BIT(World.rules->mode, WRAP_PLAY)) {
		for (x = 0; x < World.x; x++) {
			for (y = 0; y < World.y; y
					+= (!x || x == World.x - 1) ? 1
							: (World.y - (World.y
									> 1))) {
				if (walldist[x][y] > 1) {
					walldist[x][y] = 2;
					q[qback].x = x;
					q[qback].y = y;
					qback++;
				}
			}
		}
	}
	while (qfront != qback) {
		x = q[qfront].x;
		y = q[qfront].y;
		if (++qfront == World.x * World.y) {
			qfront = 0;
		}
		dist = walldist[x][y];
		mindist = dist + 2;
		if (mindist >= 255) {
			continue;
		}
		for (dx = -1; dx <= 1; dx++) {
			if (BIT(World.rules->mode, WRAP_PLAY) || (x + dx >= 0
					&& x + dx < World.x)) {
				wx = WRAP_XBLOCK(x + dx);
				for (dy = -1; dy <= 1; dy++) {
					if (BIT(World.rules->mode, WRAP_PLAY)
							|| (y + dy >= 0
									&& y
											+ dy
											< World.y)) {
						wy = WRAP_YBLOCK(y + dy);
						if (walldist[wx][wy] > mindist) {
							newdist = mindist;
							if (dist == 0) {
								if (World.block[x][y]
										== REC_LD) {
									if (dx
											== +1
											&& dy
													== +1) {
										newdist
												= mindist
														+ 1;
									}
								}
								else if (World.block[x][y]
										== REC_RD) {
									if (dx
											== -1
											&& dy
													== +1) {
										newdist
												= mindist
														+ 1;
									}
								}
								else if (World.block[x][y]
										== REC_LU) {
									if (dx
											== +1
											&& dy
													== -1) {
										newdist
												= mindist
														+ 1;
									}
								}
								else if (World.block[x][y]
										== REC_RU) {
									if (dx
											== -1
											&& dy
													== -1) {
										newdist
												= mindist
														+ 1;
									}
								}
							}
							if (newdist
									< walldist[wx][wy]) {
								walldist[wx][wy]
										= newdist;
								q[qback].x = wx;
								q[qback].y = wy;
								if (++qback
										== World.x
												* World.y) {
									qback
											= 0;
								}
							}
						}
					}
				}
			}
		}
	}
	free(q);
}

void Walls_init(void)
{
	Walldist_alloc();
	Walldist_init();
}

void Move_init(void)
{
	LIMIT(maxObjectWallBounceSpeed, 0, World.hypotenuse);
	LIMIT(maxShieldedWallBounceSpeed, 0, World.hypotenuse);
	LIMIT(maxUnshieldedWallBounceSpeed, 0, World.hypotenuse);
	LIMIT(playerWallBrakeFactor, 0, 1);
	LIMIT(objectWallBrakeFactor, 0, 1);
	LIMIT(objectWallBounceLifeFactor, 0, 1);

	mp.obj_bounce_mask = 0;
	if (sparksWallBounce) {
		SET_BIT(mp.obj_bounce_mask, OBJ_SPARK_BIT);
	}
	if (debrisWallBounce) {
		SET_BIT(mp.obj_bounce_mask, OBJ_DEBRIS_BIT);
	}
	if (shotsWallBounce) {
		SET_BIT(mp.obj_bounce_mask, OBJ_SHOT_BIT);
	}

	SET_BIT(mp.obj_bounce_mask, OBJ_BALL_BIT);

	mp.obj_treasure_mask = mp.obj_bounce_mask | OBJ_BALL_BIT;
}

static void Bounce_wall(move_state_t *ms, move_bounce_t bounce)
{
	if (!ms->mip->wall_bounce) {
		ms->crash = CrashWall;
		return;
	}
	if (bounce == BounceHorLo) {
		ms->todo.cx = -ms->todo.cx;
		ms->vel.x = -ms->vel.x;
		if (!ms->mip->pl) {
			ms->dir = MOD2(ANGLE_RESOLUTION / 2 - ms->dir, ANGLE_RESOLUTION);
		}
	}
	else if (bounce == BounceHorHi) {
		ms->todo.cx = -ms->todo.cx;
		ms->vel.x = -ms->vel.x;
		if (!ms->mip->pl) {
			ms->dir = MOD2(ANGLE_RESOLUTION / 2 - ms->dir, ANGLE_RESOLUTION);
		}
	}
	else if (bounce == BounceVerLo) {
		ms->todo.cy = -ms->todo.cy;
		ms->vel.y = -ms->vel.y;
		if (!ms->mip->pl) {
			ms->dir = MOD2(ANGLE_RESOLUTION - ms->dir, ANGLE_RESOLUTION);
		}
	}
	else if (bounce == BounceVerHi) {
		ms->todo.cy = -ms->todo.cy;
		ms->vel.y = -ms->vel.y;
		if (!ms->mip->pl) {
			ms->dir = MOD2(ANGLE_RESOLUTION - ms->dir, ANGLE_RESOLUTION);
		}
	}
	else {
		clvec_t t = ms->todo;
		vector_t v = ms->vel;
		if (bounce == BounceLeftDown) {
			ms->todo.cx = -t.cy;
			ms->todo.cy = -t.cx;
			ms->vel.x = -v.y;
			ms->vel.y = -v.x;
			if (!ms->mip->pl) {
				ms->dir = MOD2(3 * ANGLE_RESOLUTION / 4 - ms->dir, ANGLE_RESOLUTION);
			}
		}
		else if (bounce == BounceLeftUp) {
			ms->todo.cx = t.cy;
			ms->todo.cy = t.cx;
			ms->vel.x = v.y;
			ms->vel.y = v.x;
			if (!ms->mip->pl) {
				ms->dir = MOD2(ANGLE_RESOLUTION / 4 - ms->dir, ANGLE_RESOLUTION);
			}
		}
		else if (bounce == BounceRightDown) {
			ms->todo.cx = t.cy;
			ms->todo.cy = t.cx;
			ms->vel.x = v.y;
			ms->vel.y = v.x;
			if (!ms->mip->pl) {
				ms->dir = MOD2(ANGLE_RESOLUTION / 4 - ms->dir, ANGLE_RESOLUTION);
			}
		}
		else if (bounce == BounceRightUp) {
			ms->todo.cx = -t.cy;
			ms->todo.cy = -t.cx;
			ms->vel.x = -v.y;
			ms->vel.y = -v.x;
			if (!ms->mip->pl) {
				ms->dir = MOD2(3 * ANGLE_RESOLUTION / 4 - ms->dir, ANGLE_RESOLUTION);
			}
		}
	}
	ms->bounce = bounce;
}

/*
 * Move a point through one block and detect
 * wall collisions or bounces within that block.
 * Complications arise when the point starts at
 * the edge of a block.  E.g., if a point is on the edge
 * of a block to which block does it belong to?
 *
 * The caller supplies a set of input parameters and expects
 * the following output:
 *  - the number of pixels moved within this block.  (ms->done)
 *  - the number of pixels that still remain to be traversed. (ms->todo)
 *  - whether a crash happened, in which case no pixels will have been
 *    traversed. (ms->crash)
 *  - some extra optional output parameters depending upon the type
 *    of the crash. (ms->treasure)
 *  - whether the point bounced, in which case no pixels will have been
 *    traversed, only a change in direction. (ms->bounce, ms->vel, ms->todo)
 */
static void Move_segment(move_state_t *ms)
{
	ipos_t block; /* block index */
	ivec_t sign; /* sign (-1 or 1) of direction */
	clpos_t enter; /* enter block position in clicks */

	/*
	 * Fill in default return values.
	 */
	ms->crash = NotACrash;
	ms->bounce = NotABounce;
	ms->done.cx = 0;
	ms->done.cy = 0;

	enter = ms->pos;
	enter.cx = WRAP_XCLICK(enter.cx);
	enter.cy = WRAP_YCLICK(enter.cy);

	sign.x = (ms->vel.x < 0) ? -1 : 1;
	sign.y = (ms->vel.y < 0) ? -1 : 1;

	block.x = CLICK_TO_BLOCK(enter.cx);
	block.y = CLICK_TO_BLOCK(enter.cy);

	/* If there is enough space, execute the move. Otherwise handle a wall collision. */
	if (walldist[block.x][block.y] > 2) {
		int32_t maxcl =  BLOCK_TO_CLICK(walldist[block.x][block.y] - 2) / 2;

		if (maxcl >= sign.x * ms->todo.cx && maxcl >= sign.y * ms->todo.cy) {
			/* entire movement is possible. */
			ms->done.cx = ms->todo.cx;
			ms->done.cy = ms->todo.cy;
		}
		else if (sign.x * ms->todo.cx > sign.y * ms->todo.cy) {
			/* horizontal movement. */
			ms->done.cx = sign.x * maxcl;
			ms->done.cy = ms->todo.cy * maxcl / (sign.x * ms->todo.cx);
		}
		else {
			/* vertical movement. */
			ms->done.cx = ms->todo.cx * maxcl / (sign.y * ms->todo.cy);
			ms->done.cy = sign.y * maxcl;
		}

		ms->todo.cx -= ms->done.cx;
		ms->todo.cy -= ms->done.cy;
	}
	else {
		Collision_object_wall_check(ms);
	}
}

void Collision_object_wall_check(move_state_t *ms)
{
	int32_t need_adjust; /* other param (x or y) needs recalc */
	int32_t inside; /* inside the block or else on edge */
	clpos_t delta; /* delta position in clicks */
	clpos_t leave; /* leave block position in clicks */
	clpos_t offset; /* offset within block in clicks */
	const move_info_t * const mi = ms->mip; /* alias */
	object_t *obj;
	int32_t i;
	int32_t block_type; /* type of block we're going through */
	uint32_t wall_bounce; /* are we bouncing? what direction? */
	ipos_t blk2; /* new block index */
	clpos_t off2; /* last offset in block in clicks */
	clpos_t mid; /* the mean of (offset+off2)/2 */

	ipos_t block; /* block index */
	ivec_t sign; /* sign (-1 or 1) of direction */
	clpos_t enter; /* enter block position in clicks */

	enter = ms->pos;
	enter.cx = WRAP_XCLICK(enter.cx);
	enter.cy = WRAP_YCLICK(enter.cy);

	sign.x = (ms->vel.x < 0) ? -1 : 1;
	sign.y = (ms->vel.y < 0) ? -1 : 1;

	block.x = CLICK_TO_BLOCK(enter.cx);
	block.y = CLICK_TO_BLOCK(enter.cy);

	/* Compute the offset from block boundary - cannot use BLOCK_TO_CLICK() here */
	offset.cx = enter.cx - BLOCK_TO_CLICK(block.x);
	offset.cy = enter.cy - BLOCK_TO_CLICK(block.y);

	inside = 1;
	if (offset.cx == 0) {
		inside = 0;
		if (sign.x == -1 && (offset.cx = BLOCK_CLICKS, --block.x < 0)) {
			block.x += World.x;
		}
	}

	if (offset.cy == 0) {
		inside = 0;
		if (sign.y == -1 && (offset.cy = BLOCK_CLICKS, --block.y < 0)) {
			block.y += World.y;
		}
	}

	need_adjust = 0;
	if (sign.x == -1) {
		if (offset.cx + ms->todo.cx < 0) {
			leave.cx = enter.cx - offset.cx;
			need_adjust = 1;
		}
		else {
			leave.cx = enter.cx + ms->todo.cx;
		}
	}
	else {
		if (offset.cx + ms->todo.cx > BLOCK_CLICKS) {
			leave.cx = enter.cx + BLOCK_CLICKS - offset.cx;
			need_adjust = 1;
		}
		else {
			leave.cx = enter.cx + ms->todo.cx;
		}
	}
	if (sign.y == -1) {
		if (offset.cy + ms->todo.cy < 0) {
			leave.cy = enter.cy - offset.cy;
			need_adjust = 1;
		}
		else {
			leave.cy = enter.cy + ms->todo.cy;
		}
	}
	else {
		if (offset.cy + ms->todo.cy > BLOCK_CLICKS) {
			leave.cy = enter.cy + BLOCK_CLICKS - offset.cy;
			need_adjust = 1;
		}
		else {
			leave.cy = enter.cy + ms->todo.cy;
		}
	}
	if (need_adjust && ms->todo.cy && ms->todo.cx) {
		double wx = (double) (leave.cx - enter.cx) / ms->todo.cx;
		double wy = (double) (leave.cy - enter.cy) / ms->todo.cy;
		if (wx > wy) {
			double x = ms->todo.cx * wy;
			leave.cx = enter.cx + DOUBLE_TO_INT32(x);
		}
		else if (wx < wy) {
			double y = ms->todo.cy * wx;
			leave.cy = enter.cy + DOUBLE_TO_INT32(y);
		}
	}

	delta.cx = leave.cx - enter.cx;
	delta.cy = leave.cy - enter.cy;

	block_type = World.block[block.x][block.y];
	obj = mi->obj;

	/*
	 * We test for several different bouncing directions against the wall.
	 * Sometimes there is more than one bounce possible if the point
	 * starts at the corner of a block.
	 * Therefore we maintain a bit mask for the bouncing possibilities
	 * and later we will determine which bounce is appropriate.
	 */
	wall_bounce = 0;

	switch (block_type) {
	case TREASURE:
		/* Ball-treasure collisions */
		if (mi->treasure_crashes) {
			const DFLOAT r = 0.5f * BLOCK_CLICKS;

			/*
			 * Test if the movement is within the upper half of
			 * the treasure, which is the upper half of a circle.
			 * If this is the case then we test if 3 samples
			 * are not hitting the treasure.
			 */
			off2.cx = offset.cx + delta.cx;
			off2.cy = offset.cy + delta.cy;
			mid.cx = (offset.cx + off2.cx) / 2;
			mid.cy = (offset.cy + off2.cy) / 2;
			if (offset.cy > r && off2.cy > r && sqr(mid.cx - r) + sqr(mid.cy - r) > sqr(r)
					&& sqr(off2.cx - r) + sqr(off2.cy - r) > sqr(r)
					&& sqr(offset.cx - r) + sqr(offset.cy - r) > sqr(r)) {
				break;
			}

			for (i = 0; ; i++) {
				if (World.treasures[i].pos.bx == block.x && World.treasures[i].pos.by == block.y) {
					break;
				}
			}
			ms->treasure = &World.treasures[i];

			/*
			 * This function operates on objects and players, and here a distinction has to be made.
			 * Players basically crash against the treasure box, but in case of objects (esp. balls)
			 * it's much more complicated to handle.
			 */
			if (obj) {
				/* cases when the object shouldn't be destroyed */
				if (ms->treasure == obj->treasure) {
					if (Object_is_type(obj, OBJ_BALL) && obj->owner && obj->owner->team != obj->treasure->team) {
						/*
						 * The ball hasn't left the box boundaries, but has been tampered with.
						 * We reset the frame counter of the ball here, because we want ballruns
						 * to start when the ball leaves the box completely.
						 */
						obj->loose_count = 0;
						obj->loose_count_ticks = 0;

						ms->crash = NotACrash;
						break;
					}
				}

				/* below: cases when the object should be destroyed */
				ms->crash = CrashTreasure;
				Object_expire(obj);

				/* Further processing concerns balls only */
				if (!Object_is_type(obj, OBJ_BALL)) {
					return;
				}

				/*
				 * Time out the ball immediately if the treasure doesn't belong to any team or
				 * the ball doesn't belong to anyone.
				 */
				if (!ms->treasure->team || !obj->owner) {
					return;
				}

				/* replace */
				if (ms->treasure == obj->treasure) {
					/*
					 * Ball has been replaced back in the hoop from whence
					 * it came.  If the player is on the same team as the
					 * hoop, then it should be replaced into the hoop without
					 * exploding and gets the player some points.  Otherwise
					 * nothing interesting happens.
					 */
					player_t *pl = NULL;
					treasure_t *tt = ms->treasure;

					pl = obj->owner;

					Message_game_important_print("%s (team %d) has replaced the treasure", pl->name, pl->team->Num);
					SCORE(pl, 5, &tt->pos, "Treasure: ");
					Rank_saved_ball(pl);
					return;
				}

				if (ms->treasure->team == obj->owner->team) {
					/*
					 * Ball has been brought back to home treasure.
					 * The team should be punished.
					 */
					Message_game_important_print("The ball was loose for %d frames / %d ticks (best %.2f) / %.2fs",
							obj->loose_count, obj->loose_count_ticks,
							Rank_get_best_ballrun(obj->owner), obj->loose_count_ticks / gameSpeed);

					SCORE_LegacyBallCash(ms, (double) obj->loose_count_ticks);

					return;
				}
			}

			/* Non-objects (players) are handled here */
			else {
				ms->crash = CrashTreasure;
				return;
			}

			return;
		}

	/* NOTE: fall through if objects do not crash against treasure boxes */

	case FUEL:
	case FILLED:
		if (inside) {
			/* Could happen for targets reappearing and in case of bugs. */
			ms->crash = CrashWall;
			return;
		}
		if (offset.cx == 0) {
			if (ms->vel.x > 0) {
				wall_bounce |= BounceHorLo;
			}
		}
		else if (offset.cx == BLOCK_CLICKS) {
			if (ms->vel.x < 0) {
				wall_bounce |= BounceHorHi;
			}
		}
		if (offset.cy == 0) {
			if (ms->vel.y > 0) {
				wall_bounce |= BounceVerLo;
			}
		}
		else if (offset.cy == BLOCK_CLICKS) {
			if (ms->vel.y < 0) {
				wall_bounce |= BounceVerHi;
			}
		}
		if (wall_bounce) {
			break;
		}
		if (!(ms->todo.cx | ms->todo.cy)) {
			/* no bouncing possible and no movement.  OK. */
			break;
		}
		if (!ms->todo.cx && (offset.cx == 0 || offset.cx
				== BLOCK_CLICKS)) {
			/* tricky */
			break;
		}
		if (!ms->todo.cy && (offset.cy == 0 || offset.cy
				== BLOCK_CLICKS)) {
			/* tricky */
			break;
		}
		/* what happened? we should never reach this */
		ms->crash = CrashWall;
		return;

	case REC_LD:
		/* test for bounces first. */
		if (offset.cx == 0) {
			if (ms->vel.x > 0) {
				wall_bounce |= BounceHorLo;
			}
			if (offset.cy == BLOCK_CLICKS && ms->vel.x + ms->vel.y
					< 0) {
				wall_bounce |= BounceLeftDown;
			}
		}
		if (offset.cy == 0) {
			if (ms->vel.y > 0) {
				wall_bounce |= BounceVerLo;
			}
			if (offset.cx == BLOCK_CLICKS && ms->vel.x + ms->vel.y
					< 0) {
				wall_bounce |= BounceLeftDown;
			}
		}
		if (wall_bounce) {
			break;
		}
		if (offset.cx + offset.cy < BLOCK_CLICKS) {
			ms->crash = CrashWall;
			return;
		}
		if (offset.cx + delta.cx + offset.cy + delta.cy >= BLOCK_CLICKS) {
			/* movement is entirely within the space part of the block. */
			break;
		}
		/*
		 * Find out where we bounce exactly
		 * and how far we can move before bouncing.
		 */
		if (sign.x * ms->todo.cx >= sign.y * ms->todo.cy) {
			double w = (double) ms->todo.cy / ms->todo.cx;
			delta.cx = (int32_t) ((BLOCK_CLICKS - offset.cx - offset.cy) / (1 + w));
			delta.cy = (int32_t) (delta.cx * w);
			if (offset.cx + delta.cx + offset.cy + delta.cy
					< BLOCK_CLICKS) {
				delta.cx++;
				delta.cy = (int32_t) (delta.cx * w);
			}
			leave.cx = enter.cx + delta.cx;
			leave.cy = enter.cy + delta.cy;
			if (!delta.cx) {
				wall_bounce |= BounceLeftDown;
				break;
			}
		}
		else {
			double w = (double) ms->todo.cx / ms->todo.cy;
			delta.cy = (int32_t) ((BLOCK_CLICKS - offset.cx - offset.cy) / (1 + w));
			delta.cx = (int32_t) (delta.cy * w);
			if (offset.cx + delta.cx + offset.cy + delta.cy < BLOCK_CLICKS) {
				delta.cy++;
				delta.cx = (int32_t) (delta.cy * w);
			}
			leave.cx = enter.cx + delta.cx;
			leave.cy = enter.cy + delta.cy;
			if (!delta.cy) {
				wall_bounce |= BounceLeftDown;
				break;
			}
		}
		break;

	case REC_LU:
		if (offset.cx == 0) {
			if (ms->vel.x > 0) {
				wall_bounce |= BounceHorLo;
			}
			if (offset.cy == 0 && ms->vel.x < ms->vel.y) {
				wall_bounce |= BounceLeftUp;
			}
		}
		if (offset.cy == BLOCK_CLICKS) {
			if (ms->vel.y < 0) {
				wall_bounce |= BounceVerHi;
			}
			if (offset.cx == BLOCK_CLICKS && ms->vel.x < ms->vel.y) {
				wall_bounce |= BounceLeftUp;
			}
		}
		if (wall_bounce) {
			break;
		}
		if (offset.cx < offset.cy) {
			ms->crash = CrashWall;
			return;
		}
		if (offset.cx + delta.cx >= offset.cy + delta.cy) {
			break;
		}
		if (sign.x * ms->todo.cx >= sign.y * ms->todo.cy) {
			double w = (double) ms->todo.cy / ms->todo.cx;
			delta.cx = (int32_t) ((offset.cy - offset.cx) / (1 - w));
			delta.cy = (int32_t) (delta.cx * w);
			if (offset.cx + delta.cx < offset.cy + delta.cy) {
				delta.cx++;
				delta.cy = (int32_t) (delta.cx * w);
			}
			leave.cx = enter.cx + delta.cx;
			leave.cy = enter.cy + delta.cy;
			if (!delta.cx) {
				wall_bounce |= BounceLeftUp;
				break;
			}
		}
		else {
			double w = (double) ms->todo.cx / ms->todo.cy;
			delta.cy = (int32_t) ((offset.cx - offset.cy) / (1 - w));
			delta.cx = (int32_t) (delta.cy * w);
			if (offset.cx + delta.cx < offset.cy + delta.cy) {
				delta.cy--;
				delta.cx = (int32_t) (delta.cy * w);
			}
			leave.cx = enter.cx + delta.cx;
			leave.cy = enter.cy + delta.cy;
			if (!delta.cy) {
				wall_bounce |= BounceLeftUp;
				break;
			}
		}
		break;

	case REC_RD:
		if (offset.cx == BLOCK_CLICKS) {
			if (ms->vel.x < 0) {
				wall_bounce |= BounceHorHi;
			}
			if (offset.cy == BLOCK_CLICKS && ms->vel.x > ms->vel.y) {
				wall_bounce |= BounceRightDown;
			}
		}
		if (offset.cy == 0) {
			if (ms->vel.y > 0) {
				wall_bounce |= BounceVerLo;
			}
			if (offset.cx == 0 && ms->vel.x > ms->vel.y) {
				wall_bounce |= BounceRightDown;
			}
		}
		if (wall_bounce) {
			break;
		}
		if (offset.cx > offset.cy) {
			ms->crash = CrashWall;
			return;
		}
		if (offset.cx + delta.cx <= offset.cy + delta.cy) {
			break;
		}
		if (sign.x * ms->todo.cx >= sign.y * ms->todo.cy) {
			double w = (double) ms->todo.cy / ms->todo.cx;
			delta.cx = (int32_t) ((offset.cy - offset.cx) / (1 - w));
			delta.cy = (int32_t) (delta.cx * w);
			if (offset.cx + delta.cx > offset.cy + delta.cy) {
				delta.cx--;
				delta.cy = (int32_t) (delta.cx * w);
			}
			leave.cx = enter.cx + delta.cx;
			leave.cy = enter.cy + delta.cy;
			if (!delta.cx) {
				wall_bounce |= BounceRightDown;
				break;
			}
		}
		else {
			double w = (double) ms->todo.cx / ms->todo.cy;
			delta.cy = (int32_t) ((offset.cx - offset.cy) / (1 - w));
			delta.cx = (int32_t) (delta.cy * w);
			if (offset.cx + delta.cx > offset.cy + delta.cy) {
				delta.cy++;
				delta.cx = (int32_t) (delta.cy * w);
			}
			leave.cx = enter.cx + delta.cx;
			leave.cy = enter.cy + delta.cy;
			if (!delta.cy) {
				wall_bounce |= BounceRightDown;
				break;
			}
		}
		break;

	case REC_RU:
		if (offset.cx == BLOCK_CLICKS) {
			if (ms->vel.x < 0) {
				wall_bounce |= BounceHorHi;
			}
			if (offset.cy == 0 && ms->vel.x + ms->vel.y > 0) {
				wall_bounce |= BounceRightUp;
			}
		}
		if (offset.cy == BLOCK_CLICKS) {
			if (ms->vel.y < 0) {
				wall_bounce |= BounceVerHi;
			}
			if (offset.cx == 0 && ms->vel.x + ms->vel.y > 0) {
				wall_bounce |= BounceRightUp;
			}
		}
		if (wall_bounce) {
			break;
		}
		if (offset.cx + offset.cy > BLOCK_CLICKS) {
			ms->crash = CrashWall;
			return;
		}
		if (offset.cx + delta.cx + offset.cy + delta.cy <= BLOCK_CLICKS) {
			break;
		}
		if (sign.x * ms->todo.cx >= sign.y * ms->todo.cy) {
			double w = (double) ms->todo.cy / ms->todo.cx;
			delta.cx = (int32_t) ((BLOCK_CLICKS - offset.cx - offset.cy) / (1 + w));
			delta.cy = (int32_t) (delta.cx * w);
			if (offset.cx + delta.cx + offset.cy + delta.cy
					> BLOCK_CLICKS) {
				delta.cx--;
				delta.cy = (int32_t) (delta.cx * w);
			}
			leave.cx = enter.cx + delta.cx;
			leave.cy = enter.cy + delta.cy;
			if (!delta.cx) {
				wall_bounce |= BounceRightUp;
				break;
			}
		}
		else {
			double w = (double) ms->todo.cx / ms->todo.cy;
			delta.cy = (int32_t) ((BLOCK_CLICKS - offset.cx - offset.cy) / (1 + w));
			delta.cx = (int32_t) (delta.cy * w);
			if (offset.cx + delta.cx + offset.cy + delta.cy > BLOCK_CLICKS) {
				delta.cy--;
				delta.cx = (int32_t) (delta.cy * w);
			}
			leave.cx = enter.cx + delta.cx;
			leave.cy = enter.cy + delta.cy;
			if (!delta.cy) {
				wall_bounce |= BounceRightUp;
				break;
			}
		}
		break;

	default:
		break;
	}

	if (wall_bounce) {
		/*
		 * Bouncing.  As there may be more than one possible bounce
		 * test which bounce is not feasible because of adjacent walls.
		 * If there still is more than one possible then pick one randomly.
		 * Else if it turns out that none is feasible then we must have
		 * been trapped inbetween two blocks.  This happened in the early
		 * stages of this code.
		 */
		int32_t count = 0;
		uint32_t bit;
		uint32_t save_wall_bounce = wall_bounce;
		uint32_t block_mask = FILLED_BIT | FUEL_BIT;

		if (!mi->treasure_crashes) {
			block_mask |= TREASURE_BIT;
		}
		for (bit = 1; bit <= wall_bounce; bit <<= 1) {
			if (!(wall_bounce & bit)) {
				continue;
			}

			CLR_BIT(wall_bounce, bit);
			switch (bit) {

			case BounceHorLo:
				blk2.x = block.x - 1;
				if (blk2.x < 0) {
					blk2.x += World.x;
				}
				blk2.y = block.y;
				if (BIT(1 << World.block[blk2.x][blk2.y], block_mask | REC_RU_BIT | REC_RD_BIT)) {
					continue;
				}
				break;

			case BounceHorHi:
				blk2.x = block.x + 1;
				if (blk2.x >= World.x) {
					blk2.x -= World.x;
				}
				blk2.y = block.y;
				if (BIT(1 << World.block[blk2.x][blk2.y], block_mask | REC_LU_BIT | REC_LD_BIT)) {
					continue;
				}
				break;

			case BounceVerLo:
				blk2.x = block.x;
				blk2.y = block.y - 1;
				if (blk2.y < 0) {
					blk2.y += World.y;
				}
				if (BIT(1 << World.block[blk2.x][blk2.y], block_mask | REC_RU_BIT | REC_LU_BIT)) {
					continue;
				}
				break;

			case BounceVerHi:
				blk2.x = block.x;
				blk2.y = block.y + 1;
				if (blk2.y >= World.y) {
					blk2.y -= World.y;
				}
				if (BIT(1 << World.block[blk2.x][blk2.y], block_mask | REC_RD_BIT | REC_LD_BIT)) {
					continue;
				}
				break;
			}

			SET_BIT(wall_bounce, bit);
			count++;
		}

		if (!count) {
			wall_bounce = save_wall_bounce;
			switch (wall_bounce) {
			case BounceHorLo | BounceVerLo:
				wall_bounce = BounceLeftDown;
				break;
			case BounceHorLo | BounceVerHi:
				wall_bounce = BounceLeftUp;
				break;
			case BounceHorHi | BounceVerLo:
				wall_bounce = BounceRightDown;
				break;
			case BounceHorHi | BounceVerHi:
				wall_bounce = BounceRightUp;
				break;
			default:
				switch (block_type) {
				case REC_LD:
					if ((offset.cx == 0) ? (offset.cy == BLOCK_CLICKS) : (offset.cx == BLOCK_CLICKS
							&& offset.cy == 0) && ms->vel.x + ms->vel.y >= 0) {
						wall_bounce = 0;
					}
					break;
				case REC_LU:
					if ((offset.cx == 0) ? (offset.cy == 0) : (offset.cx == BLOCK_CLICKS
							&& offset.cy == BLOCK_CLICKS) && ms->vel.x >= ms->vel.y) {
						wall_bounce = 0;
					}
					break;
				case REC_RD:
					if ((offset.cx == 0) ? (offset.cy == 0): (offset.cx == BLOCK_CLICKS
							&& offset.cy == BLOCK_CLICKS) && ms->vel.x <= ms->vel.y) {
						wall_bounce = 0;
					}
					break;
				case REC_RU:
					if ((offset.cx == 0) ? (offset.cy == BLOCK_CLICKS) : (offset.cx == BLOCK_CLICKS
							&& offset.cy == 0) && ms->vel.x + ms->vel.y <= 0) {
						wall_bounce = 0;
					}
					break;
				}
				if (wall_bounce) {
					ms->crash = CrashWall;
					return;
				}
			}
		}
		else if (count > 1) {
			/*
			 * More than one bounce possible.
			 * Pick one randomly.
			 */
			count = (int32_t) (rfrac() * count);
			for (bit = 1; bit <= wall_bounce; bit <<= 1) {
				if (wall_bounce & bit) {
					if (count == 0) {
						wall_bounce = bit;
						break;
					}
					else {
						count--;
					}
				}
			}
		}
	}

	if (wall_bounce) {
		Bounce_wall(ms, (move_bounce_t) wall_bounce);
	}
	else {
		ms->done.cx += delta.cx;
		ms->done.cy += delta.cy;
		ms->todo.cx -= delta.cx;
		ms->todo.cy -= delta.cy;
	}
}

static void Object_crash(move_state_t *ms)
{
	object_t *obj = ms->mip->obj;

	switch (ms->crash) {

	default:
		break;

	case CrashTreasure:
		/*
		 * Ball type has already been handled.
		 */
		if (Object_is_type(obj, OBJ_BALL)) {
			break;
		}
		Object_expire(obj);
		break;

	case CrashWall:
		Object_expire(obj);
		break;

	case CrashUniverse:
		Object_expire(obj);
		break;

	case CrashUnknown:
		Object_expire(obj);
		break;
	}
}

void Move_object(object_t *obj)
{
	int32_t nothing_done = 0;
	int32_t dist;
	move_info_t mi;
	move_state_t ms;
	bool pos_update = false;

	if (Frame_is_real()) {
		dist = walldist[obj->pos.bx][obj->pos.by];
	}
	else {
		dist = walldist[obj->pos_interp.bx][obj->pos_interp.by];
	}

	if (dist > 2) {
		int32_t max = ((dist - 2) * BLOCK_SZ) >> 1;

		/* Check whether a wall collision is going to occur within
		 * the next frame.
		 */
		if (Frame_is_real()) {
			if (sqr(max) >= sqr(obj->vel.x) + sqr(obj->vel.y)) {
				clpos_t pos;

				Position_simple_set(&pos,
						obj->pos.cx + FLOAT_TO_CLICK(obj->vel.x),
						obj->pos.cy + FLOAT_TO_CLICK(obj->vel.y));
				Position_set(&obj->pos, &pos);

				return;
			}
		}
		else {
	                if (sqr(max) >= sqr(obj->vel_interp.x * ticksPerFrame) + sqr(obj->vel_interp.y) * ticksPerFrame) {
	                        clpos_t pos;

	                        Position_simple_set(&pos,
	                                        obj->pos_interp.cx + FLOAT_TO_CLICK(obj->vel_interp.x * ticksPerFrame),
	                                        obj->pos_interp.cy + FLOAT_TO_CLICK(obj->vel_interp.y * ticksPerFrame));
	                        Position_set(&obj->pos_interp, &pos);

	                        return;
	                }
		}
	}

	mi.pl = NULL;
	mi.obj = obj;
	mi.wall_bounce = BIT(mp.obj_bounce_mask, OBJ_TYPEBIT(obj->type));
	mi.treasure_crashes = BIT(mp.obj_treasure_mask, OBJ_TYPEBIT(obj->type));

	if (Frame_is_real()) {
		ms.pos.cx = obj->pos.cx;
		ms.pos.cy = obj->pos.cy;
		ms.vel = obj->vel;
		ms.todo.cx = FLOAT_TO_CLICK(ms.vel.x);
		ms.todo.cy = FLOAT_TO_CLICK(ms.vel.y);
	}
	else {
	        ms.pos.cx = obj->pos_interp.cx;
	        ms.pos.cy = obj->pos_interp.cy;
	        ms.vel = obj->vel_interp;
	        ms.todo.cx = FLOAT_TO_CLICK(ms.vel.x * ticksPerFrame);
	        ms.todo.cy = FLOAT_TO_CLICK(ms.vel.y * ticksPerFrame);
	}

	ms.dir = obj->dir;
	ms.mip = &mi;

	for (;;) {
		Move_segment(&ms);
		if (!(ms.done.cx | ms.done.cy)) {
			pos_update |= (ms.crash | ms.bounce);
			if (ms.crash) {
				break;
			}
			if (ms.bounce && ms.bounce != BounceEdge) {
				if (!Object_is_type(obj, OBJ_BALL)) {
					obj->obj_life = (int32_t) (obj->obj_life * objectWallBounceLifeFactor);
				}
				if (Object_is_expired(obj)) {
					break;
				}
				/*
				 * Any bouncing sparks are no longer owner immune to give
				 * "reactive" thrust.  This is exactly like ground effect
				 * in the real world.  Very useful for stopping against walls.
				 *
				 * If the FROMBOUNCE bit is set the spark was caused by
				 * the player bouncing of a wall and thus although the spark
				 * should bounce, it is not reactive thrust otherwise wall
				 * bouncing would cause acceleration of the player.
				 */
				if (!BIT(obj->obj_status, FROMBOUNCE) && Object_is_type(obj, OBJ_SPARK)) {
					CLR_BIT(obj->obj_status, OWNERIMMUNE);
				}
				if (sqr(ms.vel.x) + sqr(ms.vel.y) > sqr(maxObjectWallBounceSpeed)) {
					Object_expire(obj);
					break;
				}
				ms.vel.x *= objectWallBrakeFactor;
				ms.vel.y *= objectWallBrakeFactor;
				ms.todo.cx = (int32_t) (ms.todo.cx * objectWallBrakeFactor);
				ms.todo.cy = (int32_t) (ms.todo.cy * objectWallBrakeFactor);
			}
			if (++nothing_done >= 5) {
				ms.crash = CrashUnknown;
				break;
			}
		}
		else {
			Position_simple_add(&ms.pos, ms.done.cx, ms.done.cy);
			nothing_done = 0;
		}

		if (!(ms.todo.cx | ms.todo.cy)) {
			break;
		}
	}

	if (Frame_is_real()) {
		Position_set(&obj->pos, &ms.pos);
		obj->vel = ms.vel;
	}
	else {
	        Position_set(&obj->pos_interp, &ms.pos);
	        obj->vel_interp = ms.vel;
	}

	obj->dir = ms.dir;

	if (Frame_is_real()) {
		if (ms.crash) {
			Object_crash(&ms);
		}
		if (pos_update) {
//			Object_position_remember(obj);
		}
	}
}

static void Player_crash(move_state_t *ms, int32_t pt, bool turning)
{
	player_t *pl = ms->mip->pl;
	const char *howfmt = NULL;
	const char *hudmsg = NULL;

	switch (ms->crash) {

	default:
	case NotACrash:
		errno = 0;
		error("Player_crash not a crash %d", ms->crash);
		break;

	case CrashWall:
		howfmt = "%s crashed%s against a wall";
		hudmsg = "[Wall]";
		break;

	case CrashWallSpeed:
		howfmt = "%s smashed%s against a wall";
		hudmsg = "[Wall]";
		break;

	case CrashWallNoFuel:
		howfmt = "%s smacked%s against a wall";
		hudmsg = "[Wall]";
		break;

	case CrashTreasure:
		howfmt = "%s smashed%s against a treasure";
		hudmsg = "[Treasure]";
		break;

	case CrashUniverse:
		howfmt = "%s left the known universe%s";
		hudmsg = "[Universe]";
		break;

	case CrashUnknown:
		howfmt = "%s slammed%s into a programming error";
		hudmsg = "[Bug]";
		break;
	}

	if (howfmt && hudmsg) {
		char msg[MSG_LEN];
		player_t *pushers[MAX_RECORDED_SHOVES];
		int32_t cnt[MAX_RECORDED_SHOVES];
		int32_t num_pushers = 0;
		int32_t total_pusher_count = 0;
		int32_t total_pusher_score = 0;
		int32_t i, j, sc;

		Player_set_state(pl, PL_STATE_KILLED);
		sprintf(msg, howfmt, pl->name, (!pt) ? " head first" : "");

		/* get a list of who pushed me */
		for (i = 0; i < MAX_RECORDED_SHOVES; i++) {
			shove_t *shove = &pl->shove_record[i];
			if (shove->pusher_pl == NULL) {
				continue;
			}
			if (shove->time < frame_loops - 20) {
				continue;
			}
			for (j = 0; j < num_pushers; j++) {
				if (shove->pusher_pl == pushers[j]) {
					cnt[j]++;
					break;
				}
			}
			if (j == num_pushers) {
				pushers[num_pushers++] = shove->pusher_pl;
				cnt[j] = 1;
			}
			total_pusher_count++;
			total_pusher_score += pushers[j]->score;
		}
		if (num_pushers == 0) {
			strcat(msg, ".");
			sc = Rate(WALL_SCORE, pl->score);
			SCORE(pl, -sc, &pl->pos, hudmsg);
			pl->self_deaths++;
		}
		else {
			char msg[MSG_LEN];
			int32_t msg_len = strlen(msg);
			char *msg_ptr = &msg[msg_len];
			int32_t average_pusher_score = total_pusher_score
					/ total_pusher_count;

			for (i = 0; i < num_pushers; i++) {
				player_t *pusher = pushers[i];

				const char *sep = (!i) ? " with help from "
						: (i < num_pushers - 1) ? ", "
								: " and ";
				int32_t sep_len = strlen(sep);
				int32_t name_len = strlen(pusher->name);

				if (msg_len + sep_len + name_len + 2 < sizeof msg) {
					strcpy(msg_ptr, sep);
					msg_len += sep_len;
					msg_ptr += sep_len;
					strcpy(msg_ptr, pusher->name);
					msg_len += name_len;
					msg_ptr += name_len;
				}

				sc = cnt[i] * (int32_t) floor(Rate(pusher->score, pl->score)) / total_pusher_count;
				SCORE(pusher, sc, &pl->pos, pl->name);
			}

			sc = (int32_t) floor(Rate(average_pusher_score, pl->score));
			SCORE(pl, -sc, &pl->pos, "[Shove]");
			strcpy(msg_ptr, ".");


		}

		Message_game_print(msg);
	}
}

void Move_player(player_t *pl)
{
	int32_t nothing_done = 0;
	int32_t i;
	int32_t dist;
	move_info_t mi;
	move_state_t ms[ANGLE_RESOLUTION];
	int32_t worst = 0;
	int32_t crash;
	int32_t bounce;
	int32_t moves_made;
	clpos_t pos;
	clvec_t todo;
	clvec_t done;
	vector_t vel;
	vector_t r[ANGLE_RESOLUTION];
	ivec_t sign; /* sign (-1 or 1) of direction */
	ipos_t block; /* block index */
	bool pos_update = false;
	shipshape_t *ship = pl->ship;

	if (!pl->oldturn) {
		ship = Circle_ship();
	}

	/* Travelling to home base */
	if (!Player_is_alive(pl)) {
		if (Frame_is_real()) {
			pos.cx = pl->pos.cx + FLOAT_TO_CLICK(pl->vel.x);
			pos.cy = pl->pos.cy + FLOAT_TO_CLICK(pl->vel.y);
			Position_set(&pl->pos, &pos);
			pl->velocity = VECTOR_LENGTH(pl->vel);
		}
		else {
	                pos.cx = pl->pos_interp.cx + FLOAT_TO_CLICK(pl->vel_interp.x * ticksPerFrame);
	                pos.cy = pl->pos_interp.cy + FLOAT_TO_CLICK(pl->vel_interp.y * ticksPerFrame);
	                Position_set(&pl->pos_interp, &pos);
	                pl->velocity_interp = hypot((double) (pl->vel_interp.x * ticksPerFrame),
	                		(double) (pl->vel_interp.y * ticksPerFrame));
		}

		return;
	}

	if (Frame_is_real()) {
		Player_position_remember(pl);
	}


	if (Frame_is_real()) {
		dist = walldist[pl->pos.bx][pl->pos.by];
	}
	else {
		dist = walldist[pl->pos_interp.bx][pl->pos_interp.by];
	}

	if (dist > 3) {
		int32_t max = ((dist - 3) * BLOCK_SZ) >> 1;

		if (Frame_is_real()) {
			if (max >= pl->velocity) {
				pos.cx = pl->pos.cx + FLOAT_TO_CLICK(pl->vel.x);
				pos.cy = pl->pos.cy + FLOAT_TO_CLICK(pl->vel.y);
				Position_set(&pl->pos, &pos);
				pl->velocity = VECTOR_LENGTH(pl->vel);

				return;
			}
		}
		else {
	                if (max >= pl->velocity_interp) {
	                        pos.cx = pl->pos_interp.cx + FLOAT_TO_CLICK(pl->vel_interp.x * ticksPerFrame);
	                        pos.cy = pl->pos_interp.cy + FLOAT_TO_CLICK(pl->vel_interp.y * ticksPerFrame);
	                        Position_set(&pl->pos_interp, &pos);
	                        pl->velocity_interp = hypot((double) (pl->vel_interp.x * ticksPerFrame),
	                                        (double) (pl->vel_interp.y * ticksPerFrame));

	                        return;
	                }
	        }
	}

	mi.pl = pl;
	mi.obj = NULL;
	mi.wall_bounce = true;
	mi.treasure_crashes = true;

	if (Frame_is_real()) {
		vel = pl->vel;
		todo.cx = FLOAT_TO_CLICK(vel.x);
		todo.cy = FLOAT_TO_CLICK(vel.y);
	}
	else {
		vel = pl->vel_interp;
	        todo.cx = FLOAT_TO_CLICK(vel.x * ticksPerFrame);
	        todo.cy = FLOAT_TO_CLICK(vel.y * ticksPerFrame);
	}

	for (i = 0; i < ship->num_points; i++) {
		DFLOAT x = ship->pts[i][pl->dir].x;
		DFLOAT y = ship->pts[i][pl->dir].y;

		if (Frame_is_real()) {
			ms[i].pos.cx = pl->pos.cx + FLOAT_TO_CLICK(x);
			ms[i].pos.cy = pl->pos.cy + FLOAT_TO_CLICK(y);
		}
		else {
	                ms[i].pos.cx = pl->pos_interp.cx + FLOAT_TO_CLICK(x);
	                ms[i].pos.cy = pl->pos_interp.cy + FLOAT_TO_CLICK(y);
		}

		ms[i].vel = vel;
		ms[i].todo = todo;
		ms[i].dir = pl->dir;
		ms[i].mip = &mi;
	}

	moves_made = 0;
	for (;; moves_made++) {
		pos.cx = ms[0].pos.cx - FLOAT_TO_CLICK(ship->pts[0][ms[0].dir].x);
		pos.cy = ms[0].pos.cy - FLOAT_TO_CLICK(ship->pts[0][ms[0].dir].y);
		pos.cx = WRAP_XCLICK(pos.cx);
		pos.cy = WRAP_YCLICK(pos.cy);
		block.x = CLICK_TO_BLOCK(pos.cx);
		block.y = CLICK_TO_BLOCK(pos.cy);

		if (walldist[block.x][block.y] > 3) {
			int32_t maxcl = BLOCK_TO_CLICK(walldist[block.x][block.y] - 3) / 2;
			todo = ms[0].todo;
			sign.x = (todo.cx < 0) ? -1 : 1;
			sign.y = (todo.cy < 0) ? -1 : 1;
			if (maxcl >= sign.x * todo.cx && maxcl >= sign.y * todo.cy) {
				/* entire movement is possible. */
				done.cx = todo.cx;
				done.cy = todo.cy;
			}
			else if (sign.x * todo.cx > sign.y * todo.cy) {
				/* horizontal movement. */
				done.cx = sign.x * maxcl;
				done.cy = todo.cy * maxcl / (sign.x * todo.cx);
			}
			else {
				/* vertical movement. */
				done.cx = todo.cx * maxcl / (sign.y * todo.cy);
				done.cy = sign.y * maxcl;
			}
			todo.cx -= done.cx;
			todo.cy -= done.cy;
			for (i = 0; i < ship->num_points; i++) {
				ms[i].pos.cx += done.cx;
				ms[i].pos.cy += done.cy;
				ms[i].pos.cx = WRAP_XCLICK(ms[i].pos.cx);
				ms[i].pos.cy = WRAP_YCLICK(ms[i].pos.cy);

				ms[i].todo = todo;
				ms[i].crash = NotACrash;
				ms[i].bounce = NotABounce;
			}

			nothing_done = 0;
			if (!(todo.cx || todo.cy)) {
				break;
			}
			else {
				continue;
			}
		}

		bounce = -1;
		crash = -1;
		for (i = 0; i < ship->num_points; i++) {
			Move_segment(&ms[i]);
			pos_update |= (ms[i].crash | ms[i].bounce);
			if (ms[i].crash) {
				crash = i;
				break;
			}
			if (ms[i].bounce) {
				if (bounce == -1) {
					bounce = i;
				}
				else if (ms[bounce].bounce != BounceEdge && ms[i].bounce == BounceEdge) {
					bounce = i;
				}
				else if ((ms[bounce].bounce == BounceEdge) == (ms[i].bounce == BounceEdge)) {
					if ((int32_t) (rfrac() * (ship->num_points - bounce)) == i) {
						bounce = i;
					}
				}
				worst = bounce;
			}
		}
		if (crash != -1) {
			worst = crash;
			break;
		}
		else if (bounce != -1) {
			worst = bounce;
			if (ms[worst].bounce != BounceEdge) {
				DFLOAT speed = VECTOR_LENGTH(ms[worst].vel);
				int32_t delta_dir, abs_delta_dir, wall_dir;
				DFLOAT max_speed = Player_uses_property(pl, USES_SHIELD)
							? maxShieldedWallBounceSpeed
							: maxUnshieldedWallBounceSpeed;

				if (Player_uses_property(pl, USES_SHIELD)) {
					if (max_speed < 100) {
						max_speed = 100;
					}
				}

				ms[worst].vel.x *= playerWallBrakeFactor;
				ms[worst].vel.y *= playerWallBrakeFactor;
				ms[worst].todo.cx = (int32_t) (ms[worst].todo.cx
						* playerWallBrakeFactor);
				ms[worst].todo.cy = (int32_t) (ms[worst].todo.cy
						* playerWallBrakeFactor);

				if (speed > max_speed) {
					crash = worst;
					ms[worst].crash = CrashWallSpeed;
					break;
				}

				switch (ms[worst].bounce) {
				case BounceHorLo:
					wall_dir = 4 * ANGLE_RESOLUTION / 8;
					break;
				case BounceHorHi:
					wall_dir = 0 * ANGLE_RESOLUTION / 8;
					break;
				case BounceVerLo:
					wall_dir = 6 * ANGLE_RESOLUTION / 8;
					break;
				default:
				case BounceVerHi:
					wall_dir = 2 * ANGLE_RESOLUTION / 8;
					break;
				case BounceLeftDown:
					wall_dir = 1 * ANGLE_RESOLUTION / 8;
					break;
				case BounceLeftUp:
					wall_dir = 7 * ANGLE_RESOLUTION / 8;
					break;
				case BounceRightDown:
					wall_dir = 3 * ANGLE_RESOLUTION / 8;
					break;
				case BounceRightUp:
					wall_dir = 5 * ANGLE_RESOLUTION / 8;
					break;
				}
				if (pl->dir >= wall_dir) {
					delta_dir = (pl->dir - wall_dir <= ANGLE_RESOLUTION
							/ 2) ? -(pl->dir - wall_dir) : (wall_dir
							+ ANGLE_RESOLUTION - pl->dir);
				}
				else {
					delta_dir = (wall_dir - pl->dir <= ANGLE_RESOLUTION
							/ 2) ? (wall_dir - pl->dir) : -(pl->dir
							+ ANGLE_RESOLUTION - wall_dir);
				}
				abs_delta_dir = ABS(delta_dir);
				if (abs_delta_dir <= ANGLE_RESOLUTION / 16) {
					pl->float_dir += (1.0f - playerWallBrakeFactor) * delta_dir;
					if (pl->float_dir >= ANGLE_RESOLUTION) {
						pl->float_dir -= ANGLE_RESOLUTION;
					}
					else if (pl->float_dir < 0) {
						pl->float_dir += ANGLE_RESOLUTION;
					}
				}

				/* crash in wall if no fuel left */
				if (!pl->fuel.sum) {
					crash = worst;
					ms[worst].crash = CrashWallNoFuel;
					break;
				}
			}
		}
		else {
			for (i = 0; i < ship->num_points; i++) {
				r[i].x = (vel.x) ? (DFLOAT) ms[i].todo.cx / vel.x : 0;
				r[i].y = (vel.y) ? (DFLOAT) ms[i].todo.cy / vel.y : 0;
				r[i].x = ABS(r[i].x);
				r[i].y = ABS(r[i].y);
			}
			worst = 0;
			for (i = 1; i < ship->num_points; i++) {
				if (r[i].x > r[worst].x || r[i].y > r[worst].y) {
					worst = i;
				}
			}
		}

		if (!(ms[worst].done.cx | ms[worst].done.cy)) {
			if (++nothing_done >= 5) {
				ms[worst].crash = CrashUnknown;
				break;
			}
		}
		else {
			nothing_done = 0;
			ms[worst].pos.cx += ms[worst].done.cx;
			ms[worst].pos.cy += ms[worst].done.cy;
		}
		if (!(ms[worst].todo.cx | ms[worst].todo.cy)) {
			break;
		}

		vel = ms[worst].vel;
		for (i = 0; i < ship->num_points; i++) {
			if (i != worst) {
				ms[i].pos.cx += ms[worst].done.cx;
				ms[i].pos.cy += ms[worst].done.cy;
				ms[i].vel = vel;
				ms[i].todo = ms[worst].todo;
				ms[i].dir = ms[worst].dir;
			}
		}
	}

	pos.cx = ms[worst].pos.cx - FLOAT_TO_CLICK(ship->pts[worst][pl->dir].x);
	pos.cy = ms[worst].pos.cy - FLOAT_TO_CLICK(ship->pts[worst][pl->dir].y);

	if (Frame_is_real()) {
		Position_set(&pl->pos, &pos);

		pl->vel = ms[worst].vel;
		pl->velocity = VECTOR_LENGTH(pl->vel);
	}
	else {
	        Position_set(&pl->pos_interp, &pos);

	        pl->velocity_interp = hypot((double) (pl->vel_interp.x * ticksPerFrame),
	                        (double) (pl->vel_interp.y * ticksPerFrame));
	}

	if (Frame_is_real()) {
		if (ms[worst].crash) {
			Player_crash(&ms[worst], worst, false);
		}
		if (pos_update) {
//			Player_position_remember(pl);
		}
	}
}

void Turn_player(player_t *pl)
{
	pl->dir = MOD2((int32_t) (pl->float_dir + 0.5f), ANGLE_RESOLUTION);
}

void Turn_player_old(player_t *pl)
{
	int32_t i;
	move_info_t mi;
	move_state_t ms[ANGLE_RESOLUTION];
	int32_t dir;
	int32_t new_dir = MOD2((int32_t) (pl->float_dir + 0.5f), ANGLE_RESOLUTION);
	int32_t sign;
	int32_t crash = -1;
	int32_t nothing_done = 0;
	int32_t turns_done = 0;
	int32_t blocked = 0;
	clpos_t pos;
	vector_t salt;

	if (new_dir == pl->dir) {
		return;
	}
	if (!Player_is_alive(pl)) {
		pl->dir = new_dir;
		return;
	}

	if (walldist[pl->pos.bx][pl->pos.by] > 2) {
		pl->dir = new_dir;
		return;
	}

	mi.pl = pl;
	mi.obj = NULL;
	mi.wall_bounce = true;
	mi.treasure_crashes = true;

	if (new_dir > pl->dir) {
		sign = (new_dir - pl->dir <= ANGLE_RESOLUTION + pl->dir - new_dir) ? 1 : -1;
	}
	else {
		sign = (pl->dir - new_dir <= ANGLE_RESOLUTION + new_dir - pl->dir) ? -1 : 1;
	}

#if 0
	salt.x = (pl->vel.x> 0) ? 0.1f : (pl->vel.x < 0) ? -0.1f : 0;
	salt.y = (pl->vel.y> 0) ? 0.1f : (pl->vel.y < 0) ? -0.1f : 0;
#else
	salt.x = (pl->vel.x > 0) ? 1e-6f : (pl->vel.x < 0) ? -1e-6f : 0;
	salt.y = (pl->vel.y > 0) ? 1e-6f : (pl->vel.y < 0) ? -1e-6f : 0;
#endif

	pos.cx = pl->pos.cx;
	pos.cy = pl->pos.cy;
	for (; pl->dir != new_dir; turns_done++) {
		dir = MOD2(pl->dir + sign, ANGLE_RESOLUTION);

		for (i = 0; i < pl->ship->num_points; i++) {
			ms[i].mip = &mi;
			ms[i].pos.cx = pos.cx + FLOAT_TO_CLICK(pl->ship->pts[i][pl->dir].x);
			ms[i].pos.cy = pos.cy + FLOAT_TO_CLICK(pl->ship->pts[i][pl->dir].y);
			ms[i].todo.cx = pos.cx + FLOAT_TO_CLICK(pl->ship->pts[i][dir].x) - ms[i].pos.cx;
			ms[i].todo.cy = pos.cy + FLOAT_TO_CLICK(pl->ship->pts[i][dir].y) - ms[i].pos.cy;
			ms[i].vel.x = ms[i].todo.cx + salt.x;
			ms[i].vel.y = ms[i].todo.cy + salt.y;

			do {
				Move_segment(&ms[i]);
				if (ms[i].crash | ms[i].bounce) {
					if (ms[i].crash) {
						if (ms[i].crash != CrashUniverse) {
							crash = i;
						}
						blocked = 1;
						break;
					}
					if (ms[i].bounce != BounceEdge) {
						blocked = 1;
						break;
					}
					if (++nothing_done >= 5) {
						ms[i].crash = CrashUnknown;
						crash = i;
						blocked = 1;
						break;
					}
				}
				else if (ms[i].done.cx | ms[i].done.cy) {
					ms[i].pos.cx += ms[i].done.cx;
					ms[i].pos.cy += ms[i].done.cy;
					nothing_done = 0;
				}
			} while (ms[i].todo.cx | ms[i].todo.cy);
			if (blocked) {
				break;
			}
		}
		if (blocked) {
			break;
		}
		pl->dir = dir;
	}

	if (blocked) {
		pl->float_dir = (DFLOAT) pl->dir;
	}

	if (crash != -1) {
		Player_crash(&ms[crash], crash, true);
	}
}
