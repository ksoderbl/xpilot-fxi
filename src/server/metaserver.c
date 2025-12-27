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
#include <time.h>
#include <sys/time.h>

#include "xpconfig.h"
#include "debug.h"
#include "version.h"
#include "const.h"
#include "serverconst.h"
#include "types.h"
#include "global.h"
#include "proto.h"
#include "socklib.h"
#include "map.h"
#include "pack.h"
#include "metaserver.h"
#include "error.h"
#include "netserver.h"
#include "player.h"
#include "player_inline.h"
#include "server.h"

#define META_VERSION	VERSION

char metaserver_version[] = VERSION;

struct MetaServer {
	char name[64];
	char addr[16];
};
struct MetaServer meta_servers[2] = {
		{ META_HOST, META_IP },
		{ META_HOST_TWO, META_IP_TWO },
};

static int32_t Socket = -1;
static char msg[MSG_LEN];

double meta_update_count = 0.0;

extern time_t serverStartTime;


void Meta_send(char *mesg, int32_t len)
{
	int32_t i;

	if (!reportToMetaServer) {
		return;
	}

	for (i = 0; i < NELEM(meta_servers); i++) {
		if (DgramSend(Socket, meta_servers[i].addr, META_PORT, mesg, len) != len) {
			GetSocketError(Socket);
			DgramSend(Socket, meta_servers[i].addr, META_PORT, mesg, len);
		}
	}
}

int32_t Meta_from(char *addr, int32_t port)
{
	int32_t i;

	for (i = 0; i < NELEM(meta_servers); i++) {
		if (!strcmp(addr, meta_servers[i].addr)) {
			return (port == META_PORT);
		}
	}
	return 0;
}

void Meta_gone(void)
{
	if (reportToMetaServer) {
		sprintf(msg, "server %s\nremove", Server.host);
		Meta_send(msg, strlen(msg) + 1);
	}
}

void Meta_init(int32_t fd)
{
	int32_t i;
	char *addr;

	Socket = fd;

	if (!reportToMetaServer) {
		return;
	}

	xpprintf("%s Locating Internet Meta server... ", showtime());
	fflush(stdout);

	for (i = 0; i < NELEM(meta_servers); i++) {
		addr = GetAddrByName(meta_servers[i].name);
		if (addr) {
			strncpy(meta_servers[i].addr, addr,
					sizeof(meta_servers[i].addr));
			meta_servers[i].addr[sizeof(meta_servers[i].addr) - 1]
					= '\0';
		}

		if (addr) {
			xpprintf("found %d", i + 1);
		}
		else {
			xpprintf("%d not found", i + 1);
		}
		if (i + 1 == NELEM(meta_servers)) {
			xpprintf("\n");
		}
		else {
			xpprintf("... ");
		}
		fflush(stdout);
	}
}

void Meta_update(void)
{

#define SOUND_SUPPORT_STR	"no"

	char string[MAX_STR_LEN];
	char status[MAX_STR_LEN];
	int32_t i, j;
	int32_t num_active_players;
	bool first = true;
	const char *game_mode;
	char freebases[120];
	int32_t active_per_team[MAX_TEAMS];
	player_t *pl;

	if (!reportToMetaServer) {
		return;
	}

	Server_info(status, sizeof(status));

	/* Find out the number of active players. */
	num_active_players = 0;
	memset(active_per_team, 0, sizeof active_per_team);
	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		/* Report only human players who are currently in game, dead or waiting */
		if (Player_is_human(pl) && (Player_is_active(pl))) {
			num_active_players++;
			if (BIT(World.rules->mode, TEAM_PLAY)) {
				active_per_team[i]++;
			}
		}
	}

	game_mode = (game_lock) ? "locked" : "ok";

	/* calculate number of available homebases per team. */
	freebases[0] = '\0';
	if (BIT(World.rules->mode, TEAM_PLAY)) {
		j = 0;
		for (i = 0; i < MAX_TEAMS; i++) {
			if (i == robotTeam && reserveRobotTeam) {
				continue;
			}
			if (World.teams[i].NumBases > 0) {
				sprintf(&freebases[j], "%d=%d,", i, World.teams[i].NumBases - active_per_team[i]);
				j += strlen(&freebases[j]);
			}
		}
		/* strip trailing comma. */
		if (j) {
			freebases[j - 1] = '\0';
		}
	}
	else {
		sprintf(freebases, "=%d", World.NumBases - num_active_players - login_in_progress);
	}

	sprintf(string, "add server %s\n"
		"add users %d\n"
		"add version %s\n"
		"add map %s\n"
		"add sizeMap %3dx%3d\n"
		"add author %s\n"
		"add bases %d\n"
		"add fps %d\n"
		"add port %d\n"
		"add mode %s\n"
		"add teams %d\n"
		"add free %s\n"
		"add timing %d\n"
		"add stime %d\n"
		"add queue %d\n"
		"add sound " SOUND_SUPPORT_STR "\n", Server.host,
			num_active_players, META_VERSION, World.name, World.x,
			World.y, World.author, World.NumBases, fps,
			contactPort, game_mode, World.NumTeamBases, freebases,
			0, (int32_t) (time(NULL) - serverStartTime), NumQueuedPlayers);

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if (Player_is_human(pl)
				&& (Player_is_alive(pl) || Player_is_appearing(pl) || Player_is_waiting(pl) || Player_is_dead(pl))) {
			sprintf(string + strlen(string), "%s%s=%s@%s",
					(first) ? "add players " : ",",
					pl->name, pl->realname,
					pl->hostname);
			if (BIT(World.rules->mode, TEAM_PLAY)) {
				sprintf(status, "{%d}", pl->team->Num);
				strcat(string, status);
			}

			first = false;
		}
	}

	strcat(string, "\nadd status ");
	if (strlen(string) + strlen(status) >= sizeof(string)) {
		/* Prevent array overflow */
		strcpy(&status[sizeof(string) - (strlen(string) + 2)], "\n");
	}
	strcat(string, status);

	Meta_send(string, strlen(string) + 1);
}
