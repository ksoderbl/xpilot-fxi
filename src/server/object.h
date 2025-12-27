/* $Id: object.h,v 1.1.1.1 2007/01/21 16:41:22 kps Exp $
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-98 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
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

#ifndef SERVERCONST_H
/* need MAX_TANKS */
#include "serverconst.h"
#endif
#ifndef KEYS_H
/* need NUM_KEYS */
#include "keys.h"
#endif
#ifndef BIT_H
/* need BITV_DECL */
#include "bit.h"
#endif
#ifndef DRAW_H
/* need wireobj */
#include "draw.h"
#endif
#ifndef ITEM_H
/* need NUM_ITEMS */
#include "item.h"
#endif
#ifndef CLICK_H
/* need CLICK */
#include "click.h"
#endif

/*
 * Different types of objects, including player.
 * Robots and tanks are players but have an additional bit.
 * Smart missile, heatseeker and torpedoe can be merged into missile.
 * ECM doesn't really need an object type.
 * Lasers and pulses can be merged.
 */
#define OBJ_PLAYER		(1U<<0)
#define OBJ_DEBRIS		(1U<<1)
#define OBJ_SPARK		(1U<<2)
#define OBJ_AUTOPILOT		(1U<<4)
#define OBJ_BALL		(1U<<7)
#define OBJ_SHOT		(1U<<8)
#define OBJ_SHIELD		(1U<<11)
#define OBJ_REFUEL		(1U<<12)
#define OBJ_COMPASS		(1U<<14)
#define OBJ_AFTERBURNER		(1U<<19)
#define OBJ_CONNECTOR		(1U<<20)
#define OBJ_WRECKAGE		(1U<<26)

/*
 * Some object types are overloaded.
 */
#define OBJ_EXT_ROBOT		(1U<<2)

#define IS_ROBOT_IND(ind)	IS_ROBOT_PTR(Players[ind])
#define IS_HUMAN_IND(ind)	IS_HUMAN_PTR(Players[ind])

#define IS_ROBOT_PTR(pl)	(BIT((pl)->type_ext,OBJ_EXT_ROBOT)==OBJ_EXT_ROBOT)
#define IS_HUMAN_PTR(pl)	(!BIT((pl)->type_ext,OBJ_EXT_ROBOT))

#define LOCK_NONE		0x00	/* No lock */
#define LOCK_PLAYER		0x01	/* Locked on player */
#define LOCK_VISIBLE		0x02	/* Lock information was on HUD */
					/* computed just before frame shown */
					/* and client input checked */
#define LOCKBANK_MAX		4	/* Maximum number of locks in bank */

#define NOT_CONNECTED		(-1)

/*
 * Object position is non-modifiable, except at one place.
 *
 * NB: position in pixels used to be a float.
 */
typedef const struct _objposition objposition;
struct _objposition {
    int		cx, cy;			/* object position in clicks. */
    int		x, y;			/* object position in pixels. */
    int		bx, by;			/* object position in blocks. */
};
#define OBJ_X_IN_CLICKS(obj)	((obj)->pos.cx)
#define OBJ_Y_IN_CLICKS(obj)	((obj)->pos.cy)
#define OBJ_X_IN_PIXELS(obj)	((obj)->pos.x)
#define OBJ_Y_IN_PIXELS(obj)	((obj)->pos.y)
#define OBJ_X_IN_BLOCKS(obj)	((obj)->pos.bx)
#define OBJ_Y_IN_BLOCKS(obj)	((obj)->pos.by)

typedef struct _object object;
struct _object {
    byte	color;			/* Color of object */
    u_byte	dir;			/* Direction of acceleration */
    int		id;			/* For shots => id of player */
    u_short	team;			/* Team of player or cannon */
    objposition	pos;			/* World coordinates */
    ipos	prevpos;		/* Object's previous position... */
    vector	vel;
    vector	acc;
    objposition pos_interp;
    vector      vel_interp;
    vector      acc_interp;
    DFLOAT	max_speed;
    DFLOAT	mass;
    int		type;
    long	info;			/* Miscellaneous info */
    long	life;			/* No of ticks left to live */
    int		count;			/* Misc timings */
    long	status;

    /* up to here all object types (including players!) should be the same. */

    DFLOAT	turnspeed;		/* for missiles only */
    long	fuselife;		/* Ticks left when considered fused */

    object	*cell_list;		/* linked list for cell lookup */

    int 	owner;			/* Who's object is this ? */
					/* (spare for id)*/
    int		treasure;		/* Which treasure does ball belong */
    DFLOAT	length;			/* Distance between ball and player */
    DFLOAT	length_interp;
    int		spread_left;		/* how much spread time left */
    int		pl_range;		/* distance to player for collision. */
    int		pl_radius;		/* distance to player for hit. */

    u_byte	size;			/* Size of object (wreckage) */
    u_byte	rotation;		/* Rotation direction */

};


/*
 * Fuel structure, used by player
 */
typedef struct {
    long	sum;			/* Sum of fuel in all tanks */
    long	max;			/* How much fuel can you take? */
    int		current;		/* Number of currently used tank */
    int		num_tanks;		/* Number of tanks */
    long	tank[1 + MAX_TANKS];	/* main fixed tank + extra tanks. */
    long	l1;			/* Fuel critical level */
    long	l2;			/* Fuel warning level */
    long	l3;			/* Fuel notify level */
} pl_fuel_t;

struct _visibility {
    int		canSee;
    long	lastChange;
};

/*
 * Shove-information.
 *
 * This is for keeping a record of the last N times the player was shoved,
 * for assigning wall-smash-blame, where N=MAX_RECORDED_SHOVES.
 */
#define MAX_RECORDED_SHOVES 4

typedef struct {
    int		pusher_id;
    int		time;
} shove_t;

struct robot_data;

/* IMPORTANT
 *
 * This is the player structure, the first part MUST be similar to object_t,
 * this makes it possible to use the same basic operations on both of them
 * (mainly used in update.c).
 */
typedef struct player player;
struct player {
    byte	color;			/* Color of object */
    u_byte	dir;			/* Direction of acceleration */
    int		id;			/* Unique id of object */
    u_short	team;			/* What team is the player on? */
    objposition	pos;			/* World coordinates */
    ipos	prevpos;		/* Previous position... */
    vector	vel;			/* Velocity of object */
    vector	acc;			/* Acceleration constant */
    objposition pos_interp;
    vector	vel_interp;
    vector	acc_interp;
    DFLOAT	max_speed;		/* Maximum speed of object */
    DFLOAT	mass;			/* Mass of object (incl. cargo) */
    int		type;			/* Type of object */
    long	info;			/* Miscellaneous info */
    int		life;			/* Zero is dead. One is alive */
    int		count;			/* Miscellaneous timings */
    long	status;			/** Status, currently **/

    /* up to here the player type should be the same as an object. */

    int		type_ext;		/* extended type info (tank, robot) */

    DFLOAT	turnspeed;		/* How fast player acc-turns */
    DFLOAT	velocity;		/* Absolute speed */
    DFLOAT	velocity_interp;	
    int		kills;			/* Number of kills this round */
    int		deaths;			/* Number of deaths this round */

    long	used;			/** Items you use **/
    long	have;			/** Items you have **/

    int		shield_time;		/* Shields if no playerShielding */
    pl_fuel_t	fuel;			/* ship tanks and the stored fuel */
    DFLOAT	emptymass;		/* Mass of empty ship */
    DFLOAT	float_dir;		/* Direction, in float var */
    DFLOAT	turnresistance;		/* How much is lost in % */
    DFLOAT	turnvel;		/* Current velocity of turn (right) */
    DFLOAT	turnacc;		/* Current acceleration of turn */
    long	score;			/* Current score of player */
    long	prev_score;		/* Last score that has been updated */
    int		prev_life;		/* Last life that has been updated */
    wireobj	*ship;			/* wire model of ship shape */
    DFLOAT	power;			/* Force of thrust */
    DFLOAT	power_s;		/* Saved power fiks */
    DFLOAT	turnspeed_s;		/* Saved turnspeed */
    DFLOAT	turnresistance_s;	/* Saved (see above) */
    int		shots;			/* Number of active shots by player */

    int		item[NUM_ITEMS];	/* for each item type how many */

    int		shot_max;		/* Maximum number of shots active */
    int		shot_life;		/* Number of ticks shot will live */
    DFLOAT	shot_speed;		/* Speed of shots fired by player */
    long	shot_time;		/* Time of last shot fired by player */
    int		fs;			/* Connected to fuel station fs */


    int		home_base;		/* Num of home base */
    struct {
	int	    tagged;		/* Flag, what is tagged? */
	int	    pl_id;		/* Tagging player id */
	DFLOAT	    distance;		/* Distance to object */
    } lock;
    int		lockbank[LOCKBANK_MAX]; /* Saved player locks */

    char	mychar;			/* Special char for player */
    char	prev_mychar;		/* Special char for player */
    char	name[MAX_CHARS];	/* Nick-name of player */
    char	realname[MAX_CHARS];	/* Real name of player */
    char	hostname[MAX_CHARS];	/* Hostname of client player uses */
					/* (detaching!) */
    object	*ball;

    /*
     * Pointer to robot private data (dynamically allocated).
     * Only used in robot code.
     */
    struct robot_data	*robot_data_ptr;

    /*
     * A record of who's been pushing me (a circular buffer).
     */
    shove_t     shove_record[MAX_RECORDED_SHOVES];
    int         shove_next;

    struct _visibility *visibility;

    int		conn;			/* connection index, -1 if robot */
    unsigned	version;		/* XPilot version number of client */

    BITV_DECL(last_keyv, NUM_KEYS);	/* Keyboard state */
    BITV_DECL(prev_keyv, NUM_KEYS);	/* Keyboard state */

    long	frame_last_busy;	/* When player touched keyboard. */


    int		player_fps;		/* FPS that this player can do */

    int		isowner;		/* If player started this server. */
    int		isoperator;		/* If player has operator privileges. */
    bool        update_score;           /* score table info needs to be sent */
    struct ranknode     *rank;		
};

#endif
