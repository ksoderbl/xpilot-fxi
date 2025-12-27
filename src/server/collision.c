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
#include "xpconfig.h"
#include "debug.h"
#include "serverconst.h"
#include "global.h"
#include "list.h"
#include "proto.h"
#include "map.h"
#include "score.h"
#include "item.h"
#include "netserver.h"
#include "pack.h"
#include "error.h"
#include "objpos.h"
#include "rank.h"
#include "frame.h"
#include "server.h"
#include "player_inline.h"
#include "robot.h"
#include "ship.h"
#include "collision.h"

char collision_version[] = VERSION;

/** Array containing last objects on lists storing objects belonging to block (x; y) */
static object_t ***Cells;

/** List of pointers to last objects in cells (type: object_t **) */
static TList UsedCells;

/*
 * The very first "analytical" collision patch, XPilot 3.6.2
 * Faster than other patches and accurate below half warp-speed
 * Trivial common subexpressions are eliminated by any reasonable compiler,
 * and kept here for readability.
 * Written by Pontus (Rakk, Kepler) pontus@ctrl-c.liu.se Jan 1998
 * Kudos to Svenske and Mad Gurka for beta testing, and Murx for
 * invaluable insights.
 *
 * index 1: prevpos, index 2: pos
 * p: first object, q: second object
 */

int32_t Collision_occured(int32_t p1x, int32_t p1y, int32_t p2x, int32_t p2y,
						  int32_t q1x, int32_t q1y, int32_t q2x, int32_t q2y, int32_t r)
{
	int32_t fac1, fac2; /* contraction between the distance between the x and y coordinates of objects */
	double tmin, fminx, fminy;
	int32_t top, bot;
	bool mpx, mpy, mqx, mqy;

	/*
	 * Get the wrapped coordinates straight
	 */
	if (BIT(World.rules->mode, WRAP_PLAY))
	{
		if ((mpx = (ABS(p2x - p1x) > World.width / 2)))
		{
			if (p1x > p2x)
				p1x -= World.width;
			else
				p2x -= World.width;
		}
		if ((mpy = (ABS(p2y - p1y) > World.height / 2)))
		{
			if (p1y > p2y)
				p1y -= World.height;
			else
				p2y -= World.height;
		}
		if ((mqx = (ABS(q2x - q1x) > World.width / 2)))
		{
			if (q1x > q2x)
				q1x -= World.width;
			else
				q2x -= World.width;
		}
		if ((mqy = (ABS(q2y - q1y) > World.height / 2)))
		{
			if (q1y > q2y)
				q1y -= World.height;
			else
				q2y -= World.height;
		}

		if (mpx && !mqx && (q2x > World.width / 2 || q1x > World.width / 2))
		{
			q1x -= World.width;
			q2x -= World.width;
		}

		if (mqy && !mpy && (q2y > World.height / 2 || q1y > World.height / 2))
		{
			q1y -= World.height;
			q2y -= World.height;
		}

		if (mqx && !mpx && (p2x > World.width / 2 || p1x > World.width / 2))
		{
			p1x -= World.width;
			p2x -= World.width;
		}

		if (mqy && !mpy && (p2y > World.height / 2 || p1y > World.height / 2))
		{
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
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/* tests */
#if 0
/*
 * Do the detection
 */
if ((p2x - q2x) * (p2x - q2x) + (p2y - q2y) * (p2y - q2y) < r * r)
	return 1;
fac1 = (q1x - p1x) - (q2x - p2x);
fac2 = (q1y - p1y) - (q2y - p2y);
top = -(fac1 * (q2x - p2x) + fac2 * (q2y - p2y));
bot = (fac1 * fac1 + fac2 * fac2);
if (top < 0 || bot < 1 || top > bot)
	return 0;
tmin = ((double) top) / ((double) bot);
fminx = (q2x - p2x) + fac1 * tmin;
fminy = (q2y - p2y) + fac2 * tmin;
if (fminx * fminx + fminy * fminy < r * r) {
	return 1;
}
else {
	return 0;
}
#endif

void Collision_cells_free(void)
{
	if (Cells)
	{
		free(Cells);
		Cells = NULL;
	}

	if (UsedCells)
	{
		LIST_DeleteNodesOnly(UsedCells);
	}
}

void Collision_cells_allocate(void)
{
	uint32_t size;
	object_t **objp;
	int32_t x, y;

	Collision_cells_free();

	size = sizeof(object_t ***) * World.x;
	size += sizeof(object_t **) * World.x * World.y;
	if (!(Cells = (object_t ***)malloc(size)))
	{
		error("No Cell mem");
		End_game();
	}

	objp = (object_t **)&Cells[World.x];
	for (x = 0; x < World.x; x++)
	{
		Cells[x] = objp;
		for (y = 0; y < World.y; y++)
		{
			*objp++ = NULL;
		}
	}

	UsedCells = LIST_New();
}

void Collision_cells_init(void)
{
	int32_t i, x, y;
	object_t *obj, **cell;

	/* Construct a linked list of objects in block (x; y)
	 * The tail of the list is stored in the \ref Cells array */
	for (i = 0; i < NumObjs; i++)
	{
		obj = Obj[i];

		if (Object_is_expired(obj))
		{
			continue;
		}

		x = OBJ_X_IN_BLOCKS(obj);
		y = OBJ_Y_IN_BLOCKS(obj);
		cell = &Cells[x][y];

		/* insert the object into the chain */
		obj->cell_prev = *cell;

		if (obj->cell_prev)
		{
			obj->cell_prev->cell_next = obj;
		}
		else
		{
			LIST_AddItemBottom(UsedCells, cell);
		}

		*cell = obj;
	}
}

void Collision_cells_cleanup(void)
{
	object_t *obj;
	object_t *obj_tmp;
	object_t **cell;

	/* Clear the structures */
	while (!LIST_IsEmpty(UsedCells))
	{
		cell = LIST_GetLastData(UsedCells);
		obj = *cell;

		if (obj)
		{
			while (obj->cell_prev)
			{
				obj_tmp = obj->cell_prev;
				obj->cell_prev->cell_next = NULL;
				obj->cell_prev = NULL;
				obj = obj_tmp;
			}
		}

		LIST_RemoveLastItem(UsedCells);
		*cell = NULL;
	}
}

void Collision_cells_print(void)
{
	int32_t i, j;
	object_t *obj;

	for (i = 0; i < World.x; i++)
	{
		for (j = 0; j < World.y; j++)
		{
			printf("[%d][%d] ", i, j);
			for (obj = Cells[i][j]; obj; obj = obj->cell_prev)
			{
				printf("%lx ", (unsigned long)obj);
			}
			printf("\n");
		}
	}
}

void Collision_cells_objects_get(int32_t x, int32_t y, int32_t r, object_t ***list, int32_t *count)
{
	static object_t *ObjectList[MAX_TOTAL_OBJECTS + 1];
	int32_t obj_count = 0;
	int32_t minx, maxx, miny, maxy, xr, yr, xw, yw;
	object_t *obj;

	if (BIT(World.rules->mode, WRAP_PLAY))
	{
		if (2 * r > World.x)
		{
			r = World.x / 2;
		}
		if (2 * r > World.y)
		{
			r = World.y / 2;
		}
	}
	else
	{
		if (r > World.x)
		{
			r = World.x;
		}
		if (r > World.y)
		{
			r = World.y;
		}
	}
	minx = x - r;
	maxx = x + r;
	miny = y - r;
	maxy = y + r;
	if (BIT(World.rules->mode, WRAP_PLAY))
	{
		if (minx < 0)
		{
			minx += World.x;
			maxx += World.x;
		}
		if (miny < 0)
		{
			miny += World.y;
			maxy += World.y;
		}
	}
	else
	{
		if (minx < 0)
		{
			minx = 0;
		}
		if (miny < 0)
		{
			miny = 0;
		}
		if (maxx >= World.x)
		{
			maxx = World.x - 1;
		}
		if (maxy >= World.y)
		{
			maxy = World.y - 1;
		}
	}

	for (xr = xw = minx; xr <= maxx; xr++, xw++)
	{
		if (xw >= World.x)
		{
			xw -= World.x;
		}
		for (yr = yw = miny; yr <= maxy; yr++, yw++)
		{
			if (yw >= World.y)
			{
				yw -= World.y;
			}

			for (obj = Cells[xw][yw]; obj; obj = obj->cell_prev)
			{
				/* Protection against overflow */
				if (obj_count >= MAX_TOTAL_OBJECTS)
				{
					xpprintf("%s WARNING: Overflow on the list of objects within range (count=%d).\n",
							 showtime(), obj_count);
					break;
				}

				ObjectList[obj_count++] = obj;
			}
		}
	}

	ObjectList[obj_count] = NULL;
	*list = &ObjectList[0];
	if (count != NULL)
	{
		*count = obj_count;
	}
}

/**
 * TODO: could implement collisions of more than 2 players at the same time, like it's done with objects
 */
bool Collision_player_player_check(player_t *pl, player_t *pl2)
{
	int32_t sc, sc2;

	if (!Collision_occured(pl->prevpos.x, pl->prevpos.y, pl->pos.x, pl->pos.y, pl2->prevpos.x,
						   pl2->prevpos.y, pl2->pos.x, pl2->pos.y, 2 * SHIP_SZ - 6))
	{
		return false;
	}

	/*
	 * Here we can add code to do more accurate player against
	 * player collision detection.
	 * A new algorithm could be based on the following idea:
	 * - If we can draw an uninterrupted line between two players
	 * - Then test for both ships
	 * - For the three points which make up a ship
	 * - If we can draw a line between its previous
	 * position and its current position which does not
	 * cross the first line.
	 * Then the ships have not collided even though they may be
	 * very close to one another.
	 * The choosing of the first line may not be easy however.
	 */

	if (TEAM_IMMUNE(pl, pl2))
	{
		return false;
	}

	if (BIT(World.rules->mode, BOUNCE_WITH_PLAYER))
	{
		if (!Player_uses_property(pl, USES_SHIELD))
		{
			Player_fuel_add(pl, (int32_t)ED_PL_CRASH);
		}

		if (!Player_uses_property(pl2, USES_SHIELD))
		{
			Player_fuel_add(pl, (int32_t)ED_PL_CRASH);
		}

		Ship_object_repel((object_t *)pl, (object_t *)pl2, 2 * SHIP_SZ);
	}

	if (!BIT(World.rules->mode, CRASH_WITH_PLAYER))
	{
		return false;
	}

	if (pl->fuel.sum <= 0 || (!Player_uses_property(pl, USES_SHIELD)))
	{
		Player_set_state(pl, PL_STATE_KILLED);
	}
	if (pl2->fuel.sum <= 0 || (!Player_uses_property(pl2, USES_SHIELD)))
	{
		Player_set_state(pl2, PL_STATE_KILLED);
	}

	if (Player_is_killed(pl2))
	{
		if (Player_is_killed(pl))
		{
			Message_game_print("%s and %s crashed.", pl->name, pl2->name);
			sc = (int32_t)floor(Rate(pl2->score, pl->score) * crashScoreMult);
			sc2 = (int32_t)floor(Rate(pl->score, pl2->score) * crashScoreMult);
			Score_players(pl, -sc, pl2->name, pl2, -sc2, pl->name);
		}
		else
		{
			Message_game_print("%s ran over %s.", pl->name, pl2->name);
			sc = (int32_t)floor(Rate(pl->score, pl2->score));
			Score_players(pl, sc, pl2->name, pl2, -sc, pl->name);
		}

		if (Player_is_robot(pl2) && Robot_war_on_player(pl2) == pl)
		{
			Robot_reset_war(pl2);
		}
	}
	else
	{
		if (Player_is_killed(pl))
		{
			Message_game_print("%s ran over %s.", pl2->name, pl->name);
			sc = (int32_t)floor(Rate(pl2->score, pl->score));
			Score_players(pl2, sc, pl->name, pl, -sc, pl2->name);
		}
	}

	if (Player_is_killed(pl))
	{
		if (Player_is_robot(pl) && Robot_war_on_player(pl) == pl2)
		{
			Robot_reset_war(pl);
		}
	}

	return true;
}

bool Collision_player_attach_ball(player_t *pl)
{
	int32_t j;

	/* Ball handling */
	if (!Player_uses_property(pl, USES_CONNECTOR))
	{
		/* Not picking a ball at the moment */
		pl->ball_tmp = NULL;
	}
	else if (pl->ball_tmp)
	{
		/* Picking a ball now */
		object_t *ball = pl->ball_tmp;

		if (Object_is_attached(ball))
		{
			pl->ball_tmp = NULL;
		}

		/* Calculate length of the connector, attach the ball if
		 * the connector is long enough.
		 */
		else
		{
			DFLOAT distance = Map_get_distance(&ball->pos, &pl->pos);
			if (distance >= ballConnectorLength)
			{
				Object_set_attached(ball, true);

				/*
				 * This is only the team of the owner of the ball,
				 * not the team the ball belongs to. the latter is
				 * found through the ball's treasure
				 */
				ball->team = pl->team;

				ball->owner = pl;
				ball->length = distance;

				if (ball->treasure && ball->treasure->have)
				{
					/*
					 * The counter must be reset when removing the ball from its box,
					 * because computation of ball run duration uses the start value.
					 */
					ball->obj_life = MAX_OBJECT_LIFE;
					ball->treasure->have = false;
				}

				Player_get_property(pl, HAS_BALL);

				/* Ball has been attached */
				pl->ball_tmp = NULL;

				return true;
			}
		}
	}
	else
	{
		/* Searching for a ball in close proximity we can attach to.
		 * TODO We want a separate list of balls to avoid searching
		 * the object list for balls.
		 */
		int32_t dist, mindist = ballConnectorLength;
		object_t *ball;

		for (j = 0; j < NumObjs; j++)
		{
			ball = Obj[j];

			if (Object_is_expired(ball))
			{
				continue;
			}

			if (Object_is_type(ball, OBJ_BALL) && !Object_is_attached(ball))
			{
				dist = Map_get_distance(&ball->pos, &pl->pos);
				if (dist < mindist)
				{
					/* We are close enough to start connecting to the ball */
					team_t *bteam = NULL;

					if (ball->treasure)
					{
						bteam = ball->treasure->team;
					}

					/*
					 * Do NOT attach a ball in these cases:
					 *  - it belongs to the player's team AND is in treasure box
					 *  - it is already attached by someone else
					 */
					if (!(((ball->treasure->team->Num == pl->team->Num) && // belongs to player's team
						   ball->treasure->have) ||						   // is in treasure box
						  Object_is_attached(ball)))
					{
						pl->ball_tmp = ball;
						mindist = dist;
					}
				}
			}
		}
	}

	return false;
}

void Collision_players_check(void)
{
	int32_t i, j;
	player_t *pl;

	/* Player - player, checkpoint, treasure, object and wall */
	for (i = 0; i < NumPlayers; i++)
	{
		pl = Players[i];

		if (!Player_is_alive(pl))
		{
			continue;
		}

		/* Player - player */
		if (BIT(World.rules->mode, CRASH_WITH_PLAYER | BOUNCE_WITH_PLAYER))
		{
			for (j = i + 1; j < NumPlayers; j++)
			{
				if (!Player_is_alive(Players[j]))
				{
					continue;
				}

				Collision_player_player_check(pl, Players[j]);

				if (Player_is_killed(pl))
				{
					break;
				}
			}

			if (Player_is_killed(pl))
			{
				continue;
			}
		}

		Collision_player_attach_ball(pl);
		Collision_player_object_check(pl);
	}
}

void Collision_player_object_check(player_t *pl)
{
	int32_t j, range, radius, sc, obj_count;
	bool is_within_hit_area;
	object_t *obj, **obj_list;
	player_t *killer;

	/*
	 * Collision between a player and an object.
	 */
	Collision_cells_objects_get(OBJ_X_IN_BLOCKS(pl), OBJ_Y_IN_BLOCKS(pl), 4, &obj_list, &obj_count);

	for (j = 0; j < obj_count; j++)
	{
		int32_t tmp_bitfield;

		obj = obj_list[j];

		/* Do not process expired objects */
		if (Object_is_expired(obj))
		{
			continue;
		}

		range = SHIP_SZ + obj->pl_range;

		if (!(is_within_hit_area = Collision_occured(pl->prevpos.x, pl->prevpos.y,
													 pl->pos.x, pl->pos.y,
													 obj->prevpos.x, obj->prevpos.y,
													 obj->pos.x, obj->pos.y,
													 range)))
		{
			continue;
		}

		/* Check immunity to effects of the collision */
		if (obj->owner)
		{
			if (obj->owner == pl)
			{
				if (Object_is_type(obj, OBJ_SPARK) && BIT(obj->obj_status, OWNERIMMUNE))
				{
					continue;
				}
			}
			else if (BIT(World.rules->mode, TEAM_PLAY) && teamImmunity && !(Object_is_type(obj, OBJ_BALL) && !Object_is_attached(obj)) && obj->team == pl->team)
			{
				continue;
			}
		}

		/* don't process collisions before the fuse time is expired */
		if (Object_is_type(obj, OBJ_SHOT))
		{
			if (pl == obj->owner && obj->obj_life > obj->fuselife)
			{
				continue;
			}
		}

		/*
		 * Objects actually only hit the player if they are really close.
		 */
		radius = SHIP_SZ + obj->pl_radius;
		if (radius >= range)
		{
			is_within_hit_area = true;
		}

		/*
		 * Object collision.
		 */
		switch (obj->type)
		{
		case OBJ_BALL:
			if (!is_within_hit_area)
			{
				continue;
			}

			/*
			 * The ball is special, usually players bounce off of it with
			 * shields up, or die with shields down.  The treasure may
			 * be destroyed.
			 * This was a bug; balls should be popped even with shields on -pgm
			 */
			Ship_object_repel((object_t *)pl, obj, radius);
			Player_fuel_add(pl, (int32_t)ED_BALL_HIT);
			if (treasureCollisionDestroys)
			{
				Object_expire(obj);
			}

			if (pl->fuel.sum > 0)
			{
				if (!treasureCollisionMayKill || Player_uses_property(pl, USES_SHIELD))
				{
					continue;
				}
			}

			if (!obj->owner)
			{
				Message_game_print("%s was killed by a ball.", pl->name);
				SCORE(pl, PTS_PR_PL_SHOT, &pl->pos, "Ball");
			}
			else
			{
				killer = obj->owner;

				if (killer == pl)
				{
					Message_game_print("%s was killed by a ball owned by %s.  How strange!",
									   pl->name, killer->name);
					SCORE(pl, PTS_PR_PL_SHOT, &pl->pos, killer->name);
					pl->self_deaths++;
				}
				else
				{
					Message_game_print("%s was killed by a ball owned by %s.", pl->name, killer->name);
					sc = (int32_t)floor(Rate(killer->score, pl->score));

					SCORE(killer, sc, &killer->pos, killer->name);
					SCORE(pl, -sc, &pl->pos, pl->name);
					Rank_add_kill(killer);
				}
			}

			Player_set_state(pl, PL_STATE_KILLED);
			return;

		case OBJ_WRECKAGE:
		case OBJ_DEBRIS:
		{
			DFLOAT v = VECTOR_LENGTH(obj->vel);
			int32_t tmp = (int32_t)(2 * obj->mass * v);
			int32_t cost = ABS(tmp);

			if (!Player_uses_property(pl, USES_SHIELD))
			{
				Player_fuel_add(pl, -cost);
			}
			if (pl->fuel.sum == 0 || (Object_is_type(obj, OBJ_WRECKAGE) && wreckageCollisionMayKill && !Player_uses_property(pl, USES_SHIELD)))
			{
				Player_set_state(pl, PL_STATE_KILLED);
				killer = NULL;
				if (obj->owner)
				{
					killer = obj->owner;

					if (obj->owner == pl)
					{
						Message_game_print("%s succumbed to an explosion from %s.  How strange!",
										   pl->name, killer->name);
						pl->self_deaths++;
					}
					else
					{
						Message_game_print("%s succumbed to an explosion from %s.",
										   pl->name, killer->name);
					}
				}
				else
				{
					Message_game_print("%s succumbed to an explosion.", pl->name);
				}

				if (!killer || killer == pl)
				{
					SCORE(pl, PTS_PR_PL_SHOT, &pl->pos,
						  (killer == NULL) ? "[Explosion]" : ((const char *)(pl->name)));
				}
				else
				{
					sc = (int32_t)floor(Rate(killer->score, pl->score));
					Score_players(killer, sc, pl->name, pl, -sc, killer->name);
					Rank_add_kill(killer);
				}
				return;
			}

			break;
		}

		default:
			break;
		}

		/* Time out the object which collided with player */
		Object_expire(obj);

		if (is_within_hit_area)
		{
			Delta_mv((object_t *)pl, (object_t *)obj);
		}

		/* TODO: ???? */
		tmp_bitfield = OBJ_TYPEBIT(obj->type);
		if (!BIT(tmp_bitfield, KILLING_SHOTS))
		{
			continue;
		}

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

		if (Player_uses_property(pl, USES_SHIELD))
		{
			switch (obj->type)
			{
			case OBJ_SHOT:
				if (!Player_uses_property(pl, USES_SHIELD))
				{
					Player_fuel_add(pl, (int32_t)ED_SHOT_HIT);
				}
				break;

			default:
				xpprintf("%s You were hit by what?\n", showtime());
				break;
			}
			if (pl->fuel.sum <= 0)
			{
				Player_disable_property(pl, USES_SHIELD);
			}
		}
		else
		{
			switch (obj->type)
			{
			case OBJ_SHOT:
				if (!obj->owner)
				{
					Message_game_print("%s was killed by a shot.", pl->name);
					SCORE(pl, PTS_PR_PL_SHOT, &pl->pos, "N/A");
				}
				else
				{
					killer = obj->owner;

					if (killer == pl)
					{
						Message_game_print("%s was killed by a shot from %s.  How strange!",
										   pl->name, killer->name);
						SCORE(pl, PTS_PR_PL_SHOT, &pl->pos, killer->name);
						pl->self_deaths++;
					}
					else
					{
						Message_game_print("%s was killed by a shot from %s.", pl->name, killer->name);
						Rank_add_kill(killer);
						sc = (int32_t)floor(Rate(killer->score, pl->score));

						SCORE(killer, sc, &killer->pos, pl->name);
						SCORE(pl, -sc, &pl->pos, killer->name);
					}
				}

				Player_set_state(pl, PL_STATE_KILLED);
				Robot_war(pl, obj->owner);
				return;

			default:
				break;
			}
		}
	}
}
