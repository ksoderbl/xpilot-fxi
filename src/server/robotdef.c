/* $Id: robotdef.c,v 1.18 2008/08/26 20:51:06 rotunda_pk Exp $
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
/* Robot code originally submitted by Maurice Abraham. */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "xpconfig.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "score.h"
#include "bit.h"
#include "netserver.h"
#include "pack.h"
#include "robot.h"
#include "error.h"

#define WITHIN(NOW,THEN,DIFF) (NOW<=THEN && (THEN-NOW)<DIFF)

/*
 * Flags for the default robots being in different modes (or moods).
 */
#define RM_ROBOT_IDLE         	(1 << 2)
#define RM_EVADE_LEFT         	(1 << 3)
#define RM_EVADE_RIGHT          (1 << 4)
#define RM_ROBOT_CLIMB          (1 << 5)
#define RM_HARVEST            	(1 << 6)
#define RM_ATTACK             	(1 << 7)
#define RM_TAKE_OFF           	(1 << 8)
#define RM_CANNON_KILL		(1 << 9)
#define RM_REFUEL		(1 << 10)
#define RM_NAVIGATE		(1 << 11)

/* long term modes */
#define FETCH_TREASURE		(1 << 0)
#define TARGET_KILL		(1 << 1)
#define NEED_FUEL		(1 << 2)

/*
 * Map objects a robot can fly through without damage.
 */
#define EMPTY_SPACE(s)	BIT(1 << (s), SPACE_BLOCKS)

/*
 * Prototypes for methods of the default robot type.
 */
static void Robot_default_create(player_t *pl, int8_t *str);
static void Robot_default_go_home(player_t *pl);
static void Robot_default_play(player_t *pl);
static void Robot_default_set_war(player_t *pl, player_t *kp);
static player_t *Robot_default_war_on_player(player_t *pl);
static void Robot_default_message(player_t *pl, const int8_t *str);
static void Robot_default_destroy(player_t *pl);
int32_t Robot_default_setup(robot_type_t *type_ptr);

static robot_type_t robot_default_type =
	{ "default", Robot_default_create, Robot_default_go_home,
			Robot_default_play, Robot_default_set_war,
			Robot_default_war_on_player, Robot_default_message,
			Robot_default_destroy };

/*
 * The only thing we export from this file.
 * A function to initialize the robot type structure
 * with our name and the pointers to our action routines.
 *
 * Return 0 if all is OK, anything else will ignore this
 * robot type forever.
 */
int32_t Robot_default_setup(robot_type_t *type_ptr)
{
	/* Not much to do for the default robot except init the type structure. */

	*type_ptr = robot_default_type;

	return 0;
}

/*
 * Private functions.
 */
/*static bool Check_robot_navigate(player_t *pl, bool * num_evade);*/
static bool Check_robot_evade(player_t *pl, int32_t mine_i, player_t *pl2);
static bool Check_robot_target(player_t *pl, int32_t item_x, int32_t item_y,
		int32_t new_mode);
static bool Detect(player_t *pl, player_t *pl2);
static bool Ball_handler(player_t *pl);

/*
 * Function to cast from player structure to robot data structure.
 * This isolates casts (aka. type violations) to a few places.
 */
static robot_default_data_t *Robot_default_get_data(player_t *pl)
{
	return (robot_default_data_t *) pl->robot_data_ptr->private_data;
}

/*
 * A default robot is created.
 */
static void Robot_default_create(player_t *pl, int8_t *str)
{
	robot_default_data_t *my_data;

	if (!(my_data = (robot_default_data_t *) malloc(sizeof(*my_data)))) {
		error("no mem for default robot");
		End_game();
	}

	my_data->robot_mode = RM_TAKE_OFF;
	my_data->robot_count = 0;
	my_data->robot_lock = LOCK_NONE;
	my_data->robot_lock_pl = NULL;
	my_data->longterm_mode = 0;

	if (str != NULL && *str != '\0' && sscanf(str, " %d %d",
			&my_data->attack, &my_data->defense) != 2) {
		if (str && *str) {
			xpprintf(
					"%s invalid parameters for default robot: \"%s\"\n",
					showtime(), str);
			my_data->attack = (int32_t) (rfrac() * 99.5f);
			my_data->defense = 100 - my_data->attack;
		}
		LIMIT(my_data->attack, 1, 99);
		LIMIT(my_data->defense, 1, 99);
	}
	/*
	 * some parameters which may be changed to be dependent upon
	 * the `attack' and `defense' settings of this robot.
	 */

	my_data->robot_normal_speed = 6.0;
	my_data->robot_attack_speed = 15.0 + (my_data->attack / 25);
	my_data->robot_max_speed = 30.0 + (my_data->attack / 50)
			- (my_data->defense / 50);

	pl->fuel.l3 += my_data->defense - my_data->attack + (int32_t) ((rfrac()
			- 0.5f) * 20);
	pl->fuel.l2 += 2 * (my_data->defense - my_data->attack) / 5
			+ (int32_t) ((rfrac() - 0.5f) * 8);
	pl->fuel.l1 += (my_data->defense - my_data->attack) / 5
			+ (int32_t) ((rfrac() - 0.5f) * 4);

	my_data->last_used_ecm = 0;
	my_data->last_dropped_mine = 0;
	my_data->last_fired_missile = 0;
	my_data->last_thrown_ball = 0;

	my_data->longterm_mode = 0;

	pl->robot_data_ptr->private_data = (void *) my_data;
}

/*
 * A default robot is placed on its homebase.
 */
static void Robot_default_go_home(player_t *pl)
{
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	my_data->robot_mode = RM_TAKE_OFF;
	my_data->longterm_mode = 0;
}

/*
 * A default robot is declaring war (or resetting war).
 */
static void Robot_default_set_war(player_t *pl, player_t *kp)
{
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	if (kp == NULL) {
		CLR_BIT(my_data->robot_lock, LOCK_PLAYER);
	}
	else {
		my_data->robot_lock_pl = kp;
		SET_BIT(my_data->robot_lock, LOCK_PLAYER);
	}
}

/*
 * Return pointer to the player a default robot has war against (or NULL).
 */
static player_t *Robot_default_war_on_player(player_t *pl)
{
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	if (BIT(my_data->robot_lock, LOCK_PLAYER)) {
		return my_data->robot_lock_pl;
	}
	else {
		return NULL;
	}
}

/*
 * A default robot receives a message.
 */
static void Robot_default_message(player_t *pl, const int8_t *message)
{
}

/*
 * A default robot is destroyed.
 */
static void Robot_default_destroy(player_t *pl)
{
	free(pl->robot_data_ptr->private_data);
	pl->robot_data_ptr->private_data = NULL;
}

static bool Really_empty_space(player_t *pl, int32_t x, int32_t y)
{
	int32_t type = World.block[x][y];

	if (EMPTY_SPACE(type))
		return true;
	switch (type) {
	case FILLED:
	case REC_LU:
	case REC_LD:
	case REC_RU:
	case REC_RD:
	case FUEL:
	case TREASURE:
		return false;
		break;
	}
	return false;
}

static bool Check_robot_evade(player_t *pl, int32_t mine_i, player_t *pl2)
{
	int32_t i;
	object_t *shot;
	int32_t stop_dist;
	bool evade;
	bool left_ok, right_ok;
	int32_t safe_width;
	int32_t travel_dir;
	int32_t delta_dir;
	int32_t aux_dir;
	int32_t px[3], py[3];
	int32_t dist;
	int32_t dx, dy;
	DFLOAT velocity;
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	safe_width = (my_data->defense / 200) * SHIP_SZ;
	/* Prevent overflow. */
	velocity = (pl->velocity <= SPEED_LIMIT) ? pl->velocity : SPEED_LIMIT;
	stop_dist = (int32_t) ((RES * velocity) / (MAX_PLAYER_TURNSPEED
			* pl->turnresistance)
			+ (velocity * velocity * pl->mass) / (2
					* MAX_PLAYER_POWER) + safe_width);
	/*
	 * Limit the look ahead.  For very high speeds the current code
	 * is ineffective and much too inefficient.
	 */
	if (stop_dist > 10 * BLOCK_SZ) {
		stop_dist = 10 * BLOCK_SZ;
	}
	evade = false;

	travel_dir = (int32_t) findDir(pl->vel.x, pl->vel.y);

	aux_dir = MOD2(travel_dir + RES / 4, RES);
	px[0] = pl->pos.x; /* ship center x */
	py[0] = pl->pos.y; /* ship center y */
	px[1] = (int32_t) (px[0] + safe_width * tcos(aux_dir)); /* ship left side x */
	py[1] = (int32_t) (py[0] + safe_width * tsin(aux_dir)); /* ship left side y */
	px[2] = 2 * px[0] - px[1]; /* ship right side x */
	py[2] = 2 * py[0] - py[1]; /* ship right side y */

	left_ok = true;
	right_ok = true;

	for (dist = 0; dist < stop_dist + BLOCK_SZ / 2; dist += BLOCK_SZ / 2) {
		for (i = 0; i < 3; i++) {
			dx = (int32_t) ((px[i] + dist * tcos(travel_dir))
					/ BLOCK_SZ);
			dy = (int32_t) ((py[i] + dist * tsin(travel_dir))
					/ BLOCK_SZ);

			if (BIT(World.rules->mode, WRAP_PLAY)) {
				if (dx < 0)
					dx += World.x;
				else if (dx >= World.x)
					dx -= World.x;
				if (dy < 0)
					dy += World.y;
				else if (dy >= World.y)
					dy -= World.y;
			}
			if (dx < 0 || dx >= World.x || dy < 0 || dy >= World.y) {
				evade = true;
				if (i == 1)
					left_ok = false;
				if (i == 2)
					right_ok = false;
				continue;
			}
			if (!Really_empty_space(pl, dx, dy)) {
				evade = true;
				if (i == 1)
					left_ok = false;
				if (i == 2)
					right_ok = false;
				continue;
			}
		}
	}

	if (mine_i >= 0) {
		shot = Obj[mine_i];
		aux_dir = (int32_t) Wrap_findDir(shot->pos.x + shot->vel.x
				- pl->pos.x, shot->pos.y + shot->vel.y
				- pl->pos.y);
		delta_dir = MOD2(aux_dir - travel_dir, RES);
		if (delta_dir < RES / 4) {
			left_ok = false;
			evade = true;
		}
		if (delta_dir > RES * 3 / 4) {
			right_ok = false;
			evade = true;
		}
	}
	if (pl2) {
		aux_dir = (int32_t) Wrap_findDir(pl2->pos.x - pl->pos.x
				+ pl2->vel.x * 2, pl2->pos.y - pl->pos.y
				+ pl2->vel.y * 2);
		delta_dir = MOD2(aux_dir - travel_dir, RES);
		if (delta_dir < RES / 4) {
			left_ok = false;
			evade = true;
		}
		if (delta_dir > RES * 3 / 4) {
			right_ok = false;
			evade = true;
		}
	}
	if (pl->velocity > pl->max_speed)
		evade = true;

	if (!evade)
		return false;

	delta_dir = 0;
	while (!left_ok && !right_ok && delta_dir < 7 * RES / 8) {
		delta_dir += RES / 16;

		left_ok = true;
		aux_dir = MOD2(travel_dir + delta_dir, RES);
		for (dist = 0; dist < stop_dist + BLOCK_SZ / 2; dist
				+= BLOCK_SZ / 2) {
			dx = (int32_t) ((px[0] + dist * tcos(aux_dir)) / BLOCK_SZ);
			dy = (int32_t) ((py[0] + dist * tsin(aux_dir)) / BLOCK_SZ);

			if (BIT(World.rules->mode, WRAP_PLAY)) {
				if (dx < 0)
					dx += World.x;
				else if (dx >= World.x)
					dx -= World.x;
				if (dy < 0)
					dy += World.y;
				else if (dy >= World.y)
					dy -= World.y;
			}
			if (dx < 0 || dx >= World.x || dy < 0 || dy >= World.y) {
				left_ok = false;
				continue;
			}
			if (!Really_empty_space(pl, dx, dy)) {
				left_ok = false;
				continue;
			}
		}

		right_ok = true;
		aux_dir = MOD2(travel_dir - delta_dir, RES);
		for (dist = 0; dist < stop_dist + BLOCK_SZ / 2; dist
				+= BLOCK_SZ / 2) {
			dx = (int32_t) ((px[0] + dist * tcos(aux_dir)) / BLOCK_SZ);
			dy = (int32_t) ((py[0] + dist * tsin(aux_dir)) / BLOCK_SZ);

			if (BIT(World.rules->mode, WRAP_PLAY)) {
				if (dx < 0)
					dx += World.x;
				else if (dx >= World.x)
					dx -= World.x;
				if (dy < 0)
					dy += World.y;
				else if (dy >= World.y)
					dy -= World.y;
			}
			if (dx < 0 || dx >= World.x || dy < 0 || dy >= World.y) {
				right_ok = false;
				continue;
			}
			if (!Really_empty_space(pl, dx, dy)) {
				right_ok = false;
				continue;
			}
		}
	}

	pl->turnspeed = MAX_PLAYER_TURNSPEED;
	pl->power = MAX_PLAYER_POWER;

	delta_dir = MOD2(pl->dir - travel_dir, RES);

	if (my_data->robot_mode != RM_EVADE_LEFT && my_data->robot_mode
			!= RM_EVADE_RIGHT) {
		if (left_ok && !right_ok)
			my_data->robot_mode = RM_EVADE_LEFT;
		else if (right_ok && !left_ok)
			my_data->robot_mode = RM_EVADE_RIGHT;
		else
			my_data->robot_mode
					= (delta_dir < RES / 2 ? RM_EVADE_LEFT
							: RM_EVADE_RIGHT);
	}
	/*-BA If facing the way we want to go, thrust
	 *-BA If too far off, stop thrusting
	 *-BA If in between, keep doing whatever we are already doing
	 *-BA In all cases continue to straighten up
	 */
	if (delta_dir < RES / 4 || delta_dir > 3 * RES / 4) {
		pl->turnacc
				= (my_data->robot_mode == RM_EVADE_LEFT ? pl->turnspeed
						: (-pl->turnspeed));
		CLR_BIT(pl->status, THRUSTING);
	}
	else if (delta_dir < 3 * RES / 8 || delta_dir > 5 * RES / 8) {
		pl->turnacc
				= (my_data->robot_mode == RM_EVADE_LEFT ? pl->turnspeed
						: (-pl->turnspeed));
	}
	else {
		pl->turnacc = 0;
		SET_BIT(pl->status, THRUSTING);
		my_data->robot_mode = (delta_dir < RES / 2 ? RM_EVADE_LEFT
				: RM_EVADE_RIGHT);
	}

	return true;
}

static bool Check_robot_target(player_t *pl, int32_t item_x, int32_t item_y,
		int32_t new_mode)
{
	int32_t item_dist;
	int32_t item_dir;
	int32_t travel_dir;
	int32_t delta_dir;
	int32_t dx, dy;
	int32_t dist;
	bool clear_path;
	bool slowing;
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	dx = item_x - pl->pos.x, dx = WRAP_DX(dx);
	dy = item_y - pl->pos.y, dy = WRAP_DY(dy);

	item_dist = (int32_t) (LENGTH(dy, dx));
	item_dir = (int32_t) findDir(dx, dy);

	if (new_mode == RM_REFUEL)
		item_dist = item_dist - 90;

	clear_path = true;

	for (dist = 0; clear_path && dist < item_dist; dist += BLOCK_SZ / 2) {

		dx = (int32_t) ((pl->pos.x + dist * tcos(item_dir)) / BLOCK_SZ);
		dy = (int32_t) ((pl->pos.y + dist * tsin(item_dir)) / BLOCK_SZ);

		if (BIT(World.rules->mode, WRAP_PLAY)) {
			if (dx < 0)
				dx += World.x;
			else if (dx >= World.x)
				dx -= World.x;
			if (dy < 0)
				dy += World.y;
			else if (dy >= World.y)
				dy -= World.y;
		}
		if (dx < 0 || dx >= World.x || dy < 0 || dy >= World.y) {
			clear_path = false;
			continue;
		}
		if (!Really_empty_space(pl, dx, dy)) {
			clear_path = false;
			continue;
		}
	}

	if (new_mode == RM_CANNON_KILL)
		item_dist -= 3 * BLOCK_SZ;

	if (!clear_path && new_mode != RM_NAVIGATE)
		return false;

	travel_dir = (int32_t) findDir(pl->vel.x, pl->vel.y);

	pl->turnspeed = MAX_PLAYER_TURNSPEED / 2;
	pl->power = MAX_PLAYER_POWER / 2;

	delta_dir = MOD2(item_dir - travel_dir, RES);
	if (delta_dir >= RES / 4 && delta_dir <= 3 * RES / 4) {

		if (new_mode == RM_HARVEST || (new_mode == RM_NAVIGATE
				&& (clear_path || dist > 8 * BLOCK_SZ))) {
			/* reverse direction of travel */
			item_dir = MOD2(travel_dir + (delta_dir > RES / 2 ? -5
					* RES / 8 : 5 * RES / 8), RES);
		}
		pl->turnspeed = MAX_PLAYER_TURNSPEED;
		slowing = true;
	}
	else if (new_mode == RM_CANNON_KILL && item_dist <= 0) {

		/* too close, so move away */
		pl->turnspeed = MAX_PLAYER_TURNSPEED;
		item_dir = MOD2(item_dir + RES / 2, RES);
		slowing = true;
	}
	else {

		slowing = false;
	}
	if (new_mode == RM_NAVIGATE && !clear_path) {
		if (dist <= 8 * BLOCK_SZ && dist > 4 * BLOCK_SZ) {
			item_dir = MOD2(item_dir + (delta_dir > RES / 2 ? -3
					* RES / 4 : 3 * RES / 4), RES);
		}
		else if (dist <= 4 * BLOCK_SZ) {
			item_dir = MOD2(item_dir + RES / 2, RES);
		}
		pl->turnspeed = MAX_PLAYER_TURNSPEED;
		slowing = true;
	}

	delta_dir = MOD2(item_dir - pl->dir, RES);

	if (delta_dir > RES / 8 && delta_dir < 7 * RES / 8) {
		pl->turnspeed = MAX_PLAYER_TURNSPEED;
	}
	else if (delta_dir > RES / 16 && delta_dir < 15 * RES / 16) {
		pl->turnspeed = MAX_PLAYER_TURNSPEED;
	}
	else if (delta_dir > RES / 64 && delta_dir < 63 * RES / 64) {
		pl->turnspeed = MAX_PLAYER_TURNSPEED;
	}
	else {
		pl->turnspeed = 0.0;
	}
	pl->turnacc = (delta_dir < RES / 2 ? pl->turnspeed : (-pl->turnspeed));

	if (slowing || BIT(pl->used, OBJ_SHIELD)) {

		SET_BIT(pl->status, THRUSTING);

	}
	else if (item_dist < 0) {

		CLR_BIT(pl->status, THRUSTING);

	}
	else if (item_dist < 3 * BLOCK_SZ && new_mode != RM_HARVEST) {

		if (pl->velocity < my_data->robot_normal_speed / 2)
			SET_BIT(pl->status, THRUSTING);
		if (pl->velocity > my_data->robot_normal_speed)
			CLR_BIT(pl->status, THRUSTING);

	}
	else if ((new_mode != RM_ATTACK && new_mode != RM_NAVIGATE)
			|| item_dist < 8 * BLOCK_SZ || (new_mode == RM_NAVIGATE
			&& delta_dir > 3 * RES / 8 && delta_dir < 5 * RES / 8)) {

		if (pl->velocity < 2 * my_data->robot_normal_speed)
			SET_BIT(pl->status, THRUSTING);
		if (pl->velocity > 3 * my_data->robot_normal_speed)
			CLR_BIT(pl->status, THRUSTING);

	}
	else if (new_mode == RM_ATTACK || (new_mode == RM_NAVIGATE && (dist
			< 12 * BLOCK_SZ || (delta_dir > RES / 8 && delta_dir
			< 7 * RES / 8)))) {

		if (pl->velocity < my_data->robot_attack_speed / 2)
			SET_BIT(pl->status, THRUSTING);
		if (pl->velocity > my_data->robot_attack_speed)
			CLR_BIT(pl->status, THRUSTING);
	}
	else if (clear_path && (delta_dir < RES / 8 || delta_dir > 7 * RES / 8)
			&& item_dist > 18 * BLOCK_SZ) {
		if (pl->velocity < my_data->robot_max_speed
				- my_data->robot_normal_speed)
			SET_BIT(pl->status, THRUSTING);
		if (pl->velocity > my_data->robot_max_speed)
			CLR_BIT(pl->status, THRUSTING);
	}
	else {
		if (pl->velocity < my_data->robot_attack_speed)
			SET_BIT(pl->status, THRUSTING);
		if (pl->velocity > my_data->robot_max_speed
				- my_data->robot_normal_speed)
			CLR_BIT(pl->status, THRUSTING);
	}

	if ((new_mode == RM_ATTACK) || (new_mode == RM_NAVIGATE)) {

		/*-BA Be more agressive, esp if lots of ammo
		 * else if ((my_data->robot_count % 10) == 0 && pl->item[ITEM_MISSILE] > 0)
		 */
		if ((my_data->robot_count % 2) == 0 && item_dist
				< VISIBILITY_DISTANCE
		/*&& BIT(my_data->robot_lock, LOCK_PLAYER)*/) {
			if ((new_mode == RM_ATTACK && clear_path)
					|| (my_data->robot_count % 50) == 0) {
				Fire_normal_shots(pl);
			}
		}
	}
	if (new_mode == RM_CANNON_KILL && !slowing) {
		if ((my_data->robot_count % 2) == 0 && item_dist
				< VISIBILITY_DISTANCE && clear_path) {
			Fire_normal_shots(pl);
		}
	}
	my_data->robot_mode = new_mode;
	return true;
}

static bool Check_robot_hunt(player_t *pl)
{
	player_t *ship;
	int32_t ship_dir;
	int32_t travel_dir;
	int32_t delta_dir;
	int32_t adj_dir;
	int32_t toofast, tooslow;
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	if (!BIT(my_data->robot_lock, LOCK_PLAYER) || my_data->robot_lock_pl
			== pl)
		return false;
	if (pl->fuel.sum < pl->fuel.l3 /*MAX_PLAYER_FUEL/2*/)
		return false;
	if (!Detect(pl, my_data->robot_lock_pl))
		return false;

	ship = my_data->robot_lock_pl;

	ship_dir = (int32_t) Wrap_findDir(ship->pos.x - pl->pos.x, ship->pos.y
			- pl->pos.y);

	travel_dir = (int32_t) findDir(pl->vel.x, pl->vel.y);

	delta_dir = MOD2(ship_dir - travel_dir, RES);
	tooslow = (pl->velocity < my_data->robot_attack_speed / 2);
	toofast = (pl->velocity > my_data->robot_attack_speed);

	if (!tooslow && !toofast && (delta_dir <= RES / 16 || delta_dir >= 15
			* RES / 16)) {

		pl->turnacc = 0;
		CLR_BIT(pl->status, THRUSTING);
		my_data->robot_mode = RM_ROBOT_IDLE;
		return true;
	}

	adj_dir = (delta_dir < RES / 2 ? RES / 4 : (-RES / 4));

	if (tooslow)
		adj_dir = adj_dir / 2; /* point forwards more */
	if (toofast)
		adj_dir = 3 * adj_dir / 2; /* point backwards more */

	adj_dir = MOD2(travel_dir + adj_dir, RES);
	delta_dir = MOD2(adj_dir - pl->dir, RES);

	if (delta_dir >= RES / 16 && delta_dir <= 15 * RES / 16) {
		pl->turnspeed = MAX_PLAYER_TURNSPEED / 4;
		pl->turnacc = (delta_dir < RES / 2 ? pl->turnspeed
				: (-pl->turnspeed));
	}

	if (delta_dir < RES / 8 || delta_dir > 7 * RES / 8) {
		SET_BIT(pl->status, THRUSTING);
	}
	else {
		CLR_BIT(pl->status, THRUSTING);
	}

	my_data->robot_mode = RM_ROBOT_IDLE;
	return true;
}

static bool Detect(player_t *pl, player_t *pl2)
{
	return true; /* trivial */
}

/*
 * Determine how important an item is to a given player.
 * Return one of the following 3 values:
 */
#define ROBOT_MUST_HAVE_ITEM	2	/* must have */
#define ROBOT_HANDY_ITEM	1	/* handy */
#define ROBOT_IGNORE_ITEM	0	/* ignore */

static bool Ball_handler(player_t *pl)
{
	int32_t i, dist;
	int32_t closest_t_dist = LONG_MAX, closest_nt_dist = LONG_MAX,
			bdir, tdir;
	treasure_t *t;
	treasure_t *closest_t = NULL; /* closest treasure */
	treasure_t *closest_nt = NULL;

	bool clear_path = true;
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	for (i = 0; i < World.NumTreasures; i++) {
		t = &World.treasures[i];

		if ((BIT(pl->have, OBJ_BALL) || pl->ball_tmp)
				&& t->team == pl->team) {
			dist = (int32_t) Wrap_length((t->pos.x
					+ 0.5) * BLOCK_SZ - pl->pos.x,
					(t->pos.y + 0.5)
							* BLOCK_SZ - pl->pos.y);
			if (dist < closest_t_dist) {
				closest_t = t;
				closest_t_dist = dist;
			}
		}
		else if (t->team != pl->team
				&& t->team->NumMembers
						> 0 && !BIT(pl->have, OBJ_BALL)
				&& !pl->ball_tmp && t->have) {
			dist = (int32_t) Wrap_length((t->pos.x
					+ 0.5) * BLOCK_SZ - pl->pos.x,
					(t->pos.y + 0.5)
							* BLOCK_SZ - pl->pos.y);
			if (dist < closest_nt_dist) {
				closest_nt = t;
				closest_nt_dist = dist;
			}
		}
	}

	/* If the robot has a ball, or is still connecting to one */
	if (BIT(pl->have, OBJ_BALL) || pl->ball_tmp) {
		object_t *ball = NULL;
		int32_t dist_np = LONG_MAX;
		int32_t xdist, ydist;
		int32_t dx, dy;
		if (pl->ball_tmp) {
			ball = pl->ball_tmp;
		}
		else {
			for (i = 0; i < NumObjs; i++) {
				if (BIT(Obj[i]->type, OBJ_BALL) &&
						BIT(Obj[i]->status, IS_ATTACHED) &&
						Obj[i]->owner == pl) {
					ball = Obj[i];
					break;
				}
			}
		}
		for (i = 0; i < NumPlayers; i++) {
			dist = (int32_t) (LENGTH(ball->pos.x - Players[i]->pos.x,
					ball->pos.y - Players[i]->pos.y));
			if (Players[i] != pl
					&& (BIT(Players[i]->status, PLAYING
							| PAUSE | GAME_OVER)
							== PLAYING) && dist
					< dist_np)
				dist_np = dist;
		}
		bdir = (int32_t) findDir(ball->vel.x, ball->vel.y);
		tdir = (int32_t) Wrap_findDir((closest_t->pos.x
				+ 0.5) * BLOCK_SZ - ball->pos.x,
				(closest_t->pos.y + 0.5)
						* BLOCK_SZ - ball->pos.y);
		xdist = (closest_t->pos.x) - OBJ_X_IN_BLOCKS(ball);
		ydist = (closest_t->pos.y) - OBJ_Y_IN_BLOCKS(ball);
		for (dist = 0; clear_path && dist < (closest_t_dist - BLOCK_SZ); dist
				+= BLOCK_SZ / 2) {
			DFLOAT fraction = (DFLOAT) dist / closest_t_dist;
			dx = (int32_t) ((fraction * xdist) + OBJ_X_IN_BLOCKS(ball));
			dy = (int32_t) ((fraction * ydist) + OBJ_Y_IN_BLOCKS(ball));
			if (BIT(World.rules->mode, WRAP_PLAY)) {
				if (dx < 0)
					dx += World.x;
				else if (dx >= World.x)
					dx -= World.x;
				if (dy < 0)
					dy += World.y;
				else if (dy >= World.y)
					dy -= World.y;
			}
			if (dx < 0 || dx >= World.x || dy < 0 || dy >= World.y) {
				clear_path = false;
				continue;
			}
			if (!BIT(1U << World.block[dx][dy], SPACE_BLOCKS)) {
				clear_path = false;
				continue;
			}
		}
		if (tdir == bdir && dist_np > closest_t_dist && clear_path
				&& sqr(ball->vel.x) + sqr(ball->vel.y) > 60) {
			Detach_ball(pl, NULL);
			CLR_BIT(pl->used, OBJ_CONNECTOR);
			my_data->last_thrown_ball = my_data->robot_count;
			CLR_BIT(my_data->longterm_mode, FETCH_TREASURE);
		}
		else {
			SET_BIT(my_data->longterm_mode, FETCH_TREASURE);
			return (Check_robot_target(
					pl,
					((int32_t) (closest_t->pos.x
							+ 0.5) * BLOCK_SZ),
					((int32_t) (closest_t->pos.y
							+ 0.5) * BLOCK_SZ),
					RM_NAVIGATE));
		}
	}

	/* The robot doesn't have a ball and isn't connecting to any */
	else {
		int32_t ball_dist;
		int32_t closest_ball_dist = closest_nt_dist;
		object_t *closest_ball = NULL;

		for (i = 0; i < NumObjs; i++) {
			if (Obj[i]->type == OBJ_BALL) {
				if ((!BIT(Obj[i]->status, IS_ATTACHED) && Obj[i]->owner) ||
						(BIT(Obj[i]->status, IS_ATTACHED) &&
								Obj[i]->owner->team != pl->team)) {
					ball_dist
							= (int32_t) LENGTH(
									pl->pos.x
											- Obj[i]->pos.x,
									pl->pos.y
											- Obj[i]->pos.y);
					if (ball_dist < closest_ball_dist) {
						closest_ball_dist = ball_dist;
						closest_ball = Obj[i];
					}
				}
			}
		}
		if (!closest_ball && closest_nt_dist
				< (my_data->robot_count / 10) * BLOCK_SZ) {
			SET_BIT(my_data->longterm_mode, FETCH_TREASURE);
			return (Check_robot_target(
					pl,
					((int32_t) (closest_nt->pos.x
							+ 0.5) * BLOCK_SZ),
					((int32_t) (closest_nt->pos.y
							+ 0.5) * BLOCK_SZ),
					RM_NAVIGATE));
		}
		else if (closest_ball_dist < (my_data->robot_count / 10)
				* BLOCK_SZ && closest_ball_dist
				> ballConnectorLength) {
			SET_BIT(my_data->longterm_mode, FETCH_TREASURE);
			return (Check_robot_target(pl,
					closest_ball->pos.x,
					closest_ball->pos.y, RM_NAVIGATE));
		}
	}
	return false;
}

static int32_t Robot_default_play_check_map(player_t *pl)
{
	int32_t j;
	fuel_t *fuel_i;
	int32_t dx, dy;
	int32_t distance, fuel_dist;
	bool fuel_checked;
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	fuel_checked = false;

	fuel_i = NULL;
	fuel_dist = VISIBILITY_DISTANCE;

	for (j = 0; j < World.NumFuels; j++) {

		if (World.fuel[j].fuel < 100 * FUEL_SCALE_FACT)
			continue;

		if (BIT(World.rules->mode, TEAM_PLAY) && teamFuel
				&& World.fuel[j].team != pl->team)
			continue;

		if ((dx = (World.fuel[j].pix_pos.x - pl->pos.x), dx = WRAP_DX(dx), ABS(dx)) < fuel_dist && (dy
				= (World.fuel[j].pix_pos.y - pl->pos.y), dy
				= WRAP_DY(dy), ABS(dy)) < fuel_dist
				&& (distance = (int32_t) LENGTH(dx, dy))
						< fuel_dist) {
			if (World.block[World.fuel[j].blk_pos.x][World.fuel[j].blk_pos.y]
					== FUEL) {
				fuel_i = &World.fuel[j];
				fuel_dist = distance;
			}
		}
	}

	if (fuel_i && BIT(my_data->longterm_mode, NEED_FUEL)) {

		fuel_checked = true;
		dx = fuel_i->pix_pos.x;
		dy = fuel_i->pix_pos.y;

		SET_BIT(pl->used, OBJ_REFUEL);
		pl->fs = fuel_i;

		if (Check_robot_target(pl, dx, dy, RM_REFUEL)) {
			return 1;
		}
	}

	if (fuel_i && !fuel_checked && BIT(my_data->longterm_mode,
			NEED_FUEL)) {

		dx = fuel_i->pix_pos.x;
		dy = fuel_i->pix_pos.y;

		SET_BIT(pl->used, OBJ_REFUEL);
		pl->fs = fuel_i;

		if (Check_robot_target(pl, dx, dy, RM_REFUEL)) {
			return 1;
		}
	}

	return 0;
}

static void Robot_default_play_check_objects(player_t *pl, int32_t *item_i,
		int32_t *item_dist, int32_t *item_imp, int32_t *mine_i, int32_t *mine_dist)
{
	int32_t j;
	object_t *shot;
	int32_t distance;
	int32_t dx, dy;
	int32_t shield_range;
	int32_t killing_shots;
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	/*-BA Neural overload - if NumObjs too high, only consider
	 *-BA max_objs many objects - improves performance under nukes
	 *-BA 1000 is a fairly arbitrary choice.  If you wish to tune it,
	 *-BA take into account the following.  A 4 mine cluster nuke produces
	 *-BA about 4000 short lived objects.  An 8 mine cluster nuke produces
	 *-BA about 14000 short lived objects.  By default, there is a limit
	 *-BA of about 16000 objects.  Each player/robot produces between
	 *-BA 20 and 40 objects just thrusting, and up to perhaps another 100
	 *-BA by firing.  If the number is set too low the robots just fly
	 *-BA around with thier shields on looking stupid and not doing
	 *-BA much.  If too high, your system will slow down too much when
	 *-BA the cluster nukes start going off.
	 *
	 *-BD 1000 turned out to be too small for some games, using 2000 instead.
	 *
	 *-BG The max_objs limit is the reason why robots play lousy on big
	 *    maps with lots of objects.  A better solution would be to use the
	 *    Cell mechanism from collision.c to only look at the closest objects.
	 */
	const int32_t max_objs = 2000;
	int32_t first_obj, last_obj;

#if 0
	/*
	 * BG: only look at a limited number of close objects.
	 */
	typedef struct obj_dist {
		int32_t ind; /* object index. */
		int32_t dist; /* distance of object to robot. */
	}obj_dist_t;
	int32_t num_obj_dist = 0;
#endif

	killing_shots = KILLING_SHOTS;
	if (treasureCollisionMayKill) {
		killing_shots |= OBJ_BALL;
	}
	if (wreckageCollisionMayKill) {
		killing_shots |= OBJ_WRECKAGE;
	}

	if (NumObjs <= max_objs) {
		first_obj = 0;
		last_obj = NumObjs;
	}
	else {
		first_obj = (int32_t) (rfrac() * (NumObjs - max_objs));
		last_obj = first_obj + max_objs;
		LIMIT(last_obj, 0, NumObjs);
		if (BIT(pl->have, OBJ_SHIELD)) {
			SET_BIT(pl->used, OBJ_SHIELD);
		}
	}

	for (j = first_obj; j < last_obj; j++) {

		shot = Obj[j];

		/* Get rid of the most common object types first for speed. */
		if (BIT(shot->type, OBJ_DEBRIS | OBJ_SPARK)) {
			continue;
		}

		dx = WRAP_DX(shot->pos.x - pl->pos.x);
		dy = WRAP_DY(shot->pos.y - pl->pos.y);
		if (QUICK_LENGTH(dx, dy) > VISIBILITY_DISTANCE)
			continue;

		if (BIT(shot->type, OBJ_BALL) && !WITHIN(my_data->last_thrown_ball,
				my_data->robot_count,
				3 * intGameSpeed)) {
			SET_BIT(pl->used, OBJ_CONNECTOR);
		}

		/* Ignore shots if shields already up - nothing else to do anyway */
		if (BIT(shot->type, OBJ_SHOT) && BIT(pl->used, OBJ_SHIELD)) {
			continue;
		}

		/*
		 * The only thing left to do regarding objects is to check if
		 * this robot needs to put up shields to protect against objects.
		 */
		if (!BIT(shot->type, killing_shots)) {
			continue;
		}

		/*
		 * Any shot of team members excluding self are passive.
		 */
		if (BIT(World.rules->mode, TEAM_PLAY) && teamImmunity
				&& shot->team == pl->team && shot->owner != pl) {
			continue;
		}

		/* Find nearest missile/mine */
		if (BIT(shot->type, OBJ_BALL) || (BIT(shot->type, OBJ_SHOT)
				&& shot->owner != pl)
				|| (BIT(shot->type, OBJ_WRECKAGE))) {
			if (ABS(dx) < *mine_dist && ABS(dy) < *mine_dist
					&& (distance = LENGTH(dx, dy))
							< *mine_dist) {
				*mine_i = j;
				*mine_dist = distance;
			}
			if ((dx = ((shot->pos.x - pl->pos.x) + (shot->vel.x
					- pl->vel.x)), dx = WRAP_DX(dx), ABS(dx)) < *mine_dist
					&& (dy
							= ((shot->pos.y
									- pl->pos.y)
									+ (shot->vel.y
											- pl->vel.y)), dy
							= WRAP_DY(dy), ABS(
							dy)) < *mine_dist
					&& (distance = LENGTH(dx, dy))
							< *mine_dist) {
				*mine_i = j;
				*mine_dist = distance;
			}
		}

		shield_range = 21 + SHIP_SZ + shot->pl_range;

		if ((dx = (shot->pos.x + shot->vel.x - (pl->pos.x + pl->vel.x)), dx
				= WRAP_DX(dx), ABS(dx)) < shield_range
				&& (dy = (shot->pos.y + shot->vel.y
						- (pl->pos.y + pl->vel.y)), dy
						= WRAP_DY(dy), ABS(dy))
						< shield_range && sqr(dx)
				+ sqr(dy) <= sqr(shield_range)
				&& (int32_t) (rfrac() * 100) < (85
						+ (my_data->defense / 7)
						- (my_data->attack / 50))) {
			SET_BIT(pl->used, OBJ_SHIELD);
			SET_BIT(pl->status, THRUSTING);
		}
	}
}

static void Robot_default_play(player_t *pl)
{
	player_t *pl2, *pl3;
	player_t *enemy_pl;
	DFLOAT distance, ship_dist, enemy_dist, speed, x_speed, y_speed;
	int32_t item_dist, mine_dist;
	int32_t item_i, mine_i;
	int32_t j, item_imp;
	int32_t dx, dy, x, y;
	bool harvest_checked;
	bool evade_checked;
	bool navigate_checked;
	int32_t shoot_time;
	robot_default_data_t *my_data = Robot_default_get_data(pl);

	if (my_data->robot_count <= 0)
		my_data->robot_count = 1000 + (int32_t) (rfrac() * 32);

	my_data->robot_count--;

	CLR_BIT(pl->used, OBJ_SHIELD);
	harvest_checked = false;
	evade_checked = false;
	navigate_checked = false;

	mine_i = -1;
	mine_dist = SHIP_SZ + 200;
	item_i = -1;
	item_dist = (int32_t) VISIBILITY_DISTANCE;
	item_imp = ROBOT_IGNORE_ITEM;

	/*
	 if (pl->fuel.sum <= (BIT(World.rules->mode, TIMING) ? 0 : pl->fuel.l1)) {
	 if (!BIT(pl->status, SELF_DESTRUCT)) {
	 SET_BIT(pl->status, SELF_DESTRUCT);
	 pl->count = 150;
	 }
	 } else {
	 CLR_BIT(pl->status, SELF_DESTRUCT);
	 }
	 */

	if (pl->fuel.sum < pl->fuel.max * 0.80)
		for (j = 0; j < World.NumFuels; j++) {
			int32_t dx, dy;
			if (BIT(World.rules->mode, TEAM_PLAY) && teamFuel
					&& World.fuel[j].team != pl->team) {
				continue;
			}
			dx = (int32_t) (World.fuel[j].pix_pos.x - pl->pos.x);
			dy = (int32_t) (World.fuel[j].pix_pos.y - pl->pos.y);
			/* dx = WRAP_DX(dx);
			 dy = WRAP_DY(dy); */
			if (sqr(dx) + sqr(dy) <= sqr(90) && World.fuel[j].fuel
					> REFUEL_RATE) {
				pl->fs = &World.fuel[j];
				SET_BIT(pl->used, OBJ_REFUEL);
				break;
			}
			else {
				CLR_BIT(pl->used, OBJ_REFUEL);
			}
		}

	/* don't turn NEED_FUEL off until refueling stops */
	/*
	 if (pl->fuel.sum < (BIT(World.rules->mode, TIMING) ?
	 pl->fuel.l1 : pl->fuel.l3)) {
	 SET_BIT(my_data->longterm_mode, NEED_FUEL);
	 } else if (!BIT(pl->used, OBJ_REFUEL)) {
	 CLR_BIT(my_data->longterm_mode, NEED_FUEL);
	 }
	 */
	Robot_default_play_check_objects(pl, &item_i, &item_dist, &item_imp,
			&mine_i, &mine_dist);

	/* Note: Only take time to navigate if not being shot at */
	/* BD: it seems that this 'Check_robot_navigate' function caused
	 the infamous 'robot stuck under wall' bug, so I commented it out */
	/* BD: ps. I tried to change that function, but I don't grok it */
	/*if (!(BIT(pl->used, OBJ_SHIELD) && BIT(pl->status, THRUSTING))
	 && Check_robot_navigate(ind, &evade_checked)) {
	 if (playerShielding == 0
	 && playerStartsShielded != 0
	 && BIT(pl->have, OBJ_SHIELD)) {
	 SET_BIT(pl->used, OBJ_SHIELD);
	 }
	 return;
	 }*/
	/* BD: unfortunately, this introduced a new bug. robots with large
	 shipshapes don't take off from their bases. here's an attempt to
	 fix it */
	if (QUICK_LENGTH(pl->pos.x - (pl->home_base->pos.x
			* BLOCK_SZ), pl->pos.y
			- (pl->home_base->pos.y * BLOCK_SZ))
			< BLOCK_SZ) {
		SET_BIT(pl->status, THRUSTING);
	}

	//ship_i = -1;
	pl2 = NULL;
	pl3 = NULL;
	ship_dist = SHIP_SZ * 6;
	//enemy_i = -1;
	enemy_pl = NULL;

	if (pl->fuel.sum > pl->fuel.l3)
		enemy_dist = (DFLOAT) World.hypotenuse;
	else
		enemy_dist = VISIBILITY_DISTANCE;

	if (BIT(pl->used, OBJ_SHIELD))
		ship_dist = 0;

	for (j = 0; j < NumPlayers; j++) {
		pl2 = Players[j];
		if (pl2 == pl || BIT(pl2->status, PLAYING
				| GAME_OVER | PAUSE) != PLAYING || TEAM_IMMUNE(pl, pl2))
			continue;

		if (!Detect(pl, pl2))
			continue;

		if (BIT(my_data->robot_lock, LOCK_PLAYER)
				&& my_data->robot_lock_pl == pl2) {
			my_data->lock_last_seen = my_data->robot_count;
			my_data->lock_last_pos.x = pl2->pos.x;
			my_data->lock_last_pos.y = pl2->pos.y;
		}

		dx = pl2->pos.x - pl->pos.x, dx = WRAP_DX(dx);
		dy = pl2->pos.y - pl->pos.y, dy = WRAP_DY(dy);
		distance = LENGTH(dx, dy);

		if (distance < ship_dist) {
			pl3 = pl2;
			ship_dist = distance;
		}

		if (BIT(my_data->robot_lock, LOCK_PLAYER)
				&& BIT(
						my_data->robot_lock_pl->status,
						PLAYING | PAUSE | GAME_OVER)
						== PLAYING) {
			/* ignore all players unless target */
			if (my_data->robot_lock_pl == pl2 && distance
					< enemy_dist) {

				//enemy_i = j;
				enemy_pl = pl2;
				enemy_dist = distance;
			}
		}
		else {
			if ((my_data->robot_count % 3) == 0
					&& ((my_data->robot_count % 100)
							< my_data->attack)
					&& distance < enemy_dist) {
				//enemy_i = j;
				enemy_pl = pl2;
				enemy_dist = distance;
			}
		}
	}

	if (ship_dist <= 3 * SHIP_SZ && BIT(pl->have, OBJ_SHIELD)) {
		SET_BIT(pl->used, OBJ_SHIELD);
	}

	if (BIT(my_data->robot_lock, LOCK_PLAYER) && pl3 != NULL
			&& my_data->robot_lock_pl == pl3
			&& Detect(pl, pl3))
		pl2 = NULL; /* don't avoid target */

	if (enemy_pl) {
		pl2 = enemy_pl;
		if (!BIT(pl->lock.flags, LOCK_PLAYER) || (enemy_dist
				< pl->lock.distance / 2) || (enemy_dist
				< pl->lock.distance * 2 && BIT(
				World.rules->mode, TEAM_PLAY) && BIT(
				pl2->have, OBJ_BALL)) || ((pl->lock.object) && (pl2->score
				> ((player_t *)(pl->lock.object))->score))) {
			pl->lock.object = pl2;
			SET_BIT(pl->lock.flags, LOCK_PLAYER);
			pl->lock.distance = enemy_dist;
		}
	}

	if (BIT(pl->lock.flags, LOCK_PLAYER)) {
		int32_t delta_dir;
		pl2 = pl->lock.object;
		delta_dir = (int32_t) (pl->dir - Wrap_findDir(pl2->pos.x
				- pl->pos.x, pl2->pos.y - pl->pos.y));
		delta_dir = MOD2(delta_dir, RES);
		if (BIT(pl2->status, PLAYING | PAUSE | GAME_OVER) != PLAYING
				|| (BIT(my_data->robot_lock, LOCK_PLAYER)
						&& my_data->robot_lock_pl != pl2
						&& BIT(my_data->robot_lock_pl->status,
								PLAYING | PAUSE
										| GAME_OVER)
								== PLAYING)
				|| !Detect(pl, pl2)
				|| ((pl->fuel.sum <= pl->fuel.l3) && (delta_dir
						< 3 * RES / 4 || delta_dir
						> RES / 4)) || ((BIT(
				World.rules->mode, TEAM_PLAY)) && pl->team
				== pl2->team)) {
			/* unset the player lock */
			CLR_BIT(pl->lock.flags, LOCK_PLAYER);
			// TODO: is this a reasonable thing to do?
			pl->lock.object = Players[0];
			pl->lock.distance = 0;
		}
	}
	if (!evade_checked) {
		if (Check_robot_evade(pl, mine_i, pl3)) {
			if (playerShielding == 0 && playerStartsShielded != 0
					&& BIT(pl->have, OBJ_SHIELD)) {
				SET_BIT(pl->used, OBJ_SHIELD);
			}
			else if (maxShieldedWallBounceSpeed
					> maxUnshieldedWallBounceSpeed && BIT(
					pl->have, OBJ_SHIELD)) {
				SET_BIT(pl->used, OBJ_SHIELD);
			}
			return;
		}
	}
	if (item_i >= 0 && 3 * enemy_dist > 2 * item_dist && item_dist < 12
			* BLOCK_SZ && !BIT(my_data->longterm_mode,
			FETCH_TREASURE) && (!BIT(my_data->longterm_mode,
			NEED_FUEL) || Obj[item_i]->info == ITEM_FUEL
			|| Obj[item_i]->info == ITEM_TANK)) {

		if (item_imp != ROBOT_IGNORE_ITEM) {
			harvest_checked = true;
			dx = Obj[item_i]->pos.x;
			dx += (int32_t) (Obj[item_i]->vel.x * (ABS(dx - pl->pos.x)
					/ my_data->robot_normal_speed));
			dy = Obj[item_i]->pos.y;
			dy += (int32_t) (Obj[item_i]->vel.y * (ABS(dy - pl->pos.y)
					/ my_data->robot_normal_speed));

			if (Check_robot_target(pl, dx, dy, RM_HARVEST)) {
				return;
			}
		}
	}
	if (BIT(pl->lock.flags, LOCK_PLAYER) && Detect(pl, pl->lock.object)) {

		pl2 = pl->lock.object;
		shoot_time = (int32_t) (pl->lock.distance / (pl->shot_speed + 1));
		dx = (int32_t) (pl2->pos.x + pl2->vel.x * shoot_time);
		dy = (int32_t) (pl2->pos.y + pl2->vel.y * shoot_time);
		/*-BA Also allow for our own momentum. */
		dx -= (int32_t) (pl->vel.x * shoot_time);
		dy -= (int32_t) (pl->vel.y * shoot_time);

		if (Check_robot_target(pl, dx, dy, RM_ATTACK) && !BIT(
				my_data->longterm_mode, FETCH_TREASURE
						| TARGET_KILL | NEED_FUEL)) {
			return;
		}
	}
	if (BIT(World.rules->mode, TEAM_PLAY) && World.NumTreasures > 0
			&& !navigate_checked && !BIT(my_data->longterm_mode,
			TARGET_KILL | NEED_FUEL)) {
		navigate_checked = true;
		if (Ball_handler(pl))
			return;
	}
	if (item_i >= 0 && !harvest_checked && item_dist < 12 * BLOCK_SZ) {

		if (item_imp != ROBOT_IGNORE_ITEM) {
			dx = Obj[item_i]->pos.x;
			dx += (int32_t) (Obj[item_i]->vel.x * (ABS(dx - pl->pos.x)
					/ my_data->robot_normal_speed));
			dy = Obj[item_i]->pos.y;
			dy += (int32_t) (Obj[item_i]->vel.y * (ABS(dy - pl->pos.y)
					/ my_data->robot_normal_speed));

			if (Check_robot_target(pl, dx, dy, RM_HARVEST)) {
				return;
			}
		}
	}

	if (Check_robot_hunt(pl)) {
		if (playerShielding == 0 && playerStartsShielded != 0 && BIT(
				pl->have, OBJ_SHIELD)) {
			SET_BIT(pl->used, OBJ_SHIELD);
		}
		return;
	}

	if (Robot_default_play_check_map(pl) == 1) {
		return;
	}

	if (playerShielding == 0 && playerStartsShielded != 0 && BIT(pl->have,
			OBJ_SHIELD)) {
		SET_BIT(pl->used, OBJ_SHIELD);
	}

	x = OBJ_X_IN_BLOCKS(pl);
	y = OBJ_Y_IN_BLOCKS(pl);
	x_speed = pl->vel.x;
	y_speed = pl->vel.y;

	if (y_speed < (-my_data->robot_normal_speed) || (my_data->robot_count
			% 64) < 32) {

		my_data->robot_mode = RM_ROBOT_CLIMB;
		pl->turnspeed = MAX_PLAYER_TURNSPEED / 2;
		pl->power = MAX_PLAYER_POWER / 2;
		if (ABS(pl->dir - RES / 4) > RES / 16) {
			pl->turnacc = (pl->dir < RES / 4 || pl->dir >= 3 * RES
					/ 4 ? pl->turnspeed : (-pl->turnspeed));
		}
		else {
			pl->turnacc = 0.0;
		}
		if (y_speed < my_data->robot_normal_speed / 2 && pl->velocity
				< my_data->robot_attack_speed)
			SET_BIT(pl->status, THRUSTING);
		else if (y_speed > my_data->robot_normal_speed)
			CLR_BIT(pl->status, THRUSTING);
		return;
	}
	my_data->robot_mode = RM_ROBOT_IDLE;
	pl->turnspeed = MAX_PLAYER_TURNSPEED / 2;
	pl->turnacc = 0;
	pl->power = MAX_PLAYER_POWER / 2;
	CLR_BIT(pl->status, THRUSTING);
	speed = LENGTH(x_speed, y_speed);
	if (speed < my_data->robot_normal_speed / 2)
		SET_BIT(pl->status, THRUSTING);
	else if (speed > my_data->robot_normal_speed)
		CLR_BIT(pl->status, THRUSTING);
}

