/* $Id: collision.c,v 1.3 2007/06/12 18:59:38 kps Exp $
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

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "xpconfig.h"
#include "serverconst.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "score.h"
#include "item.h"
#include "netserver.h"
#include "pack.h"
#include "error.h"
#include "objpos.h"
#include "rank.h"

char collision_version[] = VERSION;

/* proposed edgewrap version: */
#define in_range_old(o1, o2, r)						\
    (BIT(World.rules->mode, WRAP_PLAY)					\
	? ((((o1)->pos.x > (o2)->pos.x)					\
	    ? (((o1)->pos.x - (o2)->pos.x > (World.width >> 1))		\
		? ((o1)->pos.x - (o2)->pos.x > World.width - (r))	\
		: ((o1)->pos.x - (o2)->pos.x < (r)))			\
	    : (((o2)->pos.x - (o1)->pos.x > (World.width >> 1))		\
		? ((o2)->pos.x - (o1)->pos.x > World.width - (r))	\
		: ((o2)->pos.x - (o1)->pos.x < (r))))			\
	    && (((o1)->pos.y > (o2)->pos.y)				\
	    ? (((o1)->pos.y - (o2)->pos.y > (World.height >> 1))	\
		? ((o1)->pos.y - (o2)->pos.y > World.height - (r))	\
		: ((o1)->pos.y - (o2)->pos.y < (r)))			\
	    : (((o2)->pos.y - (o1)->pos.y > (World.height >> 1))	\
		? ((o2)->pos.y - (o1)->pos.y > World.height - (r))	\
		: ((o2)->pos.y - (o1)->pos.y < (r)))))			\
	: (DELTA((o1)->pos.x, (o2)->pos.x) < (r)			\
	&& DELTA((o1)->pos.y, (o2)->pos.y) < (r)))
/*
 * The very first "analytical" collision patch, XPilot 3.6.2
 * Faster than other patches and accurate below half warp-speed
 * Trivial common subexpressions are eliminated by any reasonable compiler, 
 * and kept here for readability.
 * Written by Pontus (Rakk, Kepler) pontus@ctrl-c.liu.se Jan 1998
 * Kudos to Svenske and Mad Gurka for beta testing, and Murx for
 * invaluable insights.
 */


inline int in_range_acd(int p1x, int p1y, int p2x, int p2y,
		 int q1x, int q1y, int q2x, int q2y,
		 int r)
{
    long	fac1, fac2;
    double	tmin, fminx, fminy;
    long	top, bot;
    bool	mpx, mpy, mqx, mqy;

    /*
     * Get the wrapped coordinates straight 
     */
    if (BIT(World.rules->mode, WRAP_PLAY)) {
	if ((mpx = (ABS(p2x - p1x) > World.width / 2))) {
	    if (p1x > p2x)
		p1x -= World.width;
	    else
		p2x -= World.width;
	}
	if ((mpy = (ABS(p2y - p1y) > World.height / 2))) {
	    if (p1y > p2y)
		p1y -= World.height;
	    else
		p2y -= World.height;
	}
	if ((mqx = (ABS(q2x - q1x) > World.width / 2))) {
	    if (q1x > q2x)
		q1x -= World.width;
	    else
		q2x -= World.width;
	}
	if ((mqy = (ABS(q2y - q1y) > World.height / 2))) {
	    if (q1y > q2y)
		q1y -= World.height;
	    else
		q2y -= World.height;
	}

	if (mpx && !mqx && (q2x > World.width / 2 || q1x > World.width / 2)) {
	    q1x -= World.width;
	    q2x -= World.width;
	}

	if (mqy && !mpy && (q2y > World.height / 2 || q1y > World.height / 2)) {
	    q1y -= World.height;
	    q2y -= World.height;
	}

	if (mqx && !mpx && (p2x > World.width / 2 || p1x > World.width / 2)) {
	    p1x -= World.width;
	    p2x -= World.width;
	}

	if (mqy && !mpy && (p2y > World.height / 2 || p1y > World.height / 2)) {
	    p1y -= World.height;
	    p2y -= World.height;
	}
    }

    /*
     * Do the detection 
     */
    if ((p2x - q2x) * (p2x - q2x) + (p2y - q2y) * (p2y - q2y) < r * r)
	return 1;
    fac1 = -p1x + p2x + q1x - q2x;
    fac2 = -p1y + p2y + q1y - q2y;
    top = -(fac1 * (-p2x + q2x) + fac2 * (-p2y + q2y));
    bot = (fac1 * fac1 + fac2 * fac2);
    if (top < 0 || bot < 1 || top > bot)
	return 0;
    tmin = ((double)top) / ((double)bot);
    fminx = -p2x + q2x + fac1 * tmin;
    fminy = -p2y + q2y + fac2 * tmin;
    if (fminx * fminx + fminy * fminy < r * r)
	return 1;
    else
	return 0;
}

/*
 * Globals
 */
extern long KILLING_SHOTS;
static char msg[MSG_LEN];

static object_t ***Cells;
static object_t **CellsUsed[MAX_TOTAL_SHOTS];
static int cells_used_count;


int Rate(int winner, int looser);
static void PlayerCollision(void);
static void PlayerObjectCollision(int ind);


void Free_cells(void)
{
    if (Cells) {
	free(Cells);
	Cells = NULL;
    }
    cells_used_count = 0;
}


void Alloc_cells(void)
{
    unsigned		size;
    object_t		**objp;
    int			x, y;

    Free_cells();

    size = sizeof(object_t ***) * World.x;
    size += sizeof(object_t **) * World.x * World.y;
    if (!(Cells = (object_t ***) malloc(size))) {
	error("No Cell mem");
	End_game();
    }
    objp = (object_t **) &Cells[World.x];
    for (x = 0; x < World.x; x++) {
	Cells[x] = objp;
	for (y = 0; y < World.y; y++) {
	    *objp++ = NULL;
	}
    }
}


static void Cell_objects_init(void)
{
    int			i,
			x,
			y;
    object_t		*obj,
			**cell;

    for (i = 0; i < cells_used_count; i++) {
	*CellsUsed[i] = NULL;
    }
    cells_used_count = 0;
    for (i = 0; i < NumObjs; i++) {
	obj = Obj[i];
	if (obj->life <= 0) {
	    continue;
	}
	x = OBJ_X_IN_BLOCKS(obj);
	y = OBJ_Y_IN_BLOCKS(obj);
	cell = &Cells[x][y];
	if (!(obj->cell_list = *cell)) {
	    CellsUsed[cells_used_count++] = cell;
	}
	*cell = obj;
    }
}


static void Cell_objects_get(int x, int y, int r, object_t ***list, int *count)
{
    static object_t	*ObjectList[MAX_TOTAL_SHOTS + 1];
    int			i,
			minx, maxx, miny, maxy,
			xr, yr, xw, yw;
    object_t		*obj;

    if (BIT(World.rules->mode, WRAP_PLAY)) {
	if (2*r > World.x) {
	    r = World.x / 2;
	}
	if (2*r > World.y) {
	    r = World.y / 2;
	}
    }
    else {
	if (r > World.x) {
	    r = World.x;
	}
	if (r > World.y) {
	    r = World.y;
	}
    }
    minx = x - r;
    maxx = x + r;
    miny = y - r;
    maxy = y + r;
    if (BIT(World.rules->mode, WRAP_PLAY)) {
	if (minx < 0) {
	    minx += World.x;
	    maxx += World.x;
	}
	if (miny < 0) {
	    miny += World.y;
	    maxy += World.y;
	}
    }
    else {
	if (minx < 0) {
	    minx = 0;
	}
	if (miny < 0) {
	    miny = 0;
	}
	if (maxx >= World.x) {
	    maxx = World.x - 1;
	}
	if (maxy >= World.y) {
	    maxy = World.y - 1;
	}
    }
    i = 0;
    for (xr = xw = minx; xr <= maxx; xr++, xw++) {
	if (xw >= World.x) {
	    xw -= World.x;
	}
	for (yr = yw = miny; yr <= maxy; yr++, yw++) {
	    if (yw >= World.y) {
		yw -= World.y;
	    }
	    for (obj = Cells[xw][yw]; obj; obj = obj->cell_list) {
		ObjectList[i++] = obj;
	    }
	}
    }
    ObjectList[i] = NULL;
    *list = &ObjectList[0];
    if (count != NULL) {
	*count = i;
    }
}

void SCORE(int ind, int points, int x, int y, const char *msg)
{
    player_t	*pl = Players[ind];

    pl->score += (points);
    
    Rank_add_score(pl, points);
    if (pl->conn != NOT_CONNECTED)
      Send_score_object(pl->conn, points, x, y, msg);
    updateScores = true;
}

int Rate(int winner, int loser)
{
    int t;

    t = ((RATE_SIZE / 2) * RATE_RANGE) / (ABS(loser - winner) + RATE_RANGE);
    if (loser > winner)
	t = RATE_SIZE - t;
    return (t);
}

/*
 * Cause `winner' to get `winner_score' points added with message
 * `winner_msg', and similarly with the `loser' and equivalent
 * variables.
 *
 * In general the winner_score should be positive, and the loser_score
 * negative, but this need not be true.
 *
 * If the winner and loser players are on the same team, the scores are
 * made negative, since you shouldn't gain points by killing team members,
 * or being killed by a team member (it is both players faults).
 *
 * BD 28-4-98: Same for killing your own tank.
 */
static void Score_players(int winner, int winner_score, char *winner_msg,
			  int loser, int loser_score, char *loser_msg)
{
    if (TEAM(winner, loser)) {
	if (winner_score > 0)
	    winner_score = -winner_score;
	if (loser_score > 0)
	    loser_score = -loser_score;
    }
    SCORE(winner, winner_score,
	  OBJ_X_IN_BLOCKS(Players[winner]),
	  OBJ_Y_IN_BLOCKS(Players[winner]),
	  winner_msg);
    SCORE(loser, loser_score,
	  OBJ_X_IN_BLOCKS(Players[loser]),
	  OBJ_Y_IN_BLOCKS(Players[loser]),
	  loser_msg);
}

void Check_collision(void)
{
    Cell_objects_init();
    PlayerCollision();
}

static void PlayerCollision(void)
{
    int			i, j, sc, sc2;
    player_t		*pl;

    /* Player - player, checkpoint, treasure, object and wall */
    for (i=0; i<NumPlayers; i++) {
	pl = Players[i];
	if (BIT(pl->status, PLAYING|PAUSE|GAME_OVER|KILLED) != PLAYING)
	    continue;

	if (pl->pos.x < 0 || pl->pos.y < 0
	    || pl->pos.x >= World.width
	    || pl->pos.y >= World.height) {
	    SET_BIT(pl->status, KILLED);
	    sprintf(msg, "%s left the known universe.", pl->name);
	    Set_message(msg);
	    sc = Rate(WALL_SCORE, pl->score);
	    SCORE(i, -sc,
		  OBJ_X_IN_BLOCKS(pl),
		  OBJ_Y_IN_BLOCKS(pl),
		  pl->name);
	    continue;
	}

	/* Player - player */
	if (BIT(World.rules->mode, CRASH_WITH_PLAYER | BOUNCE_WITH_PLAYER)) {
	    for (j=i+1; j<NumPlayers; j++) {
	      if (BIT(Players[j]->status, PLAYING|PAUSE|GAME_OVER|KILLED)
		  != PLAYING) {
		continue;
	      }
	      if (!in_range_acd(pl->prevpos.x, pl->prevpos.y, pl->pos.x, pl->pos.y, 
				Players[j]->prevpos.x, Players[j]->prevpos.y, 
				Players[j]->pos.x, Players[j]->pos.y, 
				2*SHIP_SZ-6)) {
		continue;
	      }
	      
	      /*
	       * Here we can add code to do more accurate player against
	       * player collision detection.
	       * A new algorithm could be based on the following idea:
	       *
	       * - If we can draw an uninterupted line between two players:
	       *   - Then test for both ships:
	       *     - For the three points which make up a ship:
	       *       - If we can draw a line between its previous
	       *         position and its current position which does not
	       *         cross the first line.
	       * Then the ships have not collided even though they may be
	       * very close to one another.
	       * The choosing of the first line may not be easy however.
	       */
	      
	      if (TEAM_IMMUNE(i, j)) {
		continue;
	      }
	      if (BIT(World.rules->mode, BOUNCE_WITH_PLAYER)) {
		if (BIT(pl->used, OBJ_SHIELD) != OBJ_SHIELD)
		  Add_fuel(&(pl->fuel), (long)ED_PL_CRASH);
		
		if (BIT(Players[j]->used, OBJ_SHIELD) != OBJ_SHIELD)
		  Add_fuel(&(Players[j]->fuel), (long)ED_PL_CRASH);
		
		Obj_repel((object_t *)pl, (object_t *)Players[j],
			  2*SHIP_SZ);
	      }
	      if (!BIT(World.rules->mode, CRASH_WITH_PLAYER)) {
		continue;
	      }

	      if (pl->fuel.sum <= 0
		  || (!BIT(pl->used, OBJ_SHIELD))) {
		SET_BIT(pl->status, KILLED);
	      }
	      if (Players[j]->fuel.sum <= 0
		  || (!BIT(Players[j]->used, OBJ_SHIELD))) {
		SET_BIT(Players[j]->status, KILLED);
	      }
	      
		if (BIT(Players[j]->status, KILLED)) {
		  if (BIT(pl->status, KILLED)) {
		    sprintf(msg, "%s and %s crashed.",
			    pl->name, Players[j]->name);
		    Set_message(msg);
#define crashScoreMult 0.33
		    sc = (int)floor(Rate(Players[j]->score, pl->score)
				    * crashScoreMult);
		    sc2 = (int)floor(Rate(pl->score, Players[j]->score)
				     * crashScoreMult);
		    Score_players(i, -sc, Players[j]->name,
				  j, -sc2, pl->name);
		  } else {
		    int i_tank_owner = i;
		    sprintf(msg, "%s ran over %s.",
			    pl->name, Players[j]->name);
		    Set_message(msg);
			sc = (int)floor(Rate(pl->score, Players[j]->score));
			Score_players(i_tank_owner, sc, Players[j]->name,
				      j, -sc, pl->name);
		  }
		  
		} else {
		  if (BIT(pl->status, KILLED)) {
		    int j_tank_owner = j;
		    sprintf(msg, "%s ran over %s.",
			    Players[j]->name, pl->name);
		    Set_message(msg);
		    sc = (int)floor(Rate(Players[j]->score, pl->score));
		    Score_players(j_tank_owner, sc, pl->name,
				  i, -sc, Players[j]->name);
		  }
		}
		
		if (BIT(Players[j]->status, KILLED)) {
		  if (IS_ROBOT_IND(j)
		      && Robot_war_on_player(j) == pl->id) {
		    Robot_reset_war(j);
		  }
		}
		
		if (BIT(pl->status, KILLED)) {
		  if (IS_ROBOT_PTR(pl)
		      && Robot_war_on_player(i) == Players[j]->id) {
		    Robot_reset_war(i);
		  }
		  /* cannot crash with more than one player at the same time? */
		  /* hmm, if 3 players meet at the same point at the same time? */
		  /* break; */
		}
	    }
	}
	
	/* Player picking up ball/treasure */
	if (!BIT(pl->used, OBJ_CONNECTOR)) {
	    pl->ball = NULL;
	}
	else if (pl->ball != NULL) {
	    object_t *ball = pl->ball;
	    if (ball->life <= 0 || ball->id != -1){
	      pl->ball = NULL;
	    }
	    else {
		DFLOAT distance = Wrap_length(pl->pos.x - ball->pos.x,
					     pl->pos.y - ball->pos.y);
		if (distance >= ballConnectorLength) {
		    ball->id = pl->id;
		    /* this is only the team of the owner of the ball,
		       not the team the ball belongs to. the latter is
		       found through the ball's treasure */
		    ball->team = pl->team;
		    if (ball->owner == -1)
			ball->life=LONG_MAX;  /* for frame counter */
		    ball->owner = pl->id;
		    ball->length = distance;
		    if (ball->treasure != -1)
			World.treasures[ball->treasure].have = false;
		    SET_BIT(pl->have, OBJ_BALL);
		    pl->ball = NULL;
		}
	    }
	} else {
	    /*
	     * We want a separate list of balls to avoid searching
	     * the object list for balls.
	     */
	    int dist, mindist = ballConnectorLength;
	    for (j = 0; j < NumObjs; j++) {
		if (BIT(Obj[j]->type, OBJ_BALL) && Obj[j]->id == -1) {
		    dist = Wrap_length(pl->pos.x - Obj[j]->pos.x,
				       pl->pos.y - Obj[j]->pos.y);
		    if (dist < mindist) {
			object_t *ball = Obj[j];
			int bteam = -1;

			if (ball->treasure != -1)
			    bteam = World.treasures[ball->treasure].team;

			/*
			 * The treasure's team cannot connect before
			 * somebody else has owned the ball.
			 * This was done to stop team members
			 * taking and hiding with the ball... this was
			 * considered bad gamesmanship.
			 */
			if (ball->owner != -1
			    || (   pl->team != TEAM_NOT_SET
				&& pl->team != bteam)) {
			    pl->ball = Obj[j];
			    mindist = dist;
			}
		    }
		}
	    }
	}

	PlayerObjectCollision(i);
    }
}

static void PlayerObjectCollision(int ind)
{
    int		j, killer, range, radius, sc, hit, obj_count;
    player_t	*pl = Players[ind];
    object_t	*obj, **obj_list;
    DFLOAT   	rel_velocity;

    /*
     * Collision between a player and an object.
     */
    if (BIT(pl->status, PLAYING|PAUSE|GAME_OVER|KILLED) != PLAYING)
	return;

    Cell_objects_get(OBJ_X_IN_BLOCKS(pl), OBJ_Y_IN_BLOCKS(pl), 4,
		     &obj_list, &obj_count);

    for (j=0; j<obj_count; j++) {
	obj = obj_list[j];
	if (obj->life <= 0)
	    continue;

	range = SHIP_SZ + obj->pl_range;

	if (!in_range_acd(pl->prevpos.x, pl->prevpos.y, pl->pos.x, pl->pos.y,
			  obj->prevpos.x, obj->prevpos.y, obj->pos.x, obj->pos.y,
			  range)) {
	  continue;
	}
    

	if (obj->id != -1) {
	  if (obj->id == pl->id) {
	    if (BIT(obj->type, OBJ_SPARK)
		&& BIT(obj->status, OWNERIMMUNE)) {
	      continue;
	    }
	  } else if (BIT(World.rules->mode, TEAM_PLAY)
		     && teamImmunity
		     && obj->team == pl->team) {
	    continue;
	  }
	}

	if (BIT(obj->type, OBJ_SHOT)) {
	    if (pl->id == obj->id && obj->life > obj->fuselife) {
		continue;
	    }
	}

	/*
	 * Objects actually only hit the player if they are really close.
	 */
	radius = SHIP_SZ + obj->pl_radius;
	if (radius >= range) {
	    hit = 1;
	} else {
	  hit = in_range_acd(pl->prevpos.x,pl->prevpos.y,pl->pos.x,pl->pos.y,
			     obj->prevpos.x,obj->prevpos.y,obj->pos.x,obj->pos.y,
			     range);
	}
	rel_velocity = LENGTH(pl->vel.x - obj->vel.x, pl->vel.y - obj->vel.y);

	/*
	 * Object collision.
	 */
	switch (obj->type) {
	case OBJ_BALL:
	    if (! hit)
		continue;

	    /*
	     * The ball is special, usually players bounce off of it with
	     * shields up, or die with shields down.  The treasure may
	     * be destroyed. 
	     * This was a bug; balls should be popped even with shields on -pgm
	     */
	    Obj_repel((object_t *)pl, obj, radius);
	    Add_fuel(&(pl->fuel), (long)ED_BALL_HIT);
	    if (treasureCollisionDestroys) {
	      obj->life = 0;
	    }
	    
	    if (pl->fuel.sum > 0) {
		if (!treasureCollisionMayKill || BIT(pl->used, OBJ_SHIELD))
		    continue;
	    }
	    if (obj->owner == -1) {
		sprintf(msg, "%s was killed by a ball.", pl->name);
		SCORE(ind, PTS_PR_PL_SHOT,
		      OBJ_X_IN_BLOCKS(pl),
		      OBJ_Y_IN_BLOCKS(pl),
		      "Ball");
	    } else {
		killer = GetInd[obj->owner];

		sprintf(msg, "%s was killed by a ball owned by %s.",
			pl->name, Players[killer]->name);

		if (killer == ind) {
		    strcat(msg, "  How strange!");
		    SCORE(ind, PTS_PR_PL_SHOT,
			  OBJ_X_IN_BLOCKS(pl),
			  OBJ_Y_IN_BLOCKS(pl),
			  Players[killer]->name);
		} else {
		    sc = (int)floor(Rate(Players[killer]->score, pl->score));
		    Score_players(killer, sc, pl->name,
				  ind, -sc, Players[killer]->name);
		}
	    }
	    Set_message(msg);
	    SET_BIT(pl->status, KILLED);
	    return;

	case OBJ_WRECKAGE:
	case OBJ_DEBRIS: {
		DFLOAT		v = VECTOR_LENGTH(obj->vel);
		long		tmp = (long) (2 * obj->mass * v);
		long		cost = ABS(tmp);

		if (BIT(pl->used, OBJ_SHIELD) != OBJ_SHIELD)
		    Add_fuel(&pl->fuel, - cost);
		if (pl->fuel.sum == 0
		    || (obj->type == OBJ_WRECKAGE
			&& wreckageCollisionMayKill
			&& !BIT(pl->used, OBJ_SHIELD))) {
		    SET_BIT(pl->status, KILLED);
		    sprintf(msg, "%s succumbed to an explosion.", pl->name);
		    killer = -1;
		    if (obj->id != -1) {
			killer = GetInd[obj->id];
			sprintf(msg + strlen(msg) - 1, " from %s.",
				Players[killer]->name);
			if (obj->id == pl->id) {
			    sprintf(msg + strlen(msg), "  How strange!");
			}
		    }
		    Set_message(msg);
		    if (killer == -1 || killer == ind) {
			SCORE(ind, PTS_PR_PL_SHOT,
			      OBJ_X_IN_BLOCKS(pl),
			      OBJ_Y_IN_BLOCKS(pl),
			      (killer == -1) ? "[Explosion]" : pl->name);
		    } else {
			sc = (int)floor(Rate(Players[killer]->score, pl->score));
			Score_players(killer, sc, pl->name,
				      ind, -sc, Players[killer]->name);
		    }
		    return;
		}
	    }
	    break;

	default:
	    break;
	}

	obj->life = 0;
	if (hit) Delta_mv((object_t *)pl, (object_t *)obj);

	if (!BIT(obj->type, KILLING_SHOTS))
	    continue;

	/*
	 * Player got hit by a potentially deadly object.
	 *
	 * When a player has shields up, and not enough fuel
	 * to `absorb' the shot then shields are lowered.
	 * This is not very logical, rather in this case
	 * the shot should be considered to be deadly too.
	 *
	 * Sound effects are missing when shot is deadly.
	 */

	if (BIT(pl->used, OBJ_SHIELD)) {

	    switch(obj->type) {
	    case OBJ_SHOT:
		if (BIT(pl->used, OBJ_SHIELD) != OBJ_SHIELD) {
		    Add_fuel(&(pl->fuel), (long)ED_SHOT_HIT);
		}
		break;

	    default:
		xpprintf("%s You were hit by what?\n", showtime());
		break;
	    }
	    if (pl->fuel.sum <= 0) {
		CLR_BIT(pl->used, OBJ_SHIELD);
	    }
	} else {
	    switch (obj->type) {
	    case OBJ_SHOT:

		if (obj->id == -1) {
		    sprintf(msg, "%s was killed by a shot.", pl->name);
		    SCORE(ind, PTS_PR_PL_SHOT,
			  OBJ_X_IN_BLOCKS(pl),
			  OBJ_Y_IN_BLOCKS(pl),
			  "N/A");
		    killer = ind;
		} else {
		    sprintf(msg, "%s was killed by a shot from %s.", pl->name,
			    Players[killer=GetInd[obj->id]]->name);
		    if (killer == ind) {
			strcat(msg, "  How strange!");
			SCORE(ind, PTS_PR_PL_SHOT,
			      OBJ_X_IN_BLOCKS(pl),
			      OBJ_Y_IN_BLOCKS(pl),
			      Players[killer]->name);
		    } else {
			Rank_add_kill(Players[killer]);
			sc = (int)floor(Rate(Players[killer]->score, pl->score));
			Score_players(killer, sc, pl->name,
				      ind, -sc, Players[killer]->name);
		    }
		}
		Set_message(msg);
		SET_BIT(pl->status, KILLED);
		Robot_war(ind, killer);
		return;

	    default:
		break;
	    }
	}
    }
}
