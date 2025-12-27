/*
 * score.c
 *
 *  Created on: May 28, 2009
 *      Author: rotunda
 */

#include <stdio.h>
#include <string.h>
#include <error.h>

#include "score.h"
#include "rank.h"

#include "player.h"
#include "map.h"
#include "player_inline.h"

#include "netserver.h"
#include "proto.h"
#include "debug.h"
#include "frame.h"

char msg[MSG_LEN];

double Get_Score(player_t *pl)
{
	return pl->score;
}

void Score_set(player_t *pl, double score)
{
	pl->score = score;
}

void Score_add(player_t *pl, double score)
{
	pl->score += score;
}

void SCORE(player_t *pl, int32_t points, objposition_t *pos, const char *msg)
{
	Score_add(pl, points);

	Rank_add_score(pl, points);
	if (Player_is_connected(pl))
	{
		Send_score_object(pl->connp, points, pos, msg);
	}

	updateScores = true;
}

int32_t Rate(int32_t winner, int32_t loser)
{
	int32_t t;

	t = ((RATE_SIZE / 2) * RATE_RANGE) / (ABS(loser - winner) + RATE_RANGE);
	if (loser > winner)
		t = RATE_SIZE - t;
	return (t);
}

/*
 * Cause `winner' to get `winner_score' points added with message
 * `winner_msg', and similarly with the `loser' and equivalent
 * variables.
 *
 * In general the winner_score should be positive, and the loser_score
 * negative, but this need not be true.
 *
 * If the winner and loser players are on the same team, the scores are
 * made negative, since you shouldn't gain points by killing team members,
 * or being killed by a team member (it is both players faults).
 *
 * BD 28-4-98: Same for killing your own tank.
 */
void Score_players(player_t *winner, int32_t winner_score,
				   char *winner_msg, player_t *loser, int32_t loser_score,
				   char *loser_msg)
{
	if (TEAM(winner, loser))
	{
		if (winner_score > 0)
		{
			winner_score = -winner_score;
		}
		if (loser_score > 0)
		{
			loser_score = -loser_score;
		}
	}

	SCORE(winner, winner_score, &winner->pos, winner_msg);
	SCORE(loser, loser_score, &loser->pos, loser_msg);
}

void Score_compute_end_of_round(int32_t *average_score, int32_t *num_best_players, DFLOAT *best_ratio, player_t **best_players)
{
	int32_t i;
	DFLOAT ratio;
	player_t *pl;

	/* Initialize everything */
	*average_score = 0;
	*num_best_players = 0;
	*best_ratio = -1.0;

	/* Figure out what the average score is and who has the best kill/death */
	/* ratio for this round */
	for (i = 0; i < NumPlayers; i++)
	{
		pl = Players[i];

		if ((Player_is_paused(pl) && pl->pause_count <= 0.0))
		{
			continue;
		}

		if (!(Player_is_alive(pl) || Player_is_appearing(pl) || Player_is_dead(pl)))
		{
			continue;
		}

		*average_score += pl->score;
		ratio = (DFLOAT)pl->kills / (pl->deaths + 1);

		if (ratio > *best_ratio)
		{
			*best_ratio = ratio;
			best_players[0] = pl;
			*num_best_players = 1;
		}
		else if (ratio == *best_ratio)
		{
			best_players[(*num_best_players)++] = pl;
		}
	}

	*average_score /= NumPlayers;
}

void Score_give_bonus(int32_t average_score, int32_t num_best_players,
					  DFLOAT best_ratio, player_t **best_players)
{
	int32_t i;
	int32_t points;

	if (num_best_players < 1 || best_ratio <= 0.0f)
	{
		Message_game_print("There is no Deadly Player.");
	}
	else if (num_best_players == 1)
	{
		player_t *bp = best_players[0];

		Message_game_print("%s is the Deadliest Player with a kill ratio of %d/%d.",
						   bp->name, bp->kills, bp->deaths);
		points = (int32_t)(best_ratio * Rate(bp->score, average_score));
		SCORE(bp, points, &bp->pos, "[Deadliest]");
		Rank_add_deadliest(bp);
	}
	else
	{
		char msg[MSG_LEN] = "\0";

		for (i = 0; i < num_best_players; i++)
		{
			player_t *bp = best_players[i];
			int32_t ratio = Rate(bp->score, average_score);
			DFLOAT score = (DFLOAT)(ratio + num_best_players) / num_best_players;

			if (msg[0])
			{
				if (i == num_best_players - 1)
					strcat(msg, " and ");
				else
					strcat(msg, ", ");
			}
			if (strlen(msg) + 8 + strlen(bp->name) >= sizeof(msg))
			{
				Message_game_print(msg);
				msg[0] = '\0';
			}
			strcat(msg, bp->name);
			points = (int32_t)(best_ratio * score);
			SCORE(bp, points, &bp->pos, "[Deadly]");
			Rank_add_deadliest(bp);
		}
		if (strlen(msg) + 64 >= sizeof(msg))
		{
			Message_game_print(msg);
			msg[0] = '\0';
		}

		Message_game_print("%s are the Deadly Players with kill ratios of %d/%d.",
						   msg,
						   (best_players[0])->kills,
						   (best_players[0])->deaths);
	}
}

void Score_give_individual_bonus(player_t *pl, int32_t average_score)
{
	DFLOAT ratio;
	int32_t points;

	ratio = (DFLOAT)pl->kills / (pl->deaths + 1);
	points = (int32_t)(ratio * Rate(pl->score, average_score));
	SCORE(pl, points, &pl->pos, "[Winner]");
}

void SCORE_LegacyBallCash(move_state_t *ms, double loose_time)
{
	int32_t n;
	player_t *pl, *pl2;
	int32_t enemies = 0;
	const move_info_t *const mi = ms->mip; /* alias */

	pl = mi->obj->owner;

	/* compute amount of active enemies */
	for (n = 0; n < NumPlayers; n++)
	{
		pl2 = Players[n];

		if ((pl2->team == mi->obj->treasure->team) && (Player_is_alive(pl2) || Player_is_appearing(pl2) || Player_is_dead(pl2)))
		{
			Rank_lost_ball(pl2);
			enemies++;
		}
	}

	Rank_ballrun(pl, loose_time);

	if (enemies > 0)
	{
		Rank_cashed_ball(pl);

		for (n = 0; n < NumPlayers; n++)
		{
			pl2 = Players[n];

			if (pl2->team == pl->team && (Player_is_alive(pl2) || Player_is_appearing(pl2) || Player_is_dead(pl2)))
			{
				Rank_won_ball(pl2);
			}
		}

		Punish_team(pl, mi->obj->treasure, ms->treasure);
	}
}

int32_t Punish_team(player_t *pl, treasure_t *loser_treasure, treasure_t *winner_treasure)
{
	int32_t i;

	int32_t win_score = 0;
	int32_t lose_score = 0;
	int32_t win_team_members = 0;
	int32_t lose_team_members = 0;

	int32_t sc;
	int32_t por;
	player_t *pl2;

	/* Count scores */
	for (i = 0; i < NumPlayers; i++)
	{
		pl2 = Players[i];

		if (!(Player_is_alive(pl2) || Player_is_appearing(pl2) || Player_is_dead(pl2)))
		{
			continue;
		}

		if (pl2->team == loser_treasure->team)
		{
			lose_score += pl2->score;
			lose_team_members++;
		}
		else if (pl2->team == winner_treasure->team)
		{
			win_score += pl2->score;
			win_team_members++;
		}
	}

	Message_game_important_print("%s's (%d) team has destroyed team %d treasure",
								 pl->name, pl->team->Num, loser_treasure->team->Num);

	loser_treasure->destroyed++;
	loser_treasure->team->TreasuresLeft--;
	winner_treasure->team->TreasuresDestroyed++;

	sc = 3 * Rate(win_score, lose_score);
	por = (sc * lose_team_members) / (2 * win_team_members + 1);

	for (i = 0; i < NumPlayers; i++)
	{
		pl2 = Players[i];

		if (!(Player_is_alive(pl2) || Player_is_appearing(pl2) || Player_is_dead(pl2)))
		{
			continue;
		}

		if (pl2->team == loser_treasure->team)
		{
			SCORE(pl2, -sc, &winner_treasure->pos, "Treasure: ");
		}
		else if (pl2->team == winner_treasure->team)
		{
			SCORE(pl2, (pl2 == pl ? 3 * por : 2 * por), &winner_treasure->pos, "Treasure: ");
		}
	}

	return 1;
}
