/* $Id: proto.h,v 1.6 2007/10/21 12:45:07 kps Exp $
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

#ifndef	PROTO_H
#define	PROTO_H

#ifndef OBJECT_H
/* need player_t */
#include "object.h"
#endif

#ifndef LIST_H_INCLUDED
/* need list_t */
#include "list.h"
#endif

/*
 * Prototypes for math.c
 */
int ON(char *optval);
int OFF(char *optval);
int mod(int x, int y);
int f2i(DFLOAT f);
DFLOAT findDir(DFLOAT x, DFLOAT y);
void Make_table(void);

/*
 * Prototypes for collision.c
 */
void Free_cells(void);
void Alloc_cells(void);
void Check_collision(void);
void SCORE(int, int, int, int, const char *);

/*
 * Prototypes for id.c
 */
int peek_ID(void);
int request_ID(void);
void release_ID(int id);

/*
 * Prototypes for walls.c
 */
void Walls_init(void);
void Move_init(void);
void Move_object(int ind);
void Move_object_interpolation(int ind);
void Move_player(int ind);
void Move_player_interpolation(int ind);
void Turn_player(player_t *pl);
void Old_turn_player(int ind);

/*
 * Prototypes for event.c
 */
int Handle_keyboard(int);
void Pause_player(int ind, int onoff);
int Player_lock_closest(int ind, int next);
bool team_dead(int team);

/*
 * Prototypes for map.c
 */
void Print_map(void)			/* Debugging only. */;
void Free_map(void);
bool Grok_map(void);
void Find_base_direction(void);
unsigned short Find_closest_team(int posx, int posy);
DFLOAT Wrap_findDir(DFLOAT dx, DFLOAT dy);
DFLOAT Wrap_length(DFLOAT dx, DFLOAT dy);

/*
 * Prototypes for cmdline.c
 */
int Parse_list(int *index, char *buf);
int Parser(int argc, char **argv);
int Tune_option(char *opt, char *val);

/*
 * Prototypes for cmdline.c
 */
void tuner_none(void);
void tuner_dummy(void);
bool Init_options(void);
void Free_options(void);

/*
 * Prototypes for play.c
 */
void Thrust(int ind);
void Recoil(object_t *ship, object_t *shot);
void Record_shove(player_t *pl, player_t *pusher, long time);
void Delta_mv(object_t *ship, object_t *obj);
void Obj_repel(object_t *obj1, object_t *obj2, int repel_dist);
void Alloc_shots(int number);
void Free_shots(void);
void Add_fuel(pl_fuel_t*, long);
void Update_tanks(pl_fuel_t *);
void Move_ball(int ind);
void Move_ball_interpolation(int ind);
void Fire_shot(int ind, int type, int dir);
void Fire_general_shot(int ind, u_short team, bool cannon, DFLOAT x, DFLOAT y,
		       int type, int dir, DFLOAT speed,
		       int target);
void Fire_normal_shots(int ind);
void Fire_main_shot(int ind, int type, int dir);
void Fire_shot(int ind, int type, int dir);
void Make_treasure_ball(int treasure);
int Punish_team(int ind, int t_destroyed, int t_target);
void Delete_shot(int ind);
void Make_debris(
	    /* pos.cx, pos.cy */ int    cx,          int cy,
	    /* vel.x, vel.y   */ DFLOAT  velx,       DFLOAT vely,
	    /* owner id       */ int    id,
	    /* owner team     */ u_short team,
	    /* type           */ int    type,
	    /* mass           */ DFLOAT  mass,
	    /* status         */ long   status,
	    /* color          */ int    color,
	    /* radius         */ int    radius,
	    /* min,max debris */ int    min_debris, int    max_debris,
	    /* min,max dir    */ int    min_dir,    int    max_dir,
	    /* min,max speed  */ DFLOAT  min_speed,  DFLOAT  max_speed,
	    /* min,max life   */ int    min_life,   int    max_life
	    );
void Make_wreckage(
	    /* pos.cx, pos.cy */ int cx,            int cy,
	    /* vel.x, vel.y   */ DFLOAT velx,       DFLOAT vely,
	    /* owner id       */ int    id,
	    /* owner team     */ u_short team,
	    /* min,max mass   */ DFLOAT min_mass,   DFLOAT max_mass,
	    /* total mass     */ DFLOAT total_mass,
	    /* status         */ long   status,
	    /* color          */ int    color,
	    /* max wreckage   */ int    max_wreckage,
	    /* min,max dir    */ int    min_dir,    int    max_dir,
	    /* min,max speed  */ DFLOAT min_speed,  DFLOAT max_speed,
	    /* min,max life   */ int    min_life,   int    max_life
	    );
void Explode_fighter(int ind);

/*
 * Prototypes for command.c
 */
void Handle_player_command(player_t *pl, char *cmd);

/*
 * Prototypes for player.c
 */
void Pick_startpos(int ind);
void Go_home(int ind);
void Player_add_tank(int ind, long tank_fuel);
int Init_player(int ind, shipshape_t *ship);
void Alloc_players(int number);
void Free_players(void);
void Update_score_table(void);
void Reset_all_players(void);
void Check_team_members(int);
void Check_team_treasures(int);
void Compute_game_status(void);
void Delete_player(int ind);
void Detach_ball(int ind, int ball);
void Kill_player(int ind, bool rank_death);
void Player_death_reset(int ind, bool rank_death);
void Team_game_over(int winning_team, const char *reason);
void Individual_game_over(int winner);

/*
 * Prototypes for robot.c
 */
void Parse_robot_file(void);
void Robot_init(void);
void Robot_delete(player_t *pl, int kicked);
void Robot_destroy(player_t *pl);
void Robot_update(void);
void Robot_war(player_t *pl, player_t *kp);
void Robot_reset_war(player_t *pl);
int Robot_war_on_player(player_t *pl);
void Robot_go_home(player_t *pl);
void Robot_program(player_t *pl, int victim_id);
void Robot_message(player_t *pl, const char *message);

/*
 * Prototypes for rules.c
 */
void Set_initial_resources(void);
void Set_world_items(void);
void Set_world_rules(void);

/*
 * Prototypes for server.c
 */
void End_game(void);
int Pick_team(int pick_for_type);
void Server_info(char *str, unsigned max_size);
void Log_game(const char *heading);
void Game_Over(void);
int plock_server(int onoff);
void tuner_plock(void);
void Main_loop(void);


/*
 * Prototypes for contact.c
 */
void Contact_cleanup(void);
int Contact_init(void);
void Contact(int fd, void *arg);
void Queue_loop(void);
int Queue_advance_player(char *name, char *msg);
int Queue_show_list(char *msg);
void Set_deny_hosts(void);

/*
 * Prototypes for command.c
 */
player_t *Get_player_by_name(const char *str,
			     int *error_p, const char **errorstr_p);

/*
 * Prototypes for metaserver.c
 */
void Meta_send(char *mesg, int len);
int Meta_from(char *addr, int port);
void Meta_gone(void);
void Meta_init(int fd);
void Meta_update(int change);

/*
 * Prototypes for frame.c
 */
void Frame_update(void);
void Set_message(const char *message);
void Set_player_message(player_t *pl, const char *message);

/*
 * Prototypes for update.c
 */
void Update_radar_target(int);
void Update_objects(void);
void Init_interpolation_data(void);
void Update_objects_interpolation(void);

/*
 * Prototypes for option.c
 */
void addOption(const char *name, const char *value, int override, void *def);
char *getOption(const char *name);
bool parseDefaultsFile(const char *filename);
bool parseMapFile(const char *filename);
void parseOptions(void);

/*
 * Prototypes for option.c
 */
void Options_parse(void);
void Options_free(void);
bool Convert_string_to_int(const char *value_str, int *int_ptr);
bool Convert_string_to_float(const char *value_str, DFLOAT *float_ptr);
bool Convert_string_to_bool(const char *value_str, bool *bool_ptr);
void Convert_list_to_string(list_t list, char **string);
void Convert_string_to_list(const char *value, list_t *list_ptr);

/*
 * Prototypes for parser.c
 */
int Parser_list_option(int *index, char *buf);
bool Parser(int argc, char **argv);
int Tune_option(char *name, char *val);
int Get_option_value(const char *name, char *value, unsigned size);

/*
 * Prototypes for fileparser.c
 */
bool parseDefaultsFile(const char *filename);
bool parsePasswordFile(const char *filename);
bool parseMapFile(const char *filename);
void expandKeyword(const char *keyword);

void insert_measure(void);

#endif
