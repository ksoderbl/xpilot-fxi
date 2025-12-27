/* $Id: server.c,v 1.19 2008/09/02 19:08:52 rotunda_pk Exp $
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <pwd.h>
#include <sys/param.h>

#ifdef PLOCKSERVER
# if defined(__linux__)
#  include <sys/mman.h>
# else
#  include <sys/lock.h>
# endif
#endif

#define	SERVER
#include "version.h"
#include "commonproto.h"
#include "xpconfig.h"
#include "types.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "socklib.h"
#include "map.h"
#include "bit.h"
#include "sched.h"
#include "netserver.h"
#include "error.h"
#include "portability.h"
#include "server.h"
#include "rank.h"
#include "parser.h"


int8_t server_version[] = VERSION;

player_t **Players;
int32_t GetInd[NUM_IDS + 1];

/** @brief Number of players in game (including paused and waiting)
 *
 * This variable is modified only in @ref Init_player and @ref Delete_player.
 */
int32_t NumPlayers = 0;

/** @brief Number of objects in game (including: balls, shots, pieces of
 * debris)
 *
 * This variable is modified only in @ref Make_debris, @ref Make_wreckage,
 * @ref Make_treasure_ball, @ref Fire_normal_shots, @ref Delete_object
 */
int32_t NumObjs = 0;
object_t *Obj[MAX_TOTAL_SHOTS];

server_t Server;
int8_t ShutdownReason[MAX_CHARS];
int32_t main_loops = 0; /* needed in events.c */
int32_t main_loops_slow = 0;

/** @brief Number of operators currently logged and authorized
 *
 * This variable is modified only in @ref Cmd_password and @ref Delete_player.
 */
int32_t NumOperators = 0;

static int32_t serverSocket;
#ifdef LOG
static bool Log = true;
#endif
static bool NoPlayersEnteredYet = true;
int32_t game_lock = false;
time_t serverTime = 0;

extern bool limitedRoundsGameOver;
extern int32_t received_packets;
int32_t frameDivisor;
int32_t fps;
int32_t frame_cycle = 0;
int32_t intGameSpeed = 12;
float ticksPerFrame = 1.0;
DFLOAT gameSpeed;

extern connection_t *Conn;

static inline double timeval_to_seconds(struct timeval tv);
static void Check_server_versions(void);
static void Handle_signal(int32_t sig_no);


int main(int argc, char **argv)
{
	/*
	 * Make output always linebuffered.  By default pipes
	 * and remote shells cause stdout to be fully buffered.
	 */
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
	setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

	/*
	 * --- Output copyright notice ---
	 */

	xpprintf("  " COPYRIGHT ".\n"
			"  " TITLE " comes with ABSOLUTELY NO WARRANTY; "
			"for details see the\n"
			"  provided LICENSE file.\n\n");

	init_error(argv[0]);
	srand(time((time_t *) 0) * Get_process_id());
	Check_server_versions();

	if (!Parser(argc, (int8_t **)argv)) {
		exit(0);
	}

	plock_server(pLockServer); /* Lock the server into memory */
	Make_table(); /* Make trigonometric tables */
	Find_base_direction();
	Walls_init();

	/* Allocate memory for players, shots and messages */
	Alloc_players(World.NumBases + MAX_PSEUDO_PLAYERS);
	Alloc_shots(MAX_TOTAL_SHOTS);
	Alloc_cells();

	Move_init();

	Robot_init();

	if (BIT(World.rules->mode, TEAM_PLAY)) {
		int32_t i;
		for (i = 0; i < World.NumTreasures; i++)
			if (World.treasures[i].team != NULL)
				Make_treasure_ball(&World.treasures[i]);
	}

	Rank_init_saved_scores();

	/*
	 * Get server's official name.
	 */
	if (serverHost)
		strlcpy(Server.host, serverHost, sizeof Server.host);
	else
		GetLocalHostName(Server.host, sizeof Server.host,
				(reportToMetaServer != 0
						&& searchDomainForXPilot != 0));

	Get_login_name(Server.owner, sizeof Server.owner);

	/*
	 * Log, if enabled.
	 */
	Log_game("START");

	serverSocket = Contact_init();

	Meta_init(serverSocket);

	if (Setup_net_server() == -1) {
		End_game();
	}

	signal(SIGHUP, SIG_IGN);

	signal(SIGTERM, Handle_signal);
	signal(SIGINT, Handle_signal);
	signal(SIGPIPE, SIG_IGN);
#ifdef IGNORE_FPE
	signal(SIGFPE, SIG_IGN);
#endif

	/*
	 * Set the time the server started
	 */
	serverTime = time(NULL);

#ifndef SILENT
	xpprintf(
			"%s Server runs at %d frames per second, correction factor is %f\n",
			showtime(), fps, 1.0 / frameDivisor);
#endif

	setup_timer(fps);
	main_loops = 0;
	sched();
	xpprintf("sched returned!?");
	End_game();
	return 1;
}

void Main_loop(int32_t argv)
{
	struct timeval tv1, tv2;

	gettimeofday(&tv1, NULL);
	main_loops++;
	ticksPerFrame = 1.0f / frameDivisor;

	//printf("mainloop: %e\n",timeval_to_seconds(tv1));

	//printf("%d\n",received_packets);

	if (frame_cycle == frameDivisor)
		frame_cycle = 0;

	if (frame_cycle == 0)
		insert_measure();

	if ((main_loops & 0x3F) == 0) {
		Meta_update(false);
	}

	if (frame_cycle == 0) {
		/* uses mainloops + FPS for timings */
		Input();
	}

	if (NumPlayers > NumRobots || RawMode) {

		if (NoPlayersEnteredYet) {
			if (NumPlayers > NumRobots) {
				NoPlayersEnteredYet = false;
			}
		}

		if (frame_cycle == 0) {
			Update_objects();
			Init_interpolation_data();
			//printf("nointerp: %d\n", frame_cycle);
			Frame_update();
			main_loops_slow++;
		}

		if (frame_cycle != 0) {

			Update_objects_interpolation();
			//printf("interp: %d\n", frame_cycle);
			Frame_update();
		}

	}

	Queue_loop();
#if 0
	if (frame_cycle == 0) {
		measure_time();
	}
#endif

	gettimeofday(&tv2, NULL);
	mainLoopTime = (timeval_to_seconds(tv2) - timeval_to_seconds(tv1))
			* 1e3;
	frame_cycle++;
}

static inline double timeval_to_seconds(struct timeval tv)
{
	return (double) tv.tv_sec + tv.tv_usec * 1e-6;
}

/*
 *  Last function, exit with grace.
 */
void End_game(void)
{
	player_t *pl;
	int8_t msg[MSG_LEN];

	sprintf(msg, "server exiting");

	while (NumPlayers > 0) { /* Kick out all remaining players */
		pl = Players[NumPlayers - 1];
		if (!Player_is_connected(pl)) {
			Delete_player(pl);
		}
		else {
			Destroy_connection(pl->connp, msg);
		}
	}

	/* Tell meta server that we are gone. */
	Meta_gone();

	Contact_cleanup();

	Free_players();
	Free_shots();
	Free_map();
	Free_cells();
	Log_game("END"); /* Log end */
	exit(0);
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
team_t *Pick_team(int32_t pick_for_type)
{
	int32_t i, least_players, num_available_teams = 0, playing_teams = 0,
			losing_team;
	player_t *pl;
	int32_t playing[MAX_TEAMS];
	int32_t free_bases[MAX_TEAMS];
	int32_t available_teams[MAX_TEAMS];
	int32_t team_score[MAX_TEAMS];
	int32_t losing_score;

	for (i = 0; i < MAX_TEAMS; i++) {
		free_bases[i] = World.teams[i].NumBases
				- World.teams[i].NumMembers;
		playing[i] = 0;
		team_score[i] = 0;
		available_teams[i] = 0;
	}
	if (restrictRobots) {
		if (pick_for_type == PickForRobot) {
			if (free_bases[robotTeam] > 0) {
				return &World.teams[robotTeam];
			}
			else {
				return NULL;
			}
		}
	}
	if (reserveRobotTeam) {
		if (pick_for_type != PickForRobot) {
			free_bases[robotTeam] = 0;
		}
	}

	/*
	 * Find out which teams have actively playing members.
	 * Exclude paused players and tanks.
	 * And calculate the score for each team.
	 */
	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		if (BIT(pl->status, PAUSE)) {
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
				if (least_players == NumPlayers || playing[i]
						== least_players) {
					available_teams[num_available_teams++]
							= i;
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
		losing_score = LONG_MAX;
		for (i = 0; i < num_available_teams; i++) {
			if (team_score[available_teams[i]] < losing_score
					&& available_teams[i] != robotTeam) {
				losing_team = available_teams[i];
				losing_score = team_score[losing_team];
			}
		}
		return &World.teams[losing_team];
	}

	/*NOTREACHED*/
	return NULL;
}

/*
 * Return status for server
 */
void Server_info(int8_t *str, uint32_t max_size)
{
	int32_t i, j, k;
	player_t *pl_in_war;
	player_t *pl, **order, *best = NULL;
	DFLOAT ratio, best_ratio = -1e7;
	int8_t name[MAX_CHARS];
	int8_t lblstr[MAX_CHARS];
	int8_t msg[MSG_LEN];

	sprintf(str, "SERVER VERSION...: %s\n"
		"STATUS...........: %s\n"
		"MAX SPEED........: %d fps\n"
		"WORLD (%3dx%3d)..: %s\n"
		"      AUTHOR.....: %s\n"
		"PLAYERS (%2d/%2d)..:\n", server_version,
			(game_lock) ? "locked" : "ok", fps, World.x, World.y,
			World.name, World.author, NumPlayers, World.NumBases);

	if (strlen(str) >= max_size) {
		errno = 0;
		error("Server_info string overflow (%d)", max_size);
		str[max_size - 1] = '\0';
		return;
	}
	if (NumPlayers <= 0) {
		return;
	}

	sprintf(msg, "\nNO:  TM: NAME:             LIFE:   SC:    PLAYER:\n"
		"-------------------------------------------------\n");
	if (strlen(msg) + strlen(str) >= max_size) {
		return;
	}
	strcat(str, msg);

	if ((order = (player_t **) malloc(NumPlayers * sizeof(player_t *)))
			== NULL) {
		error("No memory for order");
		return;
	}
	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		if (BIT(World.rules->mode, LIMITED_LIVES)) {
			ratio = (DFLOAT) pl->score;
		}
		else {
			ratio = (DFLOAT) pl->score / (pl->life + 1);
		}
		if ((best == NULL || ratio > best_ratio) && !BIT(pl->status,
				PAUSE)) {
			best_ratio = ratio;
			best = pl;
		}
		for (j = 0; j < i; j++) {
			if (order[j]->score < pl->score) {
				for (k = i; k > j; k--) {
					order[k] = order[k - 1];
				}
				break;
			}
		}
		order[j] = pl;
	}
	for (i = 0; i < NumPlayers; i++) {
		pl = order[i];
		strcpy(name, pl->name);
		if (Player_is_robot(pl)) {
			if ((pl_in_war = Robot_war_on_player(pl)) != NULL) {
				sprintf(name + strlen(name), " (%s)",
						pl_in_war->name);
				if (strlen(name) >= 19) {
					strcpy(&name[17], ")");
				}
			}
		}
		sprintf(lblstr, "%c%c %-19s%03d%6d", (pl == best) ? '*'
				: pl->mychar, (pl->team == NULL) ? ' '
				: pl->team->Num + '0', name, pl->life,
				(int32_t) pl->score);
		sprintf(msg, "%2d... %-36s%s@%s\n", i + 1, lblstr,
				pl->realname, Player_is_human(pl) ? ((const char *)(pl->hostname))
						: "xpilot.org");
		if (strlen(msg) + strlen(str) >= max_size)
			break;
		strcat(str, msg);
	}
	free(order);
}

static void Handle_signal(int32_t sig_no)
{
	errno = 0;

	switch (sig_no) {

	case SIGHUP:
		signal(SIGHUP, SIG_IGN);
		return;
	case SIGINT:
		error("Caught SIGINT, terminating.");
		End_game();
		break;
	case SIGTERM:
		error("Caught SIGTERM, terminating.");
		End_game();
		break;

	default:
		error("Caught unkown signal: %d", sig_no);
		End_game();
		break;
	}
	_exit(sig_no); /* just in case */
}

void Log_game(const int8_t *heading)
{
#ifdef LOG
	int8_t str[1024];
	FILE *fp;
	int8_t timenow[81];
	struct tm *ptr;
	time_t lt;

	if (!Log)
	return;

	lt = time(NULL);
	ptr = localtime(&lt);
	strftime(timenow,79,"%I:%M:%S %p %Z %A, %B %d, %Y",ptr);

	sprintf(str,"%-50.50s\t%10.10s@%-15.15s\tWorld: %-25.25s\t%10.10s\n",
			timenow,
			Server.owner,
			Server.host,
			World.name,
			heading);

	if ((fp = fopen(Conf_logfile(), "a")) == NULL) { /* Couldn't open file */
		error("Couldn't open log file, contact %s", Conf_localguru());
		return;
	}

	fputs(str, fp);
	fclose(fp);
#endif
}

void Game_Over(void)
{
	int32_t maxsc, minsc;
	int32_t i, win, loose;
	int8_t msg[128];

	Set_message("Game over...");

	if (BIT(World.rules->mode, TEAM_PLAY)) {
		int32_t teamscore[MAX_TEAMS];
		maxsc = -32767;
		minsc = 32767;
		win = loose = -1;

		for (i = 0; i < MAX_TEAMS; i++) {
			teamscore[i] = 1234567; /* These teams are not used... */
		}
		for (i = 0; i < NumPlayers; i++) {
			int32_t team;
			if (Player_is_human(Players[i])) {
				team = Players[i]->team->Num;
				if (teamscore[team] == 1234567) {
					teamscore[team] = 0;
				}
				teamscore[team] += Players[i]->score;
			}
		}

		for (i = 0; i < MAX_TEAMS; i++) {
			if (teamscore[i] != 1234567) {
				if (teamscore[i] > maxsc) {
					maxsc = teamscore[i];
					win = i;
				}
				if (teamscore[i] < minsc) {
					minsc = teamscore[i];
					loose = i;
				}
			}
		}

		if (win != -1) {
			sprintf(msg, "Best team (%d Pts): Team %d", maxsc, win);
			Set_message(msg);
			xpprintf("%s\n", msg);
		}

		if (loose != -1 && loose != win) {
			sprintf(msg, "Worst team (%d Pts): Team %d", minsc,
					loose);
			Set_message(msg);
			xpprintf("%s\n", msg);
		}
	}

	maxsc = -32767;
	minsc = 32767;
	win = loose = -1;

	for (i = 0; i < NumPlayers; i++) {
		SET_BIT(Players[i]->status, GAME_OVER);
		if (Player_is_human(Players[i])) {
			if (Players[i]->score > maxsc) {
				maxsc = Players[i]->score;
				win = i;
			}
			if (Players[i]->score < minsc) {
				minsc = Players[i]->score;
				loose = i;
			}
		}
	}
	if (win != -1) {
		sprintf(msg, "Best human player: %s", Players[win]->name);
		Set_message(msg);
		xpprintf("%s\n", msg);
	}
	if (loose != -1 && loose != win) {
		sprintf(msg, "Worst human player: %s", Players[loose]->name);
		Set_message(msg);
		xpprintf("%s\n", msg);
	}
	Rank_write_webpage();
	Rank_write_rankfile();
	limitedRoundsGameOver = true;
}

/*
 * Verify that all source files making up this program have been
 * compiled for the same version.  Too often bugs have been reported
 * for incorrectly compiled programs.
 */
static void Check_server_versions(void)
{
	extern int8_t cmdline_version[], collision_version[], error_version[],
			event_version[], frame_version[], id_version[],
			map_version[], math_version[], metaserver_version[],
			net_version[], netserver_version[], option_version[],
			play_version[], player_version[],
			portability_version[], robot_version[],
			rules_version[], /*server_version[],*/ socklib_version[],
			sched_version[], ship_version[], shot_version[],
			update_version[], walls_version[];

	static struct file_version {
		int8_t filename[16];
		int8_t *versionstr;
	} file_versions[] =
		{
			{ "cmdline", cmdline_version },
			{ "collision", collision_version },
			{ "error", error_version },
			{ "event", event_version },
			{ "frame", frame_version },
			{ "id", id_version },
			{ "map", map_version },
			{ "math", math_version },
			{ "metaserver", metaserver_version },
			{ "net", net_version },
			{ "netserver", netserver_version },
			{ "option", option_version },
			{ "play", play_version },
			{ "player", player_version },
			{ "portability", portability_version },
			{ "robot", robot_version },
			{ "rules", rules_version },
			{ "server", server_version },
			{ "socklib", socklib_version },
			{ "sched", sched_version },
			{ "ship", ship_version },
			{ "shot", shot_version },
			{ "update", update_version },
			{ "walls", walls_version }, };
	int32_t i;
	int32_t oops = 0;

	for (i = 0; i < NELEM(file_versions); i++) {
		if (strcmp(VERSION, file_versions[i].versionstr)) {
			oops++;
			error("Source file %s.c (\"%s\") is not compiled "
				"for the current version (\"%s\")!",
					file_versions[i].filename,
					file_versions[i].versionstr, VERSION);
		}
	}
	if (oops) {
		error("%d version inconsistency errors, cannot continue.", oops);
		error("Please recompile this program properly.");
		exit(1);
	}
}

#if defined(PLOCKSERVER) && defined(__linux__)
/*
 * Patches for Linux plock support by Steve Payne <srp20@cam.ac.uk>
 * also added the -pLockServer command line option.
 * All messed up by BG again, with thanks and apologies to Steve.
 */
/* Linux doesn't seem to have plock(2).  *sigh* (BG) */
#if !defined(PROCLOCK) || !defined(UNLOCK)
#define PROCLOCK	0x01
#define UNLOCK		0x00
#endif
static int32_t plock(int32_t op)
{
#if defined(MCL_CURRENT) && defined(MCL_FUTURE)
	return op ? mlockall(MCL_CURRENT | MCL_FUTURE) : munlockall();
#else
	return -1;
#endif
}
#endif

/*
 * Lock the server process data and code segments into memory
 * if this program has been compiled with the PLOCKSERVER flag.
 * Or unlock the server process if the argument is false.
 */
int32_t plock_server(int32_t onoff)
{
#ifdef PLOCKSERVER
	int32_t op;

	if (onoff) {
		op = PROCLOCK;
	}
	else {
		op = UNLOCK;
	}
	if (plock(op) == -1) {
		static int32_t num_plock_errors;
		if (++num_plock_errors <= 3) {
			error("Can't plock(%d)", op);
		}
		return -1;
	}
	return onoff;
#else
	if (onoff) {
		xpprintf(
				"Can't plock: Server was not compiled with plock support\n");
	}
	return 0;
#endif
}
