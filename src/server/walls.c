/* $Id: walls.c,v 1.5 2007/09/12 15:17:27 kps Exp $
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
#include "rank.h"

char walls_version[] = VERSION;
extern int frame_cycle;

#define WALLDIST_MASK	\
	(FILLED_BIT | REC_LU_BIT | REC_LD_BIT | REC_RU_BIT | REC_RD_BIT \
	| FUEL_BIT | TREASURE_BIT )

unsigned SPACE_BLOCKS = (SPACE_BIT | BASE_BIT);

static struct move_parameters mp;
/*static DFLOAT wallBounceExplosionMult;*/
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
    World.cwidth = PIXEL_TO_CLICK(World.width);
    World.cheight = PIXEL_TO_CLICK(World.height);

    LIMIT(maxObjectWallBounceSpeed, 0, World.hypotenuse);
    LIMIT(maxShieldedWallBounceSpeed, 0, World.hypotenuse);
    LIMIT(maxUnshieldedWallBounceSpeed, 0, World.hypotenuse);
    LIMIT(playerWallBrakeFactor, 0, 1);
    LIMIT(objectWallBrakeFactor, 0, 1);
    LIMIT(objectWallBounceLifeFactor, 0, 1);

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
  
	SET_BIT(mp.obj_bounce_mask, OBJ_BALL);
    

    mp.obj_treasure_mask = mp.obj_bounce_mask | OBJ_BALL;
}

static void Bounce_edge(move_state_t *ms, move_bounce_t bounce)
{
    if (bounce == BounceHorLo) {
	if (ms->mip->edge_bounce) {
	    ms->todo.cx = -ms->todo.cx;
	    ms->vel.x = -ms->vel.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES / 2 - ms->dir, RES);
	    }
	}
	else {
	    ms->todo.cx = 0;
	    ms->vel.x = 0;
	    if (!ms->mip->pl) {
		ms->dir = (ms->vel.y < 0) ? (3*RES/4) : RES/4;
	    }
	}
    }
    else if (bounce == BounceHorHi) {
	if (ms->mip->edge_bounce) {
	    ms->todo.cx = -ms->todo.cx;
	    ms->vel.x = -ms->vel.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES / 2 - ms->dir, RES);
	    }
	}
	else {
	    ms->todo.cx = 0;
	    ms->vel.x = 0;
	    if (!ms->mip->pl) {
		ms->dir = (ms->vel.y < 0) ? (3*RES/4) : RES/4;
	    }
	}
    }
    else if (bounce == BounceVerLo) {
	if (ms->mip->edge_bounce) {
	    ms->todo.cy = -ms->todo.cy;
	    ms->vel.y = -ms->vel.y;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES - ms->dir, RES);
	    }
	}
	else {
	    ms->todo.cy = 0;
	    ms->vel.y = 0;
	    if (!ms->mip->pl) {
		ms->dir = (ms->vel.x < 0) ? (RES/2) : 0;
	    }
	}
    }
    else if (bounce == BounceVerHi) {
	if (ms->mip->edge_bounce) {
	    ms->todo.cy = -ms->todo.cy;
	    ms->vel.y = -ms->vel.y;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES - ms->dir, RES);
	    }
	}
	else {
	    ms->todo.cy = 0;
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
	ms->todo.cx = -ms->todo.cx;
	ms->vel.x = -ms->vel.x;
	if (!ms->mip->pl) {
	    ms->dir = MOD2(RES/2 - ms->dir, RES);
	}
    }
    else if (bounce == BounceHorHi) {
	ms->todo.cx = -ms->todo.cx;
	ms->vel.x = -ms->vel.x;
	if (!ms->mip->pl) {
	    ms->dir = MOD2(RES/2 - ms->dir, RES);
	}
    }
    else if (bounce == BounceVerLo) {
	ms->todo.cy = -ms->todo.cy;
	ms->vel.y = -ms->vel.y;
	if (!ms->mip->pl) {
	    ms->dir = MOD2(RES - ms->dir, RES);
	}
    }
    else if (bounce == BounceVerHi) {
	ms->todo.cy = -ms->todo.cy;
	ms->vel.y = -ms->vel.y;
	if (!ms->mip->pl) {
	    ms->dir = MOD2(RES - ms->dir, RES);
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
		ms->dir = MOD2(3*RES/4 - ms->dir, RES);
	    }
	}
	else if (bounce == BounceLeftUp) {
	    ms->todo.cx = t.cy;
	    ms->todo.cy = t.cx;
	    ms->vel.x = v.y;
	    ms->vel.y = v.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES/4 - ms->dir, RES);
	    }
	}
	else if (bounce == BounceRightDown) {
	    ms->todo.cx = t.cy;
	    ms->todo.cy = t.cx;
	    ms->vel.x = v.y;
	    ms->vel.y = v.x;
	    if (!ms->mip->pl) {
		ms->dir = MOD2(RES/4 - ms->dir, RES);
	    }
	}
	else if (bounce == BounceRightUp) {
	    ms->todo.cx = -t.cy;
	    ms->todo.cy = -t.cx;
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
    ipos_t		block;		/* block index */
    ipos_t		blk2;		/* new block index */
    ivec_t		sign;		/* sign (-1 or 1) of direction */
    clpos_t		delta;		/* delta position in clicks */
    clpos_t		enter;		/* enter block position in clicks */
    clpos_t		leave;		/* leave block position in clicks */
    clpos_t		offset;		/* offset within block in clicks */
    clpos_t		off2;		/* last offset in block in clicks */
    clpos_t		mid;		/* the mean of (offset+off2)/2 */
    const move_info_t	*const mi = ms->mip;	/* alias */

    /*
     * Fill in default return values.
     */
    ms->crash = NotACrash;
    ms->bounce = NotABounce;
    ms->done.cx = 0;
    ms->done.cy = 0;

    enter = ms->pos;
    if (enter.cx < 0 || enter.cx >= World.cwidth
	|| enter.cy < 0 || enter.cy >= World.cheight) {

	if (!mi->edge_wrap) {
	    ms->crash = CrashUniverse;
	    return;
	}
	if (enter.cx < 0) {
	    enter.cx += World.cwidth;
	    if (enter.cx < 0) {
		ms->crash = CrashUniverse;
		return;
	    }
	}
	else if (enter.cx >= World.cwidth) {
	    enter.cx -= World.cwidth;
	    if (enter.cx >= World.cwidth) {
		ms->crash = CrashUniverse;
		return;
	    }
	}
	if (enter.cy < 0) {
	    enter.cy += World.cheight;
	    if (enter.cy < 0) {
		ms->crash = CrashUniverse;
		return;
	    }
	}
	else if (enter.cy >= World.cheight) {
	    enter.cy -= World.cheight;
	    if (enter.cy >= World.cheight) {
		ms->crash = CrashUniverse;
		return;
	    }
	}
	ms->pos = enter;
    }

    sign.x = (ms->vel.x < 0) ? -1 : 1;
    sign.y = (ms->vel.y < 0) ? -1 : 1;
    block.x = enter.cx / BLOCK_CLICKS;
    block.y = enter.cy / BLOCK_CLICKS;
    if (walldist[block.x][block.y] > 2) {
	int maxcl = ((walldist[block.x][block.y] - 2) * BLOCK_CLICKS) >> 1;
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
	return;
    }

    offset.cx = enter.cx - block.x * BLOCK_CLICKS;
    offset.cy = enter.cy - block.y * BLOCK_CLICKS;
    inside = 1;
    if (offset.cx == 0) {
	inside = 0;
	if (sign.x == -1 && (offset.cx = BLOCK_CLICKS, --block.x < 0)) {
	    if (mi->edge_wrap) {
		block.x += World.x;
	    }
	    else {
		Bounce_edge(ms, BounceHorLo);
		return;
	    }
	}
    }
    else if (enter.cx == World.cwidth - 1
	     && !mi->edge_wrap
	     && ms->vel.x > 0) {
	Bounce_edge(ms, BounceHorHi);
	return;
    }
    if (offset.cy == 0) {
	inside = 0;
	if (sign.y == -1 && (offset.cy = BLOCK_CLICKS, --block.y < 0)) {
	    if (mi->edge_wrap) {
		block.y += World.y;
	    }
	    else {
		Bounce_edge(ms, BounceVerLo);
		return;
	    }
	}
    }
    else if (enter.cy == World.cheight - 1
	     && !mi->edge_wrap
	     && ms->vel.y > 0) {
	Bounce_edge(ms, BounceVerHi);
	return;
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
	if (leave.cx == World.cwidth && !mi->edge_wrap) {
	    leave.cx--;
	    need_adjust = 1;
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
	if (leave.cy == World.cheight && !mi->edge_wrap) {
	    leave.cy--;
	    need_adjust = 1;
	}
    }
    if (need_adjust && ms->todo.cy && ms->todo.cx) {
	double wx = (double)(leave.cx - enter.cx) / ms->todo.cx;
	double wy = (double)(leave.cy - enter.cy) / ms->todo.cy;
	if (wx > wy) {
	    double x = ms->todo.cx * wy;
	    leave.cx = enter.cx + DOUBLE_TO_INT(x);
	}
	else if (wx < wy) {
	    double y = ms->todo.cy * wx;
	    leave.cy = enter.cy + DOUBLE_TO_INT(y);
	}
    }

    delta.cx = leave.cx - enter.cx;
    delta.cy = leave.cy - enter.cy;

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
		off2.cx = offset.cx + delta.cx;
		off2.cy = offset.cy + delta.cy;
		mid.cx = (offset.cx + off2.cx) / 2;
		mid.cy = (offset.cy + off2.cy) / 2;
		if (offset.cy > r
		    && off2.cy > r
		    && sqr(mid.cx - r) + sqr(mid.cy - r) > sqr(r)
		    && sqr(off2.cx - r) + sqr(off2.cy - r) > sqr(r)
		    && sqr(offset.cx - r) + sqr(offset.cy - r) > sqr(r)) {
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
		    player_t	*pl = NULL;
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
		  player_t *pl = NULL, *pl2 = NULL;
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
	if (!ms->todo.cx && (offset.cx == 0 || offset.cx == BLOCK_CLICKS)) {
	    /* tricky */
	    break;
	}
	if (!ms->todo.cy && (offset.cy == 0 || offset.cy == BLOCK_CLICKS)) {
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
	    if (offset.cy == BLOCK_CLICKS && ms->vel.x + ms->vel.y < 0) {
		wall_bounce |= BounceLeftDown;
	    }
	}
	if (offset.cy == 0) {
	    if (ms->vel.y > 0) {
		wall_bounce |= BounceVerLo;
	    }
	    if (offset.cx == BLOCK_CLICKS && ms->vel.x + ms->vel.y < 0) {
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
	    delta.cx = (int)((BLOCK_CLICKS - offset.cx - offset.cy) / (1 + w));
	    delta.cy = (int)(delta.cx * w);
	    if (offset.cx + delta.cx + offset.cy + delta.cy < BLOCK_CLICKS) {
		delta.cx++;
		delta.cy = (int)(delta.cx * w);
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
	    delta.cy = (int)((BLOCK_CLICKS - offset.cx - offset.cy) / (1 + w));
	    delta.cx = (int)(delta.cy * w);
	    if (offset.cx + delta.cx + offset.cy + delta.cy < BLOCK_CLICKS) {
		delta.cy++;
		delta.cx = (int)(delta.cy * w);
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
	    delta.cx = (int)((offset.cy - offset.cx) / (1 - w));
	    delta.cy = (int)(delta.cx * w);
	    if (offset.cx + delta.cx < offset.cy + delta.cy) {
		delta.cx++;
		delta.cy = (int)(delta.cx * w);
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
	    delta.cy = (int)((offset.cx - offset.cy) / (1 - w));
	    delta.cx = (int)(delta.cy * w);
	    if (offset.cx + delta.cx < offset.cy + delta.cy) {
		delta.cy--;
		delta.cx = (int)(delta.cy * w);
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
	    delta.cx = (int)((offset.cy - offset.cx) / (1 - w));
	    delta.cy = (int)(delta.cx * w);
	    if (offset.cx + delta.cx > offset.cy + delta.cy) {
		delta.cx--;
		delta.cy = (int)(delta.cx * w);
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
	    delta.cy = (int)((offset.cx - offset.cy) / (1 - w));
	    delta.cx = (int)(delta.cy * w);
	    if (offset.cx + delta.cx > offset.cy + delta.cy) {
		delta.cy++;
		delta.cx = (int)(delta.cy * w);
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
	    delta.cx = (int)((BLOCK_CLICKS - offset.cx - offset.cy) / (1 + w));
	    delta.cy = (int)(delta.cx * w);
	    if (offset.cx + delta.cx + offset.cy + delta.cy > BLOCK_CLICKS) {
		delta.cx--;
		delta.cy = (int)(delta.cx * w);
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
	    delta.cy = (int)((BLOCK_CLICKS - offset.cx - offset.cy) / (1 + w));
	    delta.cx = (int)(delta.cy * w);
	    if (offset.cx + delta.cx + offset.cy + delta.cy > BLOCK_CLICKS) {
		delta.cy--;
		delta.cx = (int)(delta.cy * w);
	    }
	    leave.cx = enter.cx + delta.cx;
	    leave.cy = enter.cy + delta.cy;
	    if (!delta.cy) {
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
		    if ((offset.cx == 0) ? (offset.cy == BLOCK_CLICKS)
			: (offset.cx == BLOCK_CLICKS && offset.cy == 0)
			&& ms->vel.x + ms->vel.y >= 0) {
			wall_bounce = 0;
		    }
		    break;
		case REC_LU:
		    if ((offset.cx == 0) ? (offset.cy == 0)
			: (offset.cx == BLOCK_CLICKS && offset.cy == BLOCK_CLICKS)
			&& ms->vel.x >= ms->vel.y) {
			wall_bounce = 0;
		    }
		    break;
		case REC_RD:
		    if ((offset.cx == 0) ? (offset.cy == 0)
			: (offset.cx == BLOCK_CLICKS && offset.cy == BLOCK_CLICKS)
			&& ms->vel.x <= ms->vel.y) {
			wall_bounce = 0;
		    }
		    break;
		case REC_RU:
		    if ((offset.cx == 0) ? (offset.cy == BLOCK_CLICKS)
			: (offset.cx == BLOCK_CLICKS && offset.cy == 0)
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
	ms->done.cx += delta.cx;
	ms->done.cy += delta.cy;
	ms->todo.cx -= delta.cx;
	ms->todo.cy -= delta.cy;
    }
}

static void Object_crash(move_state_t *ms)
{
    object_t		*obj = ms->mip->obj;

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
    object_t		*obj = Obj[ind];
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
	    int cx = obj->pos.cx + FLOAT_TO_CLICK(obj->vel.x);
	    int cy = obj->pos.cy + FLOAT_TO_CLICK(obj->vel.y);
	    cx = WRAP_XCLICK(cx);
	    cy = WRAP_YCLICK(cy);
	    Object_position_set_clicks(obj, cx, cy);
	    return;
	}
    }

    mi.pl = NULL;
    mi.obj = obj;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = BIT(mp.obj_bounce_mask, obj->type);
    mi.treasure_crashes = BIT(mp.obj_treasure_mask, obj->type);

    ms.pos.cx = obj->pos.cx;
    ms.pos.cy = obj->pos.cy;
    ms.vel = obj->vel;
    ms.todo.cx = FLOAT_TO_CLICK(ms.vel.x);
    ms.todo.cy = FLOAT_TO_CLICK(ms.vel.y);
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
		ms.todo.cx = (int)(ms.todo.cx * objectWallBrakeFactor);
		ms.todo.cy = (int)(ms.todo.cy * objectWallBrakeFactor);
	    }
	    if (++nothing_done >= 5) {
		ms.crash = CrashUnknown;
		break;
	    }
	} else {
	    ms.pos.cx += ms.done.cx;
	    ms.pos.cy += ms.done.cy;
	    nothing_done = 0;
	}
	if (!(ms.todo.cx | ms.todo.cy)) {
	    break;
	}
    }
    if (mi.edge_wrap) {
	if (ms.pos.cx < 0) {
	    ms.pos.cx += World.cwidth;
	}
	if (ms.pos.cx >= World.cwidth) {
	    ms.pos.cx -= World.cwidth;
	}
	if (ms.pos.cy < 0) {
	    ms.pos.cy += World.cheight;
	}
	if (ms.pos.cy >= World.cheight) {
	    ms.pos.cy -= World.cheight;
	}
    }
    Object_position_set_clicks(obj, ms.pos.cx, ms.pos.cy);
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
    object_t		*obj = Obj[ind];
    int			nothing_done = 0;
    int			dist;
    move_info_t		mi;
    move_state_t	ms;
    bool		pos_update = false;
    float               speedfactor = ticksPerFrame;
    
    dist = walldist[obj->pos_interp.bx][obj->pos_interp.by];
    if (dist > 2) {
	int max = ((dist - 2) * BLOCK_SZ) >> 1;
	if (sqr(max) >= sqr(obj->vel_interp.x*speedfactor) + sqr(obj->vel_interp.y)*speedfactor) {
	    int cx = obj->pos_interp.cx + FLOAT_TO_CLICK(obj->vel_interp.x*speedfactor);
	    int cy = obj->pos_interp.cy + FLOAT_TO_CLICK(obj->vel_interp.y*speedfactor);
	    cx = WRAP_XCLICK(cx);
	    cy = WRAP_YCLICK(cy);
	    Object_position_set_clicks_interpolation(obj, cx, cy);
	    return;
	}
    }

    mi.pl = NULL;
    mi.obj = obj;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = BIT(mp.obj_bounce_mask, obj->type);
    mi.treasure_crashes = BIT(mp.obj_treasure_mask, obj->type);

    ms.pos.cx = obj->pos_interp.cx;
    ms.pos.cy = obj->pos_interp.cy;
    ms.vel = obj->vel_interp;
    ms.todo.cx = FLOAT_TO_CLICK(ms.vel.x*speedfactor);
    ms.todo.cy = FLOAT_TO_CLICK(ms.vel.y*speedfactor);
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
		ms.todo.cx = (int)(ms.todo.cx * objectWallBrakeFactor);
		ms.todo.cy = (int)(ms.todo.cy * objectWallBrakeFactor);
	    }
	    if (++nothing_done >= 5) {
		ms.crash = CrashUnknown;
		break;
	    }
	} else {
	    ms.pos.cx += ms.done.cx;
	    ms.pos.cy += ms.done.cy;
	    nothing_done = 0;
	}
	if (!(ms.todo.cx | ms.todo.cy)) {
	    break;
	}
    }
    if (mi.edge_wrap) {
	if (ms.pos.cx < 0) {
	    ms.pos.cx += World.cwidth;
	}
	if (ms.pos.cx >= World.cwidth) {
	    ms.pos.cx -= World.cwidth;
	}
	if (ms.pos.cy < 0) {
	    ms.pos.cy += World.cheight;
	}
	if (ms.pos.cy >= World.cheight) {
	    ms.pos.cy -= World.cheight;
	}
    }
    Object_position_set_clicks_interpolation(obj, ms.pos.cx, ms.pos.cy);
    obj->vel_interp = ms.vel;
    obj->dir = ms.dir;
}




static void Player_crash(move_state_t *ms, int pt, bool turning)
{
    player_t		*pl = ms->mip->pl;
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
	player_t		*pushers[MAX_RECORDED_SHOVES];
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
		player_t		*pusher = pushers[i];
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
		sc = cnt[i] * (int)floor(Rate(pusher->score, pl->score))
		    / total_pusher_count;
		SCORE(GetInd[pusher->id], sc,
		      OBJ_X_IN_BLOCKS(pl),
		      OBJ_Y_IN_BLOCKS(pl),
		      pl->name);
	    }
	    sc = (int)floor(Rate(average_pusher_score, pl->score));
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
    player_t		*pl = Players[ind];
    int			nothing_done = 0;
    int			i;
    int			dist;
    move_info_t		mi;
    move_state_t	ms[RES];
    int			worst = 0;
    int			crash;
    int			bounce;
    int			moves_made = 0;
    clpos_t		pos;
    clvec_t		todo;
    clvec_t		done;
    vector_t		vel;
    vector_t		r[RES];
    ivec_t		sign;		/* sign (-1 or 1) of direction */
    ipos_t		block;		/* block index */
    bool		pos_update = false;
    shipshape_t		*ship = pl->ship;

    if (!pl->oldturn)
	ship = Circle_ship();

    if (BIT(pl->status, PLAYING|PAUSE|GAME_OVER|KILLED) != PLAYING) {
	if (!BIT(pl->status, KILLED|PAUSE)) {
	    pos.cx = pl->pos.cx + FLOAT_TO_CLICK(pl->vel.x);
	    pos.cy = pl->pos.cy + FLOAT_TO_CLICK(pl->vel.y);
	    pos.cx = WRAP_XCLICK(pos.cx);
	    pos.cy = WRAP_YCLICK(pos.cy);
	    if (pos.cx != pl->pos.cx || pos.cy != pl->pos.cy) {
		Player_position_remember(pl);
		Player_position_set_clicks(pl, pos.cx, pos.cy);
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
	    pos.cx = pl->pos.cx + FLOAT_TO_CLICK(pl->vel.x);
	    pos.cy = pl->pos.cy + FLOAT_TO_CLICK(pl->vel.y);
	    pos.cx = WRAP_XCLICK(pos.cx);
	    pos.cy = WRAP_YCLICK(pos.cy);
	    Player_position_set_clicks(pl, pos.cx, pos.cy);
	    pl->velocity = VECTOR_LENGTH(pl->vel);
	    return;
	}
    }

    mi.pl = pl;
    mi.obj = (object_t *) pl;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = true;
    mi.treasure_crashes = true;

    vel = pl->vel;
    todo.cx = FLOAT_TO_CLICK(vel.x);
    todo.cy = FLOAT_TO_CLICK(vel.y);
    for (i = 0; i < ship->num_points; i++) {
	DFLOAT x = ship->pts[i][pl->dir].x;
	DFLOAT y = ship->pts[i][pl->dir].y;
	ms[i].pos.cx = pl->pos.cx + FLOAT_TO_CLICK(x);
	ms[i].pos.cy = pl->pos.cy + FLOAT_TO_CLICK(y);
	ms[i].vel = vel;
	ms[i].todo = todo;
	ms[i].dir = pl->dir;
	ms[i].mip = &mi;
    }

    for (;; moves_made++) {

	pos.cx = ms[0].pos.cx - FLOAT_TO_CLICK(ship->pts[0][ms[0].dir].x);
	pos.cy = ms[0].pos.cy - FLOAT_TO_CLICK(ship->pts[0][ms[0].dir].y);
	pos.cx = WRAP_XCLICK(pos.cx);
	pos.cy = WRAP_YCLICK(pos.cy);
	block.x = pos.cx / BLOCK_CLICKS;
	block.y = pos.cy / BLOCK_CLICKS;

	if (walldist[block.x][block.y] > 3) {
	    int maxcl = ((walldist[block.x][block.y] - 3) * BLOCK_CLICKS) >> 1;
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
		ms[i].todo = todo;
		ms[i].crash = NotACrash;
		ms[i].bounce = NotABounce;
		if (mi.edge_wrap) {
		    if (ms[i].pos.cx < 0) {
			ms[i].pos.cx += World.cwidth;
		    }
		    else if (ms[i].pos.cx >= World.cwidth) {
			ms[i].pos.cx -= World.cwidth;
		    }
		    if (ms[i].pos.cy < 0) {
			ms[i].pos.cy += World.cheight;
		    }
		    else if (ms[i].pos.cy >= World.cheight) {
			ms[i].pos.cy -= World.cheight;
		    }
		}
	    }
	    nothing_done = 0;
	    if (!(todo.cx | todo.cy)) {
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
		else if (ms[bounce].bounce != BounceEdge
		    && ms[i].bounce == BounceEdge) {
		    bounce = i;
		}
		else if ((ms[bounce].bounce == BounceEdge)
		    == (ms[i].bounce == BounceEdge)) {
		    if ((int)(rfrac() * (ship->num_points - bounce)) == i) {
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
		/*int	v = (int) speed >> 2;
		int	m = (int) (pl->mass - pl->emptymass * 0.75f);
		DFLOAT	b = 1 - 0.5f * playerWallBrakeFactor;
		long	cost = (long) (b * m * v);*/
		int	delta_dir,
			abs_delta_dir,
			wall_dir;
		DFLOAT	max_speed = BIT(pl->used, OBJ_SHIELD)
				    ? maxShieldedWallBounceSpeed
				    : maxUnshieldedWallBounceSpeed;

		if (BIT(pl->used, OBJ_SHIELD) == OBJ_SHIELD) {
		    if (max_speed < 100) {
			max_speed = 100;
		    }
		}

		ms[worst].vel.x *= playerWallBrakeFactor;
		ms[worst].vel.y *= playerWallBrakeFactor;
		ms[worst].todo.cx = (int)(ms[worst].todo.cx * playerWallBrakeFactor);
		ms[worst].todo.cy = (int)(ms[worst].todo.cy * playerWallBrakeFactor);

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
	} else {
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
    pos.cx = WRAP_XCLICK(pos.cx);
    pos.cy = WRAP_YCLICK(pos.cy);
    Player_position_set_clicks(pl, pos.cx, pos.cy);
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
    player_t		*pl = Players[ind];
    int			nothing_done = 0;
    int			i;
    int			dist;
    move_info_t		mi;
    move_state_t	ms[RES];
    int			worst = 0;
    int			crash;
    int			bounce;
    int			moves_made = 0;
    clpos_t		pos;
    clvec_t		todo;
    clvec_t		done;
    vector_t		vel;
    vector_t		r[RES];
    ivec_t		sign;		/* sign (-1 or 1) of direction */
    ipos_t		block;		/* block index */
    bool		pos_update = false;
    float               speedfactor = ticksPerFrame;
    shipshape_t		*ship = pl->ship;

    if (!pl->oldturn)
	ship = Circle_ship();

    if (BIT(pl->status, PLAYING|PAUSE|GAME_OVER|KILLED) != PLAYING) {
	if (!BIT(pl->status, KILLED|PAUSE)) {
	    pos.cx = pl->pos_interp.cx + FLOAT_TO_CLICK(pl->vel_interp.x*speedfactor);
	    pos.cy = pl->pos_interp.cy + FLOAT_TO_CLICK(pl->vel_interp.y*speedfactor);
	    pos.cx = WRAP_XCLICK(pos.cx);
	    pos.cy = WRAP_YCLICK(pos.cy);
	    if (pos.cx != pl->pos_interp.cx || pos.cy != pl->pos_interp.cy) {
		Player_position_set_clicks_interpolation(pl, pos.cx, pos.cy);
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
	    pos.cx = pl->pos_interp.cx + FLOAT_TO_CLICK(pl->vel_interp.x*speedfactor);
	    pos.cy = pl->pos_interp.cy + FLOAT_TO_CLICK(pl->vel_interp.y*speedfactor);
	    pos.cx = WRAP_XCLICK(pos.cx);
	    pos.cy = WRAP_YCLICK(pos.cy);
	    Player_position_set_clicks_interpolation(pl, pos.cx, pos.cy);

	    pl->velocity_interp = hypot((double)(pl->vel_interp.x * speedfactor), 
					(double)(pl->vel_interp.y * speedfactor));
	    /*pl->velocity_interp = VECTOR_LENGTH(pl->vel_interp);*/
	    return;
	}
    }


    mi.pl = pl;
    mi.obj = (object_t *) pl;
    mi.edge_wrap = BIT(World.rules->mode, WRAP_PLAY);
    mi.edge_bounce = edgeBounce;
    mi.wall_bounce = true;
    mi.treasure_crashes = true;

    vel = pl->vel_interp;
    todo.cx = FLOAT_TO_CLICK(vel.x*speedfactor);
    todo.cy = FLOAT_TO_CLICK(vel.y*speedfactor);
    for (i = 0; i < ship->num_points; i++) {
	DFLOAT x = ship->pts[i][pl->dir].x;
	DFLOAT y = ship->pts[i][pl->dir].y;
	ms[i].pos.cx = pl->pos_interp.cx + FLOAT_TO_CLICK(x);
	ms[i].pos.cy = pl->pos_interp.cy + FLOAT_TO_CLICK(y);
	ms[i].vel = vel;
	ms[i].todo = todo;
	ms[i].dir = pl->dir;
	ms[i].mip = &mi;
    }


    for (;; moves_made++) {

	pos.cx = ms[0].pos.cx - FLOAT_TO_CLICK(ship->pts[0][ms[0].dir].x);
	pos.cy = ms[0].pos.cy - FLOAT_TO_CLICK(ship->pts[0][ms[0].dir].y);
	pos.cx = WRAP_XCLICK(pos.cx);
	pos.cy = WRAP_YCLICK(pos.cy);
	block.x = pos.cx / BLOCK_CLICKS;
	block.y = pos.cy / BLOCK_CLICKS;

	if (walldist[block.x][block.y] > 3) {
	    int maxcl = ((walldist[block.x][block.y] - 3) * BLOCK_CLICKS) >> 1;
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
		ms[i].todo = todo;
		ms[i].crash = NotACrash;
		ms[i].bounce = NotABounce;
		if (mi.edge_wrap) {
		    if (ms[i].pos.cx < 0) {
			ms[i].pos.cx += World.cwidth;
		    }
		    else if (ms[i].pos.cx >= World.cwidth) {
			ms[i].pos.cx -= World.cwidth;
		    }
		    if (ms[i].pos.cy < 0) {
			ms[i].pos.cy += World.cheight;
		    }
		    else if (ms[i].pos.cy >= World.cheight) {
			ms[i].pos.cy -= World.cheight;
		    }
		}
	    }
	    nothing_done = 0;
	    if (!(todo.cx | todo.cy)) {
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
		else if (ms[bounce].bounce != BounceEdge
		    && ms[i].bounce == BounceEdge) {
		    bounce = i;
		}
		else if ((ms[bounce].bounce == BounceEdge)
		    == (ms[i].bounce == BounceEdge)) {
		    if ((int)(rfrac() * (ship->num_points - bounce)) == i) {
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
		/*int	v = (int) speed >> 2;
		int	m = (int) (pl->mass - pl->emptymass * 0.75f);
		DFLOAT	b = 1 - 0.5f * playerWallBrakeFactor;
		long	cost = (long) (b * m * v);*/
		int	delta_dir,
			abs_delta_dir,
			wall_dir;
		DFLOAT	max_speed = BIT(pl->used, OBJ_SHIELD)
				    ? maxShieldedWallBounceSpeed
				    : maxUnshieldedWallBounceSpeed;

		if (BIT(pl->used, OBJ_SHIELD) == OBJ_SHIELD) {
		    if (max_speed < 100) {
			max_speed = 100;
		    }
		}

		ms[worst].vel.x *= playerWallBrakeFactor;
		ms[worst].vel.y *= playerWallBrakeFactor;
		ms[worst].todo.cx = (int)(ms[worst].todo.cx * playerWallBrakeFactor);
		ms[worst].todo.cy = (int)(ms[worst].todo.cy * playerWallBrakeFactor);

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
	} else {
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

    pos.cx = WRAP_XCLICK(pos.cx);
    pos.cy = WRAP_YCLICK(pos.cy);

    Player_position_set_clicks_interpolation(pl, pos.cx, pos.cy);

    pl->velocity_interp = hypot((double)(pl->vel_interp.x*speedfactor), (double)(pl->vel_interp.y*speedfactor));
    /*    pl->velocity_interp = VECTOR_LENGTH(pl->vel_interp);*/
 

}

void Turn_player(player_t *pl)
{
    pl->dir = MOD2((int)(pl->float_dir + 0.5f), RES);
}

void Old_turn_player(int ind)
{
    player_t		*pl = Players[ind];
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
    clpos_t		pos;
    vector_t		salt;

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
    mi.obj = (object_t *) pl;
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

    pos.cx = pl->pos.cx;
    pos.cy = pl->pos.cy;
    for (; pl->dir != new_dir; turns_done++) {
	dir = MOD2(pl->dir + sign, RES);
	if (!mi.edge_wrap) {
	    if (pos.cx <= 22 * CLICK) {
		for (i = 0; i < pl->ship->num_points; i++) {
		    if (pos.cx + FLOAT_TO_CLICK(pl->ship->pts[i][dir].x) < 0) {
			pos.cx = -FLOAT_TO_CLICK(pl->ship->pts[i][dir].x);
		    }
		}
	    }
	    if (pos.cx >= World.cwidth - 22 * CLICK) {
		for (i = 0; i < pl->ship->num_points; i++) {
		    if (pos.cx + FLOAT_TO_CLICK(pl->ship->pts[i][dir].x)
			>= World.cwidth) {
			pos.cx = World.cwidth - 1
			       - FLOAT_TO_CLICK(pl->ship->pts[i][dir].x);
		    }
		}
	    }
	    if (pos.cy <= 22 * CLICK) {
		for (i = 0; i < pl->ship->num_points; i++) {
		    if (pos.cy + FLOAT_TO_CLICK(pl->ship->pts[i][dir].y) < 0) {
			pos.cy = -FLOAT_TO_CLICK(pl->ship->pts[i][dir].y);
		    }
		}
	    }
	    if (pos.cy >= World.cheight - 22 * CLICK) {
		for (i = 0; i < pl->ship->num_points; i++) {
		    if (pos.cy + FLOAT_TO_CLICK(pl->ship->pts[i][dir].y)
			>= World.cheight) {
			pos.cy = World.cheight - 1
			       - FLOAT_TO_CLICK(pl->ship->pts[i][dir].y);
		    }
		}
	    }
	    if (pos.cx != pl->pos.cx || pos.cy != pl->pos.cy) {
		Player_position_set_clicks(pl, pos.cx, pos.cy);
	    }
	}

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
