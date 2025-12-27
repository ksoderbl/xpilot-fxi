/* $Id: serverconst.h,v 1.7 2008/08/15 15:09:53 rotunda_pk Exp $
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-2001 by
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

#ifndef SERVERCONST_H
#define SERVERCONST_H

#include "const.h"

/*
 * Two macros for edge wrap of x and y coordinates measured in pixels.
 * Note that the correction needed shouldn't ever be bigger than one mapsize.
 */
#define WRAP_XPIXEL(x_)	\
	(BIT(World.rules->mode, WRAP_PLAY) \
	    ? ((x_) < 0 \
		? (x_) + World.width \
		: ((x_) >= World.width \
		    ? (x_) - World.width \
		    : (x_))) \
	    : (x_))

#define WRAP_YPIXEL(y_)	\
	(BIT(World.rules->mode, WRAP_PLAY) \
	    ? ((y_) < 0 \
		? (y_) + World.height \
		: ((y_) >= World.height \
		    ? (y_) - World.height \
		    : (y_))) \
	    : (y_))

/*
 * Two macros for edge wrap of x and y coordinates measured in map blocks.
 * Note that the correction needed shouldn't ever be bigger than one mapsize.
 */
#define WRAP_XBLOCK(x_)	\
	(BIT(World.rules->mode, WRAP_PLAY) \
	    ? ((x_) < 0 \
		? (x_) + World.x \
		: ((x_) >= World.x \
		    ? (x_) - World.x \
		    : (x_))) \
	    : (x_))

#define WRAP_YBLOCK(y_)	\
	(BIT(World.rules->mode, WRAP_PLAY) \
	    ? ((y_) < 0 \
		? (y_) + World.y \
		: ((y_) >= World.y \
		    ? (y_) - World.y \
		    : (y_))) \
	    : (y_))

/*
 * Two macros for edge wrap of differences in position.
 * If the absolute value of a difference is bigger than
 * half the map size then it is wrapped.
 */
#define WRAP_DX(dx)	\
	(BIT(World.rules->mode, WRAP_PLAY) \
	    ? ((dx) < - (World.width >> 1) \
		? (dx) + World.width \
		: ((dx) > (World.width >> 1) \
		    ? (dx) - World.width \
		    : (dx))) \
	    : (dx))

#define WRAP_DY(dy)	\
	(BIT(World.rules->mode, WRAP_PLAY) \
	    ? ((dy) < - (World.height >> 1) \
		? (dy) + World.height \
		: ((dy) > (World.height >> 1) \
		    ? (dy) - World.height \
		    : (dy))) \
	    : (dy))

#define PSEUDO_TEAM(pl1, pl2)\
	((pl1)->pseudo_team == (pl2)->pseudo_team)

/*
 * Used where we wish to know if a player is simply on the same team.
 */
/* #define TEAM(i, j)							\
	(BIT(Players[i]->status|Players[j]->status, PAUSE)		\
	|| (BIT(World.rules->mode, TEAM_PLAY)				\
	   && (Players[i]->team == Players[j]->team)			\
	   && (Players[i]->team != TEAM_NOT_SET))) */
#define TEAM(pl1, pl2) \
	(BIT(World.rules->mode, TEAM_PLAY) \
	&& ((pl1)->team == (pl2)->team) \
	&& ((pl1)->team != NULL))

/*
 * Used where we wish to know if a player is on the same team
 * and has immunity to shots, thrust sparks, lasers, ecms, etc.
 */
#define TEAM_IMMUNE(pl1, pl2)	(teamImmunity && TEAM(pl1, pl2))

#define RECOVERY_DELAY		(intGameSpeed*3)
#define ROBOT_CREATE_DELAY	(intGameSpeed*2)

#define NO_ID			(-1)
#define NUM_IDS			256
#define MAX_PSEUDO_PLAYERS      16

#define MAX_TOTAL_SHOTS		16384	/* must be <= 65536 */

#define LG2_MAX_AFTERBURNER     4
#define ALT_SPARK_MASS_FACT     4.2
#define ALT_FUEL_FACT           3
#define MAX_AFTERBURNER        ((1<<LG2_MAX_AFTERBURNER)-1)
#define AFTER_BURN_SPARKS(s,n)  (((s)*(n))>>LG2_MAX_AFTERBURNER)
#define AFTER_BURN_POWER_FACTOR(n) \
 (1.0+(n)*((ALT_SPARK_MASS_FACT-1.0)/(MAX_AFTERBURNER+1.0)))
#define AFTER_BURN_POWER(p,n)   \
 ((p)*AFTER_BURN_POWER_FACTOR(n))
#define AFTER_BURN_FUEL(f,n)    \
 (((f)*((MAX_AFTERBURNER+1)+(n)*(ALT_FUEL_FACT-1)))/(MAX_AFTERBURNER+1.0))

#define THRUST_MASS             0.7

#define MAX_TANKS               8
#define TANK_MASS               (ShipMass/10)
#define TANK_CAP(n)             (!(n)?MAX_PLAYER_FUEL:(MAX_PLAYER_FUEL/3))
#define TANK_REFILL_LIMIT       (MIN_PLAYER_FUEL/8)
#define MAX_SERVER_FPS          100

#define ENERGY_RANGE_FACTOR	(2.5/FUEL_SCALE_FACT)

/* map dimension limitation: ((0x7FFF - 1280) / 35) */
#define MAX_MAP_SIZE		900

#endif
