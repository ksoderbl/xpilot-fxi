/* $Id: walls.c,v 1.3 2007/02/03 13:37:55 pgma Exp $
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-98 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
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
#include <assert.h>

#define SERVER
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
#include "click.h"
#include "objpos.h"

char walls_version[] = VERSION;
extern int frame_cycle;

#define WALLDIST_MASK	\
	(FILLED_BIT | REC_LU_BIT | REC_LD_BIT | REC_RU_BIT | REC_RD_BIT \
	| FUEL_BIT | TREASURE_BIT )

unsigned SPACE_BLOCKS = (SPACE_BIT | BASE_BIT);

static struct move_parameters mp;
static DFLOAT wallBounceExplosionMult;
static char msg[MSG_LEN];

/*
 * Two dimensional array giving for each point the distance
 * to the nearest wall.  Measured in blocks times 2.
 */
static unsigned char **walldist;

/*
 * Allocate memory for the two dimensional "walldist" array.
 */
static void Walldist_alloc(void)
{
    int			x;
    unsigned char	*wall_line;
    unsigned char	**wall_ptr;

    walldist = (unsigned char **)malloc(
		World.x * sizeof(unsigned char *) + World.x * World.y);
    if (!walldist) {
	error("No memory for walldist");
	exit(1);
    }
    wall_ptr = walldist;
    wall_line = (unsigned char *)(wall_ptr + World.x);
    for (x = 0; x < World.x; x++) {
	*wall_ptr = wall_line;
	wall_ptr += 1;
	wall_line += World.y;
    }
}

static void Walldist_init(void)
{
    int			x, y, dx, dy, wx, wy;
    int			dist;
    int			mindist;
    int			maxdist = 2 * MIN(World.x, World.y);
    int			newdist;

    typedef struct Qelmt { short x, y; } Qelmt_t;
    Qelmt_t		*q;
    int			qfront = 0, qback = 0;

    if (maxdist > 255) {
	maxdist = 255;
    }
    q = (Qelmt_t *)malloc(World.x * World.y * sizeof(Qelmt_t));
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
	    } else {
		walldist[x][y] = maxdist;
	    }
	}
    }
    if (!BIT(World.rules->mode, WRAP_PLAY)) {
	for (x = 0; x < World.x; x++) {
	    for (y = 0; y < World.y; y += (!x || x == World.x - 1)
					? 1 : (World.y - (World.y > 1))) {
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
	    if (BIT(World.rules->mode, WRAP_PLAY)
		|| (x + dx >= 0 && x + dx < World.x)) {
		wx = WRAP_XBLOCK(x + dx);
		for (dy = -1; dy <= 1; dy++) {
		    if (BIT(World.rules->mode, WRAP_PLAY)
			|| (y + dy >= 0 && y + dy < World.y)) {
			wy = WRAP_YBLOCK(y + dy);
			if (walldist[wx][wy] > mindist) {
			    newdist = mindist;
			    if (dist == 0) {
				if (World.block[x][y] == REC_LD) {
				    if (dx == +1 && dy == +1) {
					newdist = mindist + 1;
				    }
				}
				else if (World.block[x][y] == REC_RD) {
				    if (dx == -1 && dy == +1) {
					newdist = mindist + 1;
				    }
				}
				else if (World.block[x][y] == REC_LU) {
				    if (dx == +1 && dy == -1) {
					newdist = mindist + 1;
				    }
				}
				else if (World.block[x][y] == REC_RU) {
				    if (dx == -1 && dy == -1) {
					newdist = mindist + 1;
				    }
				}
			    }
			    if (newdist < walldist[wx][wy]) {
				walldist[wx][wy] = newdist;
				q[qback].x = wx;
				q[qback].y = wy;
				if (++qback == World.x * World.y) {
				    qback = 0;
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
    mp.click_width = PIXEL_TO_CLICK(World.width);
    mp.click_height = PIXEL_TO_CLICK(World.height);

    LIMIT(maxObjectWallBounceSpeed, 0, World.hypotenuse);
    LIMIT(maxShieldedWallBounceSpeed, 0, World.hypotenuse);
    LIMIT(maxUnshieldedWallBounceSpeed, 0, World.hypotenuse);
    LIMIT(maxShieldedWallBounceAngle, 0, 180);
    LIMIT(maxUnshieldedWallBounceAngle, 0, 180);
    LIMIT(playerWallBrakeFactor, 0, 1);
    LIMIT(objectWallBrakeFactor, 0, 1);
    LIMIT(objectWallBounceLifeFactor, 0, 1);

    mp.max_shielded_angle = (int)(maxShieldedWallBounceAngle * RES / 360);
    mp.max_unshielded_angle = (int)(maxUnshieldedWallBounceAngle * RES / 360);

    mp.obj_bounce_mask = 0;
    if (sparksWallBounce) {
	SET_BIT(mp.obj_bounce_mask, OBJ_SPARK);
    }
    if (debrisWallBounce) {
	SET_BIT(mp.obj_bounce_mask, OBJ_DEBRIS);
    }
    if (shotsWallBounce) {
	SET_BIT(mp.obj_bounce_mask, OBJ_SHOT);
    }
    if (ballsWallBounce) {
	SET_BIT(mp.obj_bounce_mask, OBJ_BALL);
    }

    mp.obj_treasure_mask = mp.obj_bounce_mask | OBJ_BALL;
}

static void Bounce_edge(move_state_t *ms, move_bounce_t bounce)
{
    if (bounce == BounceHorLo) {
	if (ms->mip->edge_bounce) {
	    ms->todo.x = -ms->todo.x;
	    ms->vel.x = -ms->vel.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES / 2 - ms->dir, RES);
	    }
	}
	else {
	    ms->todo.x = 0;
	    ms->vel.x = 0;
	    if (!ms->mip->pl) {
		ms->dir = (ms->vel.y < 0) ? (3*RES/4) : RES/4;
	    }
	}
    }
    else if (bounce == BounceHorHi) {
	if (ms->mip->edge_bounce) {
	    ms->todo.x = -ms->todo.x;
	    ms->vel.x = -ms->vel.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES / 2 - ms->dir, RES);
	    }
	}
	else {
	    ms->todo.x = 0;
	    ms->vel.x = 0;
	    if (!ms->mip->pl) {
		ms->dir = (ms->vel.y < 0) ? (3*RES/4) : RES/4;
	    }
	}
    }
    else if (bounce == BounceVerLo) {
	if (ms->mip->edge_bounce) {
	    ms->todo.y = -ms->todo.y;
	    ms->vel.y = -ms->vel.y;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES - ms->dir, RES);
	    }
	}
	else {
	    ms->todo.y = 0;
	    ms->vel.y = 0;
	    if (!ms->mip->pl) {
		ms->dir = (ms->vel.x < 0) ? (RES/2) : 0;
	    }
	}
    }
    else if (bounce == BounceVerHi) {
	if (ms->mip->edge_bounce) {
	    ms->todo.y = -ms->todo.y;
	    ms->vel.y = -ms->vel.y;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES - ms->dir, RES);
	    }
	}
	else {
	    ms->todo.y = 0;
	    ms->vel.y = 0;
	    if (!ms->mip->pl) {
		ms->dir = (ms->vel.x < 0) ? (RES/2) : 0;
	    }
	}
    }
    ms->bounce = BounceEdge;
}

static void Bounce_wall(move_state_t *ms, move_bounce_t bounce)
{
    if (!ms->mip->wall_bounce) {
	ms->crash = CrashWall;
	return;
    }
    if (bounce == BounceHorLo) {
	ms->todo.x = -ms->todo.x;
	ms->vel.x = -ms->vel.x;
	if (!ms->mip->pl) {
	    ms->dir = MOD2(RES/2 - ms->dir, RES);
	}
    }
    else if (bounce == BounceHorHi) {
	ms->todo.x = -ms->todo.x;
	ms->vel.x = -ms->vel.x;
	if (!ms->mip->pl) {
	    ms->dir = MOD2(RES/2 - ms->dir, RES);
	}
    }
    else if (bounce == BounceVerLo) {
	ms->todo.y = -ms->todo.y;
	ms->vel.y = -ms->vel.y;
	if (!ms->mip->pl) {
	    ms->dir = MOD2(RES - ms->dir, RES);
	}
    }
    else if (bounce == BounceVerHi) {
	ms->todo.y = -ms->todo.y;
	ms->vel.y = -ms->vel.y;
	if (!ms->mip->pl) {
	    ms->dir = MOD2(RES - ms->dir, RES);
	}
    }
    else {
	clvec t = ms->todo;
	vector v = ms->vel;
	if (bounce == BounceLeftDown) {
	    ms->todo.x = -t.y;
	    ms->todo.y = -t.x;
	    ms->vel.x = -v.y;
	    ms->vel.y = -v.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(3*RES/4 - ms->dir, RES);
	    }
	}
	else if (bounce == BounceLeftUp) {
	    ms->todo.x = t.y;
	    ms->todo.y = t.x;
	    ms->vel.x = v.y;
	    ms->vel.y = v.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES/4 - ms->dir, RES);
	    }
	}
	else if (bounce == BounceRightDown) {
	    ms->todo.x = t.y;
	    ms->todo.y = t.x;
	    ms->vel.x = v.y;
	    ms->vel.y = v.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES/4 - ms->dir, RES);
	    }
	}
	else if (bounce == BounceRightUp) {
	    ms->todo.x = -t.y;
	    ms->todo.y = -t.x;
	    ms->vel.x = -v.y;
	    ms->vel.y = -v.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(3*RES/4 - ms->dir, RES);
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
    int			i;
    int			block_type;	/* type of block we're going through */
    int			inside;		/* inside the block or else on edge */
    int			need_adjust;	/* other param (x or y) needs recalc */
    unsigned		wall_bounce;	/* are we bouncing? what direction? */
    ipos		block;		/* block index */
    ipos		blk2;		/* new block index */
    ivec		sign;		/* sign (-1 or 1) of direction */
    clpos		delta;		/* delta position in clicks */
    clpos		enter;		/* enter block position in clicks */
    clpos		leave;		/* leave block position in clicks */
    clpos		offset;		/* offset within block in clicks */
    clpos		off2;		/* last offset in block in clicks */
    clpos		mid;		/* the mean of (offset+off2)/2 */
    const move_info_t	*const mi = ms->mip;	/* alias */

    /*
     * Fill in default return values.
     */
    ms->crash = NotACrash;
    ms->bounce = NotABounce;
    ms->done.x = 0;
    ms->done.y = 0;

    enter = ms->pos;
    if (enter.x < 0 || enter.x >= mp.click_width
	|| enter.y < 0 || enter.y >= mp.click_height) {

	if (!mi->edge_wrap) {
	    ms->crash = CrashUniverse;
	    return;
	}
	if (enter.x < 0) {
	    enter.x += mp.click_width;
	    if (enter.x < 0) {
		ms->crash = CrashUniverse;
		return;
	    }
	}
	else if (enter.x >= mp.click_width) {
	    enter.x -= mp.click_width;
	    if (enter.x >= mp.click_width) {
		ms->crash = CrashUniverse;
		return;
	    }
	}
	if (enter.y < 0) {
	    enter.y += mp.click_height;
	    if (enter.y < 0) {
		ms->crash = CrashUniverse;
		return;
	    }
	}
	else if (enter.y >= mp.click_height) {
	    enter.y -= mp.click_height;
	    if (enter.y >= mp.click_height) {
		ms->crash = CrashUniverse;
		return;
	    }
	}
	ms->pos = enter;
    }

    sign.x = (ms->vel.x < 0) ? -1 : 1;
    sign.y = (ms->vel.y < 0) ? -1 : 1;
    block.x = enter.x / BLOCK_CLICKS;
    block.y = enter.y / BLOCK_CLICKS;
    if (walldist[block.x][block.y] > 2) {
	int maxcl = ((walldist[block.x][block.y] - 2) * BLOCK_CLICKS) >> 1;
	if (maxcl >= sign.x * ms->todo.x && maxcl >= sign.y * ms->todo.y) {
	    /* entire movement is possible. */
	    ms->done.x = ms->todo.x;
	    ms->done.y = ms->todo.y;
	}
	else if (sign.x * ms->todo.x > sign.y * ms->todo.y) {
	    /* horizontal movement. */
	    ms->done.x = sign.x * maxcl;
	    ms->done.y = ms->todo.y * maxcl / (sign.x * ms->todo.x);
	}
	else {
	    /* vertical movement. */
	    ms->done.x = ms->todo.x * maxcl / (sign.y * ms->todo.y);
	    ms->done.y = sign.y * maxcl;
	}
	ms->todo.x -= ms->done.x;
	ms->todo.y -= ms->done.y;
	return;
    }

    offset.x = enter.x - block.x * BLOCK_CLICKS;
    offset.y = enter.y - block.y * BLOCK_CLICKS;
    inside = 1;
    if (offset.x == 0) {
	inside = 0;
	if (sign.x == -1 && (offset.x = BLOCK_CLICKS, --block.x < 0)) {
	    if (mi->edge_wrap) {
		block.x += World.x;
	    }
	    else {
		Bounce_edge(ms, BounceHorLo);
		return;
	    }
	}
    }
    else if (enter.x == mp.click_width - 1
	     && !mi->edge_wrap
	     && ms->vel.x > 0) {
	Bounce_edge(ms, BounceHorHi);
	return;
    }
    if (offset.y == 0) {
	inside = 0;
	if (sign.y == -1 && (offset.y = BLOCK_CLICKS, --block.y < 0)) {
	    if (mi->edge_wrap) {
		block.y += World.y;
	    }
	    else {
		Bounce_edge(ms, BounceVerLo);
		return;
	    }
	}
    }
    else if (enter.y == mp.click_height - 1
	     && !mi->edge_wrap
	     && ms->vel.y > 0) {
	Bounce_edge(ms, BounceVerHi);
	return;
    }

    need_adjust = 0;
    if (sign.x == -1) {
	if (offset.x + ms->todo.x < 0) {
	    leave.x = enter.x - offset.x;
	    need_adjust = 1;
	}
	else {
	    leave.x = enter.x + ms->todo.x;
	}
    }
    else {
	if (offset.x + ms->todo.x > BLOCK_CLICKS) {
	    leave.x = enter.x + BLOCK_CLICKS - offset.x;
	    need_adjust = 1;
	}
	else {
	    leave.x = enter.x + ms->todo.x;
	}
	if (leave.x == mp.click_width && !mi->edge_wrap) {
	    leave.x--;
	    need_adjust = 1;
	}
    }
    if (sign.y == -1) {
	if (offset.y + ms->todo.y < 0) {
	    leave.y = enter.y - offset.y;
	    need_adjust = 1;
	}
	else {
	    leave.y = enter.y + ms->todo.y;
	}
    }
    else {
	if (offset.y + ms->todo.y > BLOCK_CLICKS) {
	    leave.y = enter.y + BLOCK_CLICKS - offset.y;
	    need_adjust = 1;
	}
	else {
	    leave.y = enter.y + ms->todo.y;
	}
	if (leave.y == mp.click_height && !mi->edge_wrap) {
	    leave.y--;
	    need_adjust = 1;
	}
    }
    if (need_adjust && ms->todo.y && ms->todo.x) {
	double wx = (double)(leave.x - enter.x) / ms->todo.x;
	double wy = (double)(leave.y - enter.y) / ms->todo.y;
	if (wx > wy) {
	    double x = ms->todo.x * wy;
	    leave.x = enter.x + DOUBLE_TO_INT(x);
	}
	else if (wx < wy) {
	    double y = ms->todo.y * wx;
	    leave.y = enter.y + DOUBLE_TO_INT(y);
	}
    }

    delta.x = leave.x - enter.x;
    delta.y = leave.y - enter.y;

    block_type = World.block[block.x][block.y];

    /*
     * We test for several different bouncing directions against the wall.
     * Sometimes there is more than one bounce possible if the point
     * starts at the corner of a block.
     * Therefore we maintain a bit mask for the bouncing possibilities
     * and later we will determine which bounce is appropriate.
     */
    wall_bounce = 0;

    switch (block_type) {

    default:
	break;

    case TREASURE:

      if (block_type == TREASURE) {
	if (mi->treasure_crashes) {
		/*
		 * Test if the movement is within the upper half of
		 * the treasure, which is the upper half of a circle.
		 * If this is the case then we test if 3 samples
		 * are not hitting the treasure.
		 */
		const DFLOAT r = 0.5f * BLOCK_CLICKS;
		off2.x = offset.x + delta.x;
		off2.y = offset.y + delta.y;
		mid.x = (offset.x + off2.x) / 2;
		mid.y = (offset.y + off2.y) / 2;
		if (offset.y > r
		    && off2.y > r
		    && sqr(mid.x - r) + sqr(mid.y - r) > sqr(r)
		    && sqr(off2.x - r) + sqr(off2.y - r) > sqr(r)
		    && sqr(offset.x - r) + sqr(offset.y - r) > sqr(r)) {
		    break;
		}

		for (i = 0; ; i++) {
		    if (World.treasures[i].pos.x == block.x
			&& World.treasures[i].pos.y == block.y) {
			break;
		    }
		}
		ms->treasure = i;
		ms->crash = CrashTreasure;

		/*
		 * We handle balls here, because the reaction
		 * depends on which team the treasure and the ball
		 * belong to.
		 */
		if (mi->obj->type != OBJ_BALL) {
		    return;
		}
		
		/* replace */
		if (ms->treasure == mi->obj->treasure) {
		    /*
		     * Ball has been replaced back in the hoop from whence
		     * it came.  If the player is on the same team as the
		     * hoop, then it should be replaced into the hoop without
		     * exploding and gets the player some points.  Otherwise
		     * nothing interesting happens.
		     */
		    player	*pl = NULL;
		    treasure_t	*tt = &World.treasures[ms->treasure];

		    if (mi->obj->owner != -1)
			pl = Players[GetInd[mi->obj->owner]];

		    if (!pl || (pl->team !=
				World.treasures[mi->obj->treasure].team)) {
			mi->obj->life = LONG_MAX;
			ms->crash = NotACrash;
			break;
		    }
		    
		    /* replace - check also that obj->life isn't 0 for ball -pgm */
		    if (mi->obj->life !=0){
		      
		      mi->obj->life = 0;
		      SET_BIT(mi->obj->status, (NOEXPLOSION|RECREATE));
		      
		      SCORE(GetInd[pl->id], 5,
			  tt->pos.x, tt->pos.y, "Treasure: ");
		      sprintf(msg, " < %s (team %d) has replaced the treasure >",
			    pl->name, pl->team);
		      Set_message(msg);
		      Rank_saved_ball(pl);
		      break;
		    }
		}
		if (mi->obj->owner == -1) {
		    mi->obj->life = 0;
		    return;
		}
		
		/* cash - check also that obj->life isn't 0 for ball -pgm */
		if ((World.treasures[ms->treasure].team ==
		    Players[GetInd[mi->obj->owner]]->team) &&
		    (mi->obj->life != 0)){
		  player *pl = NULL, *pl2 = NULL;
		  int n, enemies = 0;
		  pl = Players[GetInd[mi->obj->owner]];

		  /* compute amount of active enemies */
		  for (n = 0; n < NumPlayers; n++){
		    pl2 = Players[n];
		    if ((pl2->team != pl->team) && (!BIT(pl2->status, PAUSE))
			&& (!(pl2->mychar == 'W'))) enemies++;
		  }
		  

		  /*
		   * Ball has been brought back to home treasure.
		   * The team should be punished.
		   */
		  sprintf(msg," < The ball was loose for %ld frames >",
			  LONG_MAX - mi->obj->life);
		  Set_message(msg);
		  if (enemies > 0){
		    Rank_cashed_ball(pl);
		    for (n = 0; n < NumPlayers; n++){
		      pl2 = Players[n];
		      if ((!BIT(pl2->status, PAUSE)) && (!(pl2->mychar == 'W')))
			Rank_won_ball(pl2);
		    }
		  }
		  Rank_ballrun(pl,  LONG_MAX - mi->obj->life);
		  if (Punish_team(GetInd[mi->obj->owner],
				  mi->obj->treasure, ms->treasure))
		    SET_BIT(mi->obj->status, RECREATE);
		}
		mi->obj->life = 0;
		return;
	    }
      }
	/*FALLTHROUGH*/
	/*}*/
    case FUEL:
    case FILLED:
	if (inside) {
	    /* Could happen for targets reappearing and in case of bugs. */
	    ms->crash = CrashWall;
	    return;
	}
	if (offset.x == 0) {
	    if (ms->vel.x > 0) {
		wall_bounce |= BounceHorLo;
	    }
	}
	else if (offset.x == BLOCK_CLICKS) {
	    if (ms->vel.x < 0) {
		wall_bounce |= BounceHorHi;
	    }
	}
	if (offset.y == 0) {
	    if (ms->vel.y > 0) {
		wall_bounce |= BounceVerLo;
	    }
	}
	else if (offset.y == BLOCK_CLICKS) {
	    if (ms->vel.y < 0) {
		wall_bounce |= BounceVerHi;
	    }
	}
	if (wall_bounce) {
	    break;
	}
	if (!(ms->todo.x | ms->todo.y)) {
	    /* no bouncing possible and no movement.  OK. */
	    break;
	}
	if (!ms->todo.x && (offset.x == 0 || offset.x == BLOCK_CLICKS)) {
	    /* tricky */
	    break;
	}
	if (!ms->todo.y && (offset.y == 0 || offset.y == BLOCK_CLICKS)) {
	    /* tricky */
	    break;
	}
	/* what happened? we should never reach this */
	ms->crash = CrashWall;
	return;

    case REC_LD:
	/* test for bounces first. */
	if (offset.x == 0) {
	    if (ms->vel.x > 0) {
		wall_bounce |= BounceHorLo;
	    }
	    if (offset.y == BLOCK_CLICKS && ms->vel.x + ms->vel.y < 0) {
		wall_bounce |= BounceLeftDown;
	    }
	}
	if (offset.y == 0) {
	    if (ms->vel.y > 0) {
		wall_bounce |= BounceVerLo;
	    }
	    if (offset.x == BLOCK_CLICKS && ms->vel.x + ms->vel.y < 0) {
		wall_bounce |= BounceLeftDown;
	    }
	}
	if (wall_bounce) {
	    break;
	}
	if (offset.x + offset.y < BLOCK_CLICKS) {
	    ms->crash = CrashWall;
	    return;
	}
	if (offset.x + delta.x + offset.y + delta.y >= BLOCK_CLICKS) {
	    /* movement is entirely within the space part of the block. */
	    break;
	}
	/*
	 * Find out where we bounce exactly
	 * and how far we can move before bouncing.
	 */
	if (sign.x * ms->todo.x >= sign.y * ms->todo.y) {
	    double w = (double) ms->todo.y / ms->todo.x;
	    delta.x = (int)((BLOCK_CLICKS - offset.x - offset.y) / (1 + w));
	    delta.y = (int)(delta.x * w);
	    if (offset.x + delta.x + offset.y + delta.y < BLOCK_CLICKS) {
		delta.x++;
		delta.y = (int)(delta.x * w);
	    }
	    leave.x = enter.x + delta.x;
	    leave.y = enter.y + delta.y;
	    if (!delta.x) {
		wall_bounce |= BounceLeftDown;
		break;
	    }
	}
	else {
	    double w = (double) ms->todo.x / ms->todo.y;
	    delta.y = (int)((BLOCK_CLICKS - offset.x - offset.y) / (1 + w));
	    delta.x = (int)(delta.y * w);
	    if (offset.x + delta.x + offset.y + delta.y < BLOCK_CLICKS) {
		delta.y++;
		delta.x = (int)(delta.y * w);
	    }
	    leave.x = enter.x + delta.x;
	    leave.y = enter.y + delta.y;
	    if (!delta.y) {
		wall_bounce |= BounceLeftDown;
		break;
	    }
	}
	break;

    case REC_LU:
	if (offset.x == 0) {
	    if (ms->vel.x > 0) {
		wall_bounce |= BounceHorLo;
	    }
	    if (offset.y == 0 && ms->vel.x < ms->vel.y) {
		wall_bounce |= BounceLeftUp;
	    }
	}
	if (offset.y == BLOCK_CLICKS) {
	    if (ms->vel.y < 0) {
		wall_bounce |= BounceVerHi;
	    }
	    if (offset.x == BLOCK_CLICKS && ms->vel.x < ms->vel.y) {
		wall_bounce |= BounceLeftUp;
	    }
	}
	if (wall_bounce) {
	    break;
	}
	if (offset.x < offset.y) {
	    ms->crash = CrashWall;
	    return;
	}
	if (offset.x + delta.x >= offset.y + delta.y) {
	    break;
	}
	if (sign.x * ms->todo.x >= sign.y * ms->todo.y) {
	    double w = (double) ms->todo.y / ms->todo.x;
	    delta.x = (int)((offset.y - offset.x) / (1 - w));
	    delta.y = (int)(delta.x * w);
	    if (offset.x + delta.x < offset.y + delta.y) {
		delta.x++;
		delta.y = (int)(delta.x * w);
	    }
	    leave.x = enter.x + delta.x;
	    leave.y = enter.y + delta.y;
	    if (!delta.x) {
		wall_bounce |= BounceLeftUp;
		break;
	    }
	}
	else {
	    double w = (double) ms->todo.x / ms->todo.y;
	    delta.y = (int)((offset.x - offset.y) / (1 - w));
	    delta.x = (int)(delta.y * w);
	    if (offset.x + delta.x < offset.y + delta.y) {
		delta.y--;
		delta.x = (int)(delta.y * w);
	    }
	    leave.x = enter.x + delta.x;
	    leave.y = enter.y + delta.y;
	    if (!delta.y) {
		wall_bounce |= BounceLeftUp;
		break;
	    }
	}
	break;

    case REC_RD:
	if (offset.x == BLOCK_CLICKS) {
	    if (ms->vel.x < 0) {
		wall_bounce |= BounceHorHi;
	    }
	    if (offset.y == BLOCK_CLICKS && ms->vel.x > ms->vel.y) {
		wall_bounce |= BounceRightDown;
	    }
	}
	if (offset.y == 0) {
	    if (ms->vel.y > 0) {
		wall_bounce |= BounceVerLo;
	    }
	    if (offset.x == 0 && ms->vel.x > ms->vel.y) {
		wall_bounce |= BounceRightDown;
	    }
	}
	if (wall_bounce) {
	    break;
	}
	if (offset.x > offset.y) {
	    ms->crash = CrashWall;
	    return;
	}
	if (offset.x + delta.x <= offset.y + delta.y) {
	    break;
	}
	if (sign.x * ms->todo.x >= sign.y * ms->todo.y) {
	    double w = (double) ms->todo.y / ms->todo.x;
	    delta.x = (int)((offset.y - offset.x) / (1 - w));
	    delta.y = (int)(delta.x * w);
	    if (offset.x + delta.x > offset.y + delta.y) {
		delta.x--;
		delta.y = (int)(delta.x * w);
	    }
	    leave.x = enter.x + delta.x;
	    leave.y = enter.y + delta.y;
	    if (!delta.x) {
		wall_bounce |= BounceRightDown;
		break;
	    }
	}
	else {
	    double w = (double) ms->todo.x / ms->todo.y;
	    delta.y = (int)((offset.x - offset.y) / (1 - w));
	    delta.x = (int)(delta.y * w);
	    if (offset.x + delta.x > offset.y + delta.y) {
		delta.y++;
		delta.x = (int)(delta.y * w);
	    }
	    leave.x = enter.x + delta.x;
	    leave.y = enter.y + delta.y;
	    if (!delta.y) {
		wall_bounce |= BounceRightDown;
		break;
	    }
	}
	break;

    case REC_RU:
	if (offset.x == BLOCK_CLICKS) {
	    if (ms->vel.x < 0) {
		wall_bounce |= BounceHorHi;
	    }
	    if (offset.y == 0 && ms->vel.x + ms->vel.y > 0) {
		wall_bounce |= BounceRightUp;
	    }
	}
	if (offset.y == BLOCK_CLICKS) {
	    if (ms->vel.y < 0) {
		wall_bounce |= BounceVerHi;
	    }
	    if (offset.x == 0 && ms->vel.x + ms->vel.y > 0) {
		wall_bounce |= BounceRightUp;
	    }
	}
	if (wall_bounce) {
	    break;
	}
	if (offset.x + offset.y > BLOCK_CLICKS) {
	    ms->crash = CrashWall;
	    return;
	}
	if (offset.x + delta.x + offset.y + delta.y <= BLOCK_CLICKS) {
	    break;
	}
	if (sign.x * ms->todo.x >= sign.y * ms->todo.y) {
	    double w = (double) ms->todo.y / ms->todo.x;
	    delta.x = (int)((BLOCK_CLICKS - offset.x - offset.y) / (1 + w));
	    delta.y = (int)(delta.x * w);
	    if (offset.x + delta.x + offset.y + delta.y > BLOCK_CLICKS) {
		delta.x--;
		delta.y = (int)(delta.x * w);
	    }
	    leave.x = enter.x + delta.x;
	    leave.y = enter.y + delta.y;
	    if (!delta.x) {
		wall_bounce |= BounceRightUp;
		break;
	    }
	}
	else {
	    double w = (double) ms->todo.x / ms->todo.y;
	    delta.y = (int)((BLOCK_CLICKS - offset.x - offset.y) / (1 + w));
	    delta.x = (int)(delta.y * w);
	    if (offset.x + delta.x + offset.y + delta.y > BLOCK_CLICKS) {
		delta.y--;
		delta.x = (int)(delta.y * w);
	    }
	    leave.x = enter.x + delta.x;
	    leave.y = enter.y + delta.y;
	    if (!delta.y) {
		wall_bounce |= BounceRightUp;
		break;
	    }
	}
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
	int count = 0;
	unsigned bit;
	unsigned save_wall_bounce = wall_bounce;
	unsigned block_mask = FILLED_BIT | FUEL_BIT;

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
		    if (!mi->edge_wrap) {
			continue;
		    }
		    blk2.x += World.x;
		}
		blk2.y = block.y;
		if (BIT(1 << World.block[blk2.x][blk2.y],
			block_mask|REC_RU_BIT|REC_RD_BIT)) {
		    continue;
		}
		break;

	    case BounceHorHi:
		blk2.x = block.x + 1;
		if (blk2.x >= World.x) {
		    if (!mi->edge_wrap) {
			continue;
		    }
		    blk2.x -= World.x;
		}
		blk2.y = block.y;
		if (BIT(1 << World.block[blk2.x][blk2.y],
			block_mask|REC_LU_BIT|REC_LD_BIT)) {
		    continue;
		}
		break;

	    case BounceVerLo:
		blk2.x = block.x;
		blk2.y = block.y - 1;
		if (blk2.y < 0) {
		    if (!mi->edge_wrap) {
			continue;
		    }
		    blk2.y += World.y;
		}
		if (BIT(1 << World.block[blk2.x][blk2.y],
			block_mask|REC_RU_BIT|REC_LU_BIT)) {
		    continue;
		}
		break;

	    case BounceVerHi:
		blk2.x = block.x;
		blk2.y = block.y + 1;
		if (blk2.y >= World.y) {
		    if (!mi->edge_wrap) {
			continue;
		    }
		    blk2.y -= World.y;
		}
		if (BIT(1 << World.block[blk2.x][blk2.y],
			block_mask|REC_RD_BIT|REC_LD_BIT)) {
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
	    case BounceHorLo|BounceVerLo:
		wall_bounce = BounceLeftDown;
		break;
	    case BounceHorLo|BounceVerHi:
		wall_bounce = BounceLeftUp;
		break;
	    case BounceHorHi|BounceVerLo:
		wall_bounce = BounceRightDown;
		break;
	    case BounceHorHi|BounceVerHi:
		wall_bounce = BounceRightUp;
		break;
	    default:
		switch (block_type) {
		case REC_LD:
		    if ((offset.x == 0) ? (offset.y == BLOCK_CLICKS)
			: (offset.x == BLOCK_CLICKS && offset.y == 0)
			&& ms->vel.x + ms->vel.y >= 0) {
			wall_bounce = 0;
		    }
		    break;
		case REC_LU:
		    if ((offset.x == 0) ? (offset.y == 0)
			: (offset.x == BLOCK_CLICKS && offset.y == BLOCK_CLICKS)
			&& ms->vel.x >= ms->vel.y) {
			wall_bounce = 0;
		    }
		    break;
		case REC_RD:
		    if ((offset.x == 0) ? (offset.y == 0)
			: (offset.x == BLOCK_CLICKS && offset.y == BLOCK_CLICKS)
			&& ms->vel.x <= ms->vel.y) {
			wall_bounce = 0;
		    }
		    break;
		case REC_RU:
		    if ((offset.x == 0) ? (offset.y == BLOCK_CLICKS)
			: (offset.x == BLOCK_CLICKS && offset.y == 0)
			&& ms->vel.x + ms->vel.y <= 0) {
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
	    count = (int)(rfrac() * count);
	    for (bit = 1; bit <= wall_bounce; bit <<= 1) {
		if (wall_bounce & bit) {
		    if (count == 0) {
			wall_bounce = bit;
			break;
		    } else {
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
	ms->done.x += delta.x;
	ms->done.y += delta.y;
	ms->todo.x -= delta.x;
	ms->todo.y -= delta.y;
    }
}

static void Object_crash(move_state_t *ms)
{
    object		*obj = ms->mip->obj;

    switch (ms->crash) {

    default:
	break;

    case CrashTreasure:
	/*
	 * Ball type has already been handled.
	 */
	if (obj->type == OBJ_BALL) {
	    break;
	}
	obj->life = 0;
	break;

    case CrashWall:
	obj->life = 0;
	break;

    case CrashUniverse:
	obj->life = 0;
	break;

    case CrashUnknown:
	obj->life = 0;
	break;
    }
}

void Move_object(int ind)
{
    object		*obj = Obj[ind];
    int			nothing_done = 0;
    int			dist;
    move_info_t		mi;
    move_state_t	ms;
    bool		pos_update = false;

    Object_position_remember(obj);

    dist = walldist[obj->pos.bx][obj->pos.by];
    if (dist > 2) {
	int max = ((dist - 2) * BLOCK_SZ) >> 1;
	if (sqr(max) >= sqr(obj->vel.x) + sqr(obj->vel.y)) {
	    DFLOAT x = obj->pos.cx + FLOAT_TO_CLICK(obj->vel.x);
	    DFLOAT y = obj->pos.cy + FLOAT_TO_CLICK(obj->vel.y);
	    x = WRAP_XCLICK(x);
	    y = WRAP_YCLICK(y);
	    Object_position_set_clicks(obj, (int)(x), (int)(y));
	    return;
	}
    }

    mi.pl = NULL;
    mi.obj = obj;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = BIT(mp.obj_bounce_mask, obj->type);
    mi.treasure_crashes = BIT(mp.obj_treasure_mask, obj->type);

    ms.pos.x = obj->pos.cx;
    ms.pos.y = obj->pos.cy;
    ms.vel = obj->vel;
    ms.todo.x = FLOAT_TO_CLICK(ms.vel.x);
    ms.todo.y = FLOAT_TO_CLICK(ms.vel.y);
    ms.dir = obj->dir;
    ms.mip = &mi;

    for (;;) {
	Move_segment(&ms);
	if (!(ms.done.x | ms.done.y)) {
	    pos_update |= (ms.crash | ms.bounce);
	    if (ms.crash) {
		break;
	    }
	    if (ms.bounce && ms.bounce != BounceEdge) {
		if (obj->type != OBJ_BALL)
		    obj->life = (long)(obj->life * objectWallBounceLifeFactor);
		if (obj->life <= 0) {
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
		if (!BIT(obj->status, FROMBOUNCE) && BIT(obj->type, OBJ_SPARK))
		    CLR_BIT(obj->status, OWNERIMMUNE);
		if (sqr(ms.vel.x) + sqr(ms.vel.y) > sqr(maxObjectWallBounceSpeed)) {
		    obj->life = 0;
		    break;
		}
		ms.vel.x *= objectWallBrakeFactor;
		ms.vel.y *= objectWallBrakeFactor;
		ms.todo.x = (int)(ms.todo.x * objectWallBrakeFactor);
		ms.todo.y = (int)(ms.todo.y * objectWallBrakeFactor);
	    }
	    if (++nothing_done >= 5) {
		ms.crash = CrashUnknown;
		break;
	    }
	} else {
	    ms.pos.x += ms.done.x;
	    ms.pos.y += ms.done.y;
	    nothing_done = 0;
	}
	if (!(ms.todo.x | ms.todo.y)) {
	    break;
	}
    }
    if (mi.edge_wrap) {
	if (ms.pos.x < 0) {
	    ms.pos.x += mp.click_width;
	}
	if (ms.pos.x >= mp.click_width) {
	    ms.pos.x -= mp.click_width;
	}
	if (ms.pos.y < 0) {
	    ms.pos.y += mp.click_height;
	}
	if (ms.pos.y >= mp.click_height) {
	    ms.pos.y -= mp.click_height;
	}
    }
    Object_position_set_clicks(obj, ms.pos.x, ms.pos.y);
    obj->vel = ms.vel;
    obj->dir = ms.dir;
    if (ms.crash) {
	Object_crash(&ms);
    }
    if (pos_update) {
      Object_position_remember(obj);
    }
}


void Move_object_interpolation(int ind)
{
    object		*obj = Obj[ind];
    int			nothing_done = 0;
    int			dist;
    move_info_t		mi;
    move_state_t	ms;
    bool		pos_update = false;
    float               speedfactor = 1.0/frameDivisor;
    
    dist = walldist[obj->pos_interp.bx][obj->pos_interp.by];
    if (dist > 2) {
	int max = ((dist - 2) * BLOCK_SZ) >> 1;
	if (sqr(max) >= sqr(obj->vel_interp.x*speedfactor) + sqr(obj->vel_interp.y)*speedfactor) {
	    DFLOAT x = obj->pos_interp.cx + FLOAT_TO_CLICK(obj->vel_interp.x*speedfactor);
	    DFLOAT y = obj->pos_interp.cy + FLOAT_TO_CLICK(obj->vel_interp.y*speedfactor);
	    x = WRAP_XCLICK(x);
	    y = WRAP_YCLICK(y);
	    Object_position_set_clicks_interpolation(obj, (int)(x), (int)(y));
	    return;
	}
    }

    mi.pl = NULL;
    mi.obj = obj;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = BIT(mp.obj_bounce_mask, obj->type);
    mi.treasure_crashes = BIT(mp.obj_treasure_mask, obj->type);

    ms.pos.x = obj->pos_interp.cx;
    ms.pos.y = obj->pos_interp.cy;
    ms.vel = obj->vel_interp;
    ms.todo.x = FLOAT_TO_CLICK(ms.vel.x*speedfactor);
    ms.todo.y = FLOAT_TO_CLICK(ms.vel.y*speedfactor);
    ms.dir = obj->dir;
    ms.mip = &mi;

    for (;;) {
	Move_segment(&ms);
	if (!(ms.done.x | ms.done.y)) {
	    pos_update |= (ms.crash | ms.bounce);
	    if (ms.crash) {
		break;
	    }
	    if (ms.bounce && ms.bounce != BounceEdge) {
		if (obj->type != OBJ_BALL)
		    obj->life = (long)(obj->life * objectWallBounceLifeFactor);
		if (obj->life <= 0) {
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
		if (!BIT(obj->status, FROMBOUNCE) && BIT(obj->type, OBJ_SPARK))
		    CLR_BIT(obj->status, OWNERIMMUNE);
		
		/* cannot destroy objects which hit walls in the interpolation routine of
		   collisions or we'll get into trouble with ballcounting times in end of round -pgm
		*/
		if (sqr(ms.vel.x) + sqr(ms.vel.y) > sqr(maxObjectWallBounceSpeed)) {
		  obj->life = 0;
		  break;
		}
		ms.vel.x *= objectWallBrakeFactor;
		ms.vel.y *= objectWallBrakeFactor;
		ms.todo.x = (int)(ms.todo.x * objectWallBrakeFactor);
		ms.todo.y = (int)(ms.todo.y * objectWallBrakeFactor);
	    }
	    if (++nothing_done >= 5) {
		ms.crash = CrashUnknown;
		break;
	    }
	} else {
	    ms.pos.x += ms.done.x;
	    ms.pos.y += ms.done.y;
	    nothing_done = 0;
	}
	if (!(ms.todo.x | ms.todo.y)) {
	    break;
	}
    }
    if (mi.edge_wrap) {
	if (ms.pos.x < 0) {
	    ms.pos.x += mp.click_width;
	}
	if (ms.pos.x >= mp.click_width) {
	    ms.pos.x -= mp.click_width;
	}
	if (ms.pos.y < 0) {
	    ms.pos.y += mp.click_height;
	}
	if (ms.pos.y >= mp.click_height) {
	    ms.pos.y -= mp.click_height;
	}
    }
    Object_position_set_clicks_interpolation(obj, ms.pos.x, ms.pos.y);
    obj->vel_interp = ms.vel;
    obj->dir = ms.dir;
}




static void Player_crash(move_state_t *ms, int pt, bool turning)
{
    player		*pl = ms->mip->pl;
    int			ind = GetInd[pl->id];
    const char		*howfmt = NULL;
    const char          *hudmsg = NULL;

    msg[0] = '\0';

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

    case CrashWallAngle:
	howfmt = "%s was trashed%s against a wall";
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
	player		*pushers[MAX_RECORDED_SHOVES];
	int		cnt[MAX_RECORDED_SHOVES];
	int		num_pushers = 0;
	int		total_pusher_count = 0;
	int		total_pusher_score = 0;
	int		i, j, sc;

	SET_BIT(pl->status, KILLED);
	sprintf(msg, howfmt, pl->name, (!pt) ? " head first" : "");

	/* get a list of who pushed me */
	for (i = 0; i < MAX_RECORDED_SHOVES; i++) {
	    shove_t *shove = &pl->shove_record[i];
	    if (shove->pusher_id == -1) {
		continue;
	    }
	    if (shove->time < frame_loops - 20) {
		continue;
	    }
	    for (j = 0; j < num_pushers; j++) {
		if (shove->pusher_id == pushers[j]->id) {
		    cnt[j]++;
		    break;
		}
	    }
	    if (j == num_pushers) {
		pushers[num_pushers++] = Players[GetInd[shove->pusher_id]];
		cnt[j] = 1;
	    }
	    total_pusher_count++;
	    total_pusher_score += pushers[j]->score;
	}
	if (num_pushers == 0) {
	    sc = Rate(WALL_SCORE, pl->score);
	    SCORE(ind, -sc,
		  OBJ_X_IN_BLOCKS(pl),
		  OBJ_Y_IN_BLOCKS(pl),
		  hudmsg);
	    strcat(msg, ".");
	    Set_message(msg);
	}
	else {
	    int		msg_len = strlen(msg);
	    char	*msg_ptr = &msg[msg_len];
	    int		average_pusher_score = total_pusher_score
						/ total_pusher_count;

	    for (i = 0; i < num_pushers; i++) {
		player		*pusher = pushers[i];
		const char	*sep = (!i) ? " with help from "
					    : (i < num_pushers - 1) ? ", "
					    : " and ";
		int		sep_len = strlen(sep);
		int		name_len = strlen(pusher->name);

		if (msg_len + sep_len + name_len + 2 < sizeof msg) {
		    strcpy(msg_ptr, sep);
		    msg_len += sep_len;
		    msg_ptr += sep_len;
		    strcpy(msg_ptr, pusher->name);
		    msg_len += name_len;
		    msg_ptr += name_len;
		}
		sc = cnt[i] * (int)floor(Rate(pusher->score, pl->score)
				    * shoveKillScoreMult) / total_pusher_count;
		SCORE(GetInd[pusher->id], sc,
		      OBJ_X_IN_BLOCKS(pl),
		      OBJ_Y_IN_BLOCKS(pl),
		      pl->name);
	    }
	    sc = (int)floor(Rate(average_pusher_score, pl->score)
		       * shoveKillScoreMult);
	    SCORE(ind, -sc,
		  OBJ_X_IN_BLOCKS(pl),
		  OBJ_Y_IN_BLOCKS(pl),
		  "[Shove]");
	    strcpy(msg_ptr, ".");
	    Set_message(msg);
	}
    }

    if (BIT(pl->status, KILLED)
	&& pl->score < 0
	&& IS_ROBOT_PTR(pl)) {
	pl->home_base = 0;
	Pick_startpos(ind);
    }
}

void Move_player(int ind)
{
    player		*pl = Players[ind];
    int			nothing_done = 0;
    int			i;
    int			dist;
    move_info_t		mi;
    move_state_t	ms[RES];
    int			worst = 0;
    int			crash;
    int			bounce;
    int			moves_made = 0;
    clpos		pos;
    clvec		todo;
    clvec		done;
    vector		vel;
    vector		r[RES];
    ivec		sign;		/* sign (-1 or 1) of direction */
    ipos		block;		/* block index */
    bool		pos_update = false;


    if (BIT(pl->status, PLAYING|PAUSE|GAME_OVER|KILLED) != PLAYING) {
	if (!BIT(pl->status, KILLED|PAUSE)) {
	    pos.x = pl->pos.cx + FLOAT_TO_CLICK(pl->vel.x);
	    pos.y = pl->pos.cy + FLOAT_TO_CLICK(pl->vel.y);
	    pos.x = WRAP_XCLICK(pos.x);
	    pos.y = WRAP_YCLICK(pos.y);
	    if (pos.x != pl->pos.cx || pos.y != pl->pos.cy) {
		Player_position_remember(pl);
		Player_position_set_clicks(pl, pos.x, pos.y);
	    }
	}
	pl->velocity = VECTOR_LENGTH(pl->vel);
	return;
    }

    Player_position_remember(pl);

    dist = walldist[pl->pos.bx][pl->pos.by];
    if (dist > 3) {
	int max = ((dist - 3) * BLOCK_SZ) >> 1;
	if (max >= pl->velocity) {
	    pos.x = pl->pos.cx + FLOAT_TO_CLICK(pl->vel.x);
	    pos.y = pl->pos.cy + FLOAT_TO_CLICK(pl->vel.y);
	    pos.x = WRAP_XCLICK(pos.x);
	    pos.y = WRAP_YCLICK(pos.y);
	    Player_position_set_clicks(pl, pos.x, pos.y);
	    pl->velocity = VECTOR_LENGTH(pl->vel);
	    return;
	}
    }

    mi.pl = pl;
    mi.obj = (object *) pl;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = true;
    mi.treasure_crashes = true;

    vel = pl->vel;
    todo.x = FLOAT_TO_CLICK(vel.x);
    todo.y = FLOAT_TO_CLICK(vel.y);
    for (i = 0; i < pl->ship->num_points; i++) {
	DFLOAT x = pl->ship->pts[i][pl->dir].x;
	DFLOAT y = pl->ship->pts[i][pl->dir].y;
	ms[i].pos.x = pl->pos.cx + FLOAT_TO_CLICK(x);
	ms[i].pos.y = pl->pos.cy + FLOAT_TO_CLICK(y);
	ms[i].vel = vel;
	ms[i].todo = todo;
	ms[i].dir = pl->dir;
	ms[i].mip = &mi;
    }

    for (;; moves_made++) {

	pos.x = ms[0].pos.x - FLOAT_TO_CLICK(pl->ship->pts[0][ms[0].dir].x);
	pos.y = ms[0].pos.y - FLOAT_TO_CLICK(pl->ship->pts[0][ms[0].dir].y);
	pos.x = WRAP_XCLICK(pos.x);
	pos.y = WRAP_YCLICK(pos.y);
	block.x = pos.x / BLOCK_CLICKS;
	block.y = pos.y / BLOCK_CLICKS;

	if (walldist[block.x][block.y] > 3) {
	    int maxcl = ((walldist[block.x][block.y] - 3) * BLOCK_CLICKS) >> 1;
	    todo = ms[0].todo;
	    sign.x = (todo.x < 0) ? -1 : 1;
	    sign.y = (todo.y < 0) ? -1 : 1;
	    if (maxcl >= sign.x * todo.x && maxcl >= sign.y * todo.y) {
		/* entire movement is possible. */
		done.x = todo.x;
		done.y = todo.y;
	    }
	    else if (sign.x * todo.x > sign.y * todo.y) {
		/* horizontal movement. */
		done.x = sign.x * maxcl;
		done.y = todo.y * maxcl / (sign.x * todo.x);
	    }
	    else {
		/* vertical movement. */
		done.x = todo.x * maxcl / (sign.y * todo.y);
		done.y = sign.y * maxcl;
	    }
	    todo.x -= done.x;
	    todo.y -= done.y;
	    for (i = 0; i < pl->ship->num_points; i++) {
		ms[i].pos.x += done.x;
		ms[i].pos.y += done.y;
		ms[i].todo = todo;
		ms[i].crash = NotACrash;
		ms[i].bounce = NotABounce;
		if (mi.edge_wrap) {
		    if (ms[i].pos.x < 0) {
			ms[i].pos.x += mp.click_width;
		    }
		    else if (ms[i].pos.x >= mp.click_width) {
			ms[i].pos.x -= mp.click_width;
		    }
		    if (ms[i].pos.y < 0) {
			ms[i].pos.y += mp.click_height;
		    }
		    else if (ms[i].pos.y >= mp.click_height) {
			ms[i].pos.y -= mp.click_height;
		    }
		}
	    }
	    nothing_done = 0;
	    if (!(todo.x | todo.y)) {
		break;
	    }
	    else {
		continue;
	    }
	}

	bounce = -1;
	crash = -1;
	for (i = 0; i < pl->ship->num_points; i++) {
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
		else if (ms[bounce].bounce != BounceEdge
		    && ms[i].bounce == BounceEdge) {
		    bounce = i;
		}
		else if ((ms[bounce].bounce == BounceEdge)
		    == (ms[i].bounce == BounceEdge)) {
		    if ((int)(rfrac() * (pl->ship->num_points - bounce)) == i) {
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
		DFLOAT	speed = VECTOR_LENGTH(ms[worst].vel);
		int	v = (int) speed >> 2;
		int	m = (int) (pl->mass - pl->emptymass * 0.75f);
		DFLOAT	b = 1 - 0.5f * playerWallBrakeFactor;
		long	cost = (long) (b * m * v);
		int	delta_dir,
			abs_delta_dir,
			wall_dir;
		DFLOAT	max_speed = BIT(pl->used, OBJ_SHIELD)
				    ? maxShieldedWallBounceSpeed
				    : maxUnshieldedWallBounceSpeed;
		int	max_angle = BIT(pl->used, OBJ_SHIELD)
				    ? mp.max_shielded_angle
				    : mp.max_unshielded_angle;

		if (BIT(pl->used, OBJ_SHIELD) == OBJ_SHIELD) {
		    if (max_speed < 100) {
			max_speed = 100;
		    }
		    max_angle = RES;
		}

		ms[worst].vel.x *= playerWallBrakeFactor;
		ms[worst].vel.y *= playerWallBrakeFactor;
		ms[worst].todo.x = (int)(ms[worst].todo.x * playerWallBrakeFactor);
		ms[worst].todo.y = (int)(ms[worst].todo.y * playerWallBrakeFactor);

		if (speed > max_speed) {
		    crash = worst;
		    ms[worst].crash = CrashWallSpeed;
		    break;
		}

		switch (ms[worst].bounce) {
		case BounceHorLo: wall_dir = 4*RES/8; break;
		case BounceHorHi: wall_dir = 0*RES/8; break;
		case BounceVerLo: wall_dir = 6*RES/8; break;
		default:
		case BounceVerHi: wall_dir = 2*RES/8; break;
		case BounceLeftDown: wall_dir = 1*RES/8; break;
		case BounceLeftUp: wall_dir = 7*RES/8; break;
		case BounceRightDown: wall_dir = 3*RES/8; break;
		case BounceRightUp: wall_dir = 5*RES/8; break;
		}
		if (pl->dir >= wall_dir) {
		    delta_dir = (pl->dir - wall_dir <= RES/2)
				? -(pl->dir - wall_dir)
				: (wall_dir + RES - pl->dir);
		} else {
		    delta_dir = (wall_dir - pl->dir <= RES/2)
				? (wall_dir - pl->dir)
				: -(pl->dir + RES - wall_dir);
		}
		abs_delta_dir = ABS(delta_dir);
		if (abs_delta_dir > max_angle) {
		    crash = worst;
		    ms[worst].crash = CrashWallAngle;
		    break;
		}
		if (abs_delta_dir <= RES/16) {
		    pl->float_dir += (1.0f - playerWallBrakeFactor) * delta_dir;
		    if (pl->float_dir >= RES) {
			pl->float_dir -= RES;
		    }
		    else if (pl->float_dir < 0) {
			pl->float_dir += RES;
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
	    for (i = 0; i < pl->ship->num_points; i++) {
		r[i].x = (vel.x) ? (DFLOAT) ms[i].todo.x / vel.x : 0;
		r[i].y = (vel.y) ? (DFLOAT) ms[i].todo.y / vel.y : 0;
		r[i].x = ABS(r[i].x);
		r[i].y = ABS(r[i].y);
	    }
	    worst = 0;
	    for (i = 1; i < pl->ship->num_points; i++) {
		if (r[i].x > r[worst].x || r[i].y > r[worst].y) {
		    worst = i;
		}
	    }
	}

	if (!(ms[worst].done.x | ms[worst].done.y)) {
	    if (++nothing_done >= 5) {
		ms[worst].crash = CrashUnknown;
		break;
	    }
	} else {
	    nothing_done = 0;
	    ms[worst].pos.x += ms[worst].done.x;
	    ms[worst].pos.y += ms[worst].done.y;
	}
	if (!(ms[worst].todo.x | ms[worst].todo.y)) {
	    break;
	}

	vel = ms[worst].vel;
	for (i = 0; i < pl->ship->num_points; i++) {
	    if (i != worst) {
		ms[i].pos.x += ms[worst].done.x;
		ms[i].pos.y += ms[worst].done.y;
		ms[i].vel = vel;
		ms[i].todo = ms[worst].todo;
		ms[i].dir = ms[worst].dir;
	    }
	}
    }

    pos.x = ms[worst].pos.x - FLOAT_TO_CLICK(pl->ship->pts[worst][pl->dir].x);
    pos.y = ms[worst].pos.y - FLOAT_TO_CLICK(pl->ship->pts[worst][pl->dir].y);
    pos.x = WRAP_XCLICK(pos.x);
    pos.y = WRAP_YCLICK(pos.y);
    Player_position_set_clicks(pl, pos.x, pos.y);
    pl->vel = ms[worst].vel;
    pl->velocity = VECTOR_LENGTH(pl->vel);

    if (ms[worst].crash) {
	Player_crash(&ms[worst], worst, false);
    }
    if (pos_update) {
	Player_position_remember(pl);
    }
}



/* 
   This routine should only be used for computing new position for
   an interpolated frame; thus all information which is related to
   player state other than the coordinate and velocity should not
   be changed; this includes dying from a wall collision for instance.
   In particular, player_position remember should not be used here, 
   as it is related to acd collision checking, and updating it here will
   definitevely mess up collision calculations(done only every n'th frame).
   However, the bounces need to be calculated as for real frames, so
   that interpolated data looks reasonable. This WILL make collision
   checking with walls different than with 12fps, but there will be
   NO wallglue.
   Also the speedfactor correction must be applied to new position, which is 
   according to xpilog-ng server. Other than that this routine is the 
   same as is used for normal Move_player which is done for real frames.
 */

void Move_player_interpolation(int ind)
{
    player		*pl = Players[ind];
    int			nothing_done = 0;
    int			i;
    int			dist;
    move_info_t		mi;
    move_state_t	ms[RES];
    int			worst = 0;
    int			crash;
    int			bounce;
    int			moves_made = 0;
    clpos		pos;
    clvec		todo;
    clvec		done;
    vector		vel;
    vector		r[RES];
    ivec		sign;		/* sign (-1 or 1) of direction */
    ipos		block;		/* block index */
    bool		pos_update = false;
    float               speedfactor = 1.0/frameDivisor;

    if (BIT(pl->status, PLAYING|PAUSE|GAME_OVER|KILLED) != PLAYING) {
	if (!BIT(pl->status, KILLED|PAUSE)) {
	    pos.x = pl->pos_interp.cx + FLOAT_TO_CLICK(pl->vel_interp.x*speedfactor);
	    pos.y = pl->pos_interp.cy + FLOAT_TO_CLICK(pl->vel_interp.y*speedfactor);
	    pos.x = WRAP_XCLICK(pos.x);
	    pos.y = WRAP_YCLICK(pos.y);
	    if (pos.x != pl->pos_interp.cx || pos.y != pl->pos_interp.cy) {
		Player_position_set_clicks_interpolation(pl, pos.x, pos.y);
	    }
	}
	/*pl->velocity_interp = VECTOR_LENGTH(pl->vel_interp);*/
	pl->velocity_interp = hypot((double)(pl->vel_interp.x * speedfactor), 
				    (double)(pl->vel_interp.y * speedfactor));
	return;
    }


    dist = walldist[pl->pos_interp.bx][pl->pos_interp.by];
    if (dist > 3) {
	int max = ((dist - 3) * BLOCK_SZ) >> 1;
	if (max >= pl->velocity_interp) {
	    pos.x = pl->pos_interp.cx + FLOAT_TO_CLICK(pl->vel_interp.x*speedfactor);
	    pos.y = pl->pos_interp.cy + FLOAT_TO_CLICK(pl->vel_interp.y*speedfactor);
	    pos.x = WRAP_XCLICK(pos.x);
	    pos.y = WRAP_YCLICK(pos.y);
	    Player_position_set_clicks_interpolation(pl, pos.x, pos.y);

	    pl->velocity_interp = hypot((double)(pl->vel_interp.x * speedfactor), 
					(double)(pl->vel_interp.y * speedfactor));
	    /*pl->velocity_interp = VECTOR_LENGTH(pl->vel_interp);*/
	    return;
	}
    }


    mi.pl = pl;
    mi.obj = (object *) pl;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = true;
    mi.treasure_crashes = true;

    vel = pl->vel_interp;
    todo.x = FLOAT_TO_CLICK(vel.x*speedfactor);
    todo.y = FLOAT_TO_CLICK(vel.y*speedfactor);
    for (i = 0; i < pl->ship->num_points; i++) {
	DFLOAT x = pl->ship->pts[i][pl->dir].x;
	DFLOAT y = pl->ship->pts[i][pl->dir].y;
	ms[i].pos.x = pl->pos_interp.cx + FLOAT_TO_CLICK(x);
	ms[i].pos.y = pl->pos_interp.cy + FLOAT_TO_CLICK(y);
	ms[i].vel = vel;
	ms[i].todo = todo;
	ms[i].dir = pl->dir;
	ms[i].mip = &mi;
    }


    for (;; moves_made++) {

	pos.x = ms[0].pos.x - FLOAT_TO_CLICK(pl->ship->pts[0][ms[0].dir].x);
	pos.y = ms[0].pos.y - FLOAT_TO_CLICK(pl->ship->pts[0][ms[0].dir].y);
	pos.x = WRAP_XCLICK(pos.x);
	pos.y = WRAP_YCLICK(pos.y);
	block.x = pos.x / BLOCK_CLICKS;
	block.y = pos.y / BLOCK_CLICKS;

	if (walldist[block.x][block.y] > 3) {
	    int maxcl = ((walldist[block.x][block.y] - 3) * BLOCK_CLICKS) >> 1;
	    todo = ms[0].todo;
	    sign.x = (todo.x < 0) ? -1 : 1;
	    sign.y = (todo.y < 0) ? -1 : 1;
	    if (maxcl >= sign.x * todo.x && maxcl >= sign.y * todo.y) {
		/* entire movement is possible. */
		done.x = todo.x;
		done.y = todo.y;
	    }
	    else if (sign.x * todo.x > sign.y * todo.y) {
		/* horizontal movement. */
		done.x = sign.x * maxcl;
		done.y = todo.y * maxcl / (sign.x * todo.x);
	    }
	    else {
		/* vertical movement. */
		done.x = todo.x * maxcl / (sign.y * todo.y);
		done.y = sign.y * maxcl;
	    }
	    todo.x -= done.x;
	    todo.y -= done.y;
	    for (i = 0; i < pl->ship->num_points; i++) {
		ms[i].pos.x += done.x;
		ms[i].pos.y += done.y;
		ms[i].todo = todo;
		ms[i].crash = NotACrash;
		ms[i].bounce = NotABounce;
		if (mi.edge_wrap) {
		    if (ms[i].pos.x < 0) {
			ms[i].pos.x += mp.click_width;
		    }
		    else if (ms[i].pos.x >= mp.click_width) {
			ms[i].pos.x -= mp.click_width;
		    }
		    if (ms[i].pos.y < 0) {
			ms[i].pos.y += mp.click_height;
		    }
		    else if (ms[i].pos.y >= mp.click_height) {
			ms[i].pos.y -= mp.click_height;
		    }
		}
	    }
	    nothing_done = 0;
	    if (!(todo.x | todo.y)) {
		break;
	    }
	    else {
		continue;
	    }
	}

	bounce = -1;
	crash = -1;
	for (i = 0; i < pl->ship->num_points; i++) {
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
		else if (ms[bounce].bounce != BounceEdge
		    && ms[i].bounce == BounceEdge) {
		    bounce = i;
		}
		else if ((ms[bounce].bounce == BounceEdge)
		    == (ms[i].bounce == BounceEdge)) {
		    if ((int)(rfrac() * (pl->ship->num_points - bounce)) == i) {
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


	        DFLOAT	speed = VECTOR_LENGTH(ms[worst].vel);
		int	v = (int) speed >> 2;
		int	m = (int) (pl->mass - pl->emptymass * 0.75f);
		DFLOAT	b = 1 - 0.5f * playerWallBrakeFactor;
		long	cost = (long) (b * m * v);
		int	delta_dir,
			abs_delta_dir,
			wall_dir;
		DFLOAT	max_speed = BIT(pl->used, OBJ_SHIELD)
				    ? maxShieldedWallBounceSpeed
				    : maxUnshieldedWallBounceSpeed;
		int	max_angle = BIT(pl->used, OBJ_SHIELD)
				    ? mp.max_shielded_angle
				    : mp.max_unshielded_angle;

		if (BIT(pl->used, OBJ_SHIELD) == OBJ_SHIELD) {
		    if (max_speed < 100) {
			max_speed = 100;
		    }
		    max_angle = RES;
		}

		ms[worst].vel.x *= playerWallBrakeFactor;
		ms[worst].vel.y *= playerWallBrakeFactor;
		ms[worst].todo.x = (int)(ms[worst].todo.x * playerWallBrakeFactor);
		ms[worst].todo.y = (int)(ms[worst].todo.y * playerWallBrakeFactor);

		if (speed > max_speed) {
		    crash = worst;
		    ms[worst].crash = CrashWallSpeed;
		    break;
		}

		switch (ms[worst].bounce) {
		case BounceHorLo: wall_dir = 4*RES/8; break;
		case BounceHorHi: wall_dir = 0*RES/8; break;
		case BounceVerLo: wall_dir = 6*RES/8; break;
		default:
		case BounceVerHi: wall_dir = 2*RES/8; break;
		case BounceLeftDown: wall_dir = 1*RES/8; break;
		case BounceLeftUp: wall_dir = 7*RES/8; break;
		case BounceRightDown: wall_dir = 3*RES/8; break;
		case BounceRightUp: wall_dir = 5*RES/8; break;
		}
		if (pl->dir >= wall_dir) {
		    delta_dir = (pl->dir - wall_dir <= RES/2)
				? -(pl->dir - wall_dir)
				: (wall_dir + RES - pl->dir);
		} else {
		    delta_dir = (wall_dir - pl->dir <= RES/2)
				? (wall_dir - pl->dir)
				: -(pl->dir + RES - wall_dir);
		}
		abs_delta_dir = ABS(delta_dir);
		if (abs_delta_dir > max_angle) {
		    crash = worst;
		    ms[worst].crash = CrashWallAngle;
		    break;
		}
		if (abs_delta_dir <= RES/16) {
		    pl->float_dir += (1.0f - playerWallBrakeFactor) * delta_dir;
		    if (pl->float_dir >= RES) {
			pl->float_dir -= RES;
		    }
		    else if (pl->float_dir < 0) {
			pl->float_dir += RES;
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
	    for (i = 0; i < pl->ship->num_points; i++) {
		r[i].x = (vel.x) ? (DFLOAT) ms[i].todo.x / vel.x : 0;
		r[i].y = (vel.y) ? (DFLOAT) ms[i].todo.y / vel.y : 0;
		r[i].x = ABS(r[i].x);
		r[i].y = ABS(r[i].y);
	    }
	    worst = 0;
	    for (i = 1; i < pl->ship->num_points; i++) {
		if (r[i].x > r[worst].x || r[i].y > r[worst].y) {
		    worst = i;
		}
	    }
	}

	if (!(ms[worst].done.x | ms[worst].done.y)) {
	    if (++nothing_done >= 5) {
		ms[worst].crash = CrashUnknown;
		break;
	    }
	} else {
	    nothing_done = 0;
	    ms[worst].pos.x += ms[worst].done.x;
	    ms[worst].pos.y += ms[worst].done.y;
	}
	if (!(ms[worst].todo.x | ms[worst].todo.y)) {
	    break;
	}

	vel = ms[worst].vel;
	for (i = 0; i < pl->ship->num_points; i++) {
	    if (i != worst) {
		ms[i].pos.x += ms[worst].done.x;
		ms[i].pos.y += ms[worst].done.y;
		ms[i].vel = vel;
		ms[i].todo = ms[worst].todo;
		ms[i].dir = ms[worst].dir;
	    }
	}
    }

    pos.x = ms[worst].pos.x - FLOAT_TO_CLICK(pl->ship->pts[worst][pl->dir].x);
    pos.y = ms[worst].pos.y - FLOAT_TO_CLICK(pl->ship->pts[worst][pl->dir].y);

    pos.x = WRAP_XCLICK(pos.x);
    pos.y = WRAP_YCLICK(pos.y);

    Player_position_set_clicks_interpolation(pl, pos.x, pos.y);

    pl->velocity_interp = hypot((double)(pl->vel_interp.x*speedfactor), (double)(pl->vel_interp.y*speedfactor));
    /*    pl->velocity_interp = VECTOR_LENGTH(pl->vel_interp);*/
 

}




void Turn_player(int ind)
{
    player		*pl = Players[ind];
    int			i;
    move_info_t		mi;
    move_state_t	ms[RES];
    int			dir;
    int			new_dir = MOD2((int)(pl->float_dir + 0.5f), RES);
    int			sign;
    int			crash = -1;
    int			nothing_done = 0;
    int			turns_done = 0;
    int			blocked = 0;
    clpos		pos;
    vector		salt;

    if (new_dir == pl->dir) {
	return;
    }
    if (BIT(pl->status, PLAYING|PAUSE|GAME_OVER|KILLED) != PLAYING) {
	pl->dir = new_dir;
	return;
    }

    if (walldist[pl->pos.bx][pl->pos.by] > 2) {
	pl->dir = new_dir;
	return;
    }

    mi.pl = pl;
    mi.obj = (object *) pl;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = true;
    mi.treasure_crashes = true;

    if (new_dir > pl->dir) {
	sign = (new_dir - pl->dir <= RES + pl->dir - new_dir) ? 1 : -1;
    }
    else {
	sign = (pl->dir - new_dir <= RES + new_dir - pl->dir) ? -1 : 1;
    }

#if 0
    salt.x = (pl->vel.x > 0) ? 0.1f : (pl->vel.x < 0) ? -0.1f : 0;
    salt.y = (pl->vel.y > 0) ? 0.1f : (pl->vel.y < 0) ? -0.1f : 0;
#else
    salt.x = (pl->vel.x > 0) ? 1e-6f : (pl->vel.x < 0) ? -1e-6f : 0;
    salt.y = (pl->vel.y > 0) ? 1e-6f : (pl->vel.y < 0) ? -1e-6f : 0;
#endif

    pos.x = pl->pos.cx;
    pos.y = pl->pos.cy;
    for (; pl->dir != new_dir; turns_done++) {
	dir = MOD2(pl->dir + sign, RES);
	if (!mi.edge_wrap) {
	    if (pos.x <= 22 * CLICK) {
		for (i = 0; i < pl->ship->num_points; i++) {
		    if (pos.x + FLOAT_TO_CLICK(pl->ship->pts[i][dir].x) < 0) {
			pos.x = -FLOAT_TO_CLICK(pl->ship->pts[i][dir].x);
		    }
		}
	    }
	    if (pos.x >= mp.click_width - 22 * CLICK) {
		for (i = 0; i < pl->ship->num_points; i++) {
		    if (pos.x + FLOAT_TO_CLICK(pl->ship->pts[i][dir].x)
			>= mp.click_width) {
			pos.x = mp.click_width - 1
			       - FLOAT_TO_CLICK(pl->ship->pts[i][dir].x);
		    }
		}
	    }
	    if (pos.y <= 22 * CLICK) {
		for (i = 0; i < pl->ship->num_points; i++) {
		    if (pos.y + FLOAT_TO_CLICK(pl->ship->pts[i][dir].y) < 0) {
			pos.y = -FLOAT_TO_CLICK(pl->ship->pts[i][dir].y);
		    }
		}
	    }
	    if (pos.y >= mp.click_height - 22 * CLICK) {
		for (i = 0; i < pl->ship->num_points; i++) {
		    if (pos.y + FLOAT_TO_CLICK(pl->ship->pts[i][dir].y)
			>= mp.click_height) {
			pos.y = mp.click_height - 1
			       - FLOAT_TO_CLICK(pl->ship->pts[i][dir].y);
		    }
		}
	    }
	    if (pos.x != pl->pos.cx || pos.y != pl->pos.cy) {
		Player_position_set_clicks(pl, pos.x, pos.y);
	    }
	}

	for (i = 0; i < pl->ship->num_points; i++) {
	    ms[i].mip = &mi;
	    ms[i].pos.x = pos.x + FLOAT_TO_CLICK(pl->ship->pts[i][pl->dir].x);
	    ms[i].pos.y = pos.y + FLOAT_TO_CLICK(pl->ship->pts[i][pl->dir].y);
	    ms[i].todo.x = pos.x + FLOAT_TO_CLICK(pl->ship->pts[i][dir].x) - ms[i].pos.x;
	    ms[i].todo.y = pos.y + FLOAT_TO_CLICK(pl->ship->pts[i][dir].y) - ms[i].pos.y;
	    ms[i].vel.x = ms[i].todo.x + salt.x;
	    ms[i].vel.y = ms[i].todo.y + salt.y;

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
		else if (ms[i].done.x | ms[i].done.y) {
		    ms[i].pos.x += ms[i].done.x;
		    ms[i].pos.y += ms[i].done.y;
		    nothing_done = 0;
		}
	    } while (ms[i].todo.x | ms[i].todo.y);
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
