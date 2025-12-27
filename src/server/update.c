/* $Id: update.c,v 1.19 2008/09/02 19:08:52 rotunda_pk Exp $
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
#include <stdio.h>

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "score.h"
#include "bit.h"
#include "objpos.h"


int8_t update_version[] = VERSION;

#define update_object_speed(o_)						\
    {									\
	(o_)->vel.x += (o_)->acc.x;					\
	(o_)->vel.y += (o_)->acc.y;					\
    }

#define	UPDATE_SCORE_DELAY  (intGameSpeed)

static int8_t msg[MSG_LEN];
extern int32_t frame_cycle;
void update_player_turn(player_t *pl);
void update_player_thrust(player_t *pl);

static void Transport_to_home(player_t *pl)
{
	/*
	 * Transport a corpse from the place where it died back to its homebase,
	 * or if in race mode, back to the last passed check point.
	 *
	 * During the first part of the distance we give it a positive constant
	 * acceleration G, during the second part we make this a negative one -G.
	 * This results in a visually pleasing take off and landing.
	 */
	// TODO: check this
	//DFLOAT bx, by, dx, dy, t, m;
	DFLOAT t, m;
	int32_t bx, by, dx, dy;
	const int32_t T = RECOVERY_DELAY;

	bx = (pl->home_base->pos.x + 0.5) * BLOCK_SZ;
	by = (pl->home_base->pos.y + 0.5) * BLOCK_SZ;
	dx = WRAP_DX(bx - pl->pos.x);
	dy = WRAP_DY(by - pl->pos.y);
	t = pl->count + 0.5f;
	if (2 * t <= T) {
		m = 2 / t;
	}
	else {
		t = T - t;
		m = (4 * t) / (T * T - 2 * t * t);
	}
	pl->vel.x = dx * m;
	pl->vel.y = dy * m;
}

/********** **********
 * Updating objects and the like.
 */

void Init_interpolation_data(void)
{
	int32_t i;
	player_t *pl;
	object_t *obj;

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		Player_position_set_clicks_interpolation(pl, pl->pos.cx,
				pl->pos.cy);
		pl->vel_interp.x = pl->vel.x;
		pl->vel_interp.y = pl->vel.y;
	}

	for (i = 0; i < NumObjs; i++) {
		obj = Obj[i];
		Object_position_set_clicks_interpolation(obj, obj->pos.cx,
				obj->pos.cy);
		obj->vel_interp.x = obj->vel.x;
		obj->vel_interp.y = obj->vel.y;
	}
}

void Update_objects_interpolation(void)
{
	int32_t i;
	player_t *pl;
	object_t *obj;
	int32_t player_fps;

	for (i = 0; i < NumObjs; i++) {
		obj = Obj[i];
		if (BIT(obj->type, OBJ_BALL)) {
			if (BIT(obj->status, IS_ATTACHED)) {
				Move_ball_interpolation(obj);
			}
		}
		Move_object_interpolation(obj);
	}

	/* printf("interp:%d %d\n", main_loops, frame_loops); */

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		player_fps = fps;
		player_fps = MIN(player_fps, pl->player_fps);

		if (player_fps < fps) {
			int32_t divisor = (fps - 1) / player_fps + 1;
			if (frame_loops % divisor)
				continue;
		}

		/* update turn for player also in interpolated frame */
		/* printf("interpolation: %d %d %d\n", main_loops, frame_loops, frame_cycle); */
		/* fflush(stdout); */
		update_player_turn(pl);

	}

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		if (!BIT(pl->status, PAUSE)) {
			Move_player_interpolation(pl);
		}
	}
}

void Update_objects(void)
{
	int32_t i;
	player_t *pl;
	object_t *obj;

	/*
	 * Update robots.
	 */

	if (frame_cycle == 0)
		Robot_update();

	/*
	 * Autorepeat fire, must unfortunately be done here, not in
	 * the player loop below, because of collisions between the shots
	 * and the auto-firing player that would otherwise occur.
	 */
	if (fireRepeatRate > 0) {
		for (i = 0; i < NumPlayers; i++) {
			pl = Players[i];
			if (BIT(pl->used, OBJ_SHOT)) {
				Fire_normal_shots(pl);
			}
		}
	}

	/*
	 * Let the fuel stations regenerate some fuel.
	 */
	if (NumPlayers > 0) {
		int32_t fuel = (int32_t) (NumPlayers * STATION_REGENERATION);
		int32_t frames_per_update = MAX_STATION_FUEL / (fuel * BLOCK_SZ);
		for (i = 0; i < World.NumFuels; i++) {
			if (World.fuel[i].fuel == MAX_STATION_FUEL) {
				continue;
			}
			if ((World.fuel[i].fuel += fuel) >= MAX_STATION_FUEL) {
				World.fuel[i].fuel = MAX_STATION_FUEL;
			}
			else if (World.fuel[i].last_change + frames_per_update
					> frame_loops) {
				/*
				 * We don't send fuelstation info to the clients every frame
				 * if it wouldn't change their display.
				 */
				continue;
			}
			World.fuel[i].conn_mask = 0;
			World.fuel[i].last_change = frame_loops;
		}
	}

	/*
	 * Update shots.
	 */
	for (i = 0; i < NumObjs; i++) {
		obj = Obj[i];

		if (BIT(obj->type, OBJ_BALL)) {
			if (BIT(obj->status, IS_ATTACHED)) {
				Move_ball(obj);
			}
		}

		else if (BIT(obj->type, OBJ_WRECKAGE)) {
			obj->rotation = (obj->rotation + (int32_t) (obj->turnspeed
					* RES)) % RES;
		}

		update_object_speed(obj);
		Move_object(obj);
	}

	/* * * * * *
	 *
	 * Player loop. Computes miscellaneous updates.
	 *
	 */
	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		/* Limits. */
		LIMIT(pl->power, MIN_PLAYER_POWER, MAX_PLAYER_POWER);
		LIMIT(pl->turnspeed, MIN_PLAYER_TURNSPEED, MAX_PLAYER_TURNSPEED);
		LIMIT(pl->turnresistance, MIN_PLAYER_TURNRESISTANCE,
				MAX_PLAYER_TURNRESISTANCE);

		if (pl->count > 0) {
			pl->count--;
			if (!BIT(pl->status, PLAYING)) {
				Transport_to_home(pl);
				Move_player(pl);
				continue;
			}
		}

		if (pl->count == 0) {
			pl->count = -1;

			if (!BIT(pl->status, PLAYING)) {
				SET_BIT(pl->status, PLAYING);
				Go_home(pl);
			}
			if (BIT(pl->status, SELF_DESTRUCT)) {
				SET_BIT(pl->status, KILLED);
				sprintf(msg, "%s has comitted suicide.",
						pl->name);
				Set_message(msg);
				Kill_player(pl, true);
				updateScores = true;
			}
		}

		if (BIT(pl->status, PLAYING | GAME_OVER | PAUSE) != PLAYING)
			continue;

		if (pl->shield_time > 0) {
			if (--pl->shield_time == 0) {
				CLR_BIT(pl->used, OBJ_SHIELD);
			}
			if (BIT(pl->used, OBJ_SHIELD) == 0) {
				/* BG 95/06/03: change test on "have" to "used". */
				CLR_BIT(pl->have, OBJ_SHIELD);
				pl->shield_time = 0;
			}
		}

		/* compute turn updates */
		/* printf("real: %d %d %d\n", main_loops, frame_loops, frame_cycle);*/
		/* fflush(stdout); */

		update_player_turn(pl);

		/*
		 * Compute energy drainage
		 */
		if (BIT(pl->used, OBJ_SHIELD))
			Add_fuel(&(pl->fuel), (int32_t) ED_SHIELD);

		if (BIT(pl->used, OBJ_REFUEL)) {
			if ((Wrap_length(
					pl->pos.x
							- pl->fs->pix_pos.x,
					pl->pos.y
							- pl->fs->pix_pos.y)
					> 90.0) || (pl->fuel.sum
					>= pl->fuel.max)
					|| (World.block[pl->fs->blk_pos.x][pl->fs->blk_pos.y]
							!= FUEL) || (BIT(
					World.rules->mode, TEAM_PLAY)
					&& teamFuel && pl->fs->team
					!= pl->team)) {
				CLR_BIT(pl->used, OBJ_REFUEL);
			}
			else {
				int32_t i = pl->fuel.num_tanks;
				int32_t ct = pl->fuel.current;

				do {
					if (pl->fs->fuel > REFUEL_RATE) {
						pl->fs->fuel -= REFUEL_RATE;
						pl->fs->conn_mask = 0;
						pl->fs->last_change = frame_loops;
						Add_fuel(&(pl->fuel),
								REFUEL_RATE);
					}
					else {
						Add_fuel(
								&(pl->fuel),
								pl->fs->fuel);
						pl->fs->fuel = 0;
						pl->fs->conn_mask = 0;
						pl->fs->last_change
								= frame_loops;
						CLR_BIT(pl->used, OBJ_REFUEL);
						break;
					}
					if (pl->fuel.current
							== pl->fuel.num_tanks)
						pl->fuel.current = 0;
					else
						pl->fuel.current += 1;
				} while (i--);
				pl->fuel.current = ct;
			}
		}

		if (pl->fuel.sum <= 0) {
			CLR_BIT(pl->used, OBJ_SHIELD);
			CLR_BIT(pl->status, THRUSTING);
		}
		if (pl->fuel.sum > (pl->fuel.max - REFUEL_RATE))
			CLR_BIT(pl->used, OBJ_REFUEL);

		update_player_thrust(pl);

		if (!BIT(pl->status, PAUSE)) {
			update_object_speed(pl); /* New position */
			Move_player(pl);
		}

		if (BIT(pl->status, THRUSTING))
			Thrust(pl);

		pl->used &= pl->have;
	}

	for (i = 0; i < NumPlayers; i++) {
		player_t *pl = Players[i];
		if (BIT(pl->lock.flags, LOCK_PLAYER)) {
			pl->lock.distance
					= Wrap_length(
							pl->pos.x
									- ((player_t *)(pl->lock.object))->pos.x,
							pl->pos.y
									- ((player_t *)(pl->lock.object))->pos.y);
		}
	}

	/*
	 * Checking for collision, updating score etc. (see collision.c)
	 */
	Check_collision();

	/*
	 * Update tanks, Kill players that ought to be killed.
	 */
	for (i = NumPlayers - 1; i >= 0; i--) {
		player_t *pl = Players[i];

		if (BIT(pl->status, PLAYING | PAUSE | GAME_OVER | KILLED)
				== PLAYING)
			Update_tanks(&(pl->fuel));
		if (BIT(pl->status, KILLED)) {
			Kill_player(pl, true);

			if (Player_is_human(pl)) {
				if (frame_loops - pl->frame_last_busy > 60
						* fps && (NumPlayers > 1)) {
					Pause_player(pl, true);
				}
			}
		}
	}

	/* Remove objects that timed out.
	 *
	 * A ball can time out for three reasons:
	 * - it has been loose for long enough (doesn't happen in normal games)
	 * - it has been replaced
	 * - somebody cashed it
	 * In all cases such a ball should be recreated.
	 */
	for (i = NumObjs - 1; i >= 0; i--) {
		if (--(Obj[i]->life) <= 0) {
			treasure_t *t = Obj[i]->treasure;
			bool is_ball = BIT(Obj[i]->type, OBJ_BALL);

			Delete_object(Obj[i]);

			if (is_ball) {
				Make_treasure_ball(t);
			}
		}
	}

	/* do we have a game over ? */
	Compute_game_status();

	/*
	 * Now update labels if need be.
	 */
	if (updateScores && main_loops_slow % UPDATE_SCORE_DELAY == 0)
		Update_score_table();
}

void update_player_turn(player_t *pl)
{

	/*
	 * Compute turn
	 */
	pl->turnvel += pl->turnacc;

	/*
	 * turnresistance is zero: client requests linear turning behaviour
	 * when playing with pointer control.
	 */

	if (pl->turnresistance) {
		pl->turnvel *= pl->turnresistance;
	}

	pl->float_dir += pl->turnvel;

	while (pl->float_dir < 0)
		pl->float_dir += RES;
	while (pl->float_dir >= RES)
		pl->float_dir -= RES;

	/*
	 * turnresistance is zero: client requests linear turning behaviour
	 * when playing with pointer control.
	 */
	if (!pl->turnresistance) {
		pl->turnvel = 0;
	}
	/*  printf("%d\n",main_loops);
	 fflush(stdout);
	 */
	if (pl->oldturn)
		Old_turn_player(pl);
	else
		Turn_player(pl);
}

void update_player_thrust(player_t *pl)
{
	/*
	 * Update acceleration vector etc.
	 */

	if (BIT(pl->status, THRUSTING)) {
		DFLOAT power = pl->power;
		DFLOAT f = pl->power * 0.0008; /* 1/(FUEL_SCALE*MIN_POWER) */
		int32_t a = pl->item[ITEM_AFTERBURNER];
		DFLOAT inert = pl->mass;

		if (a) {
			power = AFTER_BURN_POWER(power, a);
			f = AFTER_BURN_FUEL(f, a);
		}
		pl->acc.x = power * tcos(pl->dir) / inert;
		pl->acc.y = power * tsin(pl->dir) / inert;
		Add_fuel(&(pl->fuel), (int32_t) (-f * FUEL_SCALE_FACT)); /* Decrement fuel */
	}
	else {
		pl->acc.x = pl->acc.y = 0.0;
	}

	pl->mass = pl->emptymass + FUEL_MASS(pl->fuel.sum);
}

