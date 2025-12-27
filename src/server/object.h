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

#ifndef	OBJECT_H
#define	OBJECT_H

#include "structs.h"
#include "serverconst.h"
#include "keys.h"
#include "bit.h"
#include "draw.h"
#include "item.h"
#include "objpos.h"

/*
 * Different types of objects, including player.
 * Robots and tanks are players but have an additional type_ext field.
 * Smart missile, heatseeker and torpedo can be merged into missile.
 */
#define OBJ_TYPEBIT(type)       (1U<<(type))

#define OBJ_PLAYER              0
#define OBJ_DEBRIS              1
#define OBJ_SPARK               2
#define OBJ_BALL                3
#define OBJ_SHOT                4
#define OBJ_SMART_SHOT          5
#define OBJ_MINE                6
#define OBJ_TORPEDO             7
#define OBJ_HEAT_SHOT           8
#define OBJ_PULSE               9
#define OBJ_ITEM                10
#define OBJ_WRECKAGE            11
#define OBJ_ASTEROID            12
#define OBJ_CANNON_SHOT         13

#define OBJ_PLAYER_BIT          OBJ_TYPEBIT(OBJ_PLAYER)
#define OBJ_DEBRIS_BIT          OBJ_TYPEBIT(OBJ_DEBRIS)
#define OBJ_SPARK_BIT           OBJ_TYPEBIT(OBJ_SPARK)
#define OBJ_BALL_BIT            OBJ_TYPEBIT(OBJ_BALL)
#define OBJ_SHOT_BIT            OBJ_TYPEBIT(OBJ_SHOT)
#define OBJ_SMART_SHOT_BIT      OBJ_TYPEBIT(OBJ_SMART_SHOT)
#define OBJ_MINE_BIT            OBJ_TYPEBIT(OBJ_MINE)
#define OBJ_TORPEDO_BIT         OBJ_TYPEBIT(OBJ_TORPEDO)
#define OBJ_HEAT_SHOT_BIT       OBJ_TYPEBIT(OBJ_HEAT_SHOT)
#define OBJ_PULSE_BIT           OBJ_TYPEBIT(OBJ_PULSE)
#define OBJ_ITEM_BIT            OBJ_TYPEBIT(OBJ_ITEM)
#define OBJ_WRECKAGE_BIT        OBJ_TYPEBIT(OBJ_WRECKAGE)
#define OBJ_ASTEROID_BIT        OBJ_TYPEBIT(OBJ_ASTEROID)
#define OBJ_CANNON_SHOT_BIT     OBJ_TYPEBIT(OBJ_CANNON_SHOT)

/*
 * Possible object status bits.
 */
#define GRAVITY                 (1U<<0)
#define WARPING                 (1U<<1)
#define WARPED                  (1U<<2)
#define CONFUSED                (1U<<3)
#define FROMCANNON              (1U<<4)         /* Object from cannon */
#define ATTACHED		(1L<<5)		/* Ball is being carried by a player */
#define THRUSTING               (1U<<6)         /* Engine is thrusting */
#define OWNERIMMUNE             (1U<<7)         /* Owner is immune to object */
#define NOEXPLOSION             (1U<<8)         /* No recreate explosion */
#define COLLISIONSHOVE          (1U<<9)         /* Collision counts as shove */
#define RANDOM_ITEM             (1U<<10)        /* Item shows up as random */
#define FROMBOUNCE		(1L<<11)	/* Spark from wall bounce */

/*
 * Some object types are overloaded.
 */
#define OBJ_EXT_ROBOT		(1U<<2)

#define LOCK_NONE		0x00	/* No lock */
#define LOCK_PLAYER		0x01	/* Locked on player */
#define LOCK_VISIBLE		0x02	/* Lock information was on HUD */
/* computed just before frame shown */
/* and client input checked */
#define LOCKBANK_MAX		4	/* Maximum number of locks in bank */

#define NOT_CONNECTED		(-1)

#define Object_update_speed(o_)						\
    {									\
	(o_)->vel.x += (o_)->acc.x;					\
	(o_)->vel.y += (o_)->acc.y;					\
    }

#define OBJECT_BASE						\
	int8_t color; /* Color of object */			\
	uint8_t dir; /* Direction of acceleration */		\
	int32_t id; /* ID of the object */			\
	team_t *team; /* Team of player or cannon */		\
								\
	objposition_t pos; /* World coordinates */		\
	ipos_t prevpos; /* Object's previous position... */	\
	vector_t vel;						\
	vector_t acc;						\
	objposition_t pos_interp;				\
	vector_t vel_interp;					\
	vector_t acc_interp;					\
								\
	DFLOAT max_speed;					\
	DFLOAT mass;						\
	uint8_t type;						\
	int32_t info; /* Miscellaneous info (e.g. wreckage) */	\
	int32_t obj_status; /* gravity, thrusting, etc. */

struct _object {
	OBJECT_BASE

	/* up to here all object types (including players!) should be the same. */

	/*
	 * Number of frames left to live
	 * NOTE: the life of sparks and debris is counted in ticks rather than frames.
	 */
	int32_t obj_life;

	object_t *cell_prev;	/* previous object in cell, NULL if this object was added first */
	object_t *cell_next;	/* next object in cell, NULL if this object was added to the cell last */

	player_t *owner; /* Whose object is this ? */
	int32_t pl_range; /* distance to player for collision. */
	int32_t pl_radius; /* distance to player for hit. */

	/* ball-related */
	DFLOAT length; /* Distance between ball and player */
	DFLOAT length_interp;
	treasure_t *treasure; /* Which treasure does ball belong */
	int32_t loose_count;	/* Number of frames since the ball was taken out of its box */
	int32_t loose_count_ticks;	/* Number of ticks since the ball was taken out of its box */

	/* shot-, debris- and wreckage-related */
	int32_t fuselife; /* Ticks left when considered fused */

	/* wreckage-related */
	uint8_t size; /* Size of object (wreckage) */
	DFLOAT turnspeed; /* for missiles only */
	uint8_t rotation; /* Rotation direction */
};

/*
 * Shove-information.
 *
 * This is for keeping a record of the last N times the player was shoved,
 * for assigning wall-smash-blame, where N=MAX_RECORDED_SHOVES.
 */
#define MAX_RECORDED_SHOVES 4


typedef struct {
	player_t *pusher_pl;
	int32_t time;
} shove_t;

struct robot_data;

static inline bool Object_is_type(object_t *obj, uint8_t type)
{
	return (obj->type == type) ? true : false;
}

/** \brief Checks if object is attached. Concerns only balls for now.
 * \param obj	object
 * \return true - object is attached, false - object is not attached
 */
static inline bool Object_is_attached(object_t *obj)
{
	return BIT(obj->obj_status, ATTACHED) ? true : false;
}

static inline void Object_set_attached(object_t *obj, bool mode)
{
	if (mode) {
		SET_BIT(obj->obj_status, ATTACHED);
	}
	else {
		CLR_BIT(obj->obj_status, ATTACHED);
	}
}

static inline void Object_expire(object_t *obj)
{
	obj->obj_life = 0;
}

static inline bool Object_is_expired(object_t *obj)
{
	return (obj->obj_life <= 0) ? true : false;
}

int32_t Object_count_treasures_missing(team_t *team);
void Objects_remove_timed_out(void);
void Object_remove(object_t *obj);
object_t *Object_add(void);
void Objects_time_out(void);

void Ball_treasure_add(treasure_t *t);
void Ball_detach(player_t *pl, object_t *ball);
void Shot_add(player_t *pl);

void Shots_allocate(int32_t number);
void Shots_free(void);
void Ball_move(object_t *ball);

void Objects_interpolation_init(void);

#endif
