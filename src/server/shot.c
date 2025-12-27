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
#include "xpconfig.h"
#include "debug.h"
#include "const.h"
#include "global.h"
#include "proto.h"

#include "score.h"
#include "objpos.h"
#include "netserver.h"
#include "error.h"

#include "player.h"
#include "map.h"
#include "player_inline.h"
#include "frame.h"
#include "object.h"
#include "rank.h"

char shot_version[] = VERSION;

/*
 * Functions for shots.
 */

static object_t *objArray;


/** @brief Allocate memory for a specified number of objects
 * Pointers to object structures will be available through the @ref Obj array.
 *
 * @param number	requested number of objects
 */
void Shots_allocate(int32_t number)
{
	object_t *x;
	int32_t i;

	x = (object_t *) calloc(number, sizeof(object_t));
	if (!x) {
		error("Not enough memory for shots.");
		exit(1);
	}

	objArray = x;
	for (i = 0; i < number; i++) {
		x->owner = NULL;
		x->id = i;
		Obj[i] = x++;
	}
}

void Shots_free(void)
{
	if (objArray != NULL) {
		free(objArray);
		objArray = NULL;
	}
}

void Ball_treasure_add(treasure_t *t)
{
	object_t *ball;

	/* we want to place the ball exactly at the bottom of the treasure box, not in the middle
	 * TODO: the offset shouldn't be hardcoded here */
	clpos_t pos;

	ball = Object_add();

	if (!ball) {
		return;
	}

	t->have = true;

	ball->length = ballConnectorLength;
	ball->obj_life = MAX_OBJECT_LIFE;

	ball->mass = 50;
	ball->vel.x = 0; /* make the ball stuck a little */
	ball->vel.y = 0; /* longer to the ground */
	ball->acc.x = 0;
	ball->acc.y = 0;
	ball->dir = 0;
	Position_simple_set(&pos, t->pos.cx, t->pos.cy - PIXEL_TO_CLICK(7));
	Position_set(&ball->pos, &pos);
	Position_copy(&ball->pos_interp, &ball->pos);
//	Object_position_remember(ball);
	ball->owner = NULL;
	ball->team = t->team;
	ball->type = OBJ_BALL;
	ball->color = WHITE;
	ball->pl_range = BALL_RADIUS;
	ball->pl_radius = BALL_RADIUS;
	ball->obj_status = 0;
	ball->treasure = t;
}

void Shot_add(player_t *pl)
{
	int32_t fuse;
	clpos_t shotpos;
	object_t *shot;
	DFLOAT speed = pl->shot_speed;
	int32_t dir = pl->dir;

	if (main_loops_slow < (pl->shot_time + fireRepeatRate)) {
		return;
	}

	if (pl->shots >= pl->shot_max || Player_uses_property(pl, USES_SHIELD)) {
		return;
	}


	shot = Object_add();

	if (!shot) {
		return;
	}

	/* add fired shots*/
	pl->shot_time = main_loops_slow;

	/*
	 * Calculate the maximum time it would take to cross one ships width,
	 * don't fuse the shot/missile/torpedo for the owner only until that
	 * time passes.  This is a hack to stop various odd missile and shot
	 * mounting points killing the player when they're firing.
	 */

	fuse = (int32_t) ((2.0 * (DFLOAT) SHIP_SZ) / speed + 1.0);

	shot->obj_life = SHOT_LIFE_TICKS;
	shot->fuselife = shot->obj_life - fuse;
	shot->max_speed = SPEED_LIMIT;
	shot->info = 0;
	shot->type = OBJ_SHOT;
	shot->owner = pl;
	shot->team = pl->team;
	shot->color = (pl ? pl->color : WHITE);

	shotpos.cx = pl->pos.cx + FLOAT_TO_CLICK(pl->ship->m_gun[pl->dir].x);
	shotpos.cy = pl->pos.cy + FLOAT_TO_CLICK(pl->ship->m_gun[pl->dir].y);

	Position_set(&shot->pos, &shotpos);
//	Object_position_remember(shot);

	shot->acc.x = shot->acc.y = 0;

	shot->vel.x = pl->vel.x + tcos(dir) * speed;
	shot->vel.y = pl->vel.y + tsin(dir) * speed;

	shot->obj_status = 0;
	shot->dir = dir;
	shot->pl_range = 0;
	shot->pl_radius = 0;

	Rank_fire_shot(pl);
}

/** \brief Moves a ball
 *
 * The new ball movement code since XPilot version 3.4.0 as made
 * by Bretton Wade.  The code was submitted in context diff format
 * by Mark Boyns.  Here is a an excerpt from a post in
 * rec.games.computer.xpilot by Bretton Wade dated 27 Jun 1995:
 *
 *     If I'm not mistaken (not having looked very closely at the code
 *     because I wasn't sure what it was trying to do), the original move_ball
 *     routine was trying to model a Hook's law spring, but squared the
 *     deformation term, which would lead to exagerated behavior as the spring
 *     stretched too far. Not really a divide by zero, but effectively producing
 *     large numbers.
 *
 *     When I coded up the spring myself, I found that I could recreate the
 *     effect by using a VERY strong spring. This can be defeated, however, by
 *     damping. Specifically, If you compute the critical damping factor, then
 *     you could have the cable always be the correct length. This makes me
 *     wonder how to decide when the cable snaps.
 *
 *     I chose a relatively strong spring, and a small damping factor, to make
 *     for a nice realistic bounce when you grab at the treasure. It also gives a
 *     fairley close approximation to the "normal" feel of the treasure.
 *
 *     I modeled the cable as having zero mass, or at least insignificant mass as
 *     compared to the ship and ball. This greatly simplifies the math, and leads
 *     to the conclusion that there will be no change in velocity when the cable
 *     breaks. You can check this by integrating the momentum along the cable,
 *     and the ship or ball.
 *
 *     If you assume that the cable snaps in the middle, then half of the
 *     potential energy goes to each object attached. However, as you said, the
 *     total momentum of the system cannot change. Because the weight of the
 *     cable is small, the vast majority of the potential energy will become
 *     heat. I've had two physicists verify this for me, and they both worked
 *     really hard on the problem because they found it interesting.
 *
 * End of post.
 *
 * Changes since then:
 *
 * Comment from people was that the string snaps too soon.
 * Changed the value (max_spring_ratio) at which the string snaps
 * from 0.25 to 0.30.  Not sure if that helps enough, or too much.
 *
 * \param ball	ball structure
 */
void Ball_move(object_t *ball)
{
	player_t *pl = ball->owner;
	vector_t D;
	DFLOAT length, force, ratio, accell, cosine, pl_damping, ball_damping;
	DFLOAT k = ballConnectorSpringConstant;
	DFLOAT b = ballConnectorDamping;
	DFLOAT max_spring_ratio = maxBallConnectorRatio;

	/* compute the normalized vector between the ball and the player */
	if (Frame_is_real()) {
		D.x = WRAP_DX(pl->pos.x - ball->pos.x);
		D.y = WRAP_DY(pl->pos.y - ball->pos.y);
	}
	else {
		D.x = WRAP_DX(pl->pos_interp.x - ball->pos_interp.x);
		D.y = WRAP_DY(pl->pos_interp.y - ball->pos_interp.y);
	}

	length = VECTOR_LENGTH(D);
	if (length > 0.0) {
		D.x /= length;
		D.y /= length;
	}
	else {
		D.x = D.y = 0.0;
	}

	/* compute the ratio for the spring action */
	ratio = (ballConnectorLength - length) / (DFLOAT) ballConnectorLength;

	/* compute force by spring for this length */
	force = k * ratio;

	if (Frame_is_real()) {
		/* if the tether is too long or too short, release it */
		if (ABS(ratio) > max_spring_ratio) {
			Ball_detach(pl, ball);
			return;
		}
	}

	if (Frame_is_real()) {
		ball->length = length;
	}
	else {
		ball->length_interp = length;
	}

	/* compute damping for player */
	if (Frame_is_real()) {
		cosine = (pl->vel.x * D.x) + (pl->vel.y * D.y);
	}
	else {
		cosine = (pl->vel_interp.x * D.x) + (pl->vel_interp.y * D.y);
	}

	pl_damping = -b * cosine;

	/* compute damping for ball */
	if (Frame_is_real()) {
		cosine = (ball->vel.x * -D.x) + (ball->vel.y * -D.y);
	}
	else {
		cosine = (ball->vel_interp.x * -D.x) + (ball->vel_interp.y * -D.y);
	}

	ball_damping = -b * cosine;

	/* compute accelleration for player, assume t = 1 */
	accell = (force + pl_damping + ball_damping) / pl->mass;

	if (Frame_is_real()) {
		pl->vel.x += D.x * accell;
		pl->vel.y += D.y * accell;
	}
	else {
		pl->vel_interp.x += D.x * accell * ticksPerFrame;
		pl->vel_interp.y += D.y * accell * ticksPerFrame;
	}

	/* compute accelleration for ball, assume t = 1 */
	accell = (force + ball_damping + pl_damping) / ball->mass;

	if (Frame_is_real()) {
		ball->vel.x += -D.x * accell;
		ball->vel.y += -D.y * accell;
	}
	else {
		ball->vel_interp.x += -D.x * accell * ticksPerFrame;
		ball->vel_interp.y += -D.y * accell * ticksPerFrame;
	}
}

void Ball_detach(player_t *pl, object_t *ball)
{
	int32_t i, cnt;

	/* Interrupt the non-solid connector, if present */
	if (ball == NULL || ball == pl->ball_tmp) {
		pl->ball_tmp = NULL;
		Player_disable_property(pl, USES_CONNECTOR);
	}

	if (Player_has_property(pl, HAS_BALL)) {
		for (cnt = i = 0; i < NumObjs; i++) {
			if (Object_is_type(Obj[i], OBJ_BALL) &&
					Object_is_attached(Obj[i]) &&
					Obj[i]->owner == pl) {
				if (ball == NULL || ball == Obj[i]) {
					/* Don't reset owner so you can throw balls */
					Object_set_attached(Obj[i], false);
				}
				else {
					cnt++;
				}
			}
		}
		if (cnt == 0) {
			Player_lose_property(pl, HAS_BALL);
		}
	}
}
