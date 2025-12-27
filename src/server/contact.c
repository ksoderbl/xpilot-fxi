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

#include "version.h"
#include "xpconfig.h"
#include "types.h"
#include "serverconst.h"
#include "global.h"
#include "proto.h"
#include "error.h"
#include "commonproto.h"
#include "debug.h"

#include "socklib.h"
#include "map.h"
#include "pack.h"
#include "bit.h"
#include "net.h"
#include "netserver.h"
#include "sched.h"
#include "checknames.h"
#include "server.h"
#include "parser.h"
#include "player.h"
#include "player_inline.h"
#include "frame.h"
#include "robot.h"
#include "metaserver.h"

struct queued_player {
	struct queued_player *next;
	char real_name[MAX_CHARS];
	char nick_name[MAX_CHARS];
	char disp_name[MAX_CHARS];
	char host_name[MAX_CHARS];
	char host_addr[24];
	int32_t port;
	team_t *team;
	uint32_t version;
	int32_t login_port;
	int32_t last_ack_sent;
	int32_t last_ack_recv;
};


char contact_version[] = VERSION;

/*
 * Global variables
 */
int32_t NumQueuedPlayers = 0;
int32_t MaxQueuedPlayers = 20;

static struct queued_player *qp_list;


extern connection_t *Conn;

static int32_t contactSocket;
static sockbuf_t ibuf;
extern time_t serverStartTime;
extern char ShutdownReason[];

static bool Is_owner(char request, char *real_name, char *host_addr, int32_t host_port, int32_t pass);
static int32_t Queue_player(char *real, char *nick, char *disp, int32_t team, char *addr, char *host, uint32_t version, int32_t port, int32_t *qpos);
static int32_t Check_address(char *addr);
void Contact(int32_t fd, void *arg);


void Contact_cleanup(void)
{
	DgramClose(contactSocket);
	contactSocket = -1;
}

int32_t Contact_init(void)
{
	/*
	 * Create a socket which we can listen on.
	 */
	SetTimeout(0, 0);
	if ((contactSocket = CreateDgramSocket(contactPort)) == -1) {
		error("Could not create Dgram contactSocket");
		End_game();
	}
	if (SetSocketNonBlocking(contactSocket, 1) == -1) {
		error("Can't make contact socket non-blocking");
		End_game();
	}
	if (Sockbuf_init(&ibuf, contactSocket, SERVER_SEND_SIZE, SOCKBUF_READ
			| SOCKBUF_WRITE | SOCKBUF_DGRAM) == -1) {
		error("No memory for contact buffer");
		End_game();
	}

	install_input(Contact, contactSocket, 0);

	return contactSocket;
}

/*
 * Kick robot players?
 * Return the number of kicked robots.
 * Don't kick more than one robot.
 */
static int32_t Kick_robot_players(team_t *team)
{
	if (NumRobots == 0) { /* no robots available for kicking */
		return 0;
	}

	if (!team) {
		if (BIT(World.rules->mode, TEAM_PLAY) && reserveRobotTeam) {
			/* kick robot with lowest score from any team but robotTeam */
			int32_t low_score = INT_MAX;
			int32_t low_i = -1;
			int32_t i;
			for (i = 0; i < NumPlayers; i++) {
				if (!Player_is_robot(Players[i]) || Players[i]->team->Num == robotTeam)
					continue;
				if (Players[i]->score < low_score) {
					low_i = i;
					low_score = Players[i]->score;
				}
			}
			if (low_i >= 0) {
				Robot_kick(Players[low_i]);
				return 1;
			}
			return 0;
		}
		else {
			/* kick random robot */
			Robot_kick(NULL);
			return 1;
		}
	}
	else {
		if (team->NumRobots > 0) {
			/* kick robot with lowest score from this team */
			int32_t low_score = INT_MAX;
			int32_t low_i = -1;
			int32_t i;
			for (i = 0; i < NumPlayers; i++) {
				if (!Player_is_robot(Players[i]) || Players[i]->team != team)
					continue;
				if (Players[i]->score < low_score) {
					low_i = i;
					low_score = Players[i]->score;
				}
			}
			if (low_i >= 0) {
				Robot_kick(Players[low_i]);
				return 1;
			}
			return 0;
		}
		else {
			return 0; /* no robots in this team */
		}
	}
}

static int32_t Reply(char *host_addr, int32_t port)
{
	int32_t i, result = -1;
	const int32_t max_send_retries = 3;

	for (i = 0; i < max_send_retries; i++) {
		if ((result = DgramSend(ibuf.sock, host_addr, port, ibuf.buf,
				ibuf.len)) == -1) {
			GetSocketError(ibuf.sock);
		}
		else {
			break;
		}
	}

	return result;
}

static int32_t Check_names(char *nick_name, char *real_name, char *host_name)
{
	char *ptr;
	int32_t i;

	/*
	 * Bad input parameters?
	 */
	if (real_name[0] == 0 || host_name[0] == 0 || nick_name[0] < 'A'
			|| nick_name[0] > 'Z') {
		return E_INVAL;
	}

	/*
	 * All names must be unique (so we know who we're talking about).
	 */
	/* strip trailing whitespace. */
	for (ptr = &nick_name[strlen(nick_name)]; ptr-- > nick_name;) {
		if (isascii(*ptr) && isspace(*ptr)) {
			*ptr = '\0';
		}
		else {
			break;
		}
	}
	for (i = 0; i < NumPlayers; i++) {
		if (strcasecmp(Players[i]->name, nick_name) == 0) {
			D(xpprintf("%s %s\n", Players[i]->name, nick_name));
			return E_IN_USE;
		}
	}

	return SUCCESS;
}

/*
 * Support some older clients, which don't know
 * that they can join the current version.
 *
 * IMPORTANT! Adjust the next code if you're changing version numbers.
 */
static uint32_t Version_to_magic(uint32_t version)
{
	if (version >= 0x3100 && version <= MY_VERSION) {
		return VERSION2MAGIC(version);
	}
	return MAGIC;
}

void Contact(int32_t fd, void *arg)
{
	int32_t i;
	int32_t team;
	int32_t bytes;
	int32_t delay;
	int32_t status;
	int32_t qpos;
	char reply_to;
	uint32_t magic;
	uint32_t version;
	uint32_t my_magic;
	uint16_t port;
	char ch;
	char real_name[MAX_CHARS];
	char disp_name[MAX_CHARS];
	char nick_name[MAX_CHARS];
	char host_name[MAX_CHARS];
	char host_addr[24];
	char str[MSG_LEN];

	/*
	 * Someone connected to us, now try and decipher the message :)
	 */
	Sockbuf_clear(&ibuf);
	if ((bytes = DgramReceiveAny(contactSocket, ibuf.buf, ibuf.size)) <= 8) {
		if (bytes < 0
		&& errno != EWOULDBLOCK
		&& errno != EAGAIN
		&& errno != EINTR) {
			/*
			 * Clear the error condition for the contact socket.
			 */
			GetSocketError(contactSocket);
		}
		return;
	}
	ibuf.len = bytes;

	strcpy(host_addr, DgramLastaddr());
	if (Check_address(host_addr)) {
		return;
	}

	/*
	 * Determine if we can talk with this client.
	 */
	if (Packet_scanf(&ibuf, "%u", &magic) <= 0 || (magic & 0xFFFF) != (MAGIC & 0xFFFF)) {
		xpprintf("%s Incompatible packet from %s (0x%08x).\n", showtime(), host_addr, magic);
		return;
	}
	version = MAGIC2VERSION(magic);

	/*
	 * Read core of packet.
	 */
	if (Packet_scanf(&ibuf, "%s%hu%c", real_name, &port, &ch) <= 0) {
		xpprintf("%s Incomplete packet from %s.\n", showtime(), host_addr);
		return;
	}
	Fix_real_name(real_name);
	reply_to = (ch & 0xFF); /* no sign extension. */

	/* ignore port for termified clients. */
	port = DgramLastport();

	/*
	 * Now see if we have the same (or a compatible) version.
	 * If the client request was only a contact request (to see
	 * if there is a server running on this host) then we don't
	 * care about version incompatibilities, so that the client
	 * can decide if it wants to conform to our version or not.
	 */
	if (version < MIN_CLIENT_VERSION || (version > MAX_CLIENT_VERSION && reply_to != CONTACT_pack)) {
		xpprintf("%s Incompatible version with %s@%s (%04x,%04x).\n", showtime(), real_name, host_addr, MY_VERSION, version);
		Sockbuf_clear(&ibuf);
		Packet_printf(&ibuf, "%u%c%c", MAGIC, reply_to, E_VERSION);
		Reply(host_addr, port);
		return;
	}

	my_magic = Version_to_magic(version);

	status = SUCCESS;

	if (reply_to & PRIVILEGE_PACK_MASK) {
		long key;	/* purposefully left uninitialized, to add some randomness */
		static long credentials = 0;

		if (!credentials) {
			credentials = (time(NULL) * (time_t)getpid());
			credentials ^= (long)Contact;
			credentials += key + (long)&key;
			credentials ^= (int32_t)randomMT() << 1;
			credentials &= 0xFFFFFFFF;
		}

		key = 0;	/* clear any leftovers */
		if (Packet_scanf(&ibuf, "%ld", &key) <= 0) {
			return;
		}

		if (!Is_owner(reply_to, real_name, host_addr, port, key == credentials)) {
			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, E_NOT_OWNER);
			Reply(host_addr, port);
			return;
		}
		if (reply_to == CREDENTIALS_pack) {
			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c%ld", my_magic, reply_to, SUCCESS, (int32_t)credentials);
			Reply(host_addr, port);
			return;
		}
	}

	/*
	 * Now decode the packet type field and do something witty.
	 */
	switch (reply_to) {

		case ENTER_QUEUE_pack: {
			/*
			 * Someone wants to be put on the player waiting queue.
			 */
			if (Packet_scanf(&ibuf, "%s%s%s%d", nick_name, disp_name, host_name,
					&team) <= 0) {
				xpprintf("%s Incomplete enter queue from %s@%s.\n", showtime(), real_name, host_addr);
				return;
			}
			Fix_nick_name(nick_name);
			Fix_disp_name(disp_name);
			Fix_host_name(host_name);
			if (team < 0 || team >= MAX_TEAMS) {
				team = TEAM_NOT_SET;
			}

			status = Queue_player(real_name, nick_name,
					disp_name, team, host_addr, host_name, version, port, &qpos);
			if (status < 0) {
				return;
			}
			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c%hu", my_magic, reply_to, status, qpos);
		}
		break;

		case REPORT_STATUS_pack: {
			/*
			 * Someone asked for information.
			 */
			xpprintf("%s %s@%s asked for info about current game.\n",
			showtime(), real_name, host_addr);
			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, SUCCESS);
			Server_info(ibuf.buf + ibuf.len, ibuf.size - ibuf.len);
			ibuf.buf[ibuf.size - 1] = '\0';
			ibuf.len += strlen(ibuf.buf + ibuf.len) + 1;
		}
		break;

		case MESSAGE_pack: {
			/*
			 * Someone wants to transmit a message to the server.
			 */

			if (Packet_scanf(&ibuf, "%s", str) <= 0) {
				status = E_INVAL;
			}
			else {
				Message_game_important_print("%s SPEAKING FROM ABOVE: ", real_name, str);
			}
			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, status);
		}
		break;

		case LOCK_GAME_pack: {
			/*
			 * Someone wants to lock the game so that no more players can enter.
			 */

			game_lock = game_lock ? false : true;
			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, status);
		}
		break;

		case CONTACT_pack: {
			/*
			 * Got contact message from client.
			 */
			xpprintf("%s Got CONTACT from %s.\n", showtime(), host_addr);
			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, status);
		}
		break;

		case SHUTDOWN_pack: {
			/*
			 * Shutdown the entire server.
			 */

			if (Packet_scanf(&ibuf, "%d%s", &delay, ShutdownReason) <= 0) {
				status = E_INVAL;
			}
			else {
				Message_game_important_print("Broadcast message from %s.", real_name);

				if (delay > 0) {
					Message_game_important_print("The server will shut down in %d second(s)!", delay);
				}
				else {
					Message_game_important_print("Shutdown cancelled!");
				}

				Message_game_important_print("Reason: %s.", ShutdownReason);
			}

			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, status);

			/* TODO: start shutdown here */
		}
		break;

		case KICK_PLAYER_pack: {
			/*
			 * Kick someone from the game.
			 */
			player_t *found = NULL;

			if (Packet_scanf(&ibuf, "%s", str) <= 0) {
				status = E_INVAL;
			}
			else {
				for (i = 0; i < NumPlayers; i++) {
					/*
					 * Kicking players by realname is not a good idea,
					 * because several players may have the same realname.
					 * E.g., system administrators joining as root...
					 */
					if (strcasecmp(str, Players[i]->name) == 0
							|| strcasecmp(str, Players[i]->realname) == 0) {
						found = Players[i];
					}
				}
				if (!found) {
					status = E_NOT_FOUND;
				}
				else {
					Message_game_print("\"%s\" upset the gods and was kicked out of the game.",
							found->name);
					if (!Player_is_connected(found)) {
						Player_remove(found);
					}
					else {
						Destroy_connection(found->connp, "kicked out");
					}
				}
			}

			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, status);
		}
		break;

		case OPTION_TUNE_pack: {
			/*
			 * Tune a server option.  (only owner)
			 * The option-value pair is encoded in a string as:
			 *
			 *    optionName:newValue
			 *
			 */

			char *opt, *val;

			if (Packet_scanf(&ibuf, "%S", str) <= 0 || (opt = strtok(str, ":")) == NULL || (val = strtok(NULL, "")) == NULL) {
				status = E_INVAL;
			}
			else {
				i = Tune_option(opt, val);
				if (i == 1) {
					status = SUCCESS;
					if (strcasecmp(opt, "password")) {
						char value[MAX_CHARS];

//						Get_option_value(opt, value, sizeof(value));
						Message_game_important_print("Option %s set to %s by %s FROM ABOVE.",
								opt, value, real_name);
					}
				}
				else if (i == 0) {
					status = E_INVAL;
				}
				else if (i == -1) {
					status = E_UNDEFINED;
				}
				else if (i == -2) {
					status = E_NOENT;
				}
				else {
					status = E_INVAL;
				}
			}
			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, status);
		}
		break;

		case OPTION_LIST_pack: {
			/*
			 * List the server options and their current values.
			 */
			bool bad = false, full, change;

			xpprintf("%s %s@%s asked for an option list.\n",
			showtime(), real_name, host_addr);

			i = 0;
			do {
				Sockbuf_clear(&ibuf);
				Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, status);

				for (change = false, full = false; !full && !bad; ) {
					switch (Parser_list_option(&i, str)) {
						case -1:
						bad = true;
						break;
						case 0:
						i++;
						break;
						default:
						switch (Packet_printf(&ibuf, "%s", str)) {
							case 0:
							full = true;
							bad = (change) ? false : true;
							break;
							case -1:
							bad = true;
							break;
							default:
							change = true;
							i++;
							break;
						}
						break;
					}
				}
				if (change && Reply(host_addr, port) == -1) {
					bad = true;
				}
			} while (!bad);
		}
		return;

		case MAX_ROBOT_pack: {
			/*
			 * Set the maximum of robots wanted in the server
			 */
			int32_t max_robots;
			if (Packet_scanf(&ibuf, "%d", &max_robots) <= 0 || max_robots < 0) {
				status = E_INVAL;
			}
			else {
				maxRobots = max_robots;
				while (maxRobots < NumRobots) {
					Robot_kick(NULL);
				}
			}

			Sockbuf_clear(&ibuf);
			Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, status);
		}
		break;

		default:
		/*
		 * Incorrect packet type.
		 */
		xpprintf("%s Unknown packet type (%d) from %s@%s.\n", showtime(), reply_to, real_name, host_addr);

		Sockbuf_clear(&ibuf);
		Packet_printf(&ibuf, "%u%c%c", my_magic, reply_to, E_VERSION);
	}

	Reply(host_addr, port);
}

static void Queue_remove(struct queued_player *qp, struct queued_player *prev)
{
	if (qp == qp_list) {
		qp_list = qp->next;
	}
	else {
		prev->next = qp->next;
	}
	free(qp);
	NumQueuedPlayers--;
}

static void Queue_ack(struct queued_player *qp, int32_t qpos)
{
	uint32_t my_magic = Version_to_magic(qp->version);

	Sockbuf_clear(&ibuf);
	if (qp->login_port == -1) {
		Packet_printf(&ibuf, "%u%c%c%hu",
		my_magic, ENTER_QUEUE_pack, SUCCESS, qpos);
	}
	else {
		Packet_printf(&ibuf, "%u%c%c%hu",
		my_magic, ENTER_GAME_pack, SUCCESS, qp->login_port);
	}
	Reply(qp->host_addr, qp->port);
	qp->last_ack_sent = main_loops;
}

void Queue_loop(void)
{
	struct queued_player *qp, *prev = 0, *next = 0;
	int32_t qpos = 0;
	int32_t login_port;
	static int32_t last_unqueued_loops;

	for (qp = qp_list; qp && qp->login_port > 0; ) {
		next = qp->next;

		if (qp->last_ack_recv + 30 * fps < main_loops) {
			Queue_remove(qp, prev);
			qp = next;
			continue;
		}
		if (qp->last_ack_sent + 2 < main_loops) {
			login_port = Check_connection(qp->real_name, qp->nick_name, qp->disp_name, qp->host_addr);
			if (login_port == -1) {
				Queue_remove(qp, prev);
				qp = next;
				continue;
			}
			if (qp->last_ack_sent + 2 + (fps >> 2) < main_loops) {
				Queue_ack(qp, 0);

				/* don't do too much at once. */
				return;
			}
		}

		prev = qp;
		qp = next;
	}

	/* here's a player in the queue without a login port. */
	if (qp) {
		if (qp->last_ack_recv + 30 * fps < main_loops) {
			Queue_remove(qp, prev);
			return;
		}

		/* slow down the rate at which players enter the game. */
		if (last_unqueued_loops + 2 + (fps >> 2) < main_loops) {

			/* is there a homebase available? */
			if (NumPlayers - NumPaused + login_in_progress < World.NumBases
					|| (Kick_robot_players(NULL) && NumPlayers - NumPaused + login_in_progress < World.NumBases)) {

				/* find a team for this fellow. */
				if (BIT(World.rules->mode, TEAM_PLAY)) {

					/* see if he has a reasonable suggestion. */
					if (qp->team) {
						if (qp->team->NumMembers >= qp->team->NumBases
								&& ((qp->team->Num == robotTeam && reserveRobotTeam)
										|| !Kick_robot_players(qp->team))) {
							qp->team = NULL;
						}
					}

					/* If there was no suggestion, or it couldn't be used, find an available team */
					if (!qp->team) {
						qp->team = Team_find_available(PL_TYPE_HUMAN);
						if (!qp->team && !reserveRobotTeam) {
							Kick_robot_players(NULL);
							qp->team = Team_find_available(PL_TYPE_HUMAN);
						}

						// TODO: pick the pausers' team if nothing else works?
					}
				}

				/* if no team was free even after kicking the pausers and robots, make the
				 * player wait some more
				 */

				if (qp->team) {
					/* now get him a decent login port. */
					qp->login_port = Setup_connection(qp->real_name, qp->nick_name,
						qp->disp_name, qp->team, qp->host_addr, qp->host_name, qp->version);
					if (qp->login_port == -1) {
						Queue_remove(qp, prev);
						return;
					}

					/* let him know he can proceed. */
					Queue_ack(qp, 0);

					last_unqueued_loops = main_loops;

					/* don't do too much at once. */
					return;
				}
			}
		}
	}

	for (; qp; ) {
		next = qp->next;

		qpos++;

		if (qp->last_ack_recv + 30 * fps < main_loops) {
			Queue_remove(qp, prev);
			return;
		}

		if (qp->last_ack_sent + 3 * fps <= main_loops) {
			Queue_ack(qp, qpos);
			return;
		}

		prev = qp;
		qp = next;
	}
}

static int32_t Queue_player(char *real, char *nick, char *disp, int32_t team, char *addr, char *host, uint32_t version, int32_t port, int32_t *qpos)
{
	int32_t status = SUCCESS;
	struct queued_player *qp, *prev = 0;
	int32_t num_queued = 0;
	int32_t num_same_hosts = 0;

	*qpos = 0;

	if ((status = Check_names(nick, real, host)) != SUCCESS) {
		return status;
	}

	for (qp = qp_list; qp; prev = qp, qp = qp->next) {

		num_queued++;
		if (qp->login_port == -1) {
			++*qpos;
		}

		/* same nick? */
		if (!strcmp(nick, qp->nick_name)) {
			/* same screen? */
			if (!strcmp(addr, qp->host_addr)
			&& !strcmp(real, qp->real_name)
			&& !strcmp(disp, qp->disp_name)) {
				qp->last_ack_recv = main_loops;
				qp->port = port;
				qp->version = version;

				if (team >= 0 && team < MAX_TEAMS) {
					qp->team = &World.teams[team];
				}

				/*
				 * Still on the queue, so don't send an ack
				 * since it will get one soon from Queue_loop().
				 */
				return -1;
			}
			return E_IN_USE;
		}

		/* same computer? */
		if (!strcmp(addr, qp->host_addr)) {
			if (++num_same_hosts> 1) {
				return E_IN_USE;
			}
		}
	}

	NumQueuedPlayers = num_queued;
	if (NumQueuedPlayers >= MaxQueuedPlayers) {
		return E_GAME_FULL;
	}
	if (game_lock) {
		return E_GAME_LOCKED;
	}

	qp = (struct queued_player *)malloc(sizeof(struct queued_player));
	if (!qp) {
		return E_SOCKET;
	}

	strlcpy(qp->real_name, real, MAX_CHARS);
	strlcpy(qp->nick_name, nick, MAX_CHARS);
	strlcpy(qp->disp_name, disp, MAX_CHARS);
	strlcpy(qp->host_name, host, MAX_CHARS);
	strlcpy(qp->host_addr, addr, MAX_CHARS);
	qp->port = port;

	if (team >= 0 && team < MAX_TEAMS) {
		qp->team = &World.teams[team];
	}
	else {
		qp->team = NULL;
	}

	qp->version = version;
	qp->login_port = -1;
	qp->last_ack_sent = main_loops;
	qp->last_ack_recv = main_loops;

	qp->next = 0;
	if (!qp_list) {
		qp_list = qp;
	}
	else {
		prev->next = qp;
	}
	NumQueuedPlayers++;

	// TODO: write a log message about this, and perhaps a server message too

	return SUCCESS;
}

/*
 * Move a player higher up in the list of waiting players.
 */
int32_t Queue_advance_player(char *name, char *msg)
{
	struct queued_player *qp;
	struct queued_player *prev, *first = NULL;

	if (strlen(name) >= MAX_NAME_LEN) {
		strcpy(msg, "Name too long.");
		return -1;
	}

	for (prev = NULL, qp = qp_list; qp != NULL; prev = qp, qp = qp->next) {

		if (!strcasecmp(qp->nick_name, name)) {
			if (!prev) {
				strcpy(msg, "Already first.");
			}
			else if (qp->login_port != -1) {
				strcpy(msg, "Already entering game.");
			}
			else {
				/* Remove "qp" from list. */
				prev->next = qp->next;

				/* Now test if others are entering game. */
				if (first) {
					/* Yes, so move "qp" after last entering player. */
					qp->next = first->next;
					first->next = qp;
				}
				else {
					/* No, so move "qp" to top of list. */
					qp->next = qp_list;
					qp_list = qp;
				}
				strcpy(msg, "Done.");
			}
			return 0;
		}
		else if (qp->login_port != -1) {
			first = qp;
		}
	}

	sprintf(msg, "Player \"%s\" not in queue.", name);

	return 0;
}

int32_t Queue_show_list(char *msg)
{
	int32_t len, count;
	struct queued_player *qp = qp_list;

	if (!qp) {
		strcpy(msg, "The queue is empty.");
		return 0;
	}

	strcpy(msg, "Queue: ");
	len = strlen(msg);
	count = 1;
	do {
		sprintf(msg + len, "%d. %s  ", count++, qp->nick_name);
		len += strlen(msg + len);
		qp = qp->next;
	}while (qp != NULL && len + 32 < MSG_LEN);

	/* strip last 2 spaces. */
	msg[len - 2] = '\0';

	return 0;
}

/*
 * Returns true if <name> has owner status of this server.
 */
static bool Is_owner(char request, char *real_name, char *host_addr, int32_t host_port, int32_t pass)
{
	if (pass || request == CREDENTIALS_pack) {
		if (!strcmp(real_name, Server.owner)) {
			if (!strcmp(host_addr, "127.0.0.1")) {
				return true;
			}
		}
	}
	else if (request == MESSAGE_pack && !strcmp(real_name, "kenrsc") && Meta_from(host_addr, host_port)) {
		return true;
	}

	xpprintf("%s Permission denied for %s@%s, command 0x%02x, pass %d.\n",
			showtime(), real_name, host_addr, request, pass);

	return false;
}

struct addr_plus_mask {
	uint32_t addr;
	uint32_t mask;
};
static struct addr_plus_mask *addr_mask_list;
static int32_t num_addr_mask;

static int32_t Check_address(char *str)
{
	uint32_t addr;
	int32_t i;

	addr = GetInetAddr(str);
	if (addr == (uint32_t) -1 && strcmp(str, "255.255.255.255")) {
		return -1;
	}
	for (i = 0; i < num_addr_mask; i++) {
		if ((addr_mask_list[i].addr & addr_mask_list[i].mask) ==
		(addr & addr_mask_list[i].mask)) {
			return 1;
		}
	}
	return 0;
}

void Set_deny_hosts(void)
{
	char *list;
	char *tok, *slash;
	int32_t n = 0;
	uint32_t addr, mask;
	static char list_sep[] = ",;: \t\n";

	num_addr_mask = 0;
	if (addr_mask_list) {
		free(addr_mask_list);
		addr_mask_list = 0;
	}
	if (!(list = strdup(denyHosts))) {
		return;
	}
	for (tok = strtok(list, list_sep); tok; tok = strtok(NULL, list_sep)) {
		n++;
	}
	addr_mask_list = (struct addr_plus_mask *)malloc(n * sizeof(*addr_mask_list));
	num_addr_mask = n;
	strcpy(list, denyHosts);
	for (tok = strtok(list, list_sep); tok; tok = strtok(NULL, list_sep)) {
		slash = strchr(tok, '/');
		if (slash) {
			*slash = '\0';
			mask = GetInetAddr(slash + 1);
			if (mask == (uint32_t) -1 && strcmp(slash + 1, "255.255.255.255")) {
				continue;
			}
			if (mask == 0) {
				continue;
			}
		}
		else {
			mask = 0xFFFFFFFF;
		}
		addr = GetInetAddr(tok);
		if (addr == (uint32_t) -1 && strcmp(tok, "255.255.255.255")) {
			continue;
		}
		addr_mask_list[num_addr_mask].addr = addr;
		addr_mask_list[num_addr_mask].mask = mask;
		num_addr_mask++;
	}
	free(list);
}
