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
#include <stdio.h>
#include <string.h>

#include "version.h"
#include "commonproto.h"
#include "serverconst.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "score.h"
#include "bit.h"
#include "objpos.h"
#include "player.h"
#include "player_inline.h"
#include "update.h"
#include "debug.h"
#include "frame.h"
#include "robot.h"
#include "ship.h"
#include "collision.h"

char update_version[] = VERSION;

bool limitedRoundsGameOver = false;


/*
 * Update shots.
 */
void Update_mobile_objects(void)
{
	int32_t i;
	object_t *obj;

	for (i = 0; i < NumObjs; i++) {
		obj = Obj[i];

		/* for collision detection */
		if (Frame_is_real()) {
			Object_position_remember(obj);
		}

		if (Object_is_type(obj, OBJ_BALL)) {
			if (Object_is_attached(obj)) {
				Ball_move(obj);
			}
			else {
				/* don't process balls that are safely in boxes */
				if (obj->treasure->have) {
					continue;
				}
			}
		}

		if (Frame_is_real()) {
			if (Object_is_type(obj, OBJ_WRECKAGE)) {
				obj->rotation = (obj->rotation + (int32_t) (obj->turnspeed * ANGLE_RESOLUTION)) % ANGLE_RESOLUTION;
			}

			Object_update_speed(obj);
		}

		Move_object(obj);
	}
}

void Update_player_turn(player_t *pl)
{
	/*
	 * Compute turn
	 */
	pl->turnvel += pl->turnacc;

	/*
	 * turnresistance is zero: client requests linear turning behaviour
	 * when playing with pointer control.
	 */

	if (pl->turnresistance) {
		pl->turnvel *= pl->turnresistance;
	}

	pl->float_dir += pl->turnvel;

	while (pl->float_dir < 0)
		pl->float_dir += ANGLE_RESOLUTION;
	while (pl->float_dir >= ANGLE_RESOLUTION)
		pl->float_dir -= ANGLE_RESOLUTION;

	/*
	 * turnresistance is zero: client requests linear turning behaviour
	 * when playing with pointer control.
	 */
	if (!pl->turnresistance) {
		pl->turnvel = 0;
	}
	/*  printf("%d\n",main_loops);
	 fflush(stdout);
	 */
	if (pl->oldturn) {
		Turn_player_old(pl);
	}
	else {
		Turn_player(pl);
	}
}

void Update_player_thrust(player_t *pl)
{
	/*
	 * Update acceleration vector etc.
	 */

	if (BIT(pl->obj_status, THRUSTING)) {
		DFLOAT power = pl->power;
		DFLOAT f = pl->power * 0.0008; /* 1/(FUEL_SCALE*MIN_POWER) */
		int32_t a = pl->item[ITEM_AFTERBURNER];
		DFLOAT inert = pl->mass;

		if (a) {
			power = AFTER_BURN_POWER(power, a);
			f = AFTER_BURN_FUEL(f, a);
		}
		pl->acc.x = power * tcos(pl->dir) / inert;
		pl->acc.y = power * tsin(pl->dir) / inert;
		Player_fuel_add(pl, (int32_t) (-f * FUEL_SCALE_FACT)); /* Decrement fuel */
	}
	else {
		pl->acc.x = pl->acc.y = 0.0;
	}

	pl->mass = pl->emptymass + FUEL_MASS(pl->fuel.sum);
}

/* * * * * *
 *
 * Player loop. Computes miscellaneous updates.
 *
 */
void Update_players(void)
{
	int32_t i;
	player_t *pl;

        bool update_turn;
        int32_t player_fps;

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if (Frame_is_real()) {
			/* for collision detection */
			Player_position_remember(pl);

			/* Limits. */
			LIMIT(pl->power, MIN_PLAYER_POWER, MAX_PLAYER_POWER);
			LIMIT(pl->turnspeed, MIN_PLAYER_TURNSPEED, MAX_PLAYER_TURNSPEED);
			LIMIT(pl->turnresistance, MIN_PLAYER_TURNRESISTANCE,
					MAX_PLAYER_TURNRESISTANCE);

			if (Player_is_paused(pl)) {
				if (pl->pause_count > 0.0) {
					pl->pause_count -= realTimeStep;

					if (pl->pause_count <= 0.0) {
						pl->pause_count = 0.0;
					}
				}

				continue;
			}

			// NOTE: parts taken from ng - rotunda
			if (! Player_is_recovered(pl)) {
				pl->recovery_count -= realTimeStep;

				if (Player_is_recovered(pl)) {
					/* Player has recovered (unless he is already dead). */
					pl->recovery_count = 0.0;

					if (Player_is_appearing(pl)) {
						Player_set_state(pl, PL_STATE_ALIVE);
						Player_go_home(pl);
					}
		                        else if (Player_is_dead(pl)) {
		                                Player_go_home(pl);
		                        }
				}
				else {
					/* Player hasn't recovered yet. */
					Player_proceed_return_home(pl);
					Move_player(pl);
					continue;
				}
			}

			if (Player_is_self_destructing(pl)) {
				pl->self_destruct_count -= realTimeStep;

				if (pl->self_destruct_count <= 0.0) {
					Player_set_state(pl, PL_STATE_KILLED);
					Message_game_print("%s has comitted suicide.", pl->name);
					Player_destroy(pl);
					pl->self_deaths++;
				}
			}

			/*
			 * The remaining code only concerns alive players.
			 */
			if (!Player_is_alive(pl)) {
				continue;
			}

			if (pl->shield_count > 0.0) {
				pl->shield_count -= realTimeStep;

				if (pl->shield_count <= 0.0) {
					Player_disable_property(pl, USES_SHIELD);
					pl->shield_count = 0.0;
				}

				/* dump the shield if players are only allowed to use it for a short time after respawn */
				if (!playerShielding && !Player_uses_property(pl, USES_SHIELD)) {
					Player_lose_property(pl, HAS_SHIELD);
				}
			}

			Update_player_turn(pl);

			/*
			 * Compute energy drainage
			 */
			if (Player_uses_property(pl, USES_SHIELD)) {
				Player_fuel_add(pl, (int32_t) ED_SHIELD);
			}

			if (Player_uses_property(pl, USES_REFUEL)) {
				if ((Map_get_distance(&pl->fs->pos, &pl->pos) > MAX_FUEL_DISTANCE)
						|| Player_tanks_are_full(pl)
						|| (BIT(World.rules->mode, TEAM_PLAY) && teamFuel && pl->fs->team != pl->team)) {
					Player_disable_property(pl, USES_REFUEL);
				}
				else {
					/* drink from the currently connected fuel station, but not more than
					 * the per-frame limit
					 */
					if (!Player_refuel_advance(pl)) {
						Player_disable_property(pl, USES_REFUEL);
					}
				}
			}

			if (pl->fuel.sum <= 0) {
				Player_disable_property(pl, USES_SHIELD);
				CLR_BIT(pl->obj_status, THRUSTING);
			}
			if (pl->fuel.sum > (pl->fuel.max - REFUEL_RATE)) {
				Player_disable_property(pl, USES_REFUEL);
			}

			Update_player_thrust(pl);
			Object_update_speed(pl); /* New position */
			Move_player(pl);

			if (BIT(pl->obj_status, THRUSTING)) {
				Player_thrust(pl);
			}

			/* sanity check */
			pl->used &= pl->have;
		}
		else {
	                if (Player_is_recovered(pl)) {
	                        /* Player has recovered (unless he is already dead). */
	                        pl->recovery_count = 0.0;

	                        if (Player_is_appearing(pl)) {
	                                Player_set_state(pl, PL_STATE_ALIVE);
	                                Player_go_home(pl);
	                        }
	                        else if (Player_is_dead(pl)) {
	                                Player_go_home(pl);
	                        }
	                }
	                else {
	                        /* Player hasn't recovered yet. */
				Player_proceed_return_home(pl);
	                        Move_player(pl);
	                        continue;
	                }

	                /* Check the player's custom FPS limit */
	                player_fps = fps;
	                player_fps = MIN(player_fps, pl->player_fps);

	                update_turn = true;

	                if (player_fps < fps) {
	                        int32_t divisor = (fps - 1) / player_fps + 1;

	                        /* Update turns internally according to the custom FPS limit */
	                        if (frame_loops % divisor) {
	                                update_turn = false;
	                        }
	                }

	                if (Player_is_alive(pl)) {
	                        if (update_turn) {
	                                Update_player_turn(pl);
	                        }

	                        Move_player(pl);
	                }
		}
	}

	if (Frame_is_real()) {
		/* update distances to locked players */
		for (i = 0; i < NumPlayers; i++) {
			player_t *pl = Players[i];
			if (BIT(pl->lock.flags, LOCK_PLAYER)) {
				pl->lock.distance = Map_get_distance(&((player_t *)pl->lock.object)->pos, &pl->pos);
			}
		}
	}
}

void Update_robots(void)
{
	player_t *pl;
	int32_t i;

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if (!Player_is_robot(pl)) {
			/* Ignore non-robots. */
			continue;
		}

		// TODO that is meant for games without the limit of lives
#if 0
		if (Player_is_dead(pl)) {
			/* Only check for leave if not being transported to homebase. */
			if (Robot_should_leave(pl)) {
				Robot_remove(pl);
				i--;
				updateScores = true;
			}

			continue;
		}
#endif

		/*
		 * Let the robot code control this robot.
		 */
		else if (Player_is_alive(pl)) {
			Robot_play(pl);
		}
	}
}

/*
 * Let the fuel stations regenerate some fuel.
 */
void Update_fuel_stations(void)
{
	int32_t i;

	if (NumPlayers > 0) {
		int32_t fuel = (int32_t) (NumPlayers * STATION_REGENERATION);
		int32_t frames_per_update = MAX_STATION_FUEL / (fuel * BLOCK_SZ);
		for (i = 0; i < World.NumFuels; i++) {
			if (World.fuel[i].fuel == MAX_STATION_FUEL) {
				continue;
			}
			if ((World.fuel[i].fuel += fuel) >= MAX_STATION_FUEL) {
				World.fuel[i].fuel = MAX_STATION_FUEL;
			}
			else if (World.fuel[i].last_change + frames_per_update > frame_loops) {
				/*
				 * We don't send fuelstation info to the clients every frame
				 * if it wouldn't change their display.
				 */
				continue;
			}
			World.fuel[i].conn_mask = 0;
			World.fuel[i].last_change = frame_loops;
		}
	}
}

void Game_compute_team_stats(team_game_stats_t *tstats)
{
	int32_t i;
	player_t *pl;

	memset(tstats, 0, sizeof(team_game_stats_t));

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if (! Player_is_active(pl)) {
			continue;
		}

                if (Player_is_dead(pl)) {
                        tstats->num_dead_players++;
                }
                else if (Player_is_waiting(pl)) {
                	tstats->num_waiting_players++;
                }
                else if (Player_is_alive(pl) || Player_is_appearing(pl)) {
                        tstats->num_alive_players++;
                }

		if (Player_is_dead(pl) || Player_is_waiting(pl)) {
			if (tstats->team_state[pl->team->Num] == TeamEmpty) {
				/* Assume all teammembers are dead. */
				tstats->num_dead_teams++;
				tstats->team_state[pl->team->Num] = TeamDead;
			}
		}
		else if (Player_is_alive(pl) || Player_is_appearing(pl)) {
			if (tstats->team_state[pl->team->Num] != TeamAlive) {
				if (tstats->team_state[pl->team->Num] == TeamDead) {
					/* Oops!  Not all teammembers are dead yet. */
					tstats->num_dead_teams--;
				}
				tstats->team_state[pl->team->Num] = TeamAlive;
				tstats->num_alive_teams++;
				/* Remember a team which was alive. */
				tstats->winning_team = pl->team;
			}
		}
	}
}

void Update_game_status(void)
{
	int32_t i;
	char msg[MSG_LEN];
	player_t *pl;

	/* less ugly hack -pgm */
	if (limitedRoundsGameOver == true) {
		return;
	}

	if (BIT(World.rules->mode, TEAM_PLAY)) {
		team_game_stats_t tstats;

		Game_compute_team_stats(&tstats);

		if (tstats.num_alive_teams > 1) {
			char *bp;
			int32_t teams_with_treasure = 0;
			int32_t team_win[MAX_TEAMS];
			int32_t team_score[MAX_TEAMS];
			int32_t winners;
			int32_t max_destroyed = 0;
			int32_t max_left = 0;
			int32_t max_score = 0;

			/*
			 * Game is not over if more than one team which have treasures
			 * still have one remaining in play.  Note that it is possible
			 * for max_destroyed to be zero, in the case where a team
			 * destroys some treasures and then all quit, and the remaining
			 * teams did not destroy any.
			 */

			for (i = 0; i < MAX_TEAMS; i++) {
				team_score[i] = 0;

				if (i == PAUSE_TEAM_NUM || tstats.team_state[i] != TeamAlive) {
					team_win[i] = 0;
					continue;
				}

				/* mark as potential winning team (because it is not dead) */
				team_win[i] = 1;

				if (World.teams[i].TreasuresDestroyed > max_destroyed) {
					max_destroyed = World.teams[i].TreasuresDestroyed;
				}
				if (World.teams[i].TreasuresLeft > 0) {
					teams_with_treasure++;
				}
			}

			/*
			 * Game is not over if more than one team has treasure.
			 */

			if (teams_with_treasure > 1 || max_destroyed == 0) {
				return;
			}

			/*
			 * Find the winning team;
			 *	Team destroying most number of treasures;
			 *	If drawn; the one with most saved treasures,
			 *	If drawn; the team with the most points,
			 *	If drawn; an overall draw.
			 */
			for (winners = i = 0; i < MAX_TEAMS; i++) {
				if (team_win[i] == 0) {
					continue;
				}

				if (World.teams[i].TreasuresDestroyed == max_destroyed) {
					if (World.teams[i].TreasuresLeft > max_left) {
						max_left = World.teams[i].TreasuresLeft;
					}
					tstats.winning_team = &World.teams[i];
					winners++;
				}
				else {
					team_win[i] = 0;
				}
			}

			if (winners == 1) {
				sprintf(msg, " by destroying %d treasures", max_destroyed);
				Team_game_over(tstats.winning_team, msg);
				return;
			}

			for (i = 0; i < NumPlayers; i++) {
				pl = Players[i];

				if (Player_is_alive(pl) || Player_is_appearing(pl) || Player_is_dead(pl)) {
					team_score[pl->team->Num] += pl->score;
				}
			}

			for (winners = i = 0; i < MAX_TEAMS; i++) {
				if (!team_win[i]) {
					continue;
				}
				if (World.teams[i].TreasuresLeft == max_left) {
					if (team_score[i] > max_score) {
						max_score = team_score[i];
					}
					tstats.winning_team = &World.teams[i];
					winners++;
				}
				else {
					team_win[i] = 0;
				}
			}
			if (winners == 1) {
				sprintf(msg, " by destroying %d treasures and successfully defending %d",
						max_destroyed, max_left);
				Team_game_over(tstats.winning_team, msg);
				return;
			}

			for (winners = i = 0; i < MAX_TEAMS; i++) {
				if (!team_win[i]) {
					continue;
				}
				if (team_score[i] == max_score) {
					tstats.winning_team = &World.teams[i];
					winners++;
				}
				else {
					team_win[i] = 0;
				}
			}
			if (winners == 1) {
				sprintf(msg, " by destroying %d treasures, saving %d, and scoring %d points",
					max_destroyed, max_left, max_score);
				Team_game_over(tstats.winning_team, msg);
				return;
			}

			/* Highly unlikely */

			sprintf(msg, " between teams ");
			bp = msg + strlen(msg);
			for (i = 0; i < MAX_TEAMS; i++) {
				if (!team_win[i])
					continue;
				*bp++ = "0123456789"[i];
				*bp++ = ',';
				*bp++ = ' ';
			}
			bp -= 2;
			*bp = '\0';

			Team_game_over(NULL, msg);
		}

		/*
		 * Here: num_alive_teams <= 1
		 */

		else if (tstats.num_dead_teams > 0) {
			if (tstats.num_alive_teams == 1) {
				Team_game_over(tstats.winning_team, " by staying alive");
			}
			else {
				Team_game_over(NULL, " as everyone died");
			}
		}

		/*
		 * Here: num_alive_teams <= 1 && num_dead_teams == 0
		 */

#if 0
		else if (tstats.num_alive_teams == 0 && tstats.num_dead_teams) {
			if (! doneLastLeftCleanup) {
				for (treasures_destroyed = i = 0; i < MAX_TEAMS; i++) {
					treasures_destroyed += (World.teams[i].NumTreasures - World.teams[i].TreasuresLeft);
				}
				if (treasures_destroyed && (tstats.num_alive_teams == 0)) {
					Team_game_over(tstats.winning_team, " by staying in the game");
				}

				doneLastLeftCleanup = true;
			}
		}
#endif

		else {
			/*
			 * There is a possibility that the game has ended because some
			 * players have quit and there is only one active team.
			 * Reset the round if there is only one active team with alive, dead or waiting players.
			 * There is no reason players should wait in this configuration.
			 */
                        if (tstats.num_alive_teams == 1
                        		&& (tstats.num_waiting_players >= 1 || tstats.num_dead_players >= 1)
                        		&& Team_count_active_players(tstats.winning_team) == Team_count_active_players(NULL)) {
				Team_game_over(tstats.winning_team, " by staying alive");
                        }
		}
	}
	else {
		/* Do we have a winner ? (No team play) */
		int32_t num_alive_players = 0;
		int32_t num_active_players = 0;
		int32_t num_alive_robots = 0;
		int32_t num_active_humans = 0;
		int32_t winner = -1;

		for (i = 0; i < NumPlayers; i++) {
			pl = Players[i];

			if (Player_is_paused(pl) || Player_is_uninitialized(pl)) {
				continue;
			}

			if (!Player_is_waiting(pl) && !Player_is_dead(pl)) {
				num_alive_players++;
				if (Player_is_robot(pl)) {
					num_alive_robots++;
				}
				winner = i; /* Tag player that's alive */
			}
			else if (Player_is_human(pl)) {
				num_active_humans++;
			}
			num_active_players++;
		}

		if (num_alive_players == 1 && num_active_players > 1) {
			Individual_game_over(winner);
		}
		else if (num_alive_players == 0 && num_active_players >= 1) {
			Individual_game_over(-1);
		}
		else if (num_alive_robots > 1 && num_alive_players
				== num_alive_robots && num_active_humans > 0) {
			Individual_game_over(-2);
		}
	}
}
