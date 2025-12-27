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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

# include <unistd.h>

#include "version.h"
#include "serverconst.h"
#include "global.h"
#include "proto.h"
#include "error.h"
#include "netserver.h"
#include "score.h"
#include "rank.h"
#include "parser.h"
#include "commonproto.h"
#include "frame.h"

#include "player.h"
#include "map.h"
#include "player_inline.h"


char command_version[] = VERSION;
extern bool limitedRoundsGameOver;

extern connection_t *Conn;

/*
 * Look if any player's name is exactly 'str'
 */
player_t *Player_get_by_name_exact(const char *str)
{
	int32_t i;
	player_t *pl;

	/* Look for an exact match on player nickname. */
	for (i = 0; i < NumPlayers; i++) {
		pl = Player_by_index(i);
		if (!strcasecmp(pl->name, str))
			return pl;
	}

	return NULL;
}

/*
 * Look if any player's name is exactly 'str',
 * If not, look if any player's name contains 'str'.
 * The matching is case insensitive. If there is an
 * error (no matches or several matches) NULL is returned
 * and the error code is stored in 'error' if that is not NULL
 * and a string describing the error is stored in
 * 'errorstr_p' if that is not NULL.
 */
player_t *Player_get_by_name(const char *str, int32_t *error_p, const char **errorstr_p)
{
	int32_t i, id;
	player_t *found_pl = NULL, *pl;
	size_t len = 0;

	if (str == NULL || (len = strlen(str)) == 0)
		goto match_none;

	/* Get player by id */
	id = atoi(str);
	if (id > 0) {
		found_pl = Player_by_id(id);
		if (!found_pl)
			goto match_none;

		return found_pl;
	}

	/* Look for an exact match on player nickname. */
	pl = Player_get_by_name_exact(str);

	if (pl) {
		return pl;
	}

	/* Look if 'str' matches beginning of only one nick. */
	for (i = 0; i < NumPlayers; i++) {
		pl = Player_by_index(i);

		if (!strncasecmp(pl->name, str, len)) {
			if (found_pl)
				goto match_several;
			found_pl = pl;
			continue;
		}
	}
	if (found_pl)
		return found_pl;

	/*
	 * Check what players' name 'str' is a substring of (case insensitively).
	 */
	for (i = 0; i < NumPlayers; i++) {
		int32_t j;

		pl = Player_by_index(i);

		for (j = 0; j < 1 + (int32_t) strlen(pl->name) - (int32_t) len; j++) {
			if (!strncasecmp(pl->name + j, str, len)) {
				if (found_pl)
					goto match_several;
				found_pl = pl;
				break;
			}
		}
	}
	if (found_pl)
		return found_pl;

	match_none: if (error_p != NULL)
		*error_p = -1;
	if (errorstr_p != NULL)
		*errorstr_p = "Name does not match any player.";
	return NULL;

	match_several: if (error_p != NULL)
		*error_p = -2;
	if (errorstr_p != NULL)
		*errorstr_p = "Name matches several players.";
	return NULL;
}

#define CMD_RESULT_SUCCESS		0
#define CMD_RESULT_ERROR		(-1)
#define CMD_RESULT_NOT_OPERATOR		(-2)
#define CMD_RESULT_NO_NAME		(-3)

static int32_t Cmd_help(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_team(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_version(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_lock(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_password(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_pause(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_reset(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_set(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_kick(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_queue(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_advance(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_get(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_stats(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_plinfo(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_addr(char *arg, player_t *pl, bool oper, char *msg);
static int32_t Cmd_oldturn(char *arg, player_t *pl, bool oper, char *msg);

typedef struct {
	const char *name;
	const char *abbrev;
	const char *help;
	int32_t oper_only;
	int32_t (*cmd)(char *arg, player_t *pl, bool oper, char *msg);
} Command_info;

/*
 * A list of all the commands sorted alphabetically.
 */
static Command_info
		commands[] =
			{
						{
								"advance",
								"ad",
								"/advance <name of player in the queue>. "
									"Move the player to the front of the queue.  (operator)",
								1, Cmd_advance },
						{
								"get",
								"g",
								"/get <option>.  Gets a server option.",
								0, Cmd_get },
						{
								"help",
								"h",
								"Print command list.  /help <command> gives more info.",
								0, Cmd_help },
						{
								"kick",
								"k",
								"/kick <player name or ID number>.  Remove a player from game.  (operator)",
								1, Cmd_kick },
						{
								"lock",
								"l",
								"Just /lock tells lock status.  /lock 1 locks, /lock 0 unlocks.  (operator)",
								0, /* checked in the function */
								Cmd_lock },
						{
								"password",
								"pas",
								"/password <string>.  If string matches -password option "
									"gives operator status.",
								0, Cmd_password },
						{
								"pause",
								"pau",
								"/pause <player name or ID number>.  Pauses player.  (operator)",
								1, Cmd_pause },
						{
								"queue",
								"q",
								"/queue.  Show the names of players waiting to enter.",
								0, Cmd_queue },
						{
								"reset",
								"r",
								"Just /reset re-starts the round. "
									"/reset.  Resets all scores to 0.  (operator)",
								1, Cmd_reset },
						{
								"set",
								"set",
								"/set <option> <value>.  Sets a server option.  (operator)",
								1, Cmd_set },
						{
								"team",
								"t",
								"/team <team number> swaps you to given team.",
								0, Cmd_team },
						{
								"version",
								"v",
								"Print server version.",
								0, Cmd_version },
						{
								"stats",
								"st",
								"/stats <player name or ID number>.  Show player ranking info.",
								0, Cmd_stats },
						{
								"plinfo",
								"pl",
								"/plinfo <player name or ID number>.  Show misc. player info.",
								0, Cmd_plinfo },
						{
								"addr",
								"addr",
								"/addr <player name or ID number>. Show IP-address of player."
									"(operator)",
								1, Cmd_addr },
						{
								"oldturn",
								"oldturn",
								"/oldturn  Use old turning code. /oldturn 1 enables. /oldturn 0 disables.",
								0, Cmd_oldturn }, };

/*
 * cmd parameter has no leading slash.
 */
void Handle_player_command(player_t *pl, char *cmd)
{
	int32_t i, result;
	char *args;
	char msg[MSG_LEN];

	const char e_error[] = "Error.";
	const char e_noop[] = "You need operator status to use this command.";
	const char e_noplarg[] = "You must give a player name as an argument.";
	const char e_bug[] = "Bug.";
	const char e_empty[] = "";
	char *e_ptr = (char *) e_empty;

	if (!*cmd) {
		Message_player_print(pl, PL_MSG_REPLY, "No command given.  Type /help for help.");
		return;
	}

	args = strchr(cmd + 1, ' ');
	if (!args)
		/* point to end of string. */
		args = cmd + strlen(cmd);
	else {
		/* zero terminate cmd and advance 1 byte. */
		*args++ = '\0';
		while (isspace(*args))
			args++;
	}

	for (i = 0; i < NELEM(commands); i++) {
		size_t len1 = strlen(commands[i].abbrev);
		size_t len2 = strlen(cmd);

		if (!strncasecmp(cmd, commands[i].name, MAX(len1, len2)))
			break;
	}

	if (i == NELEM(commands)) {
		Message_player_print(pl, PL_MSG_REPLY, "Unknown command '%s'.", cmd);
		return;
	}

	msg[0] = '\0';
	result = (*commands[i].cmd)(args, pl, pl->isoperator, msg /*, sizeof(msg)*/);

	if (msg[0] == '\0') {
		switch (result) {
		case CMD_RESULT_SUCCESS:
			e_ptr = (char *) e_empty;
			break;

		case CMD_RESULT_ERROR:
			e_ptr = (char *) e_error;
			break;

		case CMD_RESULT_NOT_OPERATOR:
			e_ptr = (char *) e_noop;
			break;

		case CMD_RESULT_NO_NAME:
			e_ptr = (char *) e_noplarg;
			break;

		default:
			e_ptr = (char *) e_bug;
			break;
		}
	}
	else {
		e_ptr = msg;
	}

	if (e_ptr[0]) {
		Message_player_print(pl, PL_MSG_REPLY, "%s", e_ptr);
	}
}

static int32_t Cmd_advance(char *arg, player_t *pl, int32_t oper, char *msg)
{
	int32_t result;

	if (!oper) {
		return CMD_RESULT_NOT_OPERATOR;
	}

	if (!arg || !*arg) {
		return CMD_RESULT_NO_NAME;
	}

	result = Queue_advance_player(arg, msg);

	if (result < 0) {
		return CMD_RESULT_ERROR;
	}

	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_queue(char *arg, player_t *pl, int32_t oper, char *msg)
{
	int32_t result;

	result = Queue_show_list(msg);

	if (result < 0) {
		return CMD_RESULT_ERROR;
	}

	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_team(char *arg, player_t *pl, bool oper, char *msg)
{
	team_t *team_ptr = NULL;
	int32_t i;
	char *arg2;
	int32_t result = CMD_RESULT_ERROR;

	/*
	 * Assume nothing will be said or done.
	 */
	msg[0] = '\0';

	if (!BIT(World.rules->mode, TEAM_PLAY)) {
		sprintf(msg, "No team play going on.");
	}
	else if (pl->team == NULL) {
		sprintf(msg, "You do not currently have a team.");
	}
	else if (!arg) {
		sprintf(msg, "No team specified.");
	}
	else if (!isdigit(*arg)) {
		sprintf(msg, "Invalid team specification.");
	}
	else {
		/* Retrieve the team number from player's command line */
		int32_t team = strtoul(arg, (char **)(&arg2), 0);

		if (arg2 && *arg2) {
			const char *errorstr;
			size_t size = MSG_LEN;

			if (!pl->isoperator) {
				sprintf(msg, "You need operator status to swap other players.");

				result = CMD_RESULT_NOT_OPERATOR;
				goto swap_finalize;
			}

			while (isspace(*arg2)) {
				arg2++;
			}

			pl = Player_get_by_name(arg2, NULL, &errorstr);
			if (!pl) {
				strlcpy(msg, errorstr, size);
				goto swap_finalize;
			}
		}

		/* Dequeue us from swapping to any team first */
		for (i = 0; i < MAX_TEAMS; i++) {
			if (World.teams[i].Swapper == pl) {
				World.teams[i].Swapper = NULL;
			}
		}

		/* Attempt to retrieve the pointer to team structure */
		if (team >= 0 && team < MAX_TEAMS) {
			team_ptr = &World.teams[team];
		}

		/* Here we know where we want to swap to, but not whether
		 * is it possible yet
		 */

		if (!team_ptr) {
			sprintf(msg, "Team %d is not a valid team.", team);
		}
                else if (team_ptr->Num == PAUSE_TEAM_NUM && !(Player_is_close_to_base(pl) || Player_is_waiting(pl) || Player_is_dead(pl))) {
                        sprintf(msg, "You have to be close to your home base in order to pause.");
                }
		else if (team_ptr == pl->team) {
			sprintf(msg, "You already are on team %d.", team_ptr->Num);
		}
		else if (team_ptr->NumBases == 0) {
			sprintf(msg, "There are no bases for team %d on this map.", team_ptr->Num);
		}
		else if (reserveRobotTeam && team_ptr->Num == robotTeam) {
			sprintf(msg, "You cannot join the robot team on this server.");
		}

		/* Check if the destination team is not already full.
		 * Perhaps we need to queue the player?
		 */
		else if (team_ptr->NumBases <= team_ptr->NumMembers) {
			player_t *pl2;

			/* Player, who is waiting to swap to the team we are
			 * currently on.
			 */
			pl2 = pl->team->Swapper;

			/* If two teams are full and one player from each
			 * team wants to swap to the opposite team, we
			 * perform the exchange.
			 */
			if ((pl2 != NULL) && (pl2->team == team_ptr)) {
				Players_swap_teams(pl, pl2);

				Message_game_print("Some players swapped teams.");
				strcpy(msg, "");

				result = CMD_RESULT_SUCCESS;
				goto swap_finalize;
			}

			/* Swap a paused player away from the full team */
			for (i = NumPlayers - 1; i >= 0; i--) {
				pl2 = Players[i];
				if (Player_is_connected(pl2) && Player_is_paused(pl2)
						&& (pl2->team == team_ptr)) {
					Players_swap_teams(pl, pl2);

					Message_game_print("%s has swapped with paused %s.", pl->name, pl2->name);
					strcpy(msg, "");

					result = CMD_RESULT_SUCCESS;
					goto swap_finalize;
				}
			}

			/* If we got this far, that means the swap couldn't
			 * be performed. Add us to the queue.
			 */
			sprintf(msg, "You are queued for swap to team %d.", team_ptr->Num);
			team_ptr->Swapper = pl;

			result = CMD_RESULT_SUCCESS;
			goto swap_finalize;
		}

		/* If someone was queued to swap to the team we are going
		 * to leave now (there is enough room in the destination
		 * team), perform the operation.
		 */
		else if (pl->team->Swapper) {
			Players_swap_teams(pl, pl->team->Swapper);

			Message_game_print("Some players swapped teams.");
			strcpy(msg, "");

			result = CMD_RESULT_SUCCESS;
			goto swap_finalize;
		}
		else if (Player_is_paused(pl)) {
			/* Handle unpausing here */
			Player_unpause(pl, team_ptr);

			/* TODO add proper error handling here */

			result = CMD_RESULT_SUCCESS;
			goto swap_finalize;
		}
		else if (!Player_is_paused(pl) && team_ptr->Num == PAUSE_TEAM_NUM) {
			Player_pause_self(pl);

			result = CMD_RESULT_SUCCESS;
			goto swap_finalize;
		}
		else {
			/* Handle an ordinary one-way swap here */
			if (Player_is_waiting(pl)
					|| (Object_count_treasures_missing(pl->team) == 0)
					|| (Team_count_active_players(pl->team) == Team_count_active_players(NULL))) {
				Player_swap_team(pl, team_ptr);
				Message_game_print("%s has swapped to team %d.", pl->name, team_ptr->Num);
			}
			else {
				Message_player_print(pl, PL_MSG_NOTICE, "Your team's treasure(s) have to be safe before you can swap.");
			}

			// TODO: this is a lame way of not letting the same message
			// be displayed in top message field; improve this
			strcpy(msg, "");

			result = CMD_RESULT_SUCCESS;
			goto swap_finalize;
		}
	}

	/* Execute code that is common for all cases */
	swap_finalize:

	if (result == CMD_RESULT_SUCCESS) {
		updateScores = true;
	}

	return result;
}

static int32_t Cmd_kick(char *arg, player_t *pl, bool oper, char *msg)
{
	player_t *kicked_pl;
	const char *errorstr;
	size_t size = MSG_LEN;

	if (!oper) {
		return CMD_RESULT_NOT_OPERATOR;
	}

	if (!arg || !*arg) {
		return CMD_RESULT_NO_NAME;
	}

	kicked_pl = Player_get_by_name(arg, NULL, &errorstr);
	if (kicked_pl) {
		Message_game_print("%s kicked %s out!", pl->name, kicked_pl->name);

		if (!Player_is_connected(kicked_pl)) {
			Player_remove(kicked_pl);
		}
		else {
			Destroy_connection(kicked_pl->connp, "kicked out");
		}
		return CMD_RESULT_SUCCESS;
	}
	strlcpy(msg, errorstr, size);

	return CMD_RESULT_ERROR;
}

static int32_t Cmd_version(char *arg, player_t *pl, bool oper, char *msg)
{
	sprintf(msg, "XPilot version %s.", VERSION);
	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_help(char *arg, player_t *pl, bool oper, char *msg)
{
	int32_t i;

	if (!*arg) {
		strcpy(msg, "Commands: ");
		for (i = 0; i < NELEM(commands); i++) {
			strcat(msg, commands[i].name);
			strcat(msg, " ");
		}
	}
	else {
		for (i = 0; i < NELEM(commands); i++) {
			if (!strncasecmp(arg, commands[i].name, strlen(
					commands[i].abbrev))) {
				break;
			}
		}
		if (i == NELEM(commands)) {
			sprintf(msg, "No help for nonexistent command '%s'.",
					arg);
		}
		else {
			strcpy(msg, commands[i].help);
		}
	}

	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_reset(char *arg, player_t *pl, bool oper, char *msg)
{
	int32_t i;

	if (!oper) {
		return CMD_RESULT_NOT_OPERATOR;
	}

	Players_reset();

	Objects_time_out();

	/* Reset the teams */
	for (i = 0; i < MAX_TEAMS; i++) {
		World.teams[i].TreasuresDestroyed = 0;
		World.teams[i].TreasuresLeft = World.teams[i].NumTreasures;
	}

	if (arg && !strcasecmp(arg, "all")) {
		for (i = NumPlayers - 1; i >= 0; i--) {
			Score_set(Players[i], 0);
		}
		roundsPlayed = 0;

		Message_game_important_print("Total reset by %s!", pl->name);
	}
	else {
		Message_game_important_print("Round reset by %s!", pl->name);
	}

	strcpy(msg, "");

	limitedRoundsGameOver = false;
	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_password(char *arg, player_t *pl, bool oper, char *msg)
{
	if (!password || !arg || strcmp(arg, password)) {
		strcpy(msg, "Wrong.");
		if (pl->isoperator) {
			NumOperators--;
			pl->isoperator = false;
			strcat(msg, "  You lost operator status.");
		}
	}
	else {
		if (!pl->isoperator) {
			NumOperators++;
			pl->isoperator = true;
		}
		strcpy(msg, "You got operator status.");
	}
	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_lock(char *arg, player_t *pl, bool oper, char *msg)
{
	int32_t new_lock;

	if (!arg || !*arg) {
		sprintf(msg, "The game is currently %s.", game_lock ? "locked"
				: "unlocked");
		return CMD_RESULT_SUCCESS;
	}

	if (!oper) {
		return CMD_RESULT_NOT_OPERATOR;
	}

	if (!strcmp(arg, "1")) {
		new_lock = 1;
	}
	else if (!strcmp(arg, "0")) {
		new_lock = 0;
	}
	else {
		sprintf(msg, "Invalid argument '%s'.  Specify either 0 or 1.",
				arg);
		return CMD_RESULT_ERROR;
	}

	if (new_lock == game_lock) {
		sprintf(msg, "Game is already %s.", game_lock ? "locked"
				: "unlocked");
	}
	else {
		game_lock = new_lock;
		Message_game_important_print("The game has been %s by %s!", game_lock ? "locked" : "unlocked", pl->name);
		strcpy(msg, "");
	}

	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_set(char *arg, player_t *pl, bool oper, char *msg)
{
	int32_t i;
	char *option;
	char *value;

	if (!oper) {
		return CMD_RESULT_NOT_OPERATOR;
	}

	/*
	 * Second argument of second strtok is " " instead of ""
	 * which allows setting string options to values that contain spaces.
	 */
	if (!arg || !(option = strtok(arg, " ")) || !(value = strtok(NULL, ""))) {

		sprintf(msg, "Usage: /set option value.");
		return CMD_RESULT_ERROR;
	}

	i = Tune_option(option, value);
	if (i == 1) {
		if (!strcasecmp(option, "password"))
			sprintf(msg, "Operation successful.");
		else {
			char value[MAX_CHARS];
			Get_option_value(option, value, sizeof(value));
			Message_game_important_print("Option %s set to %s by %s.", option, value, pl->name);
			strcpy(msg, "");

			return CMD_RESULT_SUCCESS;
		}
	}
	else if (i == 0) {
		sprintf(msg, "Invalid value.");
	}
	else if (i == -1) {
		sprintf(msg, "This option cannot be changed at runtime.");
	}
	else if (i == -2) {
		sprintf(msg, "No option named '%s'.", option);
	}
	else {
		sprintf(msg, "Error.");
	}

	return CMD_RESULT_ERROR;
}

static int32_t Cmd_pause(char *arg, player_t *pl, bool oper, char *msg)
{
	player_t *pl2;
	const char *errorstr;
	size_t size = MSG_LEN;

	if (!oper) {
		return CMD_RESULT_NOT_OPERATOR;
	}

	if (!arg || !*arg) {
		return CMD_RESULT_NO_NAME;
	}

	pl2 = Player_get_by_name(arg, NULL, &errorstr);
	if (!pl2) {
		strlcpy(msg, errorstr, size);
		return CMD_RESULT_ERROR;
	}

	if (Player_is_connected(pl2)) {
		Player_pause_forced(pl2);
		Message_game_print("%s was paused by %s.", pl2->name, pl->name);
		strlcpy(msg, "", size);
		return CMD_RESULT_SUCCESS;
	}
	else {
		snprintf(msg, size, "Robots can't be paused.");
		return CMD_RESULT_ERROR;
	}

	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_get(char *arg, player_t *pl, bool oper, char *msg)
{
	char value[MAX_CHARS];
	int32_t i;

	if (!arg || !*arg) {
		strcpy(msg, "Usage: /get option.");
		return CMD_RESULT_ERROR;
	}

	if (!strcasecmp(arg, "password") || !strcasecmp(arg, "mapData")) {
		strcpy(msg, "Cannot retrieve that option.");
		return CMD_RESULT_ERROR;
	}

	i = Get_option_value(arg, value, sizeof(value));

	switch (i) {
	case 1:
		sprintf(msg, "The value of %s is %s.", arg, value);
		return CMD_RESULT_SUCCESS;
	case -2:
		sprintf(msg, "No option named %s.", arg);
		break;
	case -3:
		sprintf(msg, "Cannot show the value of this option.");
		break;
	case -4:
		sprintf(msg, "No value has been set for option %s.", arg);
		break;
	default:
		strcpy(msg, "Generic error.");
		break;
	}

	return CMD_RESULT_ERROR;
}

static int32_t Cmd_stats(char *arg, player_t *pl, bool oper, char *msg)
{
	/*    UNUSED_PARAM(pl); UNUSED_PARAM(oper);*/
	if (!arg || !*arg)
		return CMD_RESULT_NO_NAME;

	if (!Rank_get_stats(arg, msg)) {
		sprintf(msg, "Player \"%s\" doesn't have ranking stats.", arg);
		return CMD_RESULT_ERROR;
	}
	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_plinfo(char *arg, player_t *pl, bool oper, char *msg)
{
	const char *errorstr;
	player_t *pl2;
	size_t size = MSG_LEN;

	if (!arg || !*arg)
		return CMD_RESULT_NO_NAME;

	pl2 = Player_get_by_name(arg, NULL, &errorstr);
	if (!pl2) {
		strlcpy(msg, errorstr, size);
		return CMD_RESULT_ERROR;
	}

	if (Player_is_human(pl2)) {
		snprintf(msg, size,
				"%-15s Ver: 0x%x MaxFPS: %d Turnspeed: %.2f Turnres: %.2f RTT: %i ms RTT_dev: %i ms",
				pl2->name,
				pl2->version, pl2->player_fps, pl2->turnspeed,
				pl2->turnresistance,
				(int32_t)((pl2->connp->rtt_smoothed >> 3) * (1.0/((double)fps)) * 1000.0),
				(int32_t)((pl2->connp->rtt_dev >> 2) * (1.0/((double)fps)) * 1000.0));
	}
	else {
		snprintf(msg, size,
				"%-15s Ver: 0x%x MaxFPS: %d Turnspeed: %.2f Turnres: %.2f",
				pl2->name,
				pl2->version, pl2->player_fps, pl2->turnspeed,
				pl2->turnresistance);
	}

	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_addr(char *arg, player_t *pl, bool oper, char *msg)
{
	player_t *pl2 = NULL;
	const char *errorstr;
	size_t size = MSG_LEN;

	/*UNUSED_PARAM(pl);*/

	if (!oper)
		return CMD_RESULT_NOT_OPERATOR;

	if (!arg || !*arg)
		return CMD_RESULT_NO_NAME;

	pl2 = Player_get_by_name(arg, NULL, &errorstr);
	if (pl2) {
		const char *addr = Player_get_addr(pl2);

		if (addr == NULL)
			snprintf(msg, size, "Unable to get address for %s.",
					pl2->name);
		else
			snprintf(msg, size, "%s plays from: %s.", pl2->name,
					addr);
	}
	else {
		strlcpy(msg, errorstr, size);
		return CMD_RESULT_ERROR;
	}

	return CMD_RESULT_SUCCESS;
}

static int32_t Cmd_oldturn(char *arg, player_t *pl, bool oper, char *msg)
{
	if (!arg || !*arg) {
		sprintf(msg, "You are currently using %s turn code.",
				pl->oldturn ? "old" : "new");
		return CMD_RESULT_SUCCESS;
	}

	if (!strcmp(arg, "1")) {
		pl->oldturn = true;
	}
	else if (!strcmp(arg, "0")) {
		pl->oldturn = false;
	}
	else {
		sprintf(msg, "Invalid argument '%s'.  Specify either 0 or 1.",
				arg);
		return CMD_RESULT_ERROR;
	}

	sprintf(msg, "You are now using %s turn code.", pl->oldturn ? "old"
			: "new");

	return CMD_RESULT_SUCCESS;
}
