/*
 * team.c
 *
 *  Created on: Nov 9, 2009
 *      Author: rotunda
 */

#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "commonproto.h"

#include "team.h"

#include "player.h"
#include "map.h"
#include "player_inline.h"

#include "server.h"
#include "frame.h"
#include "rank.h"

int32_t Team_count_players(team_t *team, player_state_t state)
{
	int32_t i;
	int32_t count;
	player_t *pl;

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if (pl->team == team && Player_is_state(pl, state)) {
			count++;
		}
	}

	return count;
}

team_t *Team_get_by_id(int32_t team_id)
{
	if (team_id >= 0 && team_id < MAX_TEAMS) {
		return &World.teams[team_id];
	}

	return NULL;
}

void Teams_reset(void)
{
	int32_t i;

	/* Reset the teams */
	for (i = 0; i < MAX_TEAMS; i++) {
		World.teams[i].TreasuresDestroyed = 0;
		World.teams[i].TreasuresLeft = World.teams[i].NumTreasures;
	}
}

void Team_game_over(team_t *winning_team, const char *reason)
{
	int32_t i, j;
	int32_t average_score;
	int32_t num_best_players;
	player_t **best_players;
	DFLOAT best_ratio;

	if (!(best_players = (player_t **) malloc(NumPlayers * sizeof(player_t *)))) {
		error("no mem");
		End_game();
	}

	/* Figure out the average score and who has the best kill/death ratio */
	/* ratio for this round */
	Score_compute_end_of_round(&average_score, &num_best_players, &best_ratio, best_players);

	/* Print out the results of the round */
	if (winning_team) {
		Message_game_important_print("Team %d has won the game%s!", winning_team->Num, reason);
	}
	else {
		Message_game_important_print("We have a draw%s!", reason);
	}

	/* Give bonus to the best player */
	Score_give_bonus(average_score, num_best_players, best_ratio, best_players);

	/* Give bonuses to the winning team */
	if (winning_team) {
		player_t *pl;

		for (i = 0; i < NumPlayers; i++) {
			pl = Players[i];

			if (pl->team != winning_team) {
				continue;
			}
			if (!(Player_is_alive(pl) || Player_is_appearing(pl) || Player_is_dead(pl))) {
				continue;
			}
			for (j = 0; j < num_best_players; j++) {
				if (pl == best_players[j]) {
					break;
				}
			}
			if (j == num_best_players) {
				Score_give_individual_bonus(pl, average_score);
			}
		}
	}

	Players_reset();
	Objects_time_out();
	Teams_reset();

	Rounds_count();

	free(best_players);

	Rank_write_webpage();
	Rank_write_rankfile();
}

/*
 * Return a good team number for a player.
 *
 * If the team is not specified, the player is assigned
 * to a non-empty team which has space.
 *
 * If there is none or only one team with playing (i.e. non-paused)
 * players the player will be assigned to a randomly chosen empty team.
 *
 * If there is more than one team with playing players,
 * the player will be assigned randomly to a team which
 * has the least number of playing players.
 *
 * If all non-empty teams are full, the player is assigned
 * to a randomly chosen available team.
 *
 * Prefer not to place players in the robotTeam if possible.
 */
team_t *Team_find_available(player_type_t pl_type)
{
	int32_t i, least_players, num_available_teams = 0, playing_teams = 0,
			losing_team;
	player_t *pl;
	int32_t playing[MAX_TEAMS];
	int32_t free_bases[MAX_TEAMS];
	int32_t available_teams[MAX_TEAMS];
	int32_t team_score[MAX_TEAMS];
	int32_t losing_score;


	/* robots only */
	if (pl_type == PL_TYPE_ROBOT) {
		team_t *team;

		if (Team_num_is_valid(robotTeam)) {
			team = &World.teams[robotTeam];
			if (team->NumMembers < team->NumBases) {
				return team;
			}
		}

		return NULL;
	}

	/* humans only */
	if (pl_type != PL_TYPE_HUMAN) {
		return NULL;
	}

	for (i = 0; i < MAX_TEAMS; i++) {
		if (World.teams[i].Properties == TEAM_ONLY_PAUSERS) {
			free_bases[i] = 0;
		}
		else if (World.teams[i].Properties == TEAM_ONLY_ROBOTS && restrictRobots) {
			free_bases[i] = 0;
		}
		else {
			free_bases[i] = World.teams[i].NumBases - World.teams[i].NumMembers;
		}

		playing[i] = 0;
		team_score[i] = 0;
		available_teams[i] = 0;
	}

	/*
	 * Find out which teams have actively playing members.
	 * And calculate the score for each team.
	 */
	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

                if (!(Player_is_alive(pl) || Player_is_appearing(pl) ||
                            Player_is_dead(pl) || Player_is_waiting(pl))) {
                    continue;
                }

		if (!playing[pl->team->Num]++) {
			playing_teams++;
		}
		if (Player_is_human(pl) || Player_is_robot(pl)) {
			team_score[pl->team->Num] += pl->score;
		}
	}
	if (playing_teams <= 1) {
		for (i = 0; i < MAX_TEAMS; i++) {
			if (!playing[i] && free_bases[i] > 0) {
				available_teams[num_available_teams++] = i;
			}
		}
	}
	else {
		least_players = NumPlayers;
		for (i = 0; i < MAX_TEAMS; i++) {
			/* We fill teams with players first. */
			if (playing[i] > 0 && free_bases[i] > 0) {
				if (playing[i] < least_players) {
					least_players = playing[i];
				}
			}
		}

		for (i = 0; i < MAX_TEAMS; i++) {
			if (free_bases[i] > 0) {
				if (least_players == NumPlayers || playing[i] == least_players) {
					available_teams[num_available_teams++] = i;
				}
			}
		}
	}

	if (!num_available_teams) {
		for (i = 0; i < MAX_TEAMS; i++) {
			if (free_bases[i] > 0) {
				available_teams[num_available_teams++] = i;
			}
		}
	}

	if (num_available_teams == 1) {
		return &World.teams[available_teams[0]];
	}

	if (num_available_teams > 1) {
		losing_team = -1;
		losing_score = INT_MAX;
		for (i = 0; i < num_available_teams; i++) {
			if (team_score[available_teams[i]] < losing_score && available_teams[i] != robotTeam) {
				losing_team = available_teams[i];
				losing_score = team_score[losing_team];
			}
		}
		return &World.teams[losing_team];
	}

	/*NOTREACHED*/
	return NULL;
}

#if 0
void Team_kill(team_t *team)
{
	int32_t i;
	bool is_death = false;
	player_t *pl;

	team->TreasuresLeft--;

	if (team->TreasuresLeft <= 0) {
		is_death = true;
	}

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if (pl->team == team) {
			// TODO: punish a team member for loss of treasure
			// ...

			if (is_death) {
				Player_set_state(pl, PL_STATE_DEAD);
			}
		}
	}
}
#endif

bool Team_is_dead(team_t *team)
{
	int32_t i;
	bool alive = false;

	for (i = 0; i < NumPlayers; i++) {
		if (Players[i]->team == team
				&& (Player_is_alive(Players[i]) || Player_is_appearing(Players[i]))) {
			alive = true;
			break;
		}
	}
	return (!alive);
}

/** \brief Counts active players on team
 *
 * A player is active if (s)he is alive or appearing or waiting or just killed or dead.
 *
 * \param team	team on which players should be counted, unspecified if NULL
 * \return	number of active players on specified team
 */
int32_t Team_count_active_players(team_t *team)
{
	int32_t i;
	player_t *pl;
	int32_t count = 0;

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if ((pl->team == team || team == NULL) && Player_is_active(pl)) {
			count++;
		}
	}

	return count;
}

void Rounds_count(void)
{
	if (!roundsToPlay) {
		return;
	}

	++roundsPlayed;

	Message_game_important_print("Round %d out of %d completed.", roundsPlayed, roundsToPlay);
	if (roundsPlayed >= roundsToPlay) {
		Game_Over();
	}
}

/**
 * NOTE: 2009-11-01 Changed functionality of this function; it doesn't consider the current
 * base, because it doesn't know any more which player it is ran for.
 */
base_t *Team_pick_free_base(team_t *team)
{
        int32_t i, num_free;
        int32_t pick = 0, seen = 0;
        static int32_t prev_num_bases = 0;
        static char *free_bases = NULL;

        if (prev_num_bases != World.NumBases) {
                prev_num_bases = World.NumBases;
                if (free_bases != NULL) {
                        free(free_bases);
                }
                free_bases = (char *) malloc(World.NumBases * sizeof(*free_bases));
                if (free_bases == NULL) {
                        error("Can't allocate memory for free_bases");
                        End_game();
                }
        }

        num_free = 0;
        for (i = 0; i < World.NumBases; i++) {
                if (World.base[i].team == team) {
                        num_free++;
                        free_bases[i] = 1;
                }
                else {
                        free_bases[i] = 0; /* other team */
                }
        }

        for (i = 0; i < NumPlayers; i++) {
                if (Players[i]->home_base && free_bases[Players[i]->home_base->id]) {
                        free_bases[Players[i]->home_base->id] = 0; /* occupado */
                        num_free--;
                }
        }

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

        if (i == World.NumBases) {
                return NULL;
        }

        return &World.base[i];
}
