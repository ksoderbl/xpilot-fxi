/*
 * team.h
 *
 *  Created on: Nov 9, 2009
 *      Author: rotunda
 */

#ifndef TEAM_H_
#define TEAM_H_

#include <stdint.h>
#include "types.h"

#include "structs.h"
#include "player.h"

struct _team {
	int32_t Num; /* Number of team */

	int32_t NumMembers; /* Number of current members */
	int32_t NumRobots; /* Number of robot players */

	int32_t NumBases; /* Number of bases owned */
	int32_t NumTreasures; /* Number of treasures owned */
	int32_t TreasuresDestroyed; /* Number of destroyed treasures */
	int32_t TreasuresLeft; /* Number of treasures left */
	player_t *Swapper; /* Player swapping to this full team */
	int32_t Properties; /* Properties of the team */
};

int32_t Team_count_players(team_t *team, player_state_t state);
team_t *Team_get_by_id(int32_t team_id);
bool Team_is_dead(team_t *team);
int32_t Team_count_active_players(team_t *team);

void Teams_reset(void);
void Team_game_over(team_t *winning_team, const char *reason);
base_t *Team_pick_free_base(team_t *team);
team_t *Team_find_available(player_type_t pl_type);

void Rounds_count(void);

#endif /* TEAM_H_ */
