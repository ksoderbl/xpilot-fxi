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

#include <stdlib.h>
#include <time.h>

#include "serverconst.h"
#include "global.h"
#include "error.h"
#include "commonproto.h"

#include "proto.h"

#include "sched.h"
#include "tuner.h"

#include "player.h"
#include "map.h"
#include "player_inline.h"

#include "server.h"
#include "robot.h"


void tuner_plock(void)
{
	pLockServer = (plock_server(pLockServer) == 1) ? true : false;
}

void tuner_shotsmax(void)
{
	int32_t i;

	for (i = 0; i < NumPlayers; i++) {
		Players[i]->shot_max = ShotsMax;
	}
}

void tuner_shipmass(void)
{
	int32_t i;

	for (i = 0; i < NumPlayers; i++) {
		Players[i]->emptymass = ShipMass;
	}
}

void tuner_ballmass(void)
{
	int32_t i;

	for (i = 0; i < NumObjs; i++) {
		if (Object_is_type(Obj[i], OBJ_BALL)) {
			Obj[i]->mass = ballMass;
		}
	}
}

void tuner_maxrobots(void)
{
	if (maxRobots < 0) {
		maxRobots = World.NumBases;
	}
}

void tuner_robotteam(void)
{
	Robots_init();
}

void tuner_worldlives(void)
{
	int32_t i;

	if (worldLives < 0)
		worldLives = 0;

	Set_world_rules();

	if (BIT(World.rules->mode, LIMITED_LIVES)) {
		Players_reset();

		Objects_time_out();

		/* Reset the teams */
		for (i = 0; i < MAX_TEAMS; i++) {
			World.teams[i].TreasuresDestroyed = 0;
			World.teams[i].TreasuresLeft = World.teams[i].NumTreasures;
		}
	}
}

void tuner_framedivisor(void)
{
	LIMIT(frameDivisor, 1, 10);

	/*
	 * The global variable ticksPerFrame depends on frameDivisor, but we must
	 * not update it here, because it is likely to cause havoc in frame computations.
	 */

	frame_cycle = 0;
	setup_timer(fps);
	frame_cycle = 0;
}

void tuner_fps(void)
{

	LIMIT(fps, 1, MAX_SERVER_FPS);
	frame_cycle = 0;
	setup_timer(fps);
	frame_cycle = 0;
}

void tuner_gamespeed(void)
{
	LIMIT(gameSpeed, 1, MAX_SERVER_FPS);
}
