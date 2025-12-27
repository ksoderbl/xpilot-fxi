/*
 * update.h
 *
 *  Created on: May 21, 2009
 *      Author: rotunda
 */

#ifndef UPDATE_H_
#define UPDATE_H_


typedef struct team_game_stats_t_ {
	enum TeamState {
		TeamEmpty, TeamDead, TeamAlive
	} team_state[MAX_TEAMS];

	int32_t num_dead_teams;
	int32_t num_alive_teams;

        int32_t num_alive_players;
        int32_t num_dead_players;
        int32_t num_waiting_players;

        /*
         * The team of the last processed alive player. This is really only useful if
         * there is only one alive team currently in game.
         */
	team_t *winning_team;
} team_game_stats_t;

void Update_player_turn(player_t *pl);
void Update_player_thrust(player_t *pl);
void Update_players(void);
void Update_robots(void);
void Update_fuel_stations(void);
void Update_mobile_objects(void);

void Update_game_status(void);
void Game_compute_team_stats(team_game_stats_t *tstats);


#endif /* UPDATE_H_ */
