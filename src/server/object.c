/*
 * object.c
 *
 *  Created on: Sep 12, 2009
 *      Author: rotunda
 */

#include <stdio.h>
#include <string.h>

#include "global.h"
#include "debug.h"
#include "object.h"
#include "map.h"
#include "collision.h"
#include "frame.h"

/** @brief Number of objects in game (including: balls, shots, pieces of
 * debris)
 *
 * This variable is modified only in @ref Make_debris, @ref Make_wreckage,
 * @ref Make_treasure_ball, @ref Fire_normal_shots, @ref Delete_object
 */
int32_t NumObjs = 0;
object_t *Obj[MAX_TOTAL_OBJECTS];


/** \brief Counts missing treasure of specified team
 * \param team	team, unspecified if NULL
 * \return number of missing treasures
 */
int32_t Object_count_treasures_missing(team_t *team)
{
	int32_t i;
	int32_t count = 0;

	for (i = 0; i < World.NumTreasures; i++) {
		if (!World.treasures[i].have) {
			if (team) {
				if (World.treasures[i].team == team) {
					count++;
				}
			}
			else {
				count++;
			}
		}
	}

	return count;
}

/* Remove objects that timed out.
 *
 * A ball can time out for three reasons:
 * - it has been loose for long enough (doesn't happen in normal games)
 * - it has been replaced
 * - somebody cashed it
 * In all cases such a ball should be recreated.
 */
void Objects_remove_timed_out(void)
{
	int32_t i;
	object_t *obj;
	int32_t type;

	for (i = NumObjs - 1; i >= 0; i--) {
		obj = Obj[i];
		type = obj->type;

		/*
		 * Count the life of sparks or debris in terms of ticks rather than frames.
		 * This dirty hack preserves the old (fxi v1.4.2 and xpilot v4.5.4) behaviour of sparks).
		 */
		if (!Frame_is_real() && (type == OBJ_SPARK || type == OBJ_DEBRIS)) {
			continue;
		}

		obj->obj_life--;

		/* Increment the ball loose frame counter */
		if (type == OBJ_BALL && ! obj->treasure->have) {
			obj->loose_count++;

			if (Frame_is_real()) {
				obj->loose_count_ticks++;
			}
		}

		/* object has not timed out */
		if (obj->obj_life > 0) {
			continue;
		}

		/* object was destroyed */
		else {
			treasure_t *t = obj->treasure;

			Object_remove(obj);

			/* Restore balls */
			if (type ==  OBJ_BALL) {
				Ball_treasure_add(t);
			}
		}
	}
}

/* Removes shot from array */
void Object_remove(object_t *obj)
{
	player_t *pl;
	int32_t i;

	switch (obj->type) {

	case OBJ_SPARK:
	case OBJ_DEBRIS:
	case OBJ_WRECKAGE:
		break;

	case OBJ_BALL:
		/* Detach the ball or non-solid connector */
		if (Object_is_attached(obj)) {
			Ball_detach(obj->owner, obj);
		}
		else {
			/*
			 * Maybe some player is still busy trying to connect to this ball.
			 */
			for (i = 0; i < NumPlayers; i++) {
				if (Players[i]->ball_tmp == obj) {
					Players[i]->ball_tmp = NULL;
				}
			}
		}

		obj->treasure->have = false;
		break;

	/* Shots related to a player. */
	case OBJ_SHOT:
		if (!obj->owner) {
			break;
		}
		pl = obj->owner;
		if (Object_is_type(obj, OBJ_SHOT)) {
			if (--pl->shots <= 0) {
				pl->shots = 0;
			}
		}
		break;

	default:
		xpprintf("%s Attempted to remove object of unknown type %d.\n", showtime(), obj->type);
		ASSERT(0)
		break;
	}


	NumObjs--;

	/* Fix pointers */
	Obj[obj->id] = Obj[NumObjs];
	Obj[NumObjs] = obj;

	/* Fix indices */
	Obj[obj->id]->id = obj->id;
	Obj[NumObjs]->id = NumObjs; /* theoretically unnecessary */

	/* Wipe out the remaining data, but preserve the object ID */
	memset(obj, 0, sizeof(object_t));
	obj->id = NumObjs;
}

object_t *Object_add(void)
{
	if (NumObjs >= MAX_TOTAL_OBJECTS) {
		return NULL;
	}

	return Obj[NumObjs++];
}

/*
 * Time out all objects (balls, shots, debris, sparks) after a round end
 */
void Objects_time_out(void)
{
	int32_t i;

	for (i = 0; i < NumObjs; i++) {
		Object_expire(Obj[i]);
	}
}

void Objects_interpolation_init(void)
{
	int32_t i;
	object_t *obj;

	for (i = 0; i < NumObjs; i++) {
		obj = Obj[i];
		Position_copy(&obj->pos_interp, &obj->pos);
		obj->vel_interp.x = obj->vel.x;
		obj->vel_interp.y = obj->vel.y;
	}
}
