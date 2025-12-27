/*
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
int32_t string_is_true(char *optval);
int32_t string_is_false(char *optval);
int32_t f2i(DFLOAT f);
DFLOAT discrete_angle(DFLOAT x, DFLOAT y);

/*
 * Prototypes for id.c
 */
int32_t peek_ID(void);
int32_t request_ID(void);
void release_ID(int32_t id);

/*
 * Prototypes for cmdline.c
 */
bool Init_options(void);
void Free_options(void);
void tuner_none(void);
void tuner_dummy(void);

/*
 * Prototypes for rules.c
 */
void Set_initial_resources(void);
void Set_world_items(void);
void Set_world_rules(void);

/*
 * Prototypes for contact.c
 */
void Contact_cleanup(void);
int32_t Contact_init(void);
int32_t Queue_advance_player(char *name, char *msg);
int32_t Queue_show_list(char *msg);
void Set_deny_hosts(void);

/*
 * Prototypes for command.c
 */
player_t *Player_get_by_name_exact(const char *str);
player_t *Player_get_by_name(const char *str, int32_t *error_p, const char **errorstr_p);

/*
 * Prototypes for option.c
 */
void addOption(const char *name, const char *value, int32_t override, void *def);
char *getOption(const char *name);
void parseOptions(void);
void Options_parse(void);
void Options_free(void);
bool Convert_string_to_int(const char *value_str, int32_t *int_ptr);
bool Convert_string_to_float(const char *value_str, DFLOAT *float_ptr);
bool Convert_string_to_bool(const char *value_str, bool *bool_ptr);
void Convert_list_to_string(TList list, char **string);
void Convert_string_to_list(const char *value, TList *list_ptr);

/*
 * Prototypes for fileparser.c
 */
void expandKeyword(const char *keyword);

/*
 * Prototypes for performance.c
 */
void insert_measure(void);

#endif
