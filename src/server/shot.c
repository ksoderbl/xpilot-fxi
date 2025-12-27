/* $Id: shot.c,v 4.9 2000/03/24 14:21:52 bert Exp $
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
#include <math.h>

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "xpconfig.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "score.h"
#include "objpos.h"
#include "netserver.h"
#include "error.h"

char shot_version[] = VERSION;

/***********************
 * Functions for shots.
 */

static object *objArray;

void Alloc_shots(int number)
{
    object		*x;
    int			i;

    x = (object *) calloc(number, sizeof(object));
    if (!x) {
	error("Not enough memory for shots.");
	exit(1);
    }

    objArray = x;
    for (i = 0; i < number; i++) {
	x->owner = -1;
	Obj[i] = x++;
    }
}


void Free_shots(void)
{
    if (objArray != NULL) {
	free(objArray);
	objArray = NULL;
    }
}


void Make_treasure_ball(int treasure)
{
    object *ball;
    treasure_t *t = &(World.treasures[treasure]);
   DFLOAT	x = (t->pos.x + 0.5) * BLOCK_SZ,
		y = (t->pos.y * BLOCK_SZ) + 10;

    if (t->have) {
	xpprintf ("%s Failed Make_treasure_ball(treasure=%d):\n", showtime(), treasure);
	xpprintf ("\ttreasure: destroyed = %d, team = %d, have = %d\n",
		t->destroyed, t->team, t->have);
	return;
    }
    t->have = true;

    ball = Obj[NumObjs];

    ball->length = ballConnectorLength;
    ball->life = LONG_MAX;
    ball->mass = 50;
    ball->vel.x = 0;	  	/* make the ball stuck a little */
    ball->vel.y = 0;		/* longer to the ground */
    ball->acc.x = 0;
    ball->acc.y = 0;
    ball->dir = 0;
    Object_position_init_pixels(ball, x, y);
    ball->id = ball->owner = -1;
    ball->team = t->team;
    ball->type = OBJ_BALL;
    ball->color = WHITE;
    ball->count = 0;
    ball->pl_range = BALL_RADIUS;
    ball->pl_radius = BALL_RADIUS;
    ball->status = RECREATE;
    ball->treasure = treasure;
    NumObjs++;
}

void Fire_main_shot(int ind, int type, int dir)
{
    player *pl = Players[ind];
    DFLOAT x,
	  y;

    if (pl->shots >= pl->shot_max || BIT(pl->used, OBJ_SHIELD))
	return;

    x = pl->pos.x + pl->ship->m_gun[pl->dir].x;
    y = pl->pos.y + pl->ship->m_gun[pl->dir].y;

    Fire_general_shot(ind, pl->team, 0, x, y, type, dir,
		      pl->shot_speed, -1);
}

void Fire_general_shot(int ind, u_short team, bool cannon, DFLOAT x, DFLOAT y,
		       int type, int dir, DFLOAT speed, int target)
{
    player		*pl = (ind == -1 ? NULL : Players[ind]);
    int			life, fuse = 0,
			lock = 0,
			status = 0,
			i,
			pl_range,
			pl_radius,
			rack_no = 0,
			racks_left = 0,
			r,
			on_this_rack = 0,
			side = 0,
			fired = 0;
    DFLOAT		mass,
			turnspeed = 0,
			max_speed = SPEED_LIMIT, angle;

    vector		mv;
    position		shotpos;

    if (NumObjs >= MAX_TOTAL_SHOTS)
	return;

    if (pl) {
	mass = pl->shot_mass;
	life = pl->shot_life;
    } else {
	mass = ShotsMass;
	life = ShotsLife;
    }

    switch (type) {
    default:
	return;

    case OBJ_SHOT:
	pl_range = pl_radius = 0;
	if (pl) {
	    if (pl->fuel.sum < -ED_SHOT)
		return;
	    Add_fuel(&(pl->fuel), (long)(ED_SHOT));
	    Rank_fire_shot(pl);
	}
	break;
    }


 /*
  * Calculate the maximum time it would take to cross one ships width,
  * don't fuse the shot/missile/torpedo for the owner only until that
  * time passes.  This is a hack to stop various odd missile and shot
  * mounting points killing the player when they're firing.
  */
    fuse += (int)((2.0 * (DFLOAT)SHIP_SZ) / speed + 1.0);
    
    for (r = 0, i = 0; i < 1; i++, r++) {
      object *shot = Obj[NumObjs++];
      
      shot->life      = life;
      shot->fuselife  = shot->life - fuse;
      shot->mass      = mass;
      shot->max_speed = max_speed;   
      shot->turnspeed = turnspeed;
      shot->count     = 0;
      shot->info      = lock;
      shot->type      = type;  
      shot->id        = (pl ? pl->id : -1);
      shot->team      = team;
      shot->owner     = -1;
      shot->color     = (pl ? pl->color : WHITE);
      
      shotpos.x       = x;
      shotpos.y       = y;
      if (pl && type != OBJ_SHOT) {
	if (r == on_this_rack) {  
	  /*
	   * We've fired all the mini missiles for the current rack,
	   * we now move onto the next one. (See Comment Point 2)
	   */
	  on_this_rack = (1 - i) / racks_left--;
	  if (on_this_rack < 1) on_this_rack = 1;
	  if (++rack_no >= pl->ship->num_m_rack)
	    rack_no = 0;
	  r = 0;
            }
	shotpos.x += pl->ship->m_rack[rack_no][pl->dir].x;
	shotpos.y += pl->ship->m_rack[rack_no][pl->dir].y;
	side = (int)(pl->ship->m_rack[rack_no][0].y);
      }
      shotpos.x = WRAP_XPIXEL(shotpos.x);
      shotpos.y = WRAP_YPIXEL(shotpos.y);
      if (shotpos.x < 0 || shotpos.x >= World.width
	  || shotpos.y < 0 || shotpos.y >= World.height) {
	NumObjs--;
	continue;
      }
      
      Object_position_init_pixels(shot, shotpos.x, shotpos.y);
  
         
      mv.x = mv.y = shot->acc.x = shot->acc.y = 0;
      
      shot->vel.x     = mv.x + (pl ? pl->vel.x : 0.0) + tcos(dir) * speed;
      shot->vel.y     = mv.y + (pl ? pl->vel.y : 0.0) + tsin(dir) * speed;
      shot->status    = status;
      shot->dir       = dir;
      shot->pl_range  = pl_range;
      shot->pl_radius = pl_radius;
      fired++;
    }
}

void Fire_normal_shots(int ind)
{
    player		*pl = Players[ind];

    if (main_loops_slow < (pl->shot_time + fireRepeatRate)) {
	return;
    }

    pl->shot_time = main_loops_slow;

    Fire_main_shot(ind, OBJ_SHOT, pl->dir);
}


/* Removes shot from array */
void Delete_shot(int ind)
{
    object		*shot = Obj[ind];	/* Used when swapping places */
    player		*pl;
    int			addBall = 0;
    int			i;

    switch (shot->type) {

    case OBJ_SPARK:
    case OBJ_DEBRIS:
    case OBJ_WRECKAGE:
	break;

    case OBJ_BALL:
	if (shot->id != -1)
	    Detach_ball(GetInd[shot->id], ind);
	else {
	    /*
	     * Maybe some player is still busy trying to connect to this ball.
	     */
	    for (i = 0; i < NumPlayers; i++) {
		if (Players[i]->ball == shot) {
		    Players[i]->ball = NULL;
		}
	    }
	}
	if (shot->owner == -1) {
	    /*
	     * If the ball has never been owned, the only way it could
	     * have been destroyed is by being knocked out of the goal.
	     * Therefore we force the ball to be recreated.
	     */
	    World.treasures[shot->treasure].have = false;
	    SET_BIT(shot->status, RECREATE);
	}
	if (BIT(shot->status, RECREATE)) {
	  addBall = 1;
	  if (BIT(shot->status, NOEXPLOSION))
	    break;
	  /*removed sparks from ball explosion - undesired -pgm */
	}
	break;
	/* Shots related to a player. */

    case OBJ_SHOT:
	if (shot->id == -1)
	    break;
	pl = Players[GetInd[shot->id]];
	if (shot->type == OBJ_SHOT) {
	    if (--pl->shots <= 0) {
		pl->shots = 0;
	    }
	}
	break;

    default:
	xpprintf("%s Delete_shot(): Unkown shot type %d.\n", showtime(), shot->type);
	break;
    }

    Obj[ind] = Obj[--NumObjs];
    Obj[NumObjs] = shot;

    /* replace treasure -pgm */
    if (addBall) {
	Make_treasure_ball(shot->treasure);
    }
}

void Move_ball(int ind)
{
    /*
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
     */

    object		*ball = Obj[ind];
    player		*pl = Players[ GetInd[ball->id] ];
    vector		D;
    DFLOAT		length, force, ratio, accell, cosine, pl_damping, ball_damping;
    DFLOAT		k = ballConnectorSpringConstant;
    DFLOAT		b = ballConnectorDamping;
    DFLOAT		max_spring_ratio = maxBallConnectorRatio;

    /* compute the normalized vector between the ball and the player */
    D.x = WRAP_DX(pl->pos.x - ball->pos.x);
    D.y = WRAP_DY(pl->pos.y - ball->pos.y);
    length = VECTOR_LENGTH(D);
    if (length > 0.0) {
	D.x /= length;
	D.y /= length;
    }
    else
	D.x = D.y = 0.0;

    /* compute the ratio for the spring action */
    ratio = (ballConnectorLength - length) / (DFLOAT) ballConnectorLength;

    /* compute force by spring for this length */
    force = k * ratio;

    /* if the tether is too long or too short, release it */
    if (ABS(ratio) > max_spring_ratio) {
	Detach_ball(GetInd[ball->id], ind);
	return;
    }
    ball->length = length;

    /* compute damping for player */
    cosine = (pl->vel.x * D.x) + (pl->vel.y * D.y);
    pl_damping = -b * cosine;

    /* compute damping for ball */
    cosine = (ball->vel.x * -D.x) + (ball->vel.y * -D.y);
    ball_damping = -b * cosine;

    /* compute accelleration for player, assume t = 1 */
    accell = (force + pl_damping + ball_damping) / pl->mass;
    pl->vel.x += D.x * accell;
    pl->vel.y += D.y * accell;

    /* compute accelleration for ball, assume t = 1 */
    accell = (force + ball_damping + pl_damping) / ball->mass;
    ball->vel.x += -D.x * accell;
    ball->vel.y += -D.y * accell;
}


void Move_ball_interpolation(int ind)
{
    /*
      similar to Move_ball, but without detaching -pgm
     */

    object		*ball = Obj[ind];
    player		*pl = Players[ GetInd[ball->id] ];
    vector		D;
    DFLOAT		length, force, ratio, accell, cosine, pl_damping, ball_damping;
    DFLOAT		k = ballConnectorSpringConstant;
    DFLOAT		b = ballConnectorDamping;
    DFLOAT		max_spring_ratio = maxBallConnectorRatio;
    float               speedfactor = 1.0/frameDivisor;

    /* compute the normalized vector between the ball and the player */
    D.x = WRAP_DX(pl->pos_interp.x - ball->pos_interp.x);
    D.y = WRAP_DY(pl->pos_interp.y - ball->pos_interp.y);
    length = VECTOR_LENGTH(D);
    if (length > 0.0) {
	D.x /= length;
	D.y /= length;
    }
    else
	D.x = D.y = 0.0;

    /* compute the ratio for the spring action */
    ratio = (ballConnectorLength - length) / (DFLOAT) ballConnectorLength;

    /* compute force by spring for this length */
    force = k * ratio;

    /* if the tether is too long or too short, release it, 
       cannot do it here though, since interpolated frames should not
       interfere with game events -pgm

       if (ABS(ratio) > max_spring_ratio) {
       Detach_ball(GetInd[ball->id], ind);
       return;
       }
    */


    ball->length_interp = length;

    /* compute damping for player */
    cosine = (pl->vel_interp.x * D.x) + (pl->vel_interp.y * D.y);
    pl_damping = -b * cosine;

    /* compute damping for ball */
    cosine = (ball->vel_interp.x * -D.x) + (ball->vel_interp.y * -D.y);
    ball_damping = -b * cosine;

    /* compute acceleration for player, assume t = 1 */
    accell = (force + pl_damping + ball_damping) / pl->mass;
    pl->vel_interp.x += D.x * accell * speedfactor;
    pl->vel_interp.y += D.y * accell * speedfactor;

    /* compute acceleration for ball, assume t = 1 */
    accell = (force + ball_damping + pl_damping) / ball->mass;
    ball->vel_interp.x += -D.x * accell * speedfactor;
    ball->vel_interp.y += -D.y * accell * speedfactor;
}
