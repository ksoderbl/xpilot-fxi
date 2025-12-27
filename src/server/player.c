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
#include <string.h>
#include <stdio.h>

#include "version.h"
#include "commonproto.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "debug.h"
#include "bit.h"
#include "proto.h"
#include "error.h"
#include "objpos.h"

#include "player.h"
#include "map.h"
#include "player_inline.h"

#include "score.h"
#include "netserver.h"
#include "rank.h"
#include "frame.h"
#include "server.h"
#include "robot.h"
#include "ship.h"
#include "update.h"

char player_version[] = VERSION;

player_t **Players;
int32_t GetInd[NUM_IDS + 1];

/** @brief Number of players in game (including paused and waiting)
 *
 * This variable is modified only in @ref Init_player and @ref Delete_player.
 */
int32_t NumPlayers = 0;

/** @brief Number of pausers
 *
 */
int32_t NumPaused = 0;

bool updateScores = false;

/*
 * Functions on player array.
 */

/** \brief Sets parameters of player's flight back to the home base
 *
 * Transport a corpse from the place where it died back to its home base,
 * or if in race mode, back to the last passed check point.
 *
 * During the first part of the distance we give it a positive constant
 * acceleration G, during the second part we make this a negative one -G.
 * This results in a visually pleasing take off and landing.
 *
 * \param pl	player
 */
void Player_proceed_return_home(player_t *pl)
{
	double t, m;
	int32_t dx, dy;
	const double T = RECOVERY_DELAY_TICKS;

	t = pl->recovery_count;
	if (2.0 * t <= T)
	{
		m = 2.0 / t;
	}
	else
	{
		t = T - t;
		m = (4.0 * t) / (T * T - 2.0 * t * t);
	}

	if (Frame_is_real())
	{
		dx = WRAP_DX(pl->home_base->pos.x - pl->pos.x);
		dy = WRAP_DY(pl->home_base->pos.y - pl->pos.y);
		pl->vel.x = dx * m;
		pl->vel.y = dy * m;
	}
	else
	{
		dx = WRAP_DX(pl->home_base->pos.x - pl->pos_interp.x);
		dy = WRAP_DY(pl->home_base->pos.y - pl->pos_interp.y);
		pl->vel_interp.x = dx * m;
		pl->vel_interp.y = dy * m;
	}
}

/*
 * Fixes the player's position to his home base, stops the ship, brings up the shield if necessary.
 */
void Player_go_home(player_t *pl)
{
	pl->dir = pl->home_base->dir;
	pl->float_dir = pl->dir;

	Position_copy(&pl->pos, &pl->home_base->pos);
	Position_copy(&pl->pos_interp, &pl->home_base->pos);
	//	Player_position_remember(pl);
	pl->vel.x = 0;
	pl->vel.y = 0;
	pl->velocity = 0.0;
	pl->acc.x = pl->acc.y = 0.0;
	pl->turnacc = pl->turnvel = 0.0;

	/* set initial properties */
	pl->have = HAS_NOTHING;
	Player_get_property(pl, HAS_DEFAULT);
	pl->used &= pl->have;

	/* bring up the shield if necessary */
	if (playerStartsShielded)
	{
		Player_get_property(pl, HAS_SHIELD);
		Player_enable_property(pl, USES_SHIELD);

		if (!playerShielding)
		{
			pl->shield_count = SHIELD_DELAY_TICKS;
		}
	}

	Player_enable_property(pl, USES_COMPASS);

	if (Player_is_robot(pl))
	{
		Robot_go_home(pl);
	}
}

bool Player_is_home(player_t *pl)
{
	if (Frame_is_real())
	{
		return ((pl->pos.cx == pl->home_base->pos.cx) && (pl->pos.cy == pl->home_base->pos.cy)) ? true : false;
	}
	else
	{
		return ((pl->pos_interp.cx == pl->home_base->pos.cx) && (pl->pos_interp.cy == pl->home_base->pos.cy)) ? true : false;
	}
}

/** \brief Initialize a freshly-entered player's structure
 *
 * Modified global variables: NumPlayers
 */
player_t *Player_add(void)
{
	player_t *pl = Players[NumPlayers];
	int32_t i;

	memset(pl, 0, sizeof(player_t));

	Player_set_state(pl, PL_STATE_UNDEFINED);

	Player_reset(pl);

	pl->float_dir = pl->dir = DIR_UP;
	pl->emptymass = ShipMass;

	pl->power = 45.0;
	pl->turnspeed = 30.0;
	pl->turnresistance = 0.12;
	pl->power_s = 35.0;
	pl->turnspeed_s = 25.0;
	pl->turnresistance_s = 0.12;

	pl->type = OBJ_PLAYER;
	pl->shot_speed = ShotsSpeed;
	pl->max_speed = SPEED_LIMIT - pl->shot_speed;

	pl->shot_max = ShotsMax;

	pl->color = WHITE;

	for (i = 0; i < LOCKBANK_MAX; i++)
	{
		pl->lockbank[i] = NULL;
	}

	pl->pl_type_mychar = ' ';
	Player_set_life(pl, World.rules->lives);

	pl->player_fps = fps;

	pl->id = request_ID();
	GetInd[pl->id] = NumPlayers;
	NumPlayers++;

	pl->frame_last_busy = frame_loops; /*timing ok, used only to determine pausing -pgm*/

	pl->isoperator = false;
	pl->oldturn = false;

	return pl;
}

static player_t *playerArray;

void Players_allocate(int32_t number)
{
	player_t *p;
	int32_t i;

	/* Allocate space for pointers */
	Players = (player_t **)calloc(number + 1, sizeof(player_t *));

	/* Allocate space for all entries, all player structs */
	p = playerArray = (player_t *)calloc(number, sizeof(player_t));

	if (!Players || !playerArray)
	{
		error("Not enough memory for Players.");
		exit(1);
	}

	/* Players[-1] should evaluate to NULL. */
	Players++;

	for (i = 0; i < number; i++)
	{
		Players[i] = p++;
	}
}

void Players_free(void)
{
	if (Players)
	{
		--Players;
		free(Players);
		Players = NULL;

		free(playerArray);
	}
}

void Players_send_score(void)
{
	int32_t i, j;
	player_t *pl, *pl2;

	for (j = 0; j < NumPlayers; j++)
	{
		pl = Players[j];

		/* send only to humans */
		if (pl->update_scoreboard && Player_is_human(pl))
		{
			for (i = 0; i < NumPlayers; i++)
			{
				pl2 = Players[i];

				if (pl2->update_scoreboard)
				{
					Send_score(pl->connp, pl2);
				}
			}
		}
	}

	updateScores = false;
}

/** @brief Reset all active players at the end of round
 *
 */
void Players_reset(void)
{
	player_t *pl;
	int32_t i;

	for (i = 0; i < NumPlayers; i++)
	{
		pl = Players[i];

		if (!(Player_is_alive(pl) || Player_is_appearing(pl) || Player_is_dead(pl) || Player_is_waiting(pl)))
		{
			continue;
		}

		if (BIT(World.rules->mode, TEAM_PLAY) && !Player_is_waiting(pl))
		{
			Rank_add_round(pl);
		}

		Player_explode(pl);
		Player_reset(pl);

		if (Player_is_human(pl) && (Player_is_alive(pl) || Player_is_appearing(pl)) && Player_idle_timed_out(pl))
		{
			Player_pause_forced(pl);
			Message_game_print("%s was paused for idling.", pl->name);
		}
		else
		{
			/*
			 * The real position has to be updated, otherwise the ship's movement back to the home base
			 * will show irregularities.
			 */
			Position_copy(&pl->pos, &pl->pos_interp);

			pl->kills = 0;
			pl->deaths = 0;
			pl->self_deaths = 0;

			Player_set_state(pl, PL_STATE_APPEARING);
			Player_set_life(pl, World.rules->lives);

			updateScores = true;
		}
	}
}

/** \brief Counts players who were killed by someone else at least once during the round
 *
 * Suicides do not count.
 *
 * \param team	team on which players should be counted, unspecified if NULL
 * \return	number of killed players in round
 */
int32_t Round_count_killed_players(team_t *team)
{
	int32_t i;
	player_t *pl;
	int32_t count = 0;

	for (i = 0; i < NumPlayers; i++)
	{
		pl = Players[i];

		if (!(Player_is_alive(pl) || Player_is_appearing(pl) || Player_is_waiting(pl) || Player_is_killed(pl) || Player_is_dead(pl)))
		{
			continue;
		}

		if ((pl->team == team || team == NULL) && (pl->deaths > pl->self_deaths))
		{
			count++;
		}
	}

	return count;
}

void Player_add_to_team(player_t *pl, team_t *team)
{
	pl->team = team;
	team->NumMembers++;

	if (Player_is_robot(pl))
	{
		pl->team->NumRobots++;
	}
}

void Player_remove_from_team(player_t *pl)
{
	if (Player_is_robot(pl))
	{
		pl->team->NumRobots--;
	}

	pl->team->NumMembers--;
	pl->team = NULL;
}

void Individual_game_over(int32_t winner)
{
	int32_t i, j;
	int32_t average_score;
	int32_t num_best_players;
	player_t **best_players;
	DFLOAT best_ratio;
	player_t *pl;

	if (!(best_players = (player_t **)malloc(NumPlayers * sizeof(player_t **))))
	{
		error("no mem");
		End_game();
	}

	/* Figure out what the average score is and who has the best kill/death */
	/* ratio for this round */
	Score_compute_end_of_round(&average_score, &num_best_players,
							   &best_ratio, best_players);

	/* Print out the results of the round */
	if (winner == -1)
	{
		Message_game_important_print("We have a draw!");
	}
	else if (winner == -2)
	{
		Message_game_important_print("The robots have won the game!");
		/* Perhaps this should be a different sound? */
	}
	else
	{
		Message_game_important_print("%s has won the game!", Players[winner]->name);
	}

	/* Give bonus to the best player */
	Score_give_bonus(average_score, num_best_players, best_ratio,
					 best_players);

	/* Give bonus to the winning player */
	if (winner >= 0)
	{
		for (i = 0; i < num_best_players; i++)
		{
			if (Players[winner] == best_players[i])
			{
				break;
			}
		}
		if (i == num_best_players)
		{
			Score_give_individual_bonus(Players[winner], average_score);
		}
	}
	else if (winner == -2)
	{
		for (j = 0; j < NumPlayers; j++)
		{
			pl = Players[j];

			if (Player_is_robot(pl))
			{
				for (i = 0; i < num_best_players; i++)
				{
					if (pl == best_players[i])
					{
						break;
					}
				}
				if (i == num_best_players)
				{
					Score_give_individual_bonus(pl, average_score);
				}
			}
		}
	}

	Players_reset();

	free(best_players);
}

/** @brief Removes a player from game
 *
 * Modified global variables: NumPlayers, NumOperators, NumRobots
 * NOTE: this function must not be used directly to kick human players from game,
 * instead the connection should be shut down (with \ref Destroy_connection)
 *
 * @param pl	pointer to the player's structure
 */
void Player_remove(player_t *pl)
{
	player_t *pl2;
	object_t *obj;
	int32_t i, j, ind2;

	if (Player_is_robot(pl))
	{
		Robot_destroy(pl);
	}

	if (pl->isoperator)
	{
		if (!--NumOperators && game_lock)
		{
			game_lock = false;
			Message_game_important_print("The game has been unlocked as the last operator left!");
		}
	}

	/* Won't be swapping anywhere */
	for (i = MAX_TEAMS - 1; i >= 0; i--)
	{
		if (World.teams[i].Swapper == pl)
		{
			World.teams[i].Swapper = NULL;
		}
	}

	/* Delete remaining shots */
	for (i = NumObjs - 1; i >= 0; i--)
	{
		obj = Obj[i];

		if (obj->owner == pl)
		{
			/* Attached balls should be destroyed,
			 * ownership of floating balls should be cleared */
			if (Object_is_type(obj, OBJ_BALL))
			{
				if (Object_is_attached(obj))
				{
					Ball_detach(obj->owner, obj);
					Object_expire(obj);
				}
			}
			else if (Object_is_type(obj, OBJ_SHOT))
			{
				if (!keepShots)
				{
					Object_expire(obj);
				}
			}

			obj->owner = NULL;
		}
	}

	Free_ship_shape(pl->ship);

	if (Player_is_paused(pl))
	{
		NumPaused--;
	}

	Player_set_state(pl, PL_STATE_UNDEFINED);

	NumPlayers--;

	if (Player_is_robot(pl))
	{
		NumRobots--;
	}

	/* player left, save the rank now */
	if (pl->rank)
	{
		Rank_save_score(pl);
	}

	Player_remove_from_team(pl);

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

	for (i = NumPlayers - 1; i >= 0; i--)
	{
		pl2 = Players[i];

		if (BIT(pl2->lock.flags, LOCK_PLAYER | LOCK_VISIBLE) && (pl2->lock.object == pl || NumPlayers <= 1))
		{
			CLR_BIT(pl2->lock.flags, LOCK_PLAYER | LOCK_VISIBLE);
		}
		if (Player_is_robot(pl2) && Robot_war_on_player(pl2) == pl)
		{
			Robot_reset_war(pl2);
		}
		for (j = 0; j < LOCKBANK_MAX; j++)
		{
			if (pl2->lockbank[j] == pl)
			{
				pl2->lockbank[j] = NULL;
			}
		}
		for (j = 0; j < MAX_RECORDED_SHOVES; j++)
		{
			if (pl2->shove_record[j].pusher_pl == pl)
			{
				pl2->shove_record[j].pusher_pl = NULL;
			}
		}
	}

	for (i = NumPlayers - 1; i >= 0; i--)
	{
		if (Player_is_connected(Players[i]))
		{
			Send_leave(Players[i]->connp, pl->id);
		}
	}

	release_ID(pl->id);

	memset(pl, 0, sizeof(player_t));

	updateScores = true;
}

/** \brief Kills a player
 *
 * After execution of this function a player will either be
 * PL_STATE_DEAD or PL_STATE_APPEARING.
 *
 * \param pl	player
 */
void Player_destroy(player_t *pl)
{
	Player_explode(pl);
	Player_reset(pl);
	Rank_add_death(pl);

	if (BIT(World.rules->mode, LIMITED_LIVES))
	{
		if (pl->pl_life == 0)
		{
			Player_set_state(pl, PL_STATE_DEAD);
			//			Player_lock_clear(pl);
		}
		else
		{
			Player_set_life(pl, pl->pl_life - 1);
			Player_set_state(pl, PL_STATE_APPEARING);
		}
	}
	else
	{
		Player_set_life(pl, pl->pl_life + 1);
		Player_set_state(pl, PL_STATE_APPEARING);
	}

	updateScores = true;
}

/*
 * Reinitialize parameters of a player after a kill
 */
void Player_reset(player_t *pl)
{
	int32_t i;

	Ball_detach(pl, NULL);

	pl->vel.x = pl->vel.y = 0.0;
	pl->acc.x = pl->acc.y = 0.0;
	Player_interpolation_reset(pl);

	for (i = 0; i < NUM_ITEMS; i++)
	{
		if (!BIT(1U << i, ITEM_BIT_FUEL | ITEM_BIT_TANK))
		{
			pl->item[i] = World.items[i].initial;
		}
	}

	Player_fuel_init(pl, World.items[ITEM_FUEL].initial << FUEL_SCALE_BITS);
}

player_t *Player_owns_base(base_t *base)
{
	int i;

	for (i = 0; i < NumPlayers; i++)
	{
		if (Players[i]->home_base == base)
		{
			return Players[i];
		}
	}

	return NULL;
}

/*
 * Kick paused players?
 * Return the number of kicked players.
 */
int32_t Players_kick_paused(void)
{
	player_t *pl;
	int32_t i;
	int32_t num_unpaused = 0;

	char tmp_pl_name[MAX_CHARS];

	for (i = NumPlayers - 1; i >= 0; i--)
	{
		pl = Players[i];

		if (Player_is_connected(pl) && Player_is_paused(pl))
		{
			strncpy(tmp_pl_name, pl->name, MAX_CHARS);
			Destroy_connection(pl->connp, "no pause with full game");
			Message_game_print("The paused \"%s\" was kicked because the game is full.", tmp_pl_name);
			num_unpaused++;
		}
	}

	return num_unpaused;
}

/*
 * Pause a player without checking whether he's allowed to pause.
 * If the player is already paused, just refresh the unpause delay.
 */
void Player_pause_forced(player_t *pl)
{
	pl->pause_count = UNPAUSE_DELAY_TICKS;

	if (Player_is_paused(pl))
	{
		return;
	}

	if (NumPaused >= MAX_PAUSED_PLAYERS)
	{
		/* KABOOM!! */
		Players_kick_paused();
	}

	Player_swap_team(pl, Team_get_by_id(PAUSE_TEAM_NUM));

	if (Player_has_property(pl, HAS_BALL))
	{
		Ball_detach(pl, NULL);
	}

	pl->kills = 0;
	pl->deaths = 0;
	pl->self_deaths = 0;
	Player_set_life(pl, World.rules->lives);

	NumPaused++;
	num_pause++;
}

/*
 * Pauses a player on his request
 */
void Player_pause_self(player_t *pl)
{
	/*
	 * Do not allow to pause when a player is too far from his home base. The exceptions are when
	 * the player is waiting or dead.
	 */
	if (Player_is_close_to_base(pl) || Player_is_waiting(pl) || Player_is_dead(pl))
	{
		/*
		 * Allow to pause if:
		 * - the player is waiting to join the game, or
		 * - all treasures in the player's current team are safe, or
		 * - the team the player is on is currently the only populated team.
		 */
		if (Player_is_waiting(pl) || (Object_count_treasures_missing(pl->team) == 0) || (Team_count_active_players(pl->team) == Team_count_active_players(NULL)))
		{
			Player_pause_forced(pl);
		}
		else
		{
			Message_player_print(pl, PL_MSG_NOTICE, "Your team's treasure(s) have to be safe before you can pause.");
		}
	}
	else
	{
		Message_player_print(pl, PL_MSG_NOTICE, "You have to be close to your home base in order to pause.");
	}
}

/*
 * Unpauses a player
 */
void Player_unpause(player_t *pl, team_t *team)
{
	team_t *tmp_team;

	if (!Player_is_paused(pl))
	{
		return;
	}

	if (pl->pause_count <= 0.0)
	{
		if (team)
		{
			tmp_team = team;
		}
		else
		{
			tmp_team = Team_find_available(PL_TYPE_HUMAN);
		}

		if (tmp_team)
		{
			Player_swap_team(pl, tmp_team);
			NumPaused--;
			num_unpause++;
		}
		else
		{
			Message_player_print(pl, PL_MSG_NOTICE, "Couldn't find a suitable team.");
		}
	}
	else
	{
		Message_player_print(pl, PL_MSG_NOTICE, "You still have to wait %.1f seconds before unpausing.",
							 pl->pause_count / gameSpeed);
	}
}

/*
 * Swaps a player to a team (including to/from the pausers' team)
 *
 * The function takes care of the following:
 * - detaching all balls which the player is carrying,
 * - choosing and setting the player's entry state on the destination team,
 * - picking a free base,
 * - transporting the ship home,
 * - sending updates of the player information.
 */
void Player_swap_team(player_t *pl, team_t *team)
{
	int32_t i;
	player_state_t entry_state;

	ASSERT(pl && team)

	Ball_detach(pl, NULL);

	if (team == Team_get_by_id(PAUSE_TEAM_NUM))
	{
		for (i = 0; i < NumPlayers; i++)
		{
			if (Player_is_connected(Players[i]))
			{
				Send_release_base(Players[i]->connp, pl);
			}
		}

		pl->home_base = NULL;

		if (!Player_lock_is_initialized(pl))
		{
			Player_lock_closest(pl, false);
		}
	}
	else
	{
		base_t *tmp_base;

		tmp_base = Team_pick_free_base(team);

		if (!tmp_base)
		{
			Message_game_print("Couldn't find a suitable base for player %s on team %d. Removing from game.",
							   pl->name, pl->team->Num);

			if (Player_is_human(pl))
			{
				Destroy_connection(pl->connp, "couldn't find a suitable base");
			}
			else if (Player_is_robot(pl))
			{
				Robot_kick(pl);
			}
			else
			{
				/* unknown player type */
				ASSERT(1)
			}

			return;
		}

		pl->home_base = tmp_base;

		/* Go home only if unpausing */
		if (pl->team == Team_get_by_id(PAUSE_TEAM_NUM))
		{
			Player_go_home(pl);
		}
	}

	/* Can swap the player now */
	Player_remove_from_team(pl);
	Player_add_to_team(pl, team);

	entry_state = Player_compute_entry_state(pl, team);
	Player_set_state(pl, entry_state);

	Send_info_about_myself(pl, PL_SEND_GENERAL | PL_SEND_BASE);
	Send_info_about_player(pl, PL_SEND_GENERAL | PL_SEND_BASE);

	updateScores = true;
}

/*
 * Swaps two players who wanted to change places with each other
 *
 * The function involves:
 * - detaching of balls carried by the players,
 * - computation of the entry state on the destination teams of both players,
 * - sending updates of players' information.
 */
void Players_swap_teams(player_t *pl1, player_t *pl2)
{
	player_state_t entry_state;
	base_t *tmp_base = pl2->home_base;
	team_t *tmp_team = pl2->team;

	pl2->team = pl1->team;
	pl1->team = tmp_team;

	pl2->home_base = pl1->home_base;
	pl1->home_base = tmp_base;

	Ball_detach(pl1, NULL);
	Ball_detach(pl2, NULL);

	entry_state = Player_compute_entry_state(pl1, pl1->team);
	Player_set_state(pl1, entry_state);

	entry_state = Player_compute_entry_state(pl2, pl2->team);
	Player_set_state(pl2, entry_state);

	Send_info_about_player(pl1, PL_SEND_GENERAL | PL_SEND_BASE);
	Send_info_about_player(pl2, PL_SEND_GENERAL | PL_SEND_BASE);
}

/*
 * Sets the player's state
 */
void Player_set_state(player_t *pl, int state)
{
	pl->pl_state = state;

	/*
	 * For all statuses except PL_STATE_UNDEFINED we want the scoreboard to be updated
	 * whenever updateScores is set to true.
	 */
	pl->update_scoreboard = true;

	switch (state)
	{
	case PL_STATE_WAITING:
		Player_set_mychar(pl, 'W');
		pl->pl_old_status = OLD_GAME_OVER;
		break;
	case PL_STATE_APPEARING:
		Player_set_mychar(pl, pl->pl_type_mychar);
		pl->recovery_count = RECOVERY_DELAY_TICKS;
		break;
	case PL_STATE_ALIVE:
		Player_set_mychar(pl, pl->pl_type_mychar);
		pl->pl_old_status = OLD_PLAYING;
		break;
	case PL_STATE_KILLED:
		pl->pl_old_status = OLD_KILLED;
		break;
	case PL_STATE_DEAD:
		Player_set_mychar(pl, 'D');
		pl->pl_old_status = OLD_GAME_OVER;
		pl->recovery_count = RECOVERY_DELAY_TICKS;
		break;
	case PL_STATE_PAUSED:
		Player_set_mychar(pl, 'P');
		pl->recovery_count = 0;
		pl->pl_old_status = OLD_PAUSE;
		break;
	case PL_STATE_UNDEFINED:
		pl->update_scoreboard = false;
		break;
	default:
		/* illegal state */
		ASSERT(1)
		break;
	}
}

/** \brief Sets the initial state of player joining the specified team
 *
 * Joining a team is a general term that includes starting a new game in the team,
 * as well as swapping to that team.
 *
 * \param pl	player
 * \param team	team the player is joining
 */
player_state_t Player_compute_entry_state(player_t *pl, team_t *team)
{
	player_state_t new_state;
	int32_t num_active;
	int32_t num_active_on_team; /* number of active players on the destination team (including the player pl) */

	ASSERT(pl && team)

	/* Player can only be in the pausing state while in the pausers' team */
	if (team->Num == PAUSE_TEAM_NUM)
	{
		return PL_STATE_PAUSED;
	}

	/* Dead players should be marked as waiting, never be allowed to play anew without a round reset */
	if (Player_is_dead(pl))
	{
		return PL_STATE_WAITING;
	}

	/* Do not change status of a dead player, he has to wait till the end of round */
	if (Player_is_waiting(pl))
	{
		return PL_STATE_WAITING;
	}

	/*
	 * Let the player in immediately in practise mode (by default: maps with robots vs humans)
	 * or in a roundless game
	 */
	if (!BIT(World.rules->mode, LIMITED_LIVES) || BIT(World.rules->mode, PRACTISE_PLAY))
	{
		if (Player_is_appearing(pl))
		{
			/*
			 * Maintain the 'appearing' state in order to avoid a nasty crash if
			 * the player happens to be traveling back to base.
			 */
			return PL_STATE_APPEARING;
		}
		else
		{
			return PL_STATE_ALIVE;
		}
	}

	/*
	 * Here the player state can be one of the following:
	 * - when entering the game: PL_STATE_UNDEFINED
	 * - when unpausing: PL_STATE_PAUSED
	 * - when swapping: PL_STATE_APPEARING, PL_STATE_ALIVE
	 */
	num_active = Team_count_active_players(NULL);
	num_active_on_team = Team_count_active_players(team);

	/* determine if the player pl is included among active players; if not, he is soon going to be */
	if (!Player_is_active(pl))
	{
		// player is in state PL_STATE_UNDEFINES (just joined the server) or PL_STATE_PAUSED (just unpausing)
		num_active++;
	}

	if (num_active > 1)
	{
		// there are no active players left in other teams, no reason to keep the player waiting
		if (num_active_on_team + 1 == num_active)
		{
			new_state = PL_STATE_ALIVE;
		}

		/*
		 * Determine the player's state based on how far the game proceeded from its "clean" state.
		 * Count missing treasures, as well as lives lost during the round.
		 */
		else if ((Object_count_treasures_missing(NULL) > 0) || (Round_count_killed_players(NULL) > 0))
		{
			new_state = PL_STATE_WAITING;
		}
		else
		{
			new_state = PL_STATE_ALIVE;
		}
	}
	else
	{ // <= 1
		new_state = PL_STATE_ALIVE;
	}

	if (Player_is_appearing(pl) && new_state == PL_STATE_ALIVE)
	{
		/*
		 * Maintain the 'appearing' state in order to avoid a nasty crash if
		 * the player happens to be traveling fast back to base.
		 */
		new_state = PL_STATE_APPEARING;
	}

	return new_state;
}

/*
 * Give player the initial number of tanks and amount of fuel.
 * Upto the maximum allowed.
 */
void Player_fuel_init(player_t *pl, int32_t fuel)
{
	pl->fuel.max = TANK_CAP(0);
	pl->fuel.sum = MIN(fuel, pl->fuel.max);
	pl->mass = pl->emptymass + FUEL_MASS(pl->fuel.sum);
}

/* rewrote this (was a real ugly hack) -pgm */
void Player_refuel_start(player_t *pl)
{
	int32_t i;
	objposition_t *pos;
	int32_t l;
	int32_t l_min = INT_MAX;
	fuel_t *fuel_min = NULL;

	if (Frame_is_real())
	{
		pos = &pl->pos;
	}
	else
	{
		pos = &pl->pos_interp;
	}

	for (i = 0; i < World.NumFuels; i++)
	{
		l = Map_get_distance(&World.fuel[i].pos, pos);
		if (l < MAX_FUEL_DISTANCE && l < l_min)
		{
			l_min = l;
			fuel_min = &World.fuel[i];
		}
	}

	/* connect to the closest fuel station */
	if (l_min < INT_MAX)
	{
		pl->fs = fuel_min;
		Player_enable_property(pl, USES_REFUEL);
	}
}

/*
 * Add fuel to fighter's tanks.
 * Maybe use more than one of tank to store the fuel.
 */
void Player_fuel_add(player_t *pl, int32_t fuel)
{
	pl_fuel_t *ft = &pl->fuel;

	ASSERT(ft)

	if (ft->sum + fuel > ft->max)
	{
		fuel = ft->max - ft->sum;
	}
	else if (ft->sum + fuel < 0)
	{
		fuel = -ft->sum;
	}
	ft->sum += fuel;
	//	ft->tank[ft->current] += fuel;
}

bool Player_refuel_advance(player_t *pl)
{
	if (!pl->fs)
	{
		return false;
	}

	if (pl->fs->fuel > REFUEL_RATE)
	{
		Fuelstation_add_fuel(pl->fs, -REFUEL_RATE);
		Player_fuel_add(pl, REFUEL_RATE);
		return true;
	}
	else
	{
		Fuelstation_add_fuel(pl->fs, -pl->fs->fuel);
		Player_fuel_add(pl, pl->fs->fuel);
		return false;
	}
}

void Player_refuel_stop(player_t *pl)
{
	Player_disable_property(pl, USES_REFUEL);
	pl->fs = NULL;
}

/*
 * Return true if a lock is allowed.
 */
bool Player_lock_is_allowed(player_t *pl, player_t *pl_lock)
{
	/* we can never lock on ourselves, nor on an unspecified player. */
	if (pl == pl_lock || pl_lock == NULL)
	{
		return false;
	}

	/* if we are actively playing then we can lock since we are not viewing. */
	if (Player_is_alive(pl))
	{
		return true;
	}

	/* if there is no team play then we can always lock on anyone. */
	if (!BIT(World.rules->mode, TEAM_PLAY))
	{
		return true;
	}

	/* we can always lock on players from our own team. */
	if (TEAM(pl, pl_lock))
	{
		return true;
	}

	/* if lockOtherTeam is true then we can always lock on other teams. */
	if (lockOtherTeam)
	{
		return true;
	}

	/* if our own team is dead then we can lock on anyone. */
	if (Team_is_dead(pl->team))
	{
		return true;
	}

	/* can't find any reason why this lock should be allowed. */
	return false;
}

/*
 * Sven Mascheck:
 * If all _opponents are paused, then even LOCK_NEXT (ot LOCK_PREV)
 * might not lock_next (or lock_prev), as Player_lock_closest() might
 * be called  [ "event.c" line 466 ] :
 * This happens when the player is not locked on any one anymore -
 * and this happens if he tried to lock_closest before (if all
 * opponents are paused).
 * Player_lock_closest() is called with (ind, 0) and that means that
 * the lock is cleared in _any case_ with the current code - that could
 * be done without calling Player_lock_closest().
 * (btw, code in Player_lock_closest() looks like 'evolutionary code :)
 * I am not sure where to fix that locking problem
 * ( in "case KEY_LOCK_NEXT" or in Player_lock_closest() ).
 * I tried to find a solution but now i am bit screwed up..  :)
 *
 * \param next	false - choose the active player closest to us
 * 		true - choose the NEXT closest player
 */
bool Player_lock_closest(player_t *pl, bool next)
{
	player_t *pl_lock;
	player_t *pl_i, *pl_new;
	int32_t i;
	DFLOAT dist, best, l;

	if (!next)
	{
		CLR_BIT(pl->lock.flags, LOCK_PLAYER);
	}

	if (BIT(pl->lock.flags, LOCK_PLAYER))
	{
		pl_lock = pl->lock.object;
		dist = Map_get_distance(&pl->pos, &pl_lock->pos);
	}
	else
	{
		pl_lock = NULL;
		dist = 0.0;
	}

	pl_new = NULL;
	best = FLT_MAX;

	for (i = 0; i < NumPlayers; i++)
	{
		pl_i = Players[i];

		if (pl_i == pl_lock || !Player_is_alive(pl_i) || !Player_lock_is_allowed(pl, pl_i))
		{
			continue;
		}
		l = Map_get_distance(&pl->pos, &pl_i->pos);
		if (l >= dist && l < best)
		{
			best = l;
			pl_new = pl_i;
		}
	}
	if (pl_new == NULL)
	{
		return false;
	}

	SET_BIT(pl->lock.flags, LOCK_PLAYER);
	pl->lock.object = pl_new;

	return true;
}

void Player_lock_next(player_t *pl, bool forwards)
{
	player_t *pl_tmp1, *pl_tmp2;

	pl_tmp1 = pl_tmp2 = pl->lock.object;
	if (!BIT(pl->lock.flags, LOCK_PLAYER) || !pl_tmp2)
	{
		/* better jump to KEY_LOCK_CLOSE... */
		Player_lock_closest(pl, false);
		return;
	}
	do
	{
		int tmp_ind = GetInd[pl_tmp1->id];

		if (!forwards)
		{
			if (tmp_ind == 0)
			{
				pl_tmp1 = Players[NumPlayers - 1];
			}
			else
			{
				pl_tmp1 = Players[tmp_ind - 1];
			}
		}
		else
		{
			if (tmp_ind == NumPlayers - 1)
			{
				pl_tmp1 = Players[0];
			}
			else
			{
				pl_tmp1 = Players[tmp_ind + 1];
			}
		}
		if (pl_tmp1 == pl_tmp2)
		{
			break;
		}
	} while (pl_tmp1 == pl || Player_is_waiting(pl_tmp1) || Player_is_dead(pl_tmp1) || Player_is_paused(pl_tmp1) || !Player_lock_is_allowed(pl, pl_tmp1));
	if (pl_tmp1 == pl)
	{
		CLR_BIT(pl->lock.flags, LOCK_PLAYER);
	}
	else
	{
		pl->lock.object = pl_tmp1;
		SET_BIT(pl->lock.flags, LOCK_PLAYER);
	}
}

/*
 * Verify if the lock has ever been initialized at all
 * and if the lock is still valid.
 */
bool Player_lock_is_initialized(player_t *pl)
{
	player_t *pl2;

	if (BIT(pl->lock.flags, LOCK_PLAYER) && NumPlayers > 1 && (pl2 = pl->lock.object) && pl2 != pl)
	{
		return true;
	}

	return false;
}

void Player_lock_set(player_t *pl, player_t *pl_target)
{
	SET_BIT(pl->lock.flags, LOCK_PLAYER);
	pl->lock.object = pl_target;
}

void Player_lock_clear(player_t *pl)
{
	CLR_BIT(pl->lock.flags, LOCK_PLAYER);
	pl->lock.object = NULL;
}

void Player_change_home(player_t *pl, base_t *base)
{
	player_t *base_owner;

	ASSERT(pl && base)

	if (base == pl->home_base)
	{
		return;
	}
	if (base->team != NULL && base->team != pl->team)
	{
		return;
	}

	base_owner = Player_owns_base(base);

	if (base_owner && base_owner != pl)
	{
		base_owner->home_base = pl->home_base;

		Send_info_about_player(base_owner, PL_SEND_BASE);
		Send_info_about_myself(base_owner, PL_SEND_BASE);

		Message_game_print("%s has taken over %s's home base.", pl->name, base_owner->name);
	}
	else
	{
		Message_game_print("%s has changed home base.", pl->name);
	}

	pl->home_base = base;

	Send_info_about_player(pl, PL_SEND_BASE);
	Send_info_about_myself(pl, PL_SEND_BASE);
}

bool Player_is_close_to_base(player_t *pl)
{
	int32_t j, k, xi, yi;
	DFLOAT minv;

	xi = OBJ_X_IN_BLOCKS(pl);
	yi = OBJ_Y_IN_BLOCKS(pl);
	j = pl->home_base->pos.bx;
	k = pl->home_base->pos.by;

	if (j == xi && k == yi)
	{
		minv = 3.0f;
	}
	else
	{
		/*
		 * Hover pause doesn't work within two squares of the
		 * players home base, they would want the better pause.
		 */
		if (ABS(j - xi) <= 2 && ABS(k - yi) <= 2)
		{
			minv = 5.0f;
		}
		else
		{
			return false;
		}
	}

	if (pl->velocity > minv)
	{
		return false;
	}

	return true;
}

/*
 * Autorepeat fire, must unfortunately be done here, not in
 * the player loop below, because of collisions between the shots
 * and the auto-firing player that would otherwise occur.
 */
void Players_shoot(void)
{
	int32_t i;
	player_t *pl;

	for (i = 0; i < NumPlayers; i++)
	{
		pl = Players[i];

		if (Player_is_alive(pl) && Player_uses_property(pl, USES_SHOT))
		{
			Player_disable_property(pl, USES_SHIELD);
			Shot_add(pl);
		}
	}
}

/*
 * Update tanks, kill players that ought to be killed.
 */
void Players_handle_after_kill(void)
{
	int32_t i;
	player_t *pl;

	for (i = NumPlayers - 1; i >= 0; i--)
	{
		pl = Players[i];

		if (Player_is_killed(pl))
		{
			Player_destroy(pl);

			if (Player_is_human(pl) && Player_idle_timed_out(pl))
			{
				Player_pause_forced(pl);
				Message_game_print("%s was paused for idling.", pl->name);
			}

			updateScores = true;
		}
	}
}

void Players_remove(void)
{
	player_t *pl;

	while (NumPlayers > 0)
	{ /* Kick out all remaining players */
		pl = Players[NumPlayers - 1];
		if (!Player_is_connected(pl))
		{
			Player_remove(pl);
		}
		else
		{
			Destroy_connection(pl->connp, "server exiting");
		}
	}
}

/** \brief Returns the structure of a ball the player is carrying (order or picking is not respected)
 *
 * \param pl	player
 * \return pointer to the ball structure, NULL if the player has no ball
 */
object_t *Player_get_ball_object(player_t *pl)
{
	int32_t i;
	object_t *obj;

	for (i = 0; i < NumObjs; i++)
	{
		obj = Obj[i];

		if (Object_is_type(obj, OBJ_BALL) && Object_is_attached(obj) && obj->owner == pl)
		{
			return obj;
		}
	}

	return NULL;
}

bool Player_idle_timed_out(player_t *pl)
{
	return (frame_loops - pl->frame_last_busy > MAX_PLAYER_IDLE_TICKS && (NumPlayers > 1)) ? true : false;
}

bool Player_is_recovered(player_t *pl)
{
	return (pl->recovery_count <= 0.0) ? true : false;
}

void Players_interpolation_init(void)
{
	int32_t i;
	player_t *pl;

	for (i = 0; i < NumPlayers; i++)
	{
		pl = Players[i];
		Position_copy(&pl->pos_interp, &pl->pos);
		pl->vel_interp.x = pl->vel.x;
		pl->vel_interp.y = pl->vel.y;
	}
}

void Player_interpolation_reset(player_t *pl)
{
	pl->acc_interp.x = 0.0;
	pl->acc_interp.y = 0.0;
	pl->vel_interp.x = 0.0;
	pl->vel_interp.y = 0.0;
}
