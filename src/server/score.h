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

#ifndef SCORE_H
#define SCORE_H

#include "types.h"

#include "structs.h"
#include "walls.h"

#define ED_SHOT (-0.2 * FUEL_SCALE_FACT)
#define ED_SHIELD (-0.20 * FUEL_SCALE_FACT)
#define ED_SHOT_HIT (-25.0 * FUEL_SCALE_FACT)
#define ED_PL_CRASH (-100.0 * FUEL_SCALE_FACT)
#define ED_BALL_HIT (-50.0 * FUEL_SCALE_FACT)

#define PTS_PR_PL_SHOT -5 /* Points if you get shot by a player */
#define WALL_SCORE 2000

#define RATE_SIZE 20
#define RATE_RANGE 1024

#define crashScoreMult 0.33

double Get_Score(player_t *pl);
void Score_set(player_t *pl, double score);
void Score_add(player_t *pl, double score);

void SCORE(player_t *pl, int32_t points, objposition_t *pos, const char *msg);
int32_t Rate(int32_t winner, int32_t looser);
int32_t Punish_team(player_t *pl, treasure_t *td, treasure_t *tt);
void Score_players(player_t *winner, int32_t winner_score, char *winner_msg, player_t *loser, int32_t loser_score, char *loser_msg);
void Score_compute_end_of_round(int32_t *average_score, int32_t *num_best_players, DFLOAT *best_ratio, player_t **best_players);
void Score_give_bonus(int32_t average_score, int32_t num_best_players, DFLOAT best_ratio, player_t **best_players);
void Score_give_individual_bonus(player_t *pl, int32_t average_score);
void SCORE_LegacyBallCash(move_state_t *ms, double loose_time);

#endif
