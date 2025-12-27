/* $Id: tuner.c,v 1.4 2007/10/21 12:45:07 kps Exp $
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

#define	SERVER
#include <stdlib.h>
#include <time.h>
#include "serverconst.h"
#include "global.h"
#include "proto.h"
#include "error.h"
#include "commonproto.h"
#include "sched.h"

extern int frame_cycle;


void tuner_plock(void)
{
    pLockServer = (plock_server(pLockServer) == 1) ? true : false;
}

void tuner_shotsmax(void)
{
    int i;

    for (i = 0; i < NumPlayers; i++) {
	Players[i]->shot_max = ShotsMax;
    }
}

void tuner_shipmass(void)
{
    int i;

    for (i = 0; i < NumPlayers; i++) {
	Players[i]->emptymass = ShipMass;
    }
}

void tuner_ballmass(void)
{
    int i;

    for (i = 0; i < NumObjs; i++) {
	if (BIT(Obj[i]->type, OBJ_BALL)) {
	    Obj[i]->mass = ballMass;
	}
    }
}

void tuner_maxrobots(void)
{
    if (maxRobots < 0) {
	maxRobots = World.NumBases;
    }

    if (maxRobots < minRobots) {
	minRobots = maxRobots;
    }

    while (maxRobots < NumRobots) {
	Robot_delete(NULL, true);
    }
}

void tuner_minrobots(void)
{
    if (minRobots < 0) {
	minRobots = maxRobots;
    }

    if (maxRobots < minRobots) {
	maxRobots = minRobots;
    }
}

void tuner_worldlives(void)
{
    if (worldLives < 0)
	worldLives = 0;

    Set_world_rules();

    if (BIT(World.rules->mode, LIMITED_LIVES)) {
	Reset_all_players();
    }
}


void tuner_framedivisor(void){
 
  LIMIT(frameDivisor, 1, 10);
  frame_cycle = 0;
  setup_timer(fps);
  frame_cycle = 0;
}


void tuner_fps(void){
 
  LIMIT(fps, 1, MAX_SERVER_FPS);	
  frame_cycle = 0;
  setup_timer(fps);
  frame_cycle = 0;
}


void tuner_gamespeed(void)
{
    LIMIT(gameSpeed, 1, MAX_SERVER_FPS);
}
