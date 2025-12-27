/* $Id: rules.c,v 1.6 2008/08/16 21:07:33 rotunda_pk Exp $
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
#include <stdio.h>

#define SERVER
#include "version.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "rules.h"
#include "bit.h"

int8_t rules_version[] = VERSION;

#define MAX_FUEL                10000
#define MAX_AUTOPILOT           99

/*
 * Bitmask of object types the robot puts up shield for.
 */
int32_t KILLING_SHOTS = OBJ_SHOT;

int32_t DEF_BITS = 0;
int32_t KILL_BITS = (THRUSTING | PLAYING | KILLED | SELF_DESTRUCT | PAUSE);
int32_t DEF_HAVE = (OBJ_SHIELD | OBJ_COMPASS | OBJ_REFUEL | OBJ_CONNECTOR
		| OBJ_SHOT);
int32_t DEF_USED = (OBJ_SHIELD | OBJ_COMPASS);
int32_t USED_KILL = (OBJ_REFUEL | OBJ_CONNECTOR | OBJ_SHOT);

/*
 * Initializes special items.
 * First parameter is type,
 * second and third parameters are minimum and maximum number
 * of elements one item gives when picked up by a ship.
 */
static void Init_item(int32_t item)
{
	World.items[item].num = 0;
}

/*
 * Give (or remove) capabilities of the ships depending upon
 * the availability of initial items.
 * Limit the initial resources between minimum and maximum possible values.
 */
void Set_initial_resources(void)
{
	CLR_BIT(DEF_HAVE, OBJ_AUTOPILOT);

	if (World.items[ITEM_AUTOPILOT].initial > 0)
		SET_BIT(DEF_HAVE, OBJ_AUTOPILOT);
}

/*
 * First time initialization of all global item stuff.
 */
void Set_world_items(void)
{
	Init_item(ITEM_FUEL);
	Init_item(ITEM_TANK);
	Init_item(ITEM_AFTERBURNER);
	Init_item(ITEM_AUTOPILOT);

	Set_initial_resources();
}

void Set_world_rules(void)
{
	static rules_t rules;

	rules.mode = ((crashWithPlayer ? CRASH_WITH_PLAYER : 0)
			| (bounceWithPlayer ? BOUNCE_WITH_PLAYER : 0)
			| (playerKillings ? PLAYER_KILLINGS : 0)
			| (playerShielding ? PLAYER_SHIELDING : 0)
			| (limitedLives ? LIMITED_LIVES : 0)
			| (teamPlay ? TEAM_PLAY : 0) | (edgeWrap ? WRAP_PLAY
			: 0));
	rules.lives = worldLives;
	World.rules = &rules;

	if (!BIT(World.rules->mode, PLAYER_KILLINGS))
		CLR_BIT(KILLING_SHOTS, OBJ_SHOT);
	if (!BIT(World.rules->mode, PLAYER_SHIELDING))
		CLR_BIT(DEF_HAVE, OBJ_SHIELD);

	DEF_USED &= DEF_HAVE;
}

