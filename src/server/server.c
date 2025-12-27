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

#include "version.h"
#include "commonproto.h"
#include "xpconfig.h"
#include "serverconst.h"
#include "debug.h"
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
#include "frame.h"
#include "robot.h"
#include "update.h"
#include "player_inline.h"
#include "collision.h"
#include "metaserver.h"

char server_version[] = VERSION;

server_t Server;
char ShutdownReason[MAX_CHARS];

/** @brief Number of operators currently logged and authorized
 *
 * This variable is modified only in @ref Cmd_password and @ref Delete_player.
 */
int32_t NumOperators = 0;

static int32_t serverSocket;
#ifdef LOG
static bool Log = true;
#endif
int32_t game_lock = false;
time_t serverStartTime = 0;

extern bool limitedRoundsGameOver;
extern int32_t received_packets;

extern connection_t *Conn;

/*
 * Frame counters and FPS-related global variables
 */
int32_t main_loops = 0;		/* frame counter, needed in events.c */
int32_t main_loops_slow = 0;	/* tick counter, used for marking the time a player fired a shot */
int32_t frameDivisor;
int32_t fps;
int32_t frame_cycle = 0;	/* Intermediate frame counter */
float ticksPerFrame = 1.0;
double gameSpeed = 12.5;
double realTimeStep = 1.0;	/* Game time step between two consecutive real frames */


/*
 * Verify that all source files making up this program have been
 * compiled for the same version.  Too often bugs have been reported
 * for incorrectly compiled programs.
 */
static void Check_server_versions(void)
{
	extern char cmdline_version[], collision_version[], error_version[],
			event_version[], frame_version[], id_version[],
			map_version[], math_version[], metaserver_version[],
			net_version[], netserver_version[], option_version[],
			player_version[],
			portability_version[], robot_version[],
			rules_version[], /*server_version[],*/ socklib_version[],
			sched_version[], ship_version[], shot_version[],
			update_version[];

	static struct file_version {
		char filename[16];
		char *versionstr;
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

	if (!Parser(argc, (char **)argv)) {
		exit(0);
	}

	plock_server(pLockServer); /* Lock the server into memory */
	make_trig_table(); /* Make trigonometric tables */
	Map_compute_base_direction();
	Walls_init();

	/* Allocate memory for players, shots and messages */
	Players_allocate(World.NumBases + MAX_PSEUDO_PLAYERS);
	Shots_allocate(MAX_TOTAL_OBJECTS);
	Collision_cells_allocate();

	Move_init();

	Robots_init();

	if (BIT(World.rules->mode, TEAM_PLAY)) {
		int32_t i;
		for (i = 0; i < World.NumTreasures; i++) {
			if (World.treasures[i].team != NULL) {
				Ball_treasure_add(&World.treasures[i]);
			}
		}
	}

	Rank_init_saved_scores();

	/*
	 * Get server's official name.
	 */
	if (serverHost)
		strlcpy(Server.host, serverHost, sizeof Server.host);
	else
		GetLocalHostName(Server.host, sizeof Server.host,
				(reportToMetaServer != 0 && searchDomainForXPilot != 0));

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
	serverStartTime = time(NULL);

	xpprintf("%s Server runs at %d frames per second, correction factor is %f\n",
			showtime(), fps, 1.0 / frameDivisor);

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
	bool update_meta = false;
	bool update_rank = false;

	gettimeofday(&tv1, NULL);

	main_loops++;

	/* recalculate, if frameDivisor was changed */
	ticksPerFrame = 1.0f / frameDivisor;

	if (frame_cycle == frameDivisor) {
		frame_cycle = 0;
	}

	if (Frame_is_real()) {
		main_loops_slow++;
		insert_measure();

		/* uses mainloops + FPS for timings */
		Input();

		/*
		 * Update the ranking files and the information on meta servers if
		 * anyone enters or leaves the game. This includes also pausing
		 * and unpausing.
		 */
		meta_update_count -= realTimeStep;

		/* Reset teams and time out objects when the last player leaves the game */
		if (num_logouts > 0 || num_pause > 0) {
			if (NumPlayers - NumPaused == 0) {
				Objects_time_out();
				Teams_reset();
			}
		}

		/* Determine whether a meta or rank updates are necessary */
		if (num_logins > 0 || num_logouts > 0 || num_pause > 0 || num_unpause > 0) {
			update_rank = true;
			update_meta = true;

			num_logins = 0;
			num_logouts = 0;
			num_pause = 0;
			num_unpause = 0;
		}

		if (update_rank) {
			Rank_write_webpage();
			Rank_write_rankfile();
			update_rank = false;
		}

		if (update_meta || meta_update_count <= 0.0) {
			Meta_update();
			meta_update_count = META_UPDATE_DELAY_TICKS;
			update_meta = false;
		}
	}

	if (NumPlayers > NumRobots || RawMode) {
		if (Frame_is_real()) {
			if (NumRobots > 0) {
				Update_robots();
			}

			if (fireRepeatRate > 0) {
				Players_shoot();
			}

			Update_fuel_stations();
			Update_mobile_objects();
			Update_players();

			Collision_cells_init();
			Collision_players_check();
			Collision_cells_cleanup();

			Objects_interpolation_init();
			Players_interpolation_init();
		}
		else {
			Update_mobile_objects();
			Update_players();
		}

		/*
		 * Here some players may be in PL_STATE_KILLED state. The following code should perform
		 * a transition to either PL_STATE_APPEARING, PL_STATE_DEAD or PL_STATE_PAUSED.
		 */

		Players_handle_after_kill();

		/*
		 * Here no player should be in PL_STATE_KILLED any more.
		 */

		Update_game_status();

		Objects_remove_timed_out();

		/*
		 * Now update labels if need be.
		 */
		if (updateScores) {
			Players_send_score();
		}

		Frame_update();
	}

	Queue_loop();

	if (Frame_is_real()) {
		Robot_add_remove();

#if 0
		measure_time();
#endif
	}

	gettimeofday(&tv2, NULL);
	mainLoopTime = (timeval_to_seconds(tv2) - timeval_to_seconds(tv1)) * 1e3;

	frame_cycle++;
}

/*
 *  Last function, exit with grace.
 */
void End_game(void)
{
	Players_remove();

	/* Tell meta server that we are gone. */
	Meta_gone();

	Contact_cleanup();

	Players_free();
	Shots_free();
	Map_free();
	Collision_cells_free();
	Log_game("END"); /* Log end */
	exit(0);
}

/*
 * Return status for server
 */
void Server_info(char *str, uint32_t max_size)
{
	int32_t i, j, k;
	player_t *pl_in_war;
	player_t *pl, **order, *best = NULL;
	DFLOAT ratio, best_ratio = -1e7;
	char name[MAX_CHARS];
	char lblstr[MAX_CHARS];
	char msg[MSG_LEN];

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
			ratio = (DFLOAT) pl->score / (pl->pl_life + 1);
		}
		if ((best == NULL || ratio > best_ratio) && !Player_is_paused(pl)) {
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
				: pl->team->Num + '0', name, pl->pl_life,
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

void Log_game(const char *heading)
{
#ifdef LOG
	char str[1024];
	FILE *fp;
	char timenow[81];
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
	char msg[128];

	Message_game_important_print("Game over...");

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
			Message_game_print(msg);
			xpprintf("%s\n", msg);
		}

		if (loose != -1 && loose != win) {
			sprintf(msg, "Worst team (%d Pts): Team %d", minsc,
					loose);
			Message_game_print(msg);
			xpprintf("%s\n", msg);
		}
	}

	maxsc = -32767;
	minsc = 32767;
	win = loose = -1;

	for (i = 0; i < NumPlayers; i++) {
		Player_set_state(Players[i], PL_STATE_WAITING);

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

	updateScores = true;

	if (win != -1) {
		sprintf(msg, "Best human player: %s", Players[win]->name);
		Message_game_print(msg);
		xpprintf("%s\n", msg);
	}
	if (loose != -1 && loose != win) {
		sprintf(msg, "Worst human player: %s", Players[loose]->name);
		Message_game_print(msg);
		xpprintf("%s\n", msg);
	}
	Rank_write_webpage();
	Rank_write_rankfile();
	limitedRoundsGameOver = true;
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
