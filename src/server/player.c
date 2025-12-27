/* $Id: player.c,v 1.23 2008/10/14 20:49:29 rotunda_pk Exp $
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

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "score.h"
#include "bit.h"
#include "netserver.h"
#include "error.h"
#include "objpos.h"
#include "rank.h"

int8_t player_version[] = VERSION;

extern int32_t Rate(int32_t winner, int32_t loser);

bool updateScores = true;
bool limitedRoundsGameOver = false;

/********* **********
 * Functions on player array.
 */

void Pick_startpos(player_t *pl)
{
	int32_t i, num_free;
	int32_t pick = 0, seen = 0;
	static int32_t prev_num_bases = 0;
	static int8_t *free_bases = NULL;

	if (prev_num_bases != World.NumBases) {
		prev_num_bases = World.NumBases;
		if (free_bases != NULL) {
			free(free_bases);
		}
		free_bases = (int8_t *) malloc(World.NumBases
				* sizeof(*free_bases));
		if (free_bases == NULL) {
			error("Can't allocate memory for free_bases");
			End_game();
		}
	}

	num_free = 0;
	for (i = 0; i < World.NumBases; i++) {
		if (World.base[i].team == pl->team) {
			num_free++;
			free_bases[i] = 1;
		}
		else {
			free_bases[i] = 0; /* other team */
		}
	}

	for (i = 0; i < NumPlayers; i++) {
		if (Players[i] != pl && free_bases[Players[i]->home_base->id]) {
			free_bases[Players[i]->home_base->id] = 0; /* occupado */
			num_free--;
		}
	}

	{
		pick = (int32_t) (rfrac() * num_free);
		seen = 0;
		for (i = 0; i < World.NumBases; i++) {
			if (free_bases[i] != 0) {
				if (seen < pick) {
					seen++;
				}
				else {
					break;
				}
			}
		}
	}

	if (i == World.NumBases) {
		//	error("Can't pick startpos (ind=%d,num=%d,free=%d,pick=%d,seen=%d)",
		//	      ind, World.NumBases, num_free, pick, seen);
		error(
				"Can't pick startpos (pl=%d,num=%d,free=%d,pick=%d,seen=%d)",
				pl, World.NumBases, num_free, pick, seen);
		End_game();
	}
	else {
		pl->home_base = &World.base[i];

		for (i = 0; i < NumPlayers; i++) {
			if (Player_is_connected(Players[i])) {
				Send_base(Players[i]->connp, pl);
			}
		}
		if (BIT(pl->status, PLAYING) == 0) {
			pl->count = RECOVERY_DELAY;
		}
		else if (BIT(pl->status, PAUSE | GAME_OVER)) {
			Go_home(pl);
		}
	}
}

void Go_home(player_t *pl)
{
	int32_t x, y, dir;
	DFLOAT vx, vy, velo;

	x = pl->home_base->pos.x;
	y = pl->home_base->pos.y;
	dir = pl->home_base->dir;
	vx = vy = velo = 0;

	pl->dir = dir;
	pl->float_dir = dir;
	Player_position_init_clicks(pl, (x + 0.5) * BLOCK_CLICKS, (y + 0.5)
			* BLOCK_CLICKS);
	pl->vel.x = vx;
	pl->vel.y = vy;
	pl->velocity = velo;
	pl->acc.x = pl->acc.y = 0.0;
	pl->turnacc = pl->turnvel = 0.0;
	memset(pl->last_keyv, 0, sizeof(pl->last_keyv));
	memset(pl->prev_keyv, 0, sizeof(pl->prev_keyv));
	pl->used &= ~USED_KILL;

	if (playerStartsShielded != 0) {
		SET_BIT(pl->used, OBJ_SHIELD);
		if (playerShielding == 0) {
			pl->shield_time = 2 * intGameSpeed;
			SET_BIT(pl->have, OBJ_SHIELD);
		}
	}
	CLR_BIT(pl->status, THRUSTING);

	if (Player_is_robot(pl)) {
		Robot_go_home(pl);
	}
}

/*
 * Give ship one more tank, if possible.
 */
void Player_add_tank(player_t *pl, int32_t tank_fuel)
{
	int32_t tank_cap, add_fuel;

	if (pl->fuel.num_tanks < MAX_TANKS) {
		pl->fuel.num_tanks++;
		tank_cap = TANK_CAP(pl->fuel.num_tanks);
		add_fuel = tank_fuel;
		LIMIT(add_fuel, 0, tank_cap);
		pl->fuel.sum += add_fuel;
		pl->fuel.max += tank_cap;
		pl->fuel.tank[pl->fuel.num_tanks] = add_fuel;
		pl->emptymass += TANK_MASS;
		pl->item[ITEM_TANK] = pl->fuel.num_tanks;
	}
}

/*
 * Remove a tank from a ship, if possible.
 */
void Player_remove_tank(player_t *pl, int32_t which_tank)
{
	int32_t i, tank_ind;
	int32_t tank_fuel, tank_cap;

	if (pl->fuel.num_tanks > 0) {
		tank_ind = which_tank;
		LIMIT(tank_ind, 1, pl->fuel.num_tanks);
		pl->emptymass -= TANK_MASS;
		tank_fuel = pl->fuel.tank[tank_ind];
		tank_cap = TANK_CAP(tank_ind);
		pl->fuel.max -= tank_cap;
		pl->fuel.sum -= tank_fuel;
		pl->fuel.num_tanks--;
		if (pl->fuel.current > pl->fuel.num_tanks) {
			pl->fuel.current = 0;
		}
		else {
			for (i = tank_ind; i <= pl->fuel.num_tanks; i++) {
				pl->fuel.tank[i] = pl->fuel.tank[i + 1];
			}
		}
		pl->item[ITEM_TANK] = pl->fuel.num_tanks;
	}
}

/*
 * Give player the initial number of tanks and amount of fuel.
 * Upto the maximum allowed.
 */
static void Player_init_fuel(player_t *pl, int32_t total_fuel)
{
	int32_t fuel = total_fuel;
	int32_t i;

	pl->fuel.num_tanks = 0;
	pl->fuel.current = 0;
	pl->fuel.max = TANK_CAP(0);
	pl->fuel.sum = MIN(fuel, pl->fuel.max);
	pl->fuel.tank[0] = pl->fuel.sum;
	pl->emptymass = ShipMass;
	pl->item[ITEM_TANK] = pl->fuel.num_tanks;

	fuel -= pl->fuel.sum;

	for (i = 1; i <= World.items[ITEM_TANK].initial; i++) {
		Player_add_tank(pl, fuel);
		fuel -= pl->fuel.tank[i];
	}
}

/** @brief Initialize a freshly-entered player's structure
 *
 * Modified global variables: NumPlayers
 */
player_t* Init_player()
{
	player_t *pl = Players[NumPlayers];
	int32_t i;

	pl->vel.x = pl->vel.y = 0.0;
	pl->acc.x = pl->acc.y = 0.0;
	pl->float_dir = pl->dir = DIR_UP;
	pl->turnvel = 0.0;
	pl->turnacc = 0.0;
	pl->mass = ShipMass;
	pl->emptymass = ShipMass;

	for (i = 0; i < NUM_ITEMS; i++) {
		if (!BIT(1U << i, ITEM_BIT_FUEL | ITEM_BIT_TANK)) {
			pl->item[i] = World.items[i].initial;
		}
	}

	pl->fuel.sum = World.items[ITEM_FUEL].initial << FUEL_SCALE_BITS;
	Player_init_fuel(pl, pl->fuel.sum);

	pl->power = 45.0;
	pl->turnspeed = 30.0;
	pl->turnresistance = 0.12;
	pl->power_s = 35.0;
	pl->turnspeed_s = 25.0;
	pl->turnresistance_s = 0.12;

	pl->count = -1;
	pl->shield_time = 0;

	pl->type = OBJ_PLAYER;
	pl->type_ext = 0; /* assume human player */
	pl->shots = 0;
	pl->shot_speed = ShotsSpeed;
	pl->max_speed = SPEED_LIMIT - pl->shot_speed;
	pl->shot_max = ShotsMax;
	pl->shot_life = ShotsLife;
	pl->shot_time = 0;
	pl->color = WHITE;
	pl->score = 0;
	pl->prev_score = 0;
	pl->fs = NULL;
	pl->name[0] = '\0';

	pl->status = PLAYING | DEF_BITS;
	pl->have = DEF_HAVE;
	pl->used = DEF_USED;

	for (i = 0; i < LOCKBANK_MAX; i++)
		pl->lockbank[i] = NULL;

	pl->mychar = ' ';
	pl->prev_mychar = pl->mychar;
	pl->life = World.rules->lives;
	pl->prev_life = pl->life;
	pl->ball_tmp = NULL;
	pl->player_fps = intGameSpeed; /* kps - this affects how often robots appear to turn */

	pl->kills = 0;
	pl->deaths = 0;

	/*
	 * If limited lives you will have to wait 'til everyone gets GAME OVER.
	 */
	// TODO: modify this so that if the world state did not change
	// since the start of round (no players killed, no balls stolen, etc),
	// new players are still accepted without waiting
	if (BIT(World.rules->mode, LIMITED_LIVES) && NumPlayers > 0) {
		pl->mychar = 'W';
		pl->prev_life = pl->life = 0;
		SET_BIT(pl->status, GAME_OVER);
	}

	pl->team = NULL;

	pl->lock.flags = LOCK_NONE;
	pl->lock.object = NULL;

	pl->robot_data_ptr = NULL;

	//pl->id = peek_ID();
	pl->id = request_ID();
	GetInd[pl->id] = NumPlayers;
	NumPlayers++;
	//request_ID();

	pl->connp = NULL;

	pl->shove_next = 0;
	for (i = 0; i < MAX_RECORDED_SHOVES; i++) {
		pl->shove_record[i].pusher_pl = NULL;
	}

	pl->frame_last_busy = frame_loops; /*timing ok, used only to determine pausing -pgm*/

	pl->isowner = false;
	pl->isoperator = false;
	pl->oldturn = false;

	return pl;
}

static player_t *playerArray;

void Alloc_players(int32_t number)
{
	player_t *p;
	int32_t i;

	/* Allocate space for pointers */
	Players = (player_t **) calloc(number + 1, sizeof(player_t *));

	/* Allocate space for all entries, all player structs */
	p = playerArray = (player_t *) calloc(number, sizeof(player_t));

	if (!Players || !playerArray) {
		error("Not enough memory for Players.");
		exit(1);
	}

	/* Players[-1] should evaluate to NULL. */
	Players++;

	for (i = 0; i < number; i++) {
		Players[i] = p++;
	}
}

void Free_players(void)
{
	if (Players) {
		--Players;
		free(Players);
		Players = NULL;

		free(playerArray);
	}
}

void Update_score_table(void)
{
	int32_t i, j;
	player_t *pl;

	for (j = 0; j < NumPlayers; j++) {
		pl = Players[j];
		if (pl->score != pl->prev_score || pl->life != pl->prev_life
				|| pl->mychar != pl->prev_mychar) {
			pl->prev_score = pl->score;
			pl->prev_life = pl->life;
			pl->prev_mychar = pl->mychar;
			for (i = 0; i < NumPlayers; i++) {
				if (Player_is_connected(Players[i])) {
					Send_score(Players[i]->connp, pl);
				}
			}
		}
	}
	updateScores = false;
}

void Reset_all_players(void)
{
	player_t *pl;
	int32_t i, j;

	updateScores = true;

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		if (endOfRoundReset) {
			if (!BIT(pl->status, PAUSE)) {
				Kill_player(pl, false);
				if (pl != Players[i]) {
					/* player was deleted. */
					i--;
					continue;
				}
			}
		}

		if ((!BIT(pl->status, PAUSE)) && (!(pl->mychar == 'W')))
			Rank_add_round(pl);

		CLR_BIT(pl->status, GAME_OVER);
		CLR_BIT(pl->have, OBJ_BALL);
		pl->kills = 0;
		pl->deaths = 0;
		/* do not reset frame_last_busy after round end -pgm */
		//pl->frame_last_busy = frame_loops; /*timing ok, used only to determine pausing -pgm */
		if (!BIT(pl->status, PAUSE)) {
			pl->mychar = ' ';
			pl->life = World.rules->lives;
		}
		if (Player_is_robot(pl))
			pl->mychar = 'R';
	}

	if (BIT(World.rules->mode, TEAM_PLAY)) {
		/* Detach any balls and kill ball */
		/* We are starting all over again */
		for (j = NumObjs - 1; j >= 0; j--) {
			if (BIT(Obj[j]->type, OBJ_BALL)) {
				Delete_object(Obj[j]);
			}
		}

		/* Reset the treasures */
		for (i = 0; i < World.NumTreasures; i++) {
			World.treasures[i].destroyed = 0;
			Make_treasure_ball(&World.treasures[i]);
		}

		/* Reset the teams */
		for (i = 0; i < MAX_TEAMS; i++) {
			World.teams[i].TreasuresDestroyed = 0;
			World.teams[i].TreasuresLeft
					= World.teams[i].NumTreasures;
		}
	}

	if (endOfRoundReset) {
		for (i = 0; i < NumObjs; i++) {
			object_t *obj = Obj[i];
			if (BIT(obj->type, OBJ_SHOT | OBJ_DEBRIS | OBJ_SPARK))
				obj->life = 0;
		}
	}

	Update_score_table();
}

void Check_team_members(team_t *team)
{
	player_t *pl;
	int32_t members, i;

	if (!BIT(World.rules->mode, TEAM_PLAY))
		return;

	for (members = i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		if (pl->team != NULL && pl->team == team)
			members++;
	}
	if (team->NumMembers != members) {
		error("Server has reset team %d members from %d to %d", team->Num,
				team->NumMembers, members);
		for (i = 0; i < NumPlayers; i++) {
			pl = Players[i];
			if (pl->team != NULL && pl->team == team)
				error(
						"Team %d currently has player %d: \"%s\"",
						team, i + 1, pl->name);
		}
		team->NumMembers = members;
	}
}

static void Compute_end_of_round_values(int32_t *average_score,
		int32_t *num_best_players, DFLOAT *best_ratio, int32_t best_players[])
{
	int32_t i;
	DFLOAT ratio;

	/* Initialize everything */
	*average_score = 0;
	*num_best_players = 0;
	*best_ratio = -1.0;

	/* Figure out what the average score is and who has the best kill/death */
	/* ratio for this round */
	for (i = 0; i < NumPlayers; i++) {
		if ((BIT(Players[i]->status, PAUSE) && Players[i]->count <= 0)) {
			continue;
		}
		*average_score += Players[i]->score;
		ratio = (DFLOAT) Players[i]->kills / (Players[i]->deaths + 1);
		if (ratio > *best_ratio) {
			*best_ratio = ratio;
			best_players[0] = i;
			*num_best_players = 1;
		}
		else if (ratio == *best_ratio) {
			best_players[(*num_best_players)++] = i;
		}
	}
	*average_score /= NumPlayers;
}

static void Give_best_player_bonus(int32_t average_score, int32_t num_best_players,
		DFLOAT best_ratio, int32_t best_players[])
{
	int32_t i;
	int32_t points;
	int8_t msg[MSG_LEN];

	if (num_best_players < 1 || best_ratio <= 0.0f) {
		sprintf(msg, "There is no Deadly Player.");
	}
	else if (num_best_players == 1) {
		player_t *bp = Players[best_players[0]];

		sprintf(
				msg,
				"%s is the Deadliest Player with a kill ratio of %d/%d.",
				bp->name, bp->kills, bp->deaths);
		points = (int32_t) (best_ratio * Rate(bp->score, average_score));
		SCORE(bp, points, OBJ_X_IN_BLOCKS(bp), OBJ_Y_IN_BLOCKS(bp), "[Deadliest]");
	}
	else {
		msg[0] = '\0';
		for (i = 0; i < num_best_players; i++) {
			player_t *bp = Players[best_players[i]];
			int32_t ratio = Rate(bp->score, average_score);
			DFLOAT score = (DFLOAT) (ratio + num_best_players)
					/ num_best_players;

			if (msg[0]) {
				if (i == num_best_players - 1)
					strcat(msg, " and ");
				else
					strcat(msg, ", ");
			}
			if (strlen(msg) + 8 + strlen(bp->name) >= sizeof(msg)) {
				Set_message(msg);
				msg[0] = '\0';
			}
			strcat(msg, bp->name);
			points = (int32_t) (best_ratio * score);
			SCORE(bp, points, OBJ_X_IN_BLOCKS(bp), OBJ_Y_IN_BLOCKS(bp), "[Deadly]");
		}
		if (strlen(msg) + 64 >= sizeof(msg)) {
			Set_message(msg);
			msg[0] = '\0';
		}
		sprintf(
				msg + strlen(msg),
				" are the Deadly Players with kill ratios of %d/%d.",
				Players[best_players[0]]->kills,
				Players[best_players[0]]->deaths);
	}
	Set_message(msg);
}

static void Give_individual_bonus(player_t *pl, int32_t average_score)
{
	DFLOAT ratio;
	int32_t points;

	ratio = (DFLOAT) pl->kills / (pl->deaths + 1);
	points = (int32_t) (ratio * Rate(pl->score, average_score));
	SCORE(pl, points, OBJ_X_IN_BLOCKS(pl), OBJ_Y_IN_BLOCKS(pl), "[Winner]");
}

static void Count_rounds(void)
{
	int8_t msg[MSG_LEN];

	if (!roundsToPlay) {
		return;
	}

	++roundsPlayed;

	sprintf(msg, " < Round %d out of %d completed. >", roundsPlayed,
			roundsToPlay);
	Set_message(msg);
	if (roundsPlayed >= roundsToPlay) {
		Game_Over();
	}
}

void Team_game_over(team_t *winning_team, const int8_t *reason)
{
	int32_t i, j;
	int32_t average_score;
	int32_t num_best_players;
	int32_t *best_players;
	DFLOAT best_ratio;
	int8_t msg[MSG_LEN];

	if (!(best_players = (int32_t *) malloc(NumPlayers * sizeof(int32_t)))) {
		error("no mem");
		End_game();
	}

	/* Figure out the average score and who has the best kill/death ratio */
	/* ratio for this round */
	Compute_end_of_round_values(&average_score, &num_best_players,
			&best_ratio, best_players);

	/* Print out the results of the round */
	if (winning_team) {
		sprintf(msg, " < Team %d has won the game%s! >", winning_team->Num,
				reason);
	}
	else {
		sprintf(msg, " < We have a draw%s! >", reason);
	}
	Set_message(msg);

	/* Give bonus to the best player */
	Give_best_player_bonus(average_score, num_best_players, best_ratio,
			best_players);

	/* Give bonuses to the winning team */
	if (winning_team) {
		for (i = 0; i < NumPlayers; i++) {
			if (Players[i]->team != winning_team) {
				continue;
			}
			if ((BIT(Players[i]->status, PAUSE)
					&& Players[i]->count <= 0) || (BIT(
					Players[i]->status, GAME_OVER)
					&& Players[i]->mychar == 'W')) {
				continue;
			}
			for (j = 0; j < num_best_players; j++) {
				if (i == best_players[j]) {
					break;
				}
			}
			if (j == num_best_players) {
				Give_individual_bonus(Players[i], average_score);
			}
		}
	}

	Reset_all_players();

	Count_rounds();

	free(best_players);

	/* printf("Rank write; end of round\n");*/
	Rank_write_webpage();
	Rank_write_rankfile();

}

void Individual_game_over(int32_t winner)
{
	int32_t i, j;
	int32_t average_score;
	int32_t num_best_players;
	int32_t *best_players;
	DFLOAT best_ratio;
	int8_t msg[MSG_LEN];

	if (!(best_players = (int32_t *) malloc(NumPlayers * sizeof(int32_t)))) {
		error("no mem");
		End_game();
	}

	/* Figure out what the average score is and who has the best kill/death */
	/* ratio for this round */
	Compute_end_of_round_values(&average_score, &num_best_players,
			&best_ratio, best_players);

	/* Print out the results of the round */
	if (winner == -1) {
		Set_message(" < We have a draw! >");
	}
	else if (winner == -2) {
		Set_message(" < The robots have won the game! >");
		/* Perhaps this should be a different sound? */
	}
	else {
		sprintf(msg, " < %s has won the game! >", Players[winner]->name);
		Set_message(msg);
	}

	/* Give bonus to the best player */
	Give_best_player_bonus(average_score, num_best_players, best_ratio,
			best_players);

	/* Give bonus to the winning player */
	if (winner >= 0) {
		for (i = 0; i < num_best_players; i++) {
			if (winner == best_players[i]) {
				break;
			}
		}
		if (i == num_best_players) {
			Give_individual_bonus(Players[winner], average_score);
		}
	}
	else if (winner == -2) {
		for (j = 0; j < NumPlayers; j++) {
			if (Player_is_robot(Players[j])) {
				for (i = 0; i < num_best_players; i++) {
					if (j == best_players[i]) {
						break;
					}
				}
				if (i == num_best_players) {
					Give_individual_bonus(Players[j],
							average_score);
				}
			}
		}
	}

	Reset_all_players();

	free(best_players);
}

void Compute_game_status(void)
{
	int32_t i;
	int8_t msg[MSG_LEN];

	/* less ugly hack -pgm */
	if (limitedRoundsGameOver == true)
		return;

	if (BIT(World.rules->mode, TEAM_PLAY)) {
		/* Do we have a winning team ? */

		enum TeamState {
			TeamEmpty, TeamDead, TeamAlive
		} team_state[MAX_TEAMS];
		int32_t num_dead_teams = 0;
		int32_t num_alive_teams = 0;
		//int32_t winning_team = -1;
		team_t *winning_team = NULL;

		for (i = 0; i < MAX_TEAMS; i++) {
			team_state[i] = TeamEmpty;
		}

		for (i = 0; i < NumPlayers; i++) {
			if (BIT(Players[i]->status, PAUSE)) {
				/* Ignore paused players. */
				continue;
			}
#if 0
			/* not all teammode maps have treasures. */
			else if (World.teams[Players[i]->team].NumTreasures == 0) {
				/* Ignore players with no treasure troves */
				continue;
			}
#endif
			else if (BIT(Players[i]->status, GAME_OVER)) {
				if (team_state[Players[i]->team->Num] == TeamEmpty) {
					/* Assume all teammembers are dead. */
					num_dead_teams++;
					team_state[Players[i]->team->Num] = TeamDead;
				}
			}
			/*
			 * If the player is not paused and he is not in the
			 * game over mode and his team owns treasures then he is
			 * considered alive.
			 * But he may not be playing though if the rest of the team
			 * was genocided very quickly after game reset, while this
			 * player was still being transported back to his homebase.
			 */
			else if (team_state[Players[i]->team->Num] != TeamAlive) {
				if (team_state[Players[i]->team->Num] == TeamDead) {
					/* Oops!  Not all teammembers are dead yet. */
					num_dead_teams--;
				}
				team_state[Players[i]->team->Num] = TeamAlive;
				++num_alive_teams;
				/* Remember a team which was alive. */
				winning_team = Players[i]->team;
			}
		}

		if (num_alive_teams > 1) {
			int8_t *bp;
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
				if (team_state[i] != TeamAlive) {
					team_win[i] = 0;
					continue;
				}
				team_win[i] = 1;

				if (World.teams[i].TreasuresDestroyed
						> max_destroyed)
					max_destroyed
							= World.teams[i].TreasuresDestroyed;
				if (World.teams[i].TreasuresLeft)
					teams_with_treasure++;
			}

			/*
			 * Game is not over if more than one team has treasure.
			 */

			if (teams_with_treasure > 1 || !max_destroyed) {
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
				if (!team_win[i])
					continue;
				if (World.teams[i].TreasuresDestroyed
						== max_destroyed) {
					if (World.teams[i].TreasuresLeft
							> max_left)
						max_left
								= World.teams[i].TreasuresLeft;
					winning_team = &World.teams[i];
					winners++;
				}
				else {
					team_win[i] = 0;
				}
			}
			if (winners == 1) {
				sprintf(msg, " by destroying %d treasures",
						max_destroyed);
				Team_game_over(winning_team, msg);
				return;
			}

			for (i = 0; i < NumPlayers; i++) {
				if (BIT(Players[i]->status, PAUSE))
					continue;
				team_score[Players[i]->team->Num]
						+= Players[i]->score;
			}

			for (winners = i = 0; i < MAX_TEAMS; i++) {
				if (!team_win[i])
					continue;
				if (World.teams[i].TreasuresLeft == max_left) {
					if (team_score[i] > max_score)
						max_score = team_score[i];
					winning_team = &World.teams[i];
					winners++;
				}
				else {
					team_win[i] = 0;
				}
			}
			if (winners == 1) {
				sprintf(
						msg,
						" by destroying %d treasures and successfully defending %d",
						max_destroyed, max_left);
				Team_game_over(winning_team, msg);
				return;
			}

			for (winners = i = 0; i < MAX_TEAMS; i++) {
				if (!team_win[i])
					continue;
				if (team_score[i] == max_score) {
					winning_team = &World.teams[i];
					winners++;
				}
				else {
					team_win[i] = 0;
				}
			}
			if (winners == 1) {
				sprintf(msg,
						" by destroying %d treasures, saving %d, and "
							"scoring %d points",
						max_destroyed, max_left,
						max_score);
				Team_game_over(winning_team, msg);
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
		else if (num_dead_teams > 0) {
			if (num_alive_teams == 1)
				Team_game_over(winning_team,
						" by staying alive");
			else
				Team_game_over(NULL, " as everyone died");
		}
		else {
			/*
			 * num_alive_teams <= 1 && num_dead_teams == 0
			 *
			 * There is a possibility that the game has ended because players
			 * quit, the game over state is needed to reset treasures.  We
			 * must count how many treasures are missing, if there are any
			 * the playing team (if any) wins.
			 */
			int32_t i, treasures_destroyed;

			for (treasures_destroyed = i = 0; i < MAX_TEAMS; i++) {
				treasures_destroyed
						+= (World.teams[i].NumTreasures
								- World.teams[i].TreasuresLeft);
			}
			if (treasures_destroyed && (num_alive_teams == 0))
				Team_game_over(winning_team,
						" by staying in the game");

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
			if (BIT(Players[i]->status, PAUSE))
				continue;

			if (!BIT(Players[i]->status, GAME_OVER)) {
				num_alive_players++;
				if (Player_is_robot(Players[i])) {
					num_alive_robots++;
				}
				winner = i; /* Tag player that's alive */
			}
			else if (Player_is_human(Players[i])) {
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

/** @brief Removes a player from game
 *
 * Modified global variables: NumPlayers, NumOperators, NumRobots
 *
 * @param pl	pointer to the player's structure
 */
void Delete_player(player_t *pl)
{
	player_t *pl2;
	object_t *obj;
	int32_t i, j, ind2;

	if (Player_is_robot(pl)) {
		Robot_destroy(pl);
	}

	if (pl->isoperator) {
		if (!--NumOperators && game_lock) {
			game_lock = false;
			Set_message(" < The game has been unlocked as "
				"the last operator left! >");
		}
	}

	/* Won't be swapping anywhere */
	for (i = MAX_TEAMS - 1; i >= 0; i--)
		if (World.teams[i].Swapper == pl)
			World.teams[i].Swapper = NULL;
#if 0
	if (pl->team != NULL) {
		/* Swapping a queued player might be better */
		pl->team->Swapper = NULL;
	}
#endif

	/* Delete remaining shots */
	for (i = NumObjs - 1; i >= 0; i--) {
		obj = Obj[i];

		if (obj->owner == pl) {
                        /* Attached balls should be destroyed,
                         * ownership of floating balls should be cleared */
			if (obj->type == OBJ_BALL) {
                            if (BIT(obj->status, IS_ATTACHED)) {
				treasure_t *t = obj->treasure;

				Delete_object(obj);
				Make_treasure_ball(t);
                            }
                            else {
                                obj->owner = NULL;
                            }
			}
			else if (BIT(obj->type, OBJ_DEBRIS | OBJ_SPARK)) {
				/* Okay, so you want robot explosions to exist,
				 * even if the robot left the game. */
				obj->owner = NULL;
			}
			else {
				if (!keepShots) {
					obj->life = 0;
				}

				obj->owner = NULL;
			}
		}
	}

	Free_ship_shape(pl->ship);

	NumPlayers--;

	/* player left, save the rank now */
	if (pl->rank) {
		Rank_save_score(pl);
		if (NumPlayers == NumRobots) {
			/*printf("Rank_save\n");*/
			Rank_write_webpage();
			Rank_write_rankfile();
		}
	}

	if (pl->team != NULL) {
		pl->team->NumMembers--;
		if (Player_is_robot(pl))
			pl->team->NumRobots--;
	}

	if (Player_is_robot(pl)) {
		NumRobots--;
	}

	/*
	 * Swap entry no 'ind' with the last one.
	 *
	 * Change the Players[] pointer array to have Players[ind] point to
	 * a valid player and move our leaving player to Players[NumPlayers].
	 */
	ind2 = GetInd[pl->id];
	pl2 = Players[NumPlayers]; /* Swap pointers... */
	Players[NumPlayers] = pl;
	Players[ind2] = pl2;

	GetInd[pl2->id] = ind2;
	GetInd[pl->id] = NumPlayers;

	Check_team_members(pl->team);

	for (i = NumPlayers - 1; i >= 0; i--) {
		if (BIT(Players[i]->lock.flags, LOCK_PLAYER | LOCK_VISIBLE)
				&& (Players[i]->lock.object == pl || NumPlayers
						<= 1)) {
			CLR_BIT(Players[i]->lock.flags, LOCK_PLAYER
					| LOCK_VISIBLE);
		}
		if (Player_is_robot(Players[i]) && Robot_war_on_player(Players[i]) == pl) {
			Robot_reset_war(Players[i]);
		}
		for (j = 0; j < LOCKBANK_MAX; j++) {
			if (Players[i]->lockbank[j] == pl)
				Players[i]->lockbank[j] = NULL;
		}
		for (j = 0; j < MAX_RECORDED_SHOVES; j++) {
			if (Players[i]->shove_record[j].pusher_pl == pl) {
				Players[i]->shove_record[j].pusher_pl = NULL;
			}
		}
	}

	for (i = NumPlayers - 1; i >= 0; i--) {
		if (Player_is_connected(Players[i])) {
			Send_leave(Players[i]->connp, pl->id);
		}
	}

	release_ID(pl->id);
}

void Detach_ball(player_t *pl, object_t *ball)
{
	int32_t i, cnt;

	/* Interrupt the non-solid connector, if present */
	if (ball == NULL || ball == pl->ball_tmp) {
		pl->ball_tmp = NULL;
		CLR_BIT(pl->used, OBJ_CONNECTOR);
	}

	if (BIT(pl->have, OBJ_BALL)) {
		for (cnt = i = 0; i < NumObjs; i++) {
			if (BIT(Obj[i]->type, OBJ_BALL) &&
					BIT(Obj[i]->status, IS_ATTACHED) &&
					Obj[i]->owner == pl) {
				if (ball == NULL || ball == Obj[i]) {
					/* Don't reset owner so you can throw balls */
					CLR_BIT(Obj[i]->status, IS_ATTACHED);
				}
				else {
					cnt++;
				}
			}
		}
		if (cnt == 0)
			CLR_BIT(pl->have, OBJ_BALL);
	}
}

void Kill_player(player_t *pl, bool rank_death)
{
	Explode_fighter(pl);
	Player_death_reset(pl, rank_death);
}

void Player_death_reset(player_t *pl, bool rank_death)
{
	int32_t i;

	Detach_ball(pl, NULL);

	pl->vel.x = pl->vel.y = 0.0;
	pl->acc.x = pl->acc.y = 0.0;
	pl->emptymass = pl->mass = ShipMass;
	pl->status |= DEF_BITS;
	pl->status &= ~(KILL_BITS);

	for (i = 0; i < NUM_ITEMS; i++) {
		if (!BIT(1U << i, ITEM_BIT_FUEL | ITEM_BIT_TANK)) {
			pl->item[i] = World.items[i].initial;
		}
	}

	pl->shot_speed = ShotsSpeed;
	pl->shot_max = ShotsMax;
	pl->shot_life = ShotsLife;
	pl->count = RECOVERY_DELAY;
	pl->lock.distance = 0;

	Player_init_fuel(pl, World.items[ITEM_FUEL].initial * FUEL_SCALE_FACT);
	if (rank_death)
		Rank_add_death(pl);

	/*-BA Handle the combination of limited life games and
	 *-BA robotLeaveLife by making a robot leave iff it gets
	 *-BA eliminated in any round.  Means that robotLeaveLife
	 *-BA is ignored, but that robotsLeave is still respected.
	 *-BD Added check on race mode. Since in race mode everyone
	 *-BD gets killed at the end of the round, all robots would
	 *-BD be replaced in the next round. I don't think that's
	 *-BD the Right Thing to do.
	 *-BD Also, only check a robot's score at the end of the round.
	 *-BD 27-2-98 Check on team mode too. It's very confusing to
	 *-BD have different robots in your team every round.
	 */

	if (BIT(World.rules->mode, LIMITED_LIVES)) {
		pl->life--;
		if (pl->life == -1) {
			if (Player_is_robot(pl)) {
				if (!BIT(World.rules->mode, TEAM_PLAY)
						|| (robotsLeave
								&& pl->score
										< robotLeaveScore)) {
					Robot_delete(pl, false);
					return;
				}
			}
			pl->life = 0;
			SET_BIT(pl->status, GAME_OVER);
			pl->mychar = 'D';
			Player_lock_closest(pl, false);
		}
	}
	else {
		pl->life++;
	}

	pl->have = DEF_HAVE;
	pl->used |= DEF_USED;
	pl->used &= ~(USED_KILL);
	pl->used &= pl->have;
}

void Players_swap_each_other(player_t *pl1, player_t *pl2)
{
	base_t *tmp_base = pl2->home_base;
	team_t *tmp_team = pl2->team;

	pl2->team = pl1->team;
	pl1->team = tmp_team;

	pl2->home_base = pl1->home_base;
	pl1->home_base = tmp_base;

	Set_swapper_state(pl1);
	Set_swapper_state(pl2);
	Send_info_about_player(pl1);
	Send_info_about_player(pl2);
}

void Set_swapper_state(player_t *pl)
{
	if (BIT(pl->have, OBJ_BALL)) {
		Detach_ball(pl, NULL);
	}
	if (BIT(World.rules->mode, LIMITED_LIVES)) {
		int32_t i;

		for (i = 0; i < NumPlayers; i++) {
			if (!TEAM(pl, Players[i]) && !BIT(Players[i]->status,
					PAUSE)) {
				/* put team swapping player waiting mode. */
				if (pl->mychar == ' ') {
					pl->mychar = 'W';
				}
				pl->prev_life = pl->life = 0;
				SET_BIT(pl->status, GAME_OVER | PLAYING);
				CLR_BIT(pl->status, SELF_DESTRUCT);
				pl->count = -1;
				break;
			}
		}
	}
}

void Send_info_about_player(player_t *pl)
{
	int32_t i;

	for (i = 0; i < NumPlayers; i++) {
		if (Player_is_connected(Players[i])) {
			Send_player(Players[i]->connp, pl);
			Send_score(Players[i]->connp, pl);
			Send_base(Players[i]->connp, pl);
		}
	}
}
