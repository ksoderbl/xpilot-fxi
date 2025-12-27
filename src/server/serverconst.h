/*
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

#define PSEUDO_TEAM(pl1, pl2)\
	((pl1)->pseudo_team == (pl2)->pseudo_team)

/*
 * Used where we wish to know if a player is simply on the same team.
 */
#define TEAM(pl1, pl2) \
	(BIT(World.rules->mode, TEAM_PLAY) \
	&& ((pl1)->team == (pl2)->team) \
	&& ((pl1)->team != NULL))

/*
 * Used where we wish to know if a player is on the same team
 * and has immunity to shots, thrust sparks, lasers, ecms, etc.
 */
#define TEAM_IMMUNE(pl1, pl2)	(teamImmunity && TEAM(pl1, pl2))

/*
 * Delays and timeouts in game (both in seconds and the number real frames (ticks))
 */
#define RECOVERY_DELAY			3.0
#define RECOVERY_DELAY_TICKS		(gameSpeed * RECOVERY_DELAY)

#define UNPAUSE_DELAY			10.0
#define UNPAUSE_DELAY_TICKS		(gameSpeed * UNPAUSE_DELAY)

#define ROBOT_CREATE_DELAY		2.0
#define ROBOT_CREATE_DELAY_TICKS	(gameSpeed * ROBOT_CREATE_DELAY)

#define SELF_DESTRUCT_DELAY		15.0
#define SELF_DESTRUCT_DELAY_TICKS	(gameSpeed * SELF_DESTRUCT_DELAY)

#define MAX_PLAYER_IDLE			60.0
#define MAX_PLAYER_IDLE_TICKS		(gameSpeed * MAX_PLAYER_IDLE)

#define SHOT_DELAY			1.0
#define SHOT_DELAY_TICKS		(gameSpeed * SHOT_DELAY)

#define SHOT_LIFE_TICKS			(frameDivisor * ShotsLife)

#define META_UPDATE_DELAY		180.0
#define META_UPDATE_DELAY_TICKS		(gameSpeed * META_UPDATE_DELAY)

#define SHIELD_DELAY			2.0
#define SHIELD_DELAY_TICKS		(gameSpeed * SHIELD_DELAY)


#define MAX_OBJECT_LIFE		INT_MAX

#define NO_ID			(-1)
#define NUM_IDS			256
#define MAX_PSEUDO_PLAYERS      16
#define MAX_PAUSED_PLAYERS	8

#define MAX_TOTAL_OBJECTS		16384	/* must be <= 65536 */

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
