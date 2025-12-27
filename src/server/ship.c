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
#include <math.h>

#include "version.h"
#include "commonproto.h"
#include "config.h"
#include "const.h"
#include "global.h"

#include "ship.h"
#include "error.h"
#include "objpos.h"
#include "netserver.h"

#include "player.h"
#include "map.h"
#include "player_inline.h"

char ship_version[] = VERSION;


void Player_thrust(player_t *pl)
{
	const int32_t min_dir = (int32_t) (pl->dir + ANGLE_RESOLUTION / 2 - ANGLE_RESOLUTION * 0.2 - 1);
	const int32_t max_dir = (int32_t) (pl->dir + ANGLE_RESOLUTION / 2 + ANGLE_RESOLUTION * 0.2 + 1);
	const DFLOAT max_speed = 1 + (pl->power * 0.14);
	const int32_t max_life = 3 + (int32_t) (pl->power * 0.35);
	static int32_t keep_rand;
	int32_t this_rand = (((keep_rand >>= 2) ? (keep_rand)
			: (keep_rand = rand())) & 0x03);
	int32_t tot_sparks = (int32_t) ((pl->power * 0.15) + this_rand + 1);
	objposition_t deb_pos;
	int32_t afterburners, alt_sparks;
	clpos_t pos;

	afterburners = pl->item[ITEM_AFTERBURNER];
	alt_sparks = afterburners ? AFTER_BURN_SPARKS(tot_sparks-1, afterburners) + 1 : 0;

	Position_simple_set(&pos,
			pl->pos.cx + FLOAT_TO_CLICK(pl->ship->engine[pl->dir].x),
			pl->pos.cy + FLOAT_TO_CLICK(pl->ship->engine[pl->dir].y));
	Position_set(&deb_pos, &pos);

	Make_debris(&deb_pos, pl->vel.x, pl->vel.y, pl, pl->team, OBJ_SPARK, THRUST_MASS, OWNERIMMUNE, RED, 8,
		tot_sparks - alt_sparks, tot_sparks - alt_sparks, min_dir, max_dir, 1.0, max_speed, 3, max_life);

	Make_debris(&deb_pos, pl->vel.x, pl->vel.y, pl, pl->team, OBJ_SPARK, THRUST_MASS * ALT_SPARK_MASS_FACT,
		OWNERIMMUNE, BLUE, 8, alt_sparks, alt_sparks, min_dir, max_dir, 1.0, max_speed, 3, max_life);
}

/* Calculates the recoil if a ship fires a shot */
//void Recoil(object_t *ship, object_t *shot)
//{
//	/* new code thanks to Uoti Urpala. */
//	ship->vel.x -= (((shot->vel.x - ship->vel.x) * shot->mass) / ship->mass);
//	ship->vel.y -= (((shot->vel.y - ship->vel.y) * shot->mass) / ship->mass);
//}

void Record_shove(player_t *pl, player_t *pusher, int32_t time)
{
	shove_t *shove = &pl->shove_record[pl->shove_next];

	if (++pl->shove_next == MAX_RECORDED_SHOVES) {
		pl->shove_next = 0;
	}
	shove->pusher_pl = pusher;
	shove->time = time;
}

/* Calculates the effect of a collision between two objects */
void Delta_mv(object_t *obj1, object_t *obj2)
{
	DFLOAT vx, vy, m;

	m = obj1->mass + ABS(obj2->mass);
	vx = (obj1->vel.x * obj1->mass + obj2->vel.x * obj2->mass) / m;
	vy = (obj1->vel.y * obj1->mass + obj2->vel.y * obj2->mass) / m;
	if (Object_is_type(obj1, OBJ_PLAYER) && obj2->owner && BIT(obj2->obj_status, COLLISIONSHOVE)) {
		player_t *pl = (player_t *) obj1;
		player_t *pusher = obj2->owner;
		if (pusher != pl) {
			Record_shove(pl, pusher, frame_loops);
		}
	}

	obj1->vel.x = vx;
	obj1->vel.y = vy;
	obj2->vel.x = vx;
	obj2->vel.y = vy;
}

/* took the inelastic ballpopper from ng-465 -pgm */
void Ship_object_repel(object_t *obj1, object_t *obj2, int32_t repel_dist)
{
	DFLOAT xd, yd, dvx1, dvy1, dvx2, dvy2, m;

	xd = WRAP_DX(obj2->pos.x - obj1->pos.x);
	yd = WRAP_DY(obj2->pos.y - obj1->pos.y);

	m = obj1->mass + ABS(obj2->mass);
	dvx1 = (obj1->vel.x * obj1->mass + obj2->vel.x * obj2->mass) / m;
	dvy1 = (obj1->vel.y * obj1->mass + obj2->vel.y * obj2->mass) / m;
	dvx2 = dvx1;
	dvy2 = dvy1;

	if (Object_is_type(obj1, OBJ_PLAYER) && obj2->owner) {
		player_t *pl = (player_t *) obj1;
		player_t *pusher = obj2->owner;
		if (pusher != pl) {
			Record_shove(pl, pusher, frame_loops);
		}
	}

	if (Object_is_type(obj2, OBJ_PLAYER) && obj1->owner) {
		player_t *pl = (player_t *) obj2;
		player_t *pusher = obj1->owner;
		if (pusher != pl) {
			Record_shove(pl, pusher, frame_loops);
		}
	}

	obj1->vel.x = dvx1;
	obj1->vel.y = dvy1;
	obj2->vel.x = dvx2;
	obj2->vel.y = dvy2;
}

void Make_wreckage(objposition_t *pos, DFLOAT velx, DFLOAT vely, player_t *pl, team_t *team, DFLOAT min_mass, DFLOAT max_mass,
	DFLOAT total_mass, int32_t status, int32_t color, int32_t max_wreckage, int32_t min_dir, int32_t max_dir,
	DFLOAT min_speed, DFLOAT max_speed, int32_t min_life, int32_t max_life)
{
	object_t *wreckage;
	int32_t i, life, size;
	DFLOAT mass, sum_mass = 0.0;

	if (!useWreckage) {
		return;
	}

	if (max_life < min_life) {
		max_life = min_life;
	}

	if (SHOT_LIFE_TICKS >= gameSpeed) {
		if (min_life > SHOT_LIFE_TICKS) {
			min_life = SHOT_LIFE_TICKS;
			max_life = SHOT_LIFE_TICKS;
		}
	}

	if (min_speed * max_life > World.hypotenuse)
		min_speed = World.hypotenuse / max_life;
	if (max_speed * min_life > World.hypotenuse)
		max_speed = World.hypotenuse / min_life;
	if (max_speed < min_speed)
		max_speed = min_speed;

	if (max_wreckage > MAX_TOTAL_OBJECTS - NumObjs) {
		max_wreckage = MAX_TOTAL_OBJECTS - NumObjs;
	}

	for (i = 0; i < max_wreckage && sum_mass < total_mass; i++) {
		DFLOAT speed;
		int32_t dir, radius;

		wreckage = Object_add();

		if (!wreckage) {
			return;
		}

		wreckage->color = color;
		wreckage->owner = pl;
		wreckage->team = team;
		wreckage->type = OBJ_WRECKAGE;

		/* Position */
		wreckage->pos = *pos;
//		Object_position_remember(wreckage);

		/* Direction */
		dir = MOD2(min_dir + (int32_t) (rfrac() * MOD2(max_dir - min_dir,
				ANGLE_RESOLUTION)), ANGLE_RESOLUTION);
		wreckage->dir = dir;

		/* Velocity and acceleration */
		speed = min_speed + rfrac() * (max_speed - min_speed);
		wreckage->vel.x = velx + tcos(dir) * speed;
		wreckage->vel.y = vely + tsin(dir) * speed;
		wreckage->acc.x = 0;
		wreckage->acc.y = 0;

		/* Mass */
		mass = min_mass + rfrac() * (max_mass - min_mass);
		if (sum_mass + mass > total_mass)
			mass = total_mass - sum_mass;
		wreckage->mass = mass;
		sum_mass += mass;

		/* Lifespan  */
		life = (int32_t) (min_life + rfrac() * (max_life - min_life) + 1);
		if (life * speed > World.hypotenuse) {
			life = (int32_t) (World.hypotenuse / speed);
		}
		wreckage->obj_life = life;
		wreckage->fuselife = wreckage->obj_life;

		/* Wreckage type, rotation, and size */
		wreckage->turnspeed = 0.02 + rfrac() * 0.35;
		wreckage->rotation = (int32_t) (rfrac() * ANGLE_RESOLUTION);
		size = (int32_t) (256.0 * 1.5 * mass / total_mass);
		if (size > 255)
			size = 255;
		wreckage->size = size;
		wreckage->info = rand();

		radius = wreckage->size * 16 / 256;
		if (radius < 8)
			radius = 8;

		wreckage->pl_range = radius;
		wreckage->pl_radius = radius;
		wreckage->obj_status = status;

		if (mass < min_mass) {
//			NumObjs--;
			break;
		}
	}
}

/* Explode a fighter */
void Player_explode(player_t *pl)
{
	int32_t min_debris, max_debris;

	min_debris = (int32_t) (1 + (pl->fuel.sum / (8.0 * FUEL_SCALE_FACT)));
	max_debris = (int32_t) (min_debris + (pl->mass * 2.0));
	/* reduce debris since we also create wreckage objects */
	min_debris >>= 1;
	max_debris >>= 1;

	Make_debris(&pl->pos, pl->vel.x, pl->vel.y, pl, pl->team, OBJ_DEBRIS, 3.5, 0, RED, 8,
		min_debris, max_debris, 0, ANGLE_RESOLUTION - 1, 20.0, 20 + (((int32_t) (pl->mass)) >> 1),
		5, (int32_t) (5 + (pl->mass * 1.5)));

	if (!Player_is_killed(pl)) {
		return;
	}

	Make_wreckage(&pl->pos, pl->vel.x, pl->vel.y, pl, pl->team, MAX(pl->mass/8.0, 0.33), pl->mass,
		2.0 * pl->mass, 0, WHITE, 10, 0, ANGLE_RESOLUTION - 1, 10.0, 10 + (((int32_t) (pl->mass)) >> 1),
		5, (int32_t) (5 + (pl->mass * 1.5)));
}

/* Create debris particles */
void Make_debris(objposition_t *pos, DFLOAT velx, DFLOAT vely, player_t *pl, team_t *team, int32_t type, DFLOAT mass,
	int32_t status, int32_t color, int32_t radius, int32_t min_debris, int32_t max_debris, int32_t min_dir, int32_t max_dir,
	DFLOAT min_speed, DFLOAT max_speed, int32_t min_life, int32_t max_life)
{
	object_t *debris;
	int32_t i, num_debris, life;

	if (max_life < min_life) {
		max_life = min_life;
	}
	if (SHOT_LIFE_TICKS >= gameSpeed) {
		if (min_life > SHOT_LIFE_TICKS) {
			min_life = SHOT_LIFE_TICKS;
			max_life = SHOT_LIFE_TICKS;
		}
		else if (max_life > SHOT_LIFE_TICKS) {
			max_life = SHOT_LIFE_TICKS;
		}
	}
	if (min_speed * max_life > World.hypotenuse) {
		min_speed = World.hypotenuse / max_life;
	}
	if (max_speed * min_life > World.hypotenuse) {
		max_speed = World.hypotenuse / min_life;
	}
	if (max_speed < min_speed) {
		max_speed = min_speed;
	}

	num_debris = min_debris + (int32_t) (rfrac() * (max_debris - min_debris));
	if (num_debris > MAX_TOTAL_OBJECTS - NumObjs) {
		num_debris = MAX_TOTAL_OBJECTS - NumObjs;
	}
	for (i = 0; i < num_debris; i++) {
		DFLOAT speed, dx, dy, diroff;
		int32_t dir, dirplus;

		debris = Object_add();

		if (!debris) {
			return;
		}

		debris->color = color;
		debris->owner = pl;
		debris->team = team;
		debris->pos = *pos;

		dir = MOD2(min_dir + (int32_t) (rfrac() * (max_dir - min_dir)), ANGLE_RESOLUTION);
		dirplus = MOD2(dir + 1, ANGLE_RESOLUTION);
		diroff = rfrac();
		dx = tcos(dir) + (tcos(dirplus) - tcos(dir)) * diroff;
		dy = tsin(dir) + (tsin(dirplus) - tsin(dir)) * diroff;
		speed = min_speed + rfrac() * (max_speed - min_speed);
		debris->vel.x = velx + dx * speed;
		debris->vel.y = vely + dy * speed;
		debris->acc.x = 0;
		debris->acc.y = 0;
		debris->dir = dir;
		debris->mass = mass;
		debris->type = type;
		life = (int32_t) (min_life + rfrac() * (max_life - min_life) + 1);
		if (life * speed > World.hypotenuse) {
			life = (int32_t) (World.hypotenuse / speed);
		}
		debris->obj_life = life;
		debris->fuselife = life;
		debris->pl_range = radius;
		debris->pl_radius = radius;
		debris->obj_status = status;
	}
}
