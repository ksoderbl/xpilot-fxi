/* $Id: collision.c,v 1.19 2009/04/13 12:50:42 rotunda_pk Exp $
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

#define SERVER
#include "version.h"
//#include "commonproto.h"
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

int8_t collision_version[] = VERSION;

/*
 * Globals
 */
static int8_t msg[MSG_LEN];

static object_t ***Cells;
static object_t **CellsUsed[MAX_TOTAL_SHOTS];
static int32_t cells_used_count;

int32_t Rate(int32_t winner, int32_t looser);
void PlayerCollision(void);
static void PlayerObjectCollision(player_t *pl);
inline int32_t in_range_acd(int32_t p1x, int32_t p1y, int32_t p2x, int32_t p2y, int32_t q1x, int32_t q1y,
		int32_t q2x, int32_t q2y, int32_t r);


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

inline int32_t in_range_acd(int32_t p1x, int32_t p1y, int32_t p2x, int32_t p2y, int32_t q1x, int32_t q1y,
		int32_t q2x, int32_t q2y, int32_t r)
{
	int32_t fac1, fac2;
	double tmin, fminx, fminy;
	int32_t top, bot;
	bool mpx, mpy, mqx, mqy;

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

		if (mpx && !mqx && (q2x > World.width / 2 || q1x > World.width
				/ 2)) {
			q1x -= World.width;
			q2x -= World.width;
		}

		if (mqy && !mpy && (q2y > World.height / 2 || q1y
				> World.height / 2)) {
			q1y -= World.height;
			q2y -= World.height;
		}

		if (mqx && !mpx && (p2x > World.width / 2 || p1x > World.width
				/ 2)) {
			p1x -= World.width;
			p2x -= World.width;
		}

		if (mqy && !mpy && (p2y > World.height / 2 || p1y
				> World.height / 2)) {
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
	tmin = ((double) top) / ((double) bot);
	fminx = -p2x + q2x + fac1 * tmin;
	fminy = -p2y + q2y + fac2 * tmin;
	if (fminx * fminx + fminy * fminy < r * r)
		return 1;
	else
		return 0;
}

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
	uint32_t size;
	object_t **objp;
	int32_t x, y;

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
	int32_t i, x, y;
	object_t *obj, **cell;

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

static void Cell_objects_get(int32_t x, int32_t y, int32_t r, object_t ***list, int32_t *count)
{
	static object_t *ObjectList[MAX_TOTAL_SHOTS + 1];
	int32_t i, minx, maxx, miny, maxy, xr, yr, xw, yw;
	object_t *obj;

	if (BIT(World.rules->mode, WRAP_PLAY)) {
		if (2 * r > World.x) {
			r = World.x / 2;
		}
		if (2 * r > World.y) {
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

void SCORE(player_t *pl, int32_t points, int32_t x, int32_t y, const int8_t *msg)
{
	pl->score += (points);

	Rank_add_score(pl, points);
	if (Player_is_connected(pl))
		Send_score_object(pl->connp, points, x, y, msg);
	updateScores = true;
}

int32_t Rate(int32_t winner, int32_t loser)
{
	int32_t t;

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
static void Score_players(player_t *winner, int32_t winner_score, int8_t *winner_msg,
		player_t *loser, int32_t loser_score, int8_t *loser_msg)
{
	if (TEAM(winner, loser)) {
		if (winner_score > 0)
			winner_score = -winner_score;
		if (loser_score > 0)
			loser_score = -loser_score;
	}
	SCORE(winner, winner_score, OBJ_X_IN_BLOCKS(winner), OBJ_Y_IN_BLOCKS(winner), winner_msg);
	SCORE(loser, loser_score, OBJ_X_IN_BLOCKS(loser), OBJ_Y_IN_BLOCKS(loser), loser_msg);
}

void Check_collision(void)
{
	Cell_objects_init();
	PlayerCollision();
}

void PlayerCollision(void)
{
	int32_t i, j, sc, sc2;
	player_t *pl;

	/* Player - player, checkpoint, treasure, object and wall */
	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		if (BIT(pl->status, PLAYING | PAUSE | GAME_OVER | KILLED)
				!= PLAYING)
			continue;

		if (pl->pos.x < 0 || pl->pos.y < 0 || pl->pos.x >= World.width
				|| pl->pos.y >= World.height) {
			SET_BIT(pl->status, KILLED);
			sprintf(msg, "%s left the known universe.", pl->name);
			Set_message(msg);
			sc = Rate(WALL_SCORE, pl->score);
			SCORE(pl, -sc, OBJ_X_IN_BLOCKS(pl), OBJ_Y_IN_BLOCKS(pl), pl->name);
			continue;
		}

		/* Player - player */
		if (BIT(World.rules->mode, CRASH_WITH_PLAYER
				| BOUNCE_WITH_PLAYER)) {
			player_t *pl2;

			for (j = i + 1; j < NumPlayers; j++) {
				pl2 = Players[j];

				if (BIT(pl2->status, PLAYING | PAUSE
						| GAME_OVER | KILLED)
						!= PLAYING) {
					continue;
				}
				if (!in_range_acd(pl->prevpos.x, pl->prevpos.y,
						pl->pos.x, pl->pos.y,
						pl2->prevpos.x,
						pl2->prevpos.y,
						pl2->pos.x,
						pl2->pos.y, 2 * SHIP_SZ
								- 6)) {
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

				if (TEAM_IMMUNE(pl, pl2)) {
					continue;
				}
				if (BIT(World.rules->mode, BOUNCE_WITH_PLAYER)) {
					if (BIT(pl->used, OBJ_SHIELD)
							!= OBJ_SHIELD)
						Add_fuel(
								&(pl->fuel),
								(int32_t) ED_PL_CRASH);

					if (BIT(pl2->used, OBJ_SHIELD)
							!= OBJ_SHIELD)
						Add_fuel(
								&(pl2->fuel),
								(int32_t) ED_PL_CRASH);

					Obj_repel(
							(object_t *) pl,
							(object_t *) pl2,
							2 * SHIP_SZ);
				}
				if (!BIT(World.rules->mode, CRASH_WITH_PLAYER)) {
					continue;
				}

				if (pl->fuel.sum <= 0 || (!BIT(pl->used,
						OBJ_SHIELD))) {
					SET_BIT(pl->status, KILLED);
				}
				if (pl2->fuel.sum <= 0 || (!BIT(
						pl2->used, OBJ_SHIELD))) {
					SET_BIT(pl2->status, KILLED);
				}

				if (BIT(pl2->status, KILLED)) {
					if (BIT(pl->status, KILLED)) {
						sprintf(
								msg,
								"%s and %s crashed.",
								pl->name,
								pl2->name);
						Set_message(msg);
#define crashScoreMult 0.33
						sc
								= (int32_t) floor(
										Rate(
												pl2->score,
												pl->score)
												* crashScoreMult);
						sc2
								= (int32_t) floor(
										Rate(
												pl->score,
												pl2->score)
												* crashScoreMult);
						Score_players(
								pl,
								-sc,
								pl2->name,
								pl2, -sc2,
								pl->name);
					}
					else {
						player_t *tank_owner = pl;
						sprintf(
								msg,
								"%s ran over %s.",
								pl->name,
								pl2->name);
						Set_message(msg);
						sc
								= (int32_t) floor(
										Rate(
												pl->score,
												pl2->score));
						Score_players(
								tank_owner,
								sc,
								pl2->name,
								pl2, -sc,
								pl->name);
					}

				}
				else {
					if (BIT(pl->status, KILLED)) {
						sprintf(
								msg,
								"%s ran over %s.",
								pl2->name,
								pl->name);
						Set_message(msg);
						sc
								= (int32_t) floor(
										Rate(
												pl2->score,
												pl->score));
						Score_players(
								pl2,
								sc,
								pl->name,
								pl,
								-sc,
								pl2->name);
					}
				}

				if (BIT(pl2->status, KILLED)) {
					if (Player_is_robot(pl2)
							&& Robot_war_on_player(
									pl2)
									== pl) {
						Robot_reset_war(pl2);
					}
				}

				if (BIT(pl->status, KILLED)) {
					if (Player_is_robot(pl)
							&& Robot_war_on_player(
									pl)
									== pl2) {
						Robot_reset_war(pl);
					}
					/* cannot crash with more than one player at the same time? */
					/* hmm, if 3 players meet at the same point at the same time? */
					/* break; */
				}
			}
		}

		/* Ball handling */
		if (!BIT(pl->used, OBJ_CONNECTOR)) {
			/* Not picking a ball at the moment */
			pl->ball_tmp = NULL;
		}
		else if (pl->ball_tmp) {
			/* Picking a ball now */
			object_t *ball = pl->ball_tmp;
			if (ball->life <= 0 || BIT(ball->status, IS_ATTACHED)) {
				pl->ball_tmp = NULL;
			}

			/* Calculate length of the connector, attach the ball if
			 * the connector is long enough.
			 */
			else {
				DFLOAT distance = Wrap_length(pl->pos.x
						- ball->pos.x, pl->pos.y
						- ball->pos.y);
				if (distance >= ballConnectorLength) {
					SET_BIT(ball->status, IS_ATTACHED);

					/* this is only the team of the owner of the ball,
					 not the team the ball belongs to. the latter is
					 found through the ball's treasure */
					ball->team = pl->team;

					// TODO: test
//					if (!ball->owner) {
//						ball->life = LONG_MAX; /* for frame counter */
//					}
					ball->owner = pl;
					ball->length = distance;
					if (ball->treasure) {
						ball->treasure->have = false;
					}
					SET_BIT(pl->have, OBJ_BALL);

					/* Ball has been attached */
					pl->ball_tmp = NULL;
				}
			}
		}
		else {
			/* Searching for a ball in close proximity we can attach to.
			 * We want a separate list of balls to avoid searching
			 * the object list for balls.
			 */
			int32_t dist, mindist = ballConnectorLength;
			object_t *ball;
			for (j = 0; j < NumObjs; j++) {
				ball = Obj[j];

				if (BIT(ball->type, OBJ_BALL) && !BIT(ball->status, IS_ATTACHED)) {
					dist
							= Wrap_length(
									pl->pos.x
											- ball->pos.x,
									pl->pos.y
											- ball->pos.y);
					if (dist < mindist) {
						/* We are close enough to start connecting to the ball */
						//object_t *ball = obj;
						team_t *bteam = NULL;

						if (ball->treasure) {
							bteam = ball->treasure->team;
						}

                                                /*
                                                 * Do NOT attach a ball in these cases:
                                                 *  - it belongs to the player's team AND is in treasure box
                                                 *  - it is already attached by someone else
                                                 */
                                                if (!(((ball->treasure->team->Num == pl->team->Num) && // belongs to player's team
                                                                ball->treasure->have) || // is in treasure box
                                                            BIT(ball->status, IS_ATTACHED)))
						{
							pl->ball_tmp = ball;
							mindist = dist;
						}
					}
				}
			}
		}

		PlayerObjectCollision(pl);
	}
}

static void PlayerObjectCollision(player_t *pl)
{
	int32_t j, range, radius, sc, obj_count;
	bool is_within_hit_area;
	object_t *obj, **obj_list;
	player_t *killer;
	DFLOAT rel_velocity;

	/*
	 * Collision between a player and an object.
	 */
	if (BIT(pl->status, PLAYING | PAUSE | GAME_OVER | KILLED) != PLAYING)
		return;

	Cell_objects_get(OBJ_X_IN_BLOCKS(pl), OBJ_Y_IN_BLOCKS(pl), 4, &obj_list, &obj_count);

	for (j = 0; j < obj_count; j++) {
		obj = obj_list[j];

		/* Do not process expired objects */
		if (obj->life <= 0)
			continue;

		range = SHIP_SZ + obj->pl_range;

		if (!in_range_acd(pl->prevpos.x, pl->prevpos.y, pl->pos.x,
				pl->pos.y, obj->prevpos.x, obj->prevpos.y,
				obj->pos.x, obj->pos.y, range)) {
			continue;
		}

		/* Check immunity to effects of the collision */
		if (obj->owner) {
			if (obj->owner == pl) {
				if (BIT(obj->type, OBJ_SPARK) && BIT(
						obj->status, OWNERIMMUNE)) {
					continue;
				}
			}
			else if (BIT(World.rules->mode, TEAM_PLAY)
					&& teamImmunity
                                        && !(BIT(obj->type, OBJ_BALL) && !BIT(obj->status, IS_ATTACHED))
					&& obj->team == pl->team) {
				continue;
			}
		}

		if (BIT(obj->type, OBJ_SHOT)) {
			if (pl == obj->owner && obj->life > obj->fuselife) {
				continue;
			}
		}

		/*
		 * Objects actually only hit the player if they are really close.
		 */
		radius = SHIP_SZ + obj->pl_radius;
		if (radius >= range) {
			is_within_hit_area = true;
		}
		else {
			is_within_hit_area = in_range_acd(pl->prevpos.x, pl->prevpos.y,
					pl->pos.x, pl->pos.y, obj->prevpos.x,
					obj->prevpos.y, obj->pos.x, obj->pos.y,
					range);
		}
		rel_velocity = LENGTH(pl->vel.x - obj->vel.x, pl->vel.y
				- obj->vel.y);

		/*
		 * Object collision.
		 */
		switch (obj->type) {
		case OBJ_BALL:
			if (!is_within_hit_area)
				continue;

			/*
			 * The ball is special, usually players bounce off of it with
			 * shields up, or die with shields down.  The treasure may
			 * be destroyed.
			 * This was a bug; balls should be popped even with shields on -pgm
			 */
			Obj_repel((object_t *) pl, obj, radius);
			Add_fuel(&(pl->fuel), (int32_t) ED_BALL_HIT);
			if (treasureCollisionDestroys) {
				obj->life = 0;
			}

			if (pl->fuel.sum > 0) {
				if (!treasureCollisionMayKill || BIT(pl->used,
						OBJ_SHIELD))
					continue;
			}
			if (!obj->owner) {
				sprintf(msg, "%s was killed by a ball.",
						pl->name);
				SCORE(pl, PTS_PR_PL_SHOT, OBJ_X_IN_BLOCKS(pl), OBJ_Y_IN_BLOCKS(pl), "Ball");
			}
			else {
				killer = obj->owner;

				sprintf(
						msg,
						"%s was killed by a ball owned by %s.",
						pl->name, killer->name);

				if (killer == pl) {
					strcat(msg, "  How strange!");
					SCORE(pl, PTS_PR_PL_SHOT,
							OBJ_X_IN_BLOCKS(pl),
							OBJ_Y_IN_BLOCKS(pl),
							killer->name);
				}
				else {
					sc = (int32_t) floor(Rate(
							killer->score,
							pl->score));
					Score_players(killer, sc, pl->name,
							pl, -sc,
							killer->name);
				}
			}
			Set_message(msg);
			SET_BIT(pl->status, KILLED);
			return;

		case OBJ_WRECKAGE:
		case OBJ_DEBRIS:
		{
			DFLOAT v = VECTOR_LENGTH(obj->vel);
			int32_t tmp = (int32_t) (2 * obj->mass * v);
			int32_t cost = ABS(tmp);

			if (BIT(pl->used, OBJ_SHIELD) != OBJ_SHIELD)
				Add_fuel(&pl->fuel, -cost);
			if (pl->fuel.sum == 0 || (obj->type == OBJ_WRECKAGE
					&& wreckageCollisionMayKill && !BIT(
					pl->used, OBJ_SHIELD))) {
				SET_BIT(pl->status, KILLED);
				sprintf(msg, "%s succumbed to an explosion.",
						pl->name);
				killer = NULL;
				if (obj->owner) {
					killer = obj->owner;
					sprintf(msg + strlen(msg) - 1,
							" from %s.",
							killer->name);
					if (obj->owner == pl) {
						sprintf(msg + strlen(msg),
								"  How strange!");
					}
				}
				Set_message(msg);
				if (!killer || killer == pl) {
					SCORE(
							pl,
							PTS_PR_PL_SHOT,
							OBJ_X_IN_BLOCKS(pl),
							OBJ_Y_IN_BLOCKS(pl),
							(killer == NULL) ? "[Explosion]"
									: ((const char *)(pl->name)));
				}
				else {
					sc = (int32_t) floor(Rate(
							killer->score,
							pl->score));
					Score_players(killer, sc, pl->name,
							pl, -sc,
							killer->name);
				}
				return;
			}

			break;
		}

		default:
			break;
		}

		/* Time out the object which collided with player */
		obj->life = 0;

		if (is_within_hit_area)
			Delta_mv((object_t *) pl, (object_t *) obj);

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
			switch (obj->type) {
			case OBJ_SHOT:
				if (BIT(pl->used, OBJ_SHIELD) != OBJ_SHIELD) {
					Add_fuel(&(pl->fuel),
							(int32_t) ED_SHOT_HIT);
				}
				break;

			default:
				xpprintf("%s You were hit by what?\n",
						showtime());
				break;
			}
			if (pl->fuel.sum <= 0) {
				CLR_BIT(pl->used, OBJ_SHIELD);
			}
		}
		else {
			switch (obj->type) {
			case OBJ_SHOT:
				if (!obj->owner) {
					sprintf(
							msg,
							"%s was killed by a shot.",
							pl->name);
					SCORE(pl, PTS_PR_PL_SHOT,
							OBJ_X_IN_BLOCKS(pl),
							OBJ_Y_IN_BLOCKS(pl),
							"N/A");
					killer = pl;
				}
				else {
					killer = obj->owner;
					sprintf(
							msg,
							"%s was killed by a shot from %s.",
							pl->name,
							killer->name);
					if (killer == pl) {
						strcat(msg, "  How strange!");
						SCORE(
								pl,
								PTS_PR_PL_SHOT,
								OBJ_X_IN_BLOCKS(pl),
								OBJ_Y_IN_BLOCKS(pl),
								killer->name);
					}
					else {
						Rank_add_kill(killer);
						sc
								= (int32_t) floor(
										Rate(
												killer->score,
												pl->score));
						Score_players(
								killer,
								sc,
								pl->name,
								pl,
								-sc,
								killer->name);
					}
				}
				Set_message(msg);
				SET_BIT(pl->status, KILLED);
				Robot_war(pl, killer);
				return;

			default:
				break;
			}
		}
	}
}
