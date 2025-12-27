/* $Id: ship.c,v 1.4 2007/09/17 19:54:49 kps Exp $
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

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "error.h"
#include "objpos.h"
#include "netserver.h"

char ship_version[] = VERSION;


/******************************
 * Functions for ship movement.
 */

void Thrust(int ind)
{
    player_t		*pl = Players[ind];
    const int		min_dir = (int)(pl->dir + RES/2 - RES*0.2 - 1);
    const int		max_dir = (int)(pl->dir + RES/2 + RES*0.2 + 1);
    const DFLOAT	max_speed = 1 + (pl->power * 0.14);
    const int		max_life = 3 + (int)(pl->power * 0.35);
    static int		keep_rand;
    int			this_rand = (((keep_rand >>= 2)
					? (keep_rand)
					: (keep_rand = rand())) & 0x03);
    int			tot_sparks = (int)((pl->power * 0.15) + this_rand + 1);
    int		cx = pl->pos.cx + FLOAT_TO_CLICK(pl->ship->engine[pl->dir].x);
    int		cy = pl->pos.cy + FLOAT_TO_CLICK(pl->ship->engine[pl->dir].y);
    int			afterburners, alt_sparks;

    afterburners = pl->item[ITEM_AFTERBURNER];
    alt_sparks = afterburners
		    ? AFTER_BURN_SPARKS(tot_sparks-1, afterburners) + 1
		    : 0;

    Make_debris(
	/* pos.cx, pos.cy */ cx, cy,
	/* vel.x, vel.y   */ pl->vel.x, pl->vel.y,
	/* owner id       */ pl->id,
	/* owner team	  */ pl->team,
	/* kind           */ OBJ_SPARK,
	/* mass           */ THRUST_MASS,
	/* status         */ OWNERIMMUNE,
	/* color          */ RED,
	/* radius         */ 8,
	/* min,max debris */ tot_sparks-alt_sparks, tot_sparks-alt_sparks,
	/* min,max dir    */ min_dir, max_dir,
	/* min,max speed  */ 1.0, max_speed,
	/* min,max life   */ 3, max_life
	);

    Make_debris(
	/* pos.cx, pos.cy */ cx, cy,
	/* vel.x, vel.y   */ pl->vel.x, pl->vel.y,
	/* owner id       */ pl->id,
	/* owner team	  */ pl->team,
	/* kind           */ OBJ_SPARK,
	/* mass           */ THRUST_MASS * ALT_SPARK_MASS_FACT,
	/* status         */ OWNERIMMUNE,
	/* color          */ BLUE,
	/* radius         */ 8,
	/* min,max debris */ alt_sparks, alt_sparks,
	/* min,max dir    */ min_dir, max_dir,
	/* min,max speed  */ 1.0, max_speed,
	/* min,max life   */ 3, max_life
	);
}

void Record_shove(player_t *pl, player_t *pusher, long time)
{
    shove_t		*shove = &pl->shove_record[pl->shove_next];

    if (++pl->shove_next == MAX_RECORDED_SHOVES) {
	pl->shove_next = 0;
    }
    shove->pusher_id = pusher->id;
    shove->time = time;
}

/* Calculates the effect of a collision between two objects */
void Delta_mv(object_t *ship, object_t *obj)
{
    DFLOAT	vx, vy, m;

    m = ship->mass + ABS(obj->mass);
    vx = (ship->vel.x * ship->mass + obj->vel.x * obj->mass) / m;
    vy = (ship->vel.y * ship->mass + obj->vel.y * obj->mass) / m;
    if (ship->type == OBJ_PLAYER
	&& obj->id != -1
	&& BIT(obj->status, COLLISIONSHOVE)) {
	player_t *pl = (player_t *)ship;
	player_t *pusher = Players[GetInd[obj->id]];
	if (pusher != pl) {
	    Record_shove(pl, pusher, frame_loops);
	}
    }
    ship->vel.x = vx;
    ship->vel.y = vy;
    obj->vel.x = vx;
    obj->vel.y = vy;
}


/* took the inelastic ballpopper from ng-465 -pgm */

void Obj_repel(object_t *ship, object_t *obj2, int repel_dist)
{
  DFLOAT              xd, yd,
                      dvx1, dvy1,
                      dvx2, dvy2,
                      m;

    xd = WRAP_DX(obj2->pos.x - ship->pos.x);
    yd = WRAP_DY(obj2->pos.y - ship->pos.y);

    m = ship->mass + ABS(obj2->mass);
    dvx1 = (ship->vel.x * ship->mass + obj2->vel.x * obj2->mass) / m;
    dvy1 = (ship->vel.y * ship->mass + obj2->vel.y * obj2->mass) / m;
    dvx2 = dvx1;
    dvy2 = dvy1;

    if (ship->type == OBJ_PLAYER && obj2->id != -1) {
        player_t *pl = (player_t *)ship;
        player_t *pusher = Players[GetInd[obj2->id]];
        if (pusher != pl) {
            Record_shove(pl, pusher, frame_loops);
        }
    }

    if (obj2->type == OBJ_PLAYER && ship->id != -1) {
        player_t *pl = (player_t *)obj2;
        player_t *pusher = Players[GetInd[ship->id]];
        if (pusher != pl) {
            Record_shove(pl, pusher, frame_loops);
        }
    }

    ship->vel.x = dvx1;
    ship->vel.y = dvy1;
    
    obj2->vel.x = dvx2;
    obj2->vel.y = dvy2;
}



/*
 * Add fuel to fighter's tanks.
 * Maybe use more than one of tank to store the fuel.
 */
void Add_fuel(pl_fuel_t *ft, long fuel)
{
    if (ft->sum + fuel > ft->max)
	fuel = ft->max - ft->sum;
    else if (ft->sum + fuel < 0)
	fuel = -ft->sum;
    ft->sum += fuel;
    ft->tank[ft->current] += fuel;
}


/*
 * Move fuel from add-on tanks to main tank,
 * handle over and underflow of tanks.
 */
void Update_tanks(pl_fuel_t *ft)
{
    if (ft->num_tanks) {
	int  t, check;
	long low_level;
	long fuel;
	long *f;

	/* Set low_level to minimum fuel in each tank */
	low_level = ft->sum / (ft->num_tanks + 1) - 1;
	if (low_level < 0)
	    low_level = 0;
	if (TANK_REFILL_LIMIT < low_level)
	    low_level = TANK_REFILL_LIMIT;

	t = ft->num_tanks;
	check = MAX_TANKS<<2;
	fuel = 0;
	f = ft->tank + t;

	while (t>=0 && check--) {
	    long m = TANK_CAP(t);

	    /* Add the previous over/underflow and do a new cut */
	    *f += fuel;
	    if (*f > m) {
		fuel = *f - m;
		*f = m;
	    } else if (*f < 0) {
		fuel = *f;
		*f = 0;
	    } else
		fuel = 0;

	    /* If there is no over/underflow, let the fuel run to main-tank */
	    if (!fuel) {
		if (t
		    && t != ft->current
		    && *f >= low_level + REFUEL_RATE
		    && *(f-1) <= TANK_CAP(t-1) - REFUEL_RATE) {

		    *f -= REFUEL_RATE;
		    fuel = REFUEL_RATE;
		} else if (t && *f < low_level) {
		    *f += REFUEL_RATE;
		    fuel = -REFUEL_RATE;
		}
	    }
	    if (fuel && t == 0) {
	       t = ft->num_tanks;
	       f = ft->tank + t;
	    } else {
		t--;
		f--;
	    }
	}
	if (!check) {
	    error("fuel problem");
	    fuel = ft->sum;
	    ft->sum =
	    ft->max = 0;
	    t = 0;
	    while (t <= ft->num_tanks) {
		if (fuel) {
		    if (fuel>TANK_CAP(t)) {
			ft->tank[t] = TANK_CAP(t);
			fuel -= TANK_CAP(t);
		    } else {
			ft->tank[t] = fuel;
			fuel = 0;
		    }
		    ft->sum += ft->tank[t];
		} else
		    ft->tank[t] = 0;
		ft->max += TANK_CAP(t);
		t++;
	    }
	}
    } else
	ft->tank[0] = ft->sum;
}


void Make_wreckage(
    /* pos.cx, pos.cy   */ int    cx,           int cy,
    /* vel.x, vel.y     */ DFLOAT velx,         DFLOAT vely,
    /* owner id         */ int    id,
    /* owner team	*/ u_short team,
    /* min,max mass     */ DFLOAT min_mass,     DFLOAT max_mass,
    /* total mass       */ DFLOAT total_mass,
    /* status           */ long   status,
    /* color            */ int    color,
    /* max wreckage     */ int    max_wreckage,
    /* min,max dir      */ int    min_dir,      int    max_dir,
    /* min,max speed    */ DFLOAT min_speed,    DFLOAT max_speed,
    /* min,max life     */ int    min_life,     int    max_life
)
{
    object_t		*wreckage;
    int			i, life, size;
    DFLOAT		mass, sum_mass = 0.0;

    if (!useWreckage) {
	return;
    }
    cx = WRAP_XCLICK(cx);
    cy = WRAP_YCLICK(cy);
    if (cx < 0 || cx >= World.cwidth || cy < 0 || cy >= World.cheight) {
	return;
    }
    if (max_life < min_life)
	max_life = min_life;
    if (ShotsLife >= intGameSpeed) {
	if (min_life > ShotsLife) {
	    min_life = ShotsLife;
	    max_life = ShotsLife;
	}
    }

    if (min_speed * max_life > World.hypotenuse)
	min_speed = World.hypotenuse / max_life;
    if (max_speed * min_life > World.hypotenuse)
	max_speed = World.hypotenuse / min_life;
    if (max_speed < min_speed)
	max_speed = min_speed;

    if (max_wreckage > MAX_TOTAL_SHOTS - NumObjs) {
	max_wreckage = MAX_TOTAL_SHOTS - NumObjs;
    }

    for (i = 0; i < max_wreckage && sum_mass < total_mass; i++, NumObjs++) {
	DFLOAT		speed;
	int		dir, radius;

	wreckage = Obj[NumObjs];
	wreckage->color = color;
	wreckage->id = id;
	wreckage->team = team;
	wreckage->type = OBJ_WRECKAGE;

	/* Position */
	Object_position_init_clicks(wreckage, cx, cy);

	/* Direction */
	dir = MOD2(min_dir + (int)(rfrac() * MOD2(max_dir - min_dir, RES)), RES);
	wreckage->dir = dir;

	/* Velocity and acceleration */
	speed = min_speed + rfrac() * (max_speed - min_speed);
	wreckage->vel.x = velx + tcos(dir) * speed;
	wreckage->vel.y = vely + tsin(dir) * speed;
	wreckage->acc.x = 0;
	wreckage->acc.y = 0;

	/* Mass */
	mass = min_mass + rfrac() * (max_mass - min_mass);
	if ( sum_mass + mass > total_mass )
	    mass = total_mass - sum_mass;
	wreckage->mass = mass;
	sum_mass += mass;
	if ( mass < min_mass ) {
	    NumObjs--;
	    break;
	}

	/* Lifespan  */
	life = (int)(min_life + rfrac() * (max_life - min_life) + 1);
	if (life * speed > World.hypotenuse) {
	    life = (long)(World.hypotenuse / speed);
	}
	wreckage->life = life;
	wreckage->fuselife = wreckage->life;

	/* Wreckage type, rotation, and size */
	wreckage->turnspeed = 0.02 + rfrac() * 0.35;
	wreckage->rotation = (int)(rfrac() * RES);
	size = (int) ( 256.0 * 1.5 * mass / total_mass );
	if ( size > 255 )
	    size = 255;
	wreckage->size = size;
	wreckage->info = rand();

	radius = wreckage->size * 16 / 256;
	if ( radius < 8 ) radius = 8;

	wreckage->spread_left = 0;
	wreckage->pl_range = radius;
	wreckage->pl_radius = radius;
	wreckage->status = status;
    }
}

/* Explode a fighter */
void Explode_fighter(int ind)
{
    player_t *pl = Players[ind];
    int min_debris, max_debris;

    min_debris = (int)(1 + (pl->fuel.sum / (8.0 * FUEL_SCALE_FACT)));
    max_debris = (int)(min_debris + (pl->mass * 2.0));
    /* reduce debris since we also create wreckage objects */
    min_debris >>= 1;
    max_debris >>= 1;

    Make_debris(
	/* pos.cx, pos.cy */ pl->pos.cx, pl->pos.cy,
	/* vel.x, vel.y   */ pl->vel.x, pl->vel.y,
	/* owner id       */ pl->id,
	/* owner team	  */ pl->team,
	/* kind           */ OBJ_DEBRIS,
	/* mass           */ 3.5,
	/* status         */ 0,
	/* color          */ RED,
	/* radius         */ 8,
	/* min,max debris */ min_debris, max_debris,
	/* min,max dir    */ 0, RES-1,
	/* min,max speed  */ 20.0, 20 + (((int)(pl->mass))>>1),
	/* min,max life   */ 5, (int)(5 + (pl->mass * 1.5))
	);

    if ( !BIT(pl->status, KILLED) )
	return;
    Make_wreckage(
	/* pos.cx, pos.cy   */ pl->pos.cx, pl->pos.cy,
	/* vel.x, vel.y     */ pl->vel.x, pl->vel.y,
	/* owner id         */ pl->id,
	/* owner team	    */ pl->team,
	/* min,max mass     */ MAX(pl->mass/8.0, 0.33), pl->mass,
	/* total mass       */ 2.0 * pl->mass,
	/* status           */ 0,
	/* color            */ WHITE,
	/* max wreckage     */ 10,
	/* min,max dir      */ 0, RES-1,
	/* min,max speed    */ 10.0, 10 + (((int)(pl->mass))>>1),
	/* min,max life     */ 5, (int)(5 + (pl->mass * 1.5))
	);

}

