/* $Id: play.c,v 1.12 2008/08/15 15:09:53 rotunda_pk Exp $
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

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "score.h"
#include "objpos.h"
#include "rank.h"

int8_t play_version[] = VERSION;

extern int32_t Rate(int32_t winner, int32_t looser);

int32_t Punish_team(player_t *pl, treasure_t *td, treasure_t *tt)
{
	static int8_t msg[MSG_LEN];
	int32_t i;
	int32_t win_score = 0, lose_score = 0;
	int32_t win_team_members = 0, lose_team_members = 0;
	int32_t somebody_flag = 0;
	int32_t sc, por;

	Check_team_members(td->team);
	if (td->team == pl->team)
		return 0;

	if (BIT(World.rules->mode, TEAM_PLAY)) {
		for (i = 0; i < NumPlayers; i++) {
			if ((BIT(Players[i]->status, PAUSE)
					&& Players[i]->count <= 0) || (BIT(
					Players[i]->status, GAME_OVER)
					&& Players[i]->mychar == 'W')) {
				continue;
			}
			if (Players[i]->team == td->team) {
				lose_score += Players[i]->score;
				lose_team_members++;
				if (BIT(Players[i]->status, GAME_OVER) == 0) {
					somebody_flag = 1;
				}
			}
			else if (Players[i]->team == tt->team) {
				win_score += Players[i]->score;
				win_team_members++;
			}
		}
	}

	sprintf(msg, " < %s's (%d) team has destroyed team %d treasure >",
			pl->name, pl->team->Num, td->team->Num);
	Set_message(msg);

	td->destroyed++;
	td->team->TreasuresLeft--;
	tt->team->TreasuresDestroyed++;

	sc = 3 * Rate(win_score, lose_score);
	por = (sc * lose_team_members) / (2 * win_team_members + 1);

	for (i = 0; i < NumPlayers; i++) {
		if ((BIT(Players[i]->status, PAUSE) && Players[i]->count <= 0)
				|| (BIT(Players[i]->status, GAME_OVER)
						&& Players[i]->mychar == 'W')) {
			continue;
		}
		if (Players[i]->team == td->team) {
			SCORE(Players[i], -sc, tt->pos.x, tt->pos.y,
					"Treasure: ");
			Rank_lost_ball(Players[i]);
		}
		else if (Players[i]->team == tt->team && (Players[i]->team
				!= NULL || Players[i] == pl)) {
			SCORE(Players[i], (Players[i] == pl ? 3 * por : 2
					* por), tt->pos.x, tt->pos.y,
					"Treasure: ");
		}
	}

	updateScores = true;

	return 1;
}

/****************************
 * Functions for explosions.
 */

/* Create debris particles */
void Make_debris(
/* pos.cx, pos.cy */int32_t cx, int32_t cy,
/* vel.x, vel.y   */DFLOAT velx, DFLOAT vely,
/* owner id       */player_t *pl,
/* owner team     */team_t *team,
/* type           */int32_t type,
/* mass           */DFLOAT mass,
/* status         */int32_t status,
/* color          */int32_t color,
/* radius         */int32_t radius,
/* min,max debris */int32_t min_debris, int32_t max_debris,
/* min,max dir    */int32_t min_dir, int32_t max_dir,
/* min,max speed  */DFLOAT min_speed, DFLOAT max_speed,
/* min,max life   */int32_t min_life, int32_t max_life)
{
	object_t *debris;
	int32_t i, num_debris, life;

	cx = WRAP_XCLICK(cx);
	cy = WRAP_YCLICK(cy);
	if (cx < 0 || cx >= World.cwidth || cy < 0 || cy >= World.cheight) {
		return;
	}
	if (max_life < min_life)
		max_life = min_life;
	if (ShotsLife >= intGameSpeed) {
		if (min_life > ShotsLife) {
			min_life = ShotsLife;
			max_life = ShotsLife;
		}
		else if (max_life > ShotsLife) {
			max_life = ShotsLife;
		}
	}
	if (min_speed * max_life > World.hypotenuse)
		min_speed = World.hypotenuse / max_life;
	if (max_speed * min_life > World.hypotenuse)
		max_speed = World.hypotenuse / min_life;
	if (max_speed < min_speed)
		max_speed = min_speed;

	num_debris = min_debris + (int32_t) (rfrac() * (max_debris - min_debris));
	if (num_debris > MAX_TOTAL_SHOTS - NumObjs) {
		num_debris = MAX_TOTAL_SHOTS - NumObjs;
	}
	for (i = 0; i < num_debris; i++, NumObjs++) {
		DFLOAT speed, dx, dy, diroff;
		int32_t dir, dirplus;

		debris = Obj[NumObjs];
		debris->color = color;
		debris->owner = pl;
		debris->team = team;
		Object_position_init_clicks(debris, cx, cy);
		dir
				= MOD2(min_dir + (int32_t) (rfrac() * (max_dir
						- min_dir)), RES);
		dirplus = MOD2(dir + 1, RES);
		diroff = rfrac();
		dx = tcos(dir) + (tcos(dirplus) - tcos(dir)) * diroff;
		dy = tsin(dir) + (tsin(dirplus) - tsin(dir)) * diroff;
		speed = min_speed + rfrac() * (max_speed - min_speed);
		debris->vel.x = velx + dx * speed;
		debris->vel.y = vely + dy * speed;
		debris->acc.x = 0;
		debris->acc.y = 0;
		debris->dir = dir;
		debris->mass = mass;
		debris->type = type;
		life = (int32_t) (min_life + rfrac() * (max_life - min_life) + 1);
		if (life * speed > World.hypotenuse) {
			life = (int32_t) (World.hypotenuse / speed);
		}
		debris->life = life;
		debris->fuselife = life;
		debris->spread_left = 0;
		debris->pl_range = radius;
		debris->pl_radius = radius;
		debris->status = status;
	}
}

