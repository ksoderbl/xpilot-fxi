/* $Id: proto.h,v 1.17 2008/08/26 20:51:06 rotunda_pk Exp $
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

#include "object.h"
#include "list.h"

/*
 * Prototypes for math.c
 */
int32_t ON(int8_t *optval);
int32_t OFF(int8_t *optval);
int32_t f2i(DFLOAT f);
DFLOAT findDir(DFLOAT x, DFLOAT y);

/*
 * Prototypes for collision.c
 */
void Free_cells(void);
void Alloc_cells(void);
void Check_collision(void);
void SCORE(player_t *pl, int32_t points, int32_t x, int32_t y, const int8_t *msg);

/*
 * Prototypes for id.c
 */
int32_t peek_ID(void);
int32_t request_ID(void);
void release_ID(int32_t id);

/*
 * Prototypes for walls.c
 */
void Walls_init(void);
void Move_init(void);
void Move_object(object_t *obj);
void Move_object_interpolation(object_t *obj);
void Move_player(player_t *pl);
void Move_player_interpolation(player_t *pl);
void Turn_player(player_t *pl);
void Old_turn_player(player_t *pl);

/*
 * Prototypes for event.c
 */
int32_t Handle_keyboard(player_t *pl);
void Pause_player(player_t *pl, bool state);
bool Player_lock_closest(player_t *pl, bool next);
bool team_dead(team_t *team);

/*
 * Prototypes for map.c
 */
void Print_map(void) /* Debugging only. */;
void Free_map(void);
bool Grok_map(void);
void Find_base_direction(void);
team_t *Find_closest_team(int32_t posx, int32_t posy);
DFLOAT Wrap_findDir(DFLOAT dx, DFLOAT dy);
DFLOAT Wrap_length(DFLOAT dx, DFLOAT dy);

/*
 * Prototypes for cmdline.c
 */
int32_t Parse_list(int32_t *index, int8_t *buf);

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
void Thrust(player_t *pl);
void Recoil(object_t *ship, object_t *shot);
void Record_shove(player_t *pl, player_t *pusher, int32_t time);
void Delta_mv(object_t *ship, object_t *obj);
void Obj_repel(object_t *obj1, object_t *obj2, int32_t repel_dist);
void Alloc_shots(int32_t number);
void Free_shots(void);
void Add_fuel(pl_fuel_t*, int32_t);
void Update_tanks(pl_fuel_t *);
void Move_ball(object_t *ball);
void Move_ball_interpolation(object_t *ball);
//void Fire_shot(int32_t ind, int32_t type, int32_t dir);
//void Fire_general_shot(int32_t ind, uint16_t team, bool cannon, DFLOAT x, DFLOAT y,
//		       int32_t type, int32_t dir, DFLOAT speed,
//		       int32_t target);
void Fire_normal_shots(player_t *pl);
//void Fire_main_shot(int32_t ind, int32_t type, int32_t dir);
//void Fire_shot(int32_t ind, int32_t type, int32_t dir);
void Make_treasure_ball(treasure_t *t);
int32_t Punish_team(player_t *pl, treasure_t *td, treasure_t *tt);
void Delete_object(object_t *obj);
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
/* min,max life   */int32_t min_life, int32_t max_life);
void Make_wreckage(
/* pos.cx, pos.cy */int32_t cx, int32_t cy,
/* vel.x, vel.y   */DFLOAT velx, DFLOAT vely,
/* owner id       */player_t *pl,
/* owner team     */team_t *team,
/* min,max mass   */DFLOAT min_mass, DFLOAT max_mass,
/* total mass     */DFLOAT total_mass,
/* status         */int32_t status,
/* color          */int32_t color,
/* max wreckage   */int32_t max_wreckage,
/* min,max dir    */int32_t min_dir, int32_t max_dir,
/* min,max speed  */DFLOAT min_speed, DFLOAT max_speed,
/* min,max life   */int32_t min_life, int32_t max_life);
//void Explode(int32_t ind);
void Explode_fighter(player_t *pl);

/*
 * Prototypes for command.c
 */
void Handle_player_command(player_t *pl, int8_t *cmd);

/*
 * Prototypes for player.c
 */
void Pick_startpos(player_t *pl);
void Go_home(player_t *pl);
void Compute_sensor_range(player_t *pl);
void Player_add_tank(player_t *pl, int32_t tank_fuel);
void Player_remove_tank(player_t *pl, int32_t which_tank);
player_t* Init_player(void);
void Alloc_players(int32_t number);
void Free_players(void);
void Update_score_table(void);
void Reset_all_players(void);
void Check_team_members(team_t *team);
void Compute_game_status(void);
void Delete_player(player_t *pl);
void Detach_ball(player_t *pl, object_t *ball);
void Kill_player(player_t *pl, bool rank_death);
void Player_death_reset(player_t *pl, bool rank_death);
void Team_game_over(team_t *winning_team, const int8_t *reason);
void Individual_game_over(int32_t winner);
void Players_swap_each_other(player_t *pl1, player_t *pl2);
void Send_info_about_player(player_t *pl);
void Set_swapper_state(player_t *pl);

/*
 * Prototypes for robot.c
 */
void Parse_robot_file(void);
void Robot_init(void);
void Robot_delete(player_t *pl, int32_t kicked);
void Robot_destroy(player_t *pl);
void Robot_update(void);
void Robot_war(player_t *pl, player_t *kp);
void Robot_reset_war(player_t *pl);
player_t *Robot_war_on_player(player_t *pl);
void Robot_go_home(player_t *pl);
void Robot_program(player_t *pl, player_t *kp);
void Robot_message(player_t *pl, const int8_t *message);

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
team_t *Pick_team(int32_t pick_for_type);
void Server_info(int8_t *str, uint32_t max_size);
void Log_game(const int8_t *heading);
void Game_Over(void);
int32_t plock_server(int32_t onoff);

/*
 * Prototypes for contact.c
 */
void Contact_cleanup(void);
int32_t Contact_init(void);
int32_t Queue_advance_player(int8_t *name, int8_t *msg);
int32_t Queue_show_list(int8_t *msg);
void Set_deny_hosts(void);

/*
 * Prototypes for command.c
 */
player_t *Get_player_by_name(const int8_t *str, int32_t *error_p,
		const int8_t **errorstr_p);

/*
 * Prototypes for metaserver.c
 */
void Meta_send(int8_t *mesg, int32_t len);
int32_t Meta_from(int8_t *addr, int32_t port);
void Meta_gone(void);
void Meta_init(int32_t fd);
void Meta_update(bool change);

/*
 * Prototypes for frame.c
 */
void Frame_update(void);
void Set_message(const int8_t *message);
void Set_player_message(player_t *pl, const int8_t *message);

/*
 * Prototypes for update.c
 */
void Update_radar_target(int32_t);
void Update_objects(void);
//void Autopilot(int32_t ind, int32_t on);
void Init_interpolation_data(void);
void Update_objects_interpolation(void);

/*
 * Prototypes for option.c
 */
void addOption(const int8_t *name, const int8_t *value, int32_t override, void *def);
int8_t *getOption(const int8_t *name);
void parseOptions(void);

/*
 * Prototypes for option.c
 */
void Options_parse(void);
void Options_free(void);
bool Convert_string_to_int(const int8_t *value_str, int32_t *int_ptr);
bool Convert_string_to_float(const int8_t *value_str, DFLOAT *float_ptr);
bool Convert_string_to_bool(const int8_t *value_str, bool *bool_ptr);
void Convert_list_to_string(list_t list, int8_t **string);
void Convert_string_to_list(const int8_t *value, list_t *list_ptr);

/*
 * Prototypes for parser.c
 */
int32_t Parser_list_option(int32_t *index, int8_t *buf);
int32_t Get_option_value(const int8_t *name, int8_t *value, uint32_t size);

/*
 * Prototypes for fileparser.c
 */
void expandKeyword(const int8_t *keyword);

void insert_measure(void);

#endif
