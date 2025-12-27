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

/*
 * This is the server side of the network connnection stuff.
 *
 * We try very hard to not let the game be disturbed by
 * players logging in.  Therefore a new connection
 * passes through several states before it is actively
 * playing.
 * First we make a new connection structure available
 * with a new socket to listen on.  This socket port
 * number is told to the client via the pack mechanism.
 * In this state the client has to send a packet to this
 * newly created socket with its name and playing parameters.
 * If this succeeds the connection advances to its second state.
 * In this second state the essential server configuration
 * like the map and so on is transmitted to the client.
 * If the client has acknowledged all this data then it
 * advances to the third state, which is the
 * ready-but-not-playing-yet state.  In this state the client
 * has some time to do its final initializations, like mapping
 * its user interface windows and so on.
 * When the client is ready to accept frame updates and process
 * keyboard events then it sends the start-play packet.
 * This play packet advances the connection state into the
 * actively-playing state.  A player structure is allocated and
 * initialized and the other human players are told about this new player.
 * The newly started client is told about the already playing players and
 * play has begun.
 * Apart from these four states there are also two intermediate states.
 * These intermediate states are entered when the previous state
 * has filled the reliable data buffer and the client has not
 * acknowledged all the data yet that is in this reliable data buffer.
 * They are so called output drain states.  Not doing anything else
 * then waiting until the buffer is empty.
 * The difference between these two intermediate states is tricky.
 * The second intermediate state is entered after the
 * ready-but-not-playing-yet state and before the actively-playing state.
 * The difference being that in this second intermediate state the client
 * is already considered an active player by the rest of the server
 * but should not get frame updates yet until it has acknowledged its last
 * reliable data.
 *
 * Communication between the server and the clients is only done
 * using UDP datagrams.  The first client/serverized version of XPilot
 * was using TCP only, but this was too unplayable across the Internet,
 * because TCP is a data stream always sending the next byte.
 * If a packet gets lost then the server has to wait for a
 * timeout before a retransmission can occur.  This is too slow
 * for a real-time program like this game, which is more interested
 * in recent events than in sequenced/reliable events.
 * Therefore UDP is now used which gives more network control to the
 * program.
 * Because some data is considered crucial, like the names of
 * new players and so on, there also had to be a mechanism which
 * enabled reliable data transmission.  Here this is done by creating
 * a data stream which is piggybacked on top of the unreliable data
 * packets.  The client acknowledges this reliable data by sending
 * its byte position in the reliable data stream.  So if the client gets
 * a new reliable data packet and it has not had this data before and
 * there is also no data packet missing inbetween, then it advances
 * its byte position and acknowledges this new position to the server.
 * Otherwise it discards the packet and sends its old byte position
 * to the server meaning that it detected a packet loss.
 * The server maintains an acknowledgement timeout timer for each
 * connection so that it can retransmit a reliable data packet
 * if the acknowledgement timer expires.
 */

/* Dont delete any useless packet routines, or you lose compatibility
 * with the protocol version -pgm
 *
 */

#include "types.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#include "version.h"
#include "xpconfig.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "pack.h"
#include "bit.h"
#include "socklib.h"
#include "sched.h"
#include "net.h"
#include "error.h"
#include "netserver.h"
#include "packet.h"
#include "setup.h"
#include "checknames.h"
#include "server.h"
#include "rank.h"
#include "commonproto.h"
#include "debug.h"
#include "frame.h"
#include "player_inline.h"
#include "robot.h"
#include "metaserver.h"

#define MAX_SELECT_FD			(sizeof(int32_t) * 8 - 1)
#define MAX_RELIABLE_DATA_PACKET_SIZE	1024

char netserver_version[] = VERSION;

connection_t *Conn = NULL;
static int32_t max_connections = 0;
static setup_t *Setup = NULL;
static int32_t (*playing_receive[256])(connection_t *connp);
static int32_t (*login_receive[256])(connection_t *connp);
static int32_t (*drain_receive[256])(connection_t *connp);
int32_t compress_maps = 1;
int32_t login_in_progress;

int32_t num_logins = 0;
int32_t num_logouts = 0;
int32_t num_pause = 0;
int32_t num_unpause = 0;

int32_t received_packets = 0;

static int32_t Compress_map(uint8_t *map, int32_t size);
static int32_t Init_setup(void);
static int32_t Handle_listening(connection_t *connp);
static int32_t Handle_setup(connection_t *connp);
static bool Handle_login(connection_t *connp);
static void Handle_input(int32_t fd, void *arg);

static int32_t Receive_keyboard(connection_t *connp);
static int32_t Receive_quit(connection_t *connp);
static int32_t Receive_play(connection_t *connp);
static int32_t Receive_power(connection_t *connp);
static int32_t Receive_ack(connection_t *connp);
static int32_t Receive_ack_fuel(connection_t *connp);
static int32_t Receive_discard(connection_t *connp);
static int32_t Receive_undefined(connection_t *connp);
static int32_t Receive_talk(connection_t *connp);
static int32_t Receive_display(connection_t *connp);
static int32_t Receive_modifier_bank(connection_t *connp);
static int32_t Receive_motd(connection_t *connp);
static int32_t Receive_shape(connection_t *connp);
static int32_t Receive_pointer_move(connection_t *connp);
static int32_t Receive_audio_request(connection_t *connp);
static int32_t Receive_fps_request(connection_t *connp);

/*
 * Compress the map data using a simple Run Length Encoding algorithm.
 * If there is more than one consecutive byte with the same type
 * then we set the high bit of the byte and then the next byte
 * gives the number of repetitions.
 * This works well for most maps which have lots of series of the
 * same map object and is simple enough to got implemented quickly.
 */
static int32_t Compress_map(uint8_t *map, int32_t size)
{
	int32_t i, j, k;

	for (i = j = 0; i < size; i++, j++) {
		if (i + 1 < size && map[i] == map[i + 1]) {
			for (k = 2; i + k < size; k++) {
				if (map[i] != map[i + k]) {
					break;
				}
				if (k == 255) {
					break;
				}
			}
			map[j] = (map[i] | SETUP_COMPRESSED);
			map[++j] = k;
			i += k - 1;
		}
		else {
			map[j] = map[i];
		}
	}
	return j;
}

/*
 * Initialize the structure that gives the client information
 * about our setup.  Like the map and playing rules.
 * We only setup this structure once to save time when new
 * players log in during play.
 */
static int32_t Init_setup(void)
{
	int32_t x, y, team, type, size, treasure = 0, base = 0;
	uint8_t *mapdata, *mapptr;

	if ((mapdata = (uint8_t *) malloc(World.x * World.y)) == NULL) {
		error("No memory for mapdata");
		return -1;
	}
	memset(mapdata, SETUP_SPACE, World.x * World.y);
	mapptr = mapdata;
	errno = 0;
	for (x = 0; x < World.x; x++) {
		for (y = 0; y < World.y; y++, mapptr++) {
			type = World.block[x][y];

			switch (type) {
			case SPACE:
				*mapptr = SETUP_SPACE;
				break;
			case FILLED:
				*mapptr = SETUP_FILLED;
				break;
			case REC_RU:
				*mapptr = SETUP_REC_RU;
				break;
			case REC_RD:
				*mapptr = SETUP_REC_RD;
				break;
			case REC_LU:
				*mapptr = SETUP_REC_LU;
				break;
			case REC_LD:
				*mapptr = SETUP_REC_LD;
				break;
			case FUEL:
				*mapptr = SETUP_FUEL;
				break;

			case TREASURE:
				*mapptr = SETUP_TREASURE + World.treasures[treasure++].team->Num;
				break;
			case BASE:
				if (World.base[base].team == NULL) {
					team = 0;
				}
				else {
					team = World.base[base].team->Num;
				}
				switch (World.base[base++].dir) {
				case DIR_UP:
					*mapptr = SETUP_BASE_UP + team;
					break;
				case DIR_RIGHT:
					*mapptr = SETUP_BASE_RIGHT + team;
					break;
				case DIR_DOWN:
					*mapptr = SETUP_BASE_DOWN + team;
					break;
				case DIR_LEFT:
					*mapptr = SETUP_BASE_LEFT + team;
					break;
				default:
					error("Bad base at (%d,%d).", x, y);
					free(mapdata);
					return -1;
				}
				break;
			default:
				error("Unknown map type (%d) at (%d,%d).", type, x, y);
				*mapptr = SETUP_SPACE;
				break;
			}
		}
	}
	if (compress_maps == 0) {
		type = SETUP_MAP_UNCOMPRESSED;
		size = World.x * World.y;
	}
	else {
		type = SETUP_MAP_ORDER_XY;
		size = Compress_map(mapdata, World.x * World.y);
		if (size <= 0 || size > World.x * World.y) {
			errno = 0;
			error("Map compression error (%d)", size);
			free(mapdata);
			return -1;
		}
		if ((mapdata = (uint8_t *) realloc(mapdata, size)) == NULL) {
			error("Cannot reallocate mapdata");
			return -1;
		}
	}

	if (type != SETUP_MAP_UNCOMPRESSED) {
		xpprintf("%s Map compression ratio is %-4.2f%%\n", showtime(),
				100.0 * size / (World.x * World.y));
	}

	if ((Setup = (setup_t *) malloc(sizeof(setup_t) + size)) == NULL) {
		error("No memory to hold setup");
		free(mapdata);
		return -1;
	}
	memset(Setup, 0, sizeof(setup_t) + size);
	memcpy(Setup->map_data, mapdata, size);
	free(mapdata);
	Setup->setup_size = ((char *) &Setup->map_data[0] - (char *) Setup) + size;
	Setup->map_data_len = size;
	Setup->map_order = type;
	Setup->frames_per_second = fps;
	Setup->lives = World.rules->lives;
	Setup->mode = World.rules->mode;
	Setup->x = World.x;
	Setup->y = World.y;
	strncpy(Setup->name, World.name, sizeof(Setup->name) - 1);
	Setup->name[sizeof(Setup->name) - 1] = '\0';
	strncpy(Setup->author, World.author, sizeof(Setup->author) - 1);
	Setup->author[sizeof(Setup->author) - 1] = '\0';

	return 0;
}

/*
 * Initialize the function dispatch tables for the various client
 * connection states.  Some states use the same table.
 */
static void Init_receive(void)
{
	int32_t i;

	for (i = 0; i < 256; i++) {
		login_receive[i] = Receive_undefined;
		playing_receive[i] = Receive_undefined;
		drain_receive[i] = Receive_undefined;
	}

	drain_receive[PKT_QUIT] = Receive_quit;
	drain_receive[PKT_ACK] = Receive_ack;
	drain_receive[PKT_VERIFY] = Receive_discard;
	drain_receive[PKT_PLAY] = Receive_discard;
	drain_receive[PKT_SHAPE] = Receive_discard;

	login_receive[PKT_PLAY] = Receive_play;
	login_receive[PKT_QUIT] = Receive_quit;
	login_receive[PKT_ACK] = Receive_ack;
	login_receive[PKT_VERIFY] = Receive_discard;
	login_receive[PKT_POWER] = Receive_power;
	login_receive[PKT_POWER_S] = Receive_power;
	login_receive[PKT_TURNSPEED] = Receive_power;
	login_receive[PKT_TURNSPEED_S] = Receive_power;
	login_receive[PKT_TURNRESISTANCE] = Receive_power;
	login_receive[PKT_TURNRESISTANCE_S] = Receive_power;
	login_receive[PKT_DISPLAY] = Receive_display;
	login_receive[PKT_MODIFIERBANK] = Receive_modifier_bank;
	login_receive[PKT_MOTD] = Receive_motd;
	login_receive[PKT_SHAPE] = Receive_shape;
	login_receive[PKT_REQUEST_AUDIO] = Receive_audio_request;
	login_receive[PKT_ASYNC_FPS] = Receive_fps_request;

	playing_receive[PKT_ACK] = Receive_ack;
	playing_receive[PKT_VERIFY] = Receive_discard;
	playing_receive[PKT_PLAY] = Receive_play;
	playing_receive[PKT_QUIT] = Receive_quit;
	playing_receive[PKT_KEYBOARD] = Receive_keyboard;
	playing_receive[PKT_POWER] = Receive_power;
	playing_receive[PKT_POWER_S] = Receive_power;
	playing_receive[PKT_TURNSPEED] = Receive_power;
	playing_receive[PKT_TURNSPEED_S] = Receive_power;
	playing_receive[PKT_TURNRESISTANCE] = Receive_power;
	playing_receive[PKT_TURNRESISTANCE_S] = Receive_power;
	playing_receive[PKT_ACK_FUEL] = Receive_ack_fuel;
	playing_receive[PKT_TALK] = Receive_talk;
	playing_receive[PKT_DISPLAY] = Receive_display;
	playing_receive[PKT_MODIFIERBANK] = Receive_modifier_bank;
	playing_receive[PKT_MOTD] = Receive_motd;
	playing_receive[PKT_SHAPE] = Receive_shape;
	playing_receive[PKT_POINTER_MOVE] = Receive_pointer_move;
	playing_receive[PKT_REQUEST_AUDIO] = Receive_audio_request;
	playing_receive[PKT_ASYNC_FPS] = Receive_fps_request;
}

/*
 * Initialize the connection structures.
 */
int32_t Setup_net_server(void)
{
	size_t size;

	Init_receive();

	if (Init_setup() == -1) {
		return -1;
	}
	/*
	 * The number of connections is limited by the number of bases
	 * and the max number of possible file descriptors to use in
	 * the select(2) call minus those for stdin, stdout, stderr,
	 * the contact socket, and the socket for the resolver library routines.
	 */
	max_connections = MIN(MAX_SELECT_FD - 5, World.NumBases + MAX_PAUSED_PLAYERS);
	size = max_connections * sizeof(*Conn);
	if ((Conn = (connection_t *) malloc(size)) == NULL) {
		error("Cannot allocate memory for connections");
		return -1;
	}
	memset(Conn, 0, size);

	return 0;
}

static void Conn_set_state(connection_t *connp, int32_t state, int32_t drain_state)
{
	static int32_t num_conn_busy;
	static int32_t num_conn_playing;

	if ((connp->state & (CONN_PLAYING | CONN_READY)) != 0) {
		num_conn_playing--;
	}
	else if (connp->state == CONN_FREE) {
		num_conn_busy++;
	}

	connp->state = state;
	connp->drain_state = drain_state;

	connp->start = main_loops;

	if (connp->state == CONN_PLAYING) {
		num_conn_playing++;
		connp->timeout = IDLE_TIMEOUT;
	}
	else if (connp->state == CONN_READY) {
		num_conn_playing++;
		connp->timeout = READY_TIMEOUT;
	}
	else if (connp->state == CONN_LOGIN) {
		connp->timeout = LOGIN_TIMEOUT;
	}
	else if (connp->state == CONN_SETUP) {
		connp->timeout = SETUP_TIMEOUT;
	}
	else if (connp->state == CONN_LISTENING) {
		connp->timeout = LISTEN_TIMEOUT;
	}
	else if (connp->state == CONN_FREE) {
		num_conn_busy--;
		connp->timeout = IDLE_TIMEOUT;
	}

	login_in_progress = num_conn_busy - num_conn_playing;
}

/*
 * Cleanup a connection.  The client may not know yet that
 * it is thrown out of the game so we send it a quit packet.
 * We send it twice because of UDP it could get lost.
 * Since 3.0.6 the client receives a short message
 * explaining why the connection was terminated.
 */
void Destroy_connection(connection_t *connp, const char *reason)
{
	player_t *pl;
	int32_t len;
	int32_t sock;
	char pkt[MAX_CHARS];

	if (connp->state == CONN_FREE) {
		errno = 0;
		error("Cannot destroy empty connection (\"%s\")", reason);
		return;
	}

	sock = connp->w.sock;
	remove_input(sock);

	strncpy(&pkt[1], reason, sizeof(pkt) - 2);
	pkt[sizeof(pkt) - 1] = '\0';
	pkt[0] = PKT_QUIT;
	len = strlen(pkt) + 1;
	if (DgramWrite(sock, pkt, len) != len) {
		GetSocketError(sock);
		DgramWrite(sock, pkt, len);
	}

	xpprintf("%s Goodbye %s=%s@%s|%s (\"%s\")\n", showtime(),
			connp->nick ? ((const char *)(connp->nick)) : "",
			connp->real ? ((const char *)(connp->real)) : "",
			connp->host ? ((const char *)(connp->host)) : "", ((const char *)(connp->dpy)) ? ((const char *)(connp->dpy))
					: "", reason);

	Conn_set_state(connp, CONN_FREE, CONN_FREE);

	if ((pl = Connection_is_occupied(connp))) {
		connp->pl = NULL;
		pl->connp = NULL;
		Player_remove(pl);
	}
	if (connp->real != NULL) {
		free(connp->real);
	}
	if (connp->nick != NULL) {
		free(connp->nick);
	}
	if (connp->dpy != NULL) {
		free(connp->dpy);
	}
	if (connp->addr != NULL) {
		free(connp->addr);
	}
	if (connp->host != NULL) {
		free(connp->host);
	}
	Sockbuf_cleanup(&connp->w);
	Sockbuf_cleanup(&connp->r);
	Sockbuf_cleanup(&connp->c);
	memset(connp, 0, sizeof(*connp));

	num_logouts++;

	if (DgramWrite(sock, pkt, len) != len) {
		GetSocketError(sock);
		DgramWrite(sock, pkt, len);
	}
	DgramClose(sock);
}

int32_t Check_connection(char *real, char *nick, char *dpy, char *addr)
{
	int32_t i;
	connection_t *connp;

	for (i = 0; i < max_connections; i++) {
		connp = &Conn[i];
		if (connp->state == CONN_LISTENING) {
			if (strcasecmp(connp->nick, nick) == 0) {
				if (!strcmp(real, connp->real) && !strcmp(dpy,
						connp->dpy) && !strcmp(addr,
						connp->addr)) {
					return connp->my_port;
				}
				return -1;
			}
		}
	}
	return -1;
}

int32_t Setup_connection(char *real, char *nick, char *dpy, team_t *team, char *addr,
		char *host, uint32_t version)
{
	int32_t i, free_conn_index = max_connections, my_port, sock;
	connection_t *connp;

	ASSERT(real && nick && dpy && team && addr && host)

	for (i = 0; i < max_connections; i++) {
		connp = &Conn[i];
		if (connp->state == CONN_FREE) {
			if (free_conn_index == max_connections) {
				free_conn_index = i;
			}
			continue;
		}
		if (strcasecmp(connp->nick, nick) == 0) {
			if (connp->state == CONN_LISTENING && strcmp(real,
					connp->real) == 0 && strcmp(dpy,
					connp->dpy) == 0 && version
					== connp->version) {
				/*
				 * May happen for multi-homed hosts
				 * and if previous packet got lost.
				 */
				return connp->my_port;
			}
			else {
				/*
				 * Nick already in use.
				 */
				return -1;
			}
		}
	}

	if (free_conn_index >= max_connections) {
		xpprintf("%s Full house for %s(%s)@%s(%s)\n", showtime(), real,
				nick, host, dpy);
		return -1;
	}
	connp = &Conn[free_conn_index];

	if (clientPortStart && (!clientPortEnd || clientPortEnd > 65535)) {
		clientPortEnd = 65535;
	}
	if (clientPortEnd && (!clientPortStart || clientPortStart < 1024)) {
		clientPortStart = 1024;
	}

	if (!clientPortStart || !clientPortEnd || (clientPortStart
			> clientPortEnd)) {
		if ((sock = CreateDgramSocket(0)) == -1) {
			error("Cannot create datagram socket (%d)", sl_errno);
			return -1;
		}
	}
	else {
		int32_t found_socket = 0;
		for (i = clientPortStart; i <= clientPortEnd; i++) {
			if ((sock = CreateDgramSocket(i)) != -1) {
				found_socket = 1;
				break;
			}
		}
		if (found_socket == 0) {
			error(
					"Could not find a usable port in given port range");
			return -1;
		}
	}

	if ((my_port = GetPortNum(sock)) == -1) {
		error("Cannot get port from socket");
		DgramClose(sock);
		return -1;
	}
	if (SetSocketNonBlocking(sock, 1) == -1) {
		error("Cannot make client socket non-blocking");
		DgramClose(sock);
		return -1;
	}
	if (SetSocketReceiveBufferSize(sock, SERVER_RECV_SIZE + 256) == -1) {
		error("Cannot set receive buffer size to %d", SERVER_RECV_SIZE
				+ 256);
	}
	if (SetSocketSendBufferSize(sock, SERVER_SEND_SIZE + 256) == -1) {
		error("Cannot set send buffer size to %d", SERVER_SEND_SIZE
				+ 256);
	}

	Sockbuf_init(&connp->w, sock, SERVER_SEND_SIZE, SOCKBUF_WRITE
			| SOCKBUF_DGRAM);

	Sockbuf_init(&connp->r, sock, SERVER_RECV_SIZE, SOCKBUF_READ
			| SOCKBUF_DGRAM);

	Sockbuf_init(&connp->c, -1, MAX_SOCKBUF_SIZE, SOCKBUF_WRITE
			| SOCKBUF_READ | SOCKBUF_LOCK);

	connp->my_port = my_port;
	connp->real = strdup(real);
	connp->nick = strdup(nick);
	connp->dpy = strdup(dpy);
	connp->addr = strdup(addr);
	connp->host = strdup(host);
	connp->ship = NULL;
	connp->team = team;
	connp->version = version;
	connp->start = main_loops;
	connp->magic = rand() + my_port + sock + team->Num + main_loops;
	connp->pl = NULL;
	connp->timeout = LISTEN_TIMEOUT;
	connp->last_key_change = 0;
	connp->reliable_offset = 0;
	connp->reliable_unsent = 0;
	connp->last_send_loops = 0;
	connp->retransmit_at_loop = 0;
	connp->rtt_retransmit = DEFAULT_RETRANSMIT;
	connp->rtt_smoothed = 0;
	connp->rtt_dev = 0;
	connp->rtt_timeouts = 0;
	connp->acks = 0;
	connp->setup = 0;
	connp->view_width = DEF_VIEW_SIZE;
	connp->view_height = DEF_VIEW_SIZE;
	connp->debris_colors = 0;
	connp->spark_rand = DEF_SPARK_RAND;

	connp->cid = free_conn_index;

	Conn_set_state(connp, CONN_LISTENING, CONN_FREE);
	if (connp->w.buf == NULL || connp->r.buf == NULL || connp->c.buf
			== NULL || connp->real == NULL || connp->nick == NULL
			|| connp->dpy == NULL || connp->addr == NULL
			|| connp->host == NULL) {
		error("Not enough memory for connection");
		/* socket is not yet connected, but it doesn't matter much. */
		Destroy_connection(&Conn[free_conn_index], "no memory");
		return -1;
	}

	install_input(Handle_input, sock, (void *) connp);

	return my_port;
}

/*
 * Handle a connection that is in the listening state.
 */
static int32_t Handle_listening(connection_t *connp)
{
	uint8_t type;
	int32_t n;
	char nick[MAX_CHARS], real[MAX_CHARS];

	if (connp->state != CONN_LISTENING) {
		Destroy_connection(connp, "not listening");
		return -1;
	}
	Sockbuf_clear(&connp->r);
	errno = 0;
	n = DgramReceiveAny(connp->r.sock, connp->r.buf, connp->r.size);
	if (n <= 0) {
		if (n == 0 || errno == EWOULDBLOCK || errno == EAGAIN) {
			n = 0;
		}
		else if (n != 0) {
			Destroy_connection(connp, "read first packet error");
		}
		return n;
	}
	connp->r.len = n;
	connp->his_port = DgramLastport();
	if (DgramConnect(connp->w.sock, connp->addr, connp->his_port) == -1) {
		error("Cannot connect datagram socket (%s,%d,%d)", connp->addr,
				connp->his_port, sl_errno);
		if (GetSocketError(connp->w.sock)) {
			error("GetSocketError fails too, giving up");
			Destroy_connection(connp, "connect error");
			return -1;
		}
		errno = 0;
		if (DgramConnect(connp->w.sock, connp->addr, connp->his_port)
				== -1) {
			error(
					"Still cannot connect datagram socket (%s,%d,%d)",
					connp->addr, connp->his_port, sl_errno);
			Destroy_connection(connp, "connect error");
			return -1;
		}
	}

	xpprintf("%s Welcome %s=%s@%s|%s (%s/%d)", showtime(), connp->nick,
			connp->real, connp->host, connp->dpy, connp->addr,
			connp->his_port);
	if (connp->version != MY_VERSION)
		xpprintf(" (version %04x)\n", connp->version);
	else
		xpprintf("\n");

	if (connp->r.ptr[0] != PKT_VERIFY) {
		Send_reply(connp, PKT_VERIFY, PKT_FAILURE);
		Send_reliable(connp);
		Destroy_connection(connp, "not connecting");
		return -1;
	}
	if ((n = Packet_scanf(&connp->r, "%c%s%s", &type, real, nick)) <= 0) {
		Send_reply(connp, PKT_VERIFY, PKT_FAILURE);
		Send_reliable(connp);
		Destroy_connection(connp, "verify incomplete");
		return -1;
	}
	Fix_real_name(real);
	Fix_nick_name(nick);
	if (strcmp(real, connp->real)) {
		xpprintf("%s Client verified incorrectly (%s,%s)(%s,%s)\n",
				showtime(), real, nick, connp->real,
				connp->nick);
		Send_reply(connp, PKT_VERIFY, PKT_FAILURE);
		Send_reliable(connp);
		Destroy_connection(connp, "verify incorrect");
		return -1;
	}
	Sockbuf_clear(&connp->w);
	if (Send_reply(connp, PKT_VERIFY, PKT_SUCCESS) == -1 || Packet_printf(
			&connp->c, "%c%u", PKT_MAGIC, connp->magic) <= 0
			|| Send_reliable(connp) <= 0) {
		Destroy_connection(connp, "confirm failed");
		return -1;
	}

	Conn_set_state(connp, CONN_DRAIN, CONN_SETUP);

	return 1; /* success! */
}

/*
 * Handle a connection that is in the transmit-server-configuration-data state.
 */
static int32_t Handle_setup(connection_t *connp)
{
	char *buf;
	int32_t n, len;

	if (connp->state != CONN_SETUP) {
		Destroy_connection(connp, "not setup");
		return -1;
	}

	if (connp->setup == 0) {
		n = Packet_printf(&connp->c,
				"%ld" "%ld%hd" "%hd%hd" "%hd%hd" "%s%s",
				Setup->map_data_len, Setup->mode, Setup->lives,
				Setup->x, Setup->y, (int16_t) fps,
				Setup->map_order, Setup->name, Setup->author);
		if (n <= 0) {
			Destroy_connection(connp, "setup 0 write error");
			return -1;
		}
		connp->setup = (char *) &Setup->map_data[0] - (char *) Setup;
	}
	else if (connp->setup < Setup->setup_size) {
		if (connp->c.len > 0) {
			/* If there is still unacked reliable data test for acks. */
			Handle_input(-1, (void *) connp);
			if (connp->state == CONN_FREE) {
				return -1;
			}
		}
	}
	if (connp->setup < Setup->setup_size) {
		len = MIN(connp->c.size, 4096) - connp->c.len;
		if (len <= 0) {
			/* Wait for acknowledgement of previously transmitted data. */
			return 0;
		}
		if (len > Setup->setup_size - connp->setup) {
			len = Setup->setup_size - connp->setup;
		}
		buf = (char *) Setup;
		if (Sockbuf_write(&connp->c, &buf[connp->setup], len) != len) {
			Destroy_connection(connp, "sockbuf write setup error");
			return -1;
		}
		connp->setup += len;
		if (len >= 512) {
			connp->start += (len * fps) / (8 * 512) + 1;
		}
	}
	if (connp->setup >= Setup->setup_size) {
		Conn_set_state(connp, CONN_DRAIN, CONN_LOGIN);
	}

	return 0;
}

/*
 * A client has requested to start active play.
 * See if we can allocate a player structure for it
 * and if this succeeds update the player information
 * to all connected players.
 */
static bool Handle_login(connection_t *connp)
{
	player_t *pl;
	bool result = false;
	int32_t i, conn_bit;
	player_state_t entry_state;

	if (NumPlayers >= World.NumBases + MAX_PAUSED_PLAYERS) {
		errno = 0;
		error("Not enough bases for players");
		return false;
	}

	/* Pick the team */
	if (BIT(World.rules->mode, TEAM_PLAY)) {
		if (connp->team == NULL) {
			connp->team = Team_find_available(PL_TYPE_HUMAN);

			if (connp->team == NULL) {
				errno = 0;
				error("Can't pick team");
				return false;
			}
		}
	}
	else {
		connp->team = NULL;
	}

	/* Pick the nick name */
	for (i = 0; i < NumPlayers; i++) {
		if (strcasecmp(Players[i]->name, connp->nick) == 0) {
			errno = 0;
			error("Name already in use %s", connp->nick);
			return false;
		}
	}

	// Reserve the player structure
	if (!(pl = Player_add())) {
		return false;
	}

	/**************************************************************
	 * If an error occurs later on, the code should handle it by
	 * jumping to the \ref handle_result label, not by returning
	 * directly.
	 **************************************************************/

	pl->pl_type = PL_TYPE_HUMAN;

	if (allowShipShapes == true && connp->ship) {
		pl->ship = connp->ship;
	}
	else {
		pl->ship = Triangle_ship();
	}

	strcpy(pl->name, connp->nick);
	strcpy(pl->realname, connp->real);
	strcpy(pl->hostname, connp->host);

	if (connp->team) {
		Player_add_to_team(pl, connp->team);
	}
	pl->version = connp->version;

	if (!(pl->home_base = Team_pick_free_base(pl->team))) {
		error("Couldn't find a suitable base for player %s on team %d", pl->name, pl->team->Num);
		goto handle_result;
	}

	Player_go_home(pl);

	Rank_get_saved_score(pl);

	entry_state = Player_compute_entry_state(pl, pl->team);
	Player_set_state(pl, entry_state);

	connp->pl = pl;
	pl->connp = connp;
	memset(pl->last_keyv, 0, sizeof(pl->last_keyv));
	memset(pl->prev_keyv, 0, sizeof(pl->prev_keyv));

	Conn_set_state(connp, CONN_READY, CONN_PLAYING);

	if (Send_reply(connp, PKT_PLAY, PKT_SUCCESS) <= 0) {
		error("Cannot send play reply");
		goto handle_result;
	}

	xpprintf("%s %s (%d) starts at startpos %d.\n", showtime(), pl->name,
			NumPlayers, pl->home_base->id);

	Send_info_about_myself(pl, PL_SEND_GENERAL | PL_SEND_SCORE | PL_SEND_BASE);
	Send_info_about_others(pl, PL_SEND_GENERAL | PL_SEND_SCORE | PL_SEND_BASE);
	Send_info_about_player(pl, PL_SEND_GENERAL | PL_SEND_SCORE | PL_SEND_BASE);

	if (NumPlayers == 1) {
		Message_game_print("Welcome to \"%s\", made by %s.", World.name, World.author);
	}
	else if (BIT(World.rules->mode, TEAM_PLAY)) {
		Message_game_print("%s (%s, team %d) has entered \"%s\", made by %s.",
				pl->name, pl->realname, pl->team->Num, World.name,
				World.author);
	}
	else {
		Message_game_print("%s (%s) has entered \"%s\", made by %s.",
				pl->name, pl->realname, World.name, World.author);
	}

	if (greeting) {
		Message_player_print(pl, PL_MSG_GREETING, "%s", greeting);
	}

	conn_bit = (1 << connp->cid);
	for (i = 0; i < World.NumFuels; i++) {
		/*
		 * The client assumes at startup that all fuelstations are filled.
		 */
		if (World.fuel[i].fuel == MAX_STATION_FUEL) {
			SET_BIT(World.fuel[i].conn_mask, conn_bit);
		}
		else {
			CLR_BIT(World.fuel[i].conn_mask, conn_bit);
		}
	}

	num_logins++;

	result = true;

	/* Handle exceptions here */
	handle_result:

	if (!result) {
		Destroy_connection(pl->connp, "error initializing the player");
	}

	updateScores = true;

	return result;
}

/*
 * Process a client packet.
 * The client may be in one of several states,
 * therefore we use function dispatch tables for easy processing.
 * Some functions may process requests from clients being
 * in different states.
 */
static void Handle_input(int32_t fd, void *arg)
{
	connection_t *connp = (connection_t *) arg;
	int32_t type, result, (**receive_tbl)(connection_t *connp);

	if (connp->state & (CONN_PLAYING | CONN_READY)) {
		receive_tbl = &playing_receive[0];
	}
	else if (connp->state == CONN_LOGIN) {
		receive_tbl = &login_receive[0];
	}
	else if (connp->state & (CONN_DRAIN | CONN_SETUP)) {
		receive_tbl = &drain_receive[0];
	}
	else if (connp->state == CONN_LISTENING) {
		Handle_listening(connp);
		return;
	}
	else {
		if (connp->state != CONN_FREE) {
			Destroy_connection(connp, "not input");
		}
		return;
	}
	connp->num_keyboard_updates = 0;

	Sockbuf_clear(&connp->r);
	if (Sockbuf_read(&connp->r) == -1) {
		Destroy_connection(connp, "input error");
		return;
	}
	if (connp->r.len <= 0) {
		/*
		 * No input.
		 */
		return;
	}
	while (connp->r.ptr < connp->r.buf + connp->r.len) {
		type = (connp->r.ptr[0] & 0xFF);
		result = (*receive_tbl[type])(connp);
		if (result == -1) {
			/*
			 * Unrecoverable error.
			 * Connection has been destroyed.
			 */
			return;
		}
		if (result == 0) {
			/*
			 * Incomplete client packet.
			 * Drop rest of packet.
			 */
			Sockbuf_clear(&connp->r);
			break;
		}
		if (connp->state == CONN_PLAYING) {
			connp->start = main_loops;
		}
	}
}

int32_t Input(void)
{
	int32_t i, ind, num_reliable = 0, input_reliable[MAX_SELECT_FD];
	connection_t *connp;

	for (i = 0; i < max_connections; i++) {
		connp = &Conn[i];
		if (connp->state == CONN_FREE) {
			continue;
		}
		if (connp->start + connp->timeout * fps < main_loops) {
			char msg[MSG_LEN];

			/*
			 * Timeout this fellow if we have not heard a single thing
			 * from him for a long time.
			 */
			if (connp->state & (CONN_PLAYING | CONN_READY)) {
				Message_game_print("%s mysteriously disappeared!?", connp->nick);
			}

			sprintf(msg, "timeout %02x", connp->state);
			Destroy_connection(connp, msg);
			continue;
		}
		if (connp->state != CONN_PLAYING) {
			input_reliable[num_reliable++] = i;
			if (connp->state == CONN_SETUP) {
				Handle_setup(connp);
				continue;
			}
		}
	}

	for (i = 0; i < num_reliable; i++) {
		ind = input_reliable[i];
		connp = &Conn[ind];
		if (connp->state & (CONN_DRAIN | CONN_READY | CONN_SETUP
				| CONN_LOGIN)) {
			if (connp->c.len > 0) {
				if (Send_reliable(connp) == -1) {
					continue;
				}
			}
		}
	}

	return login_in_progress;
}

/*
 * Send a reply to a special client request.
 * Not used consistently everywhere.
 * It could be used to setup some form of reliable
 * communication from the client to the server.
 */
int32_t Send_reply(connection_t *connp, int32_t replyto, int32_t result)
{
	int32_t n;

	n = Packet_printf(&connp->c, "%c%c%c", PKT_REPLY, replyto, result);
	if (n == -1) {
		Destroy_connection(connp, "write error");
		return -1;
	}
	return n;
}

static int32_t Send_modifiers(connection_t *connp, char *mods)
{
	return Packet_printf(&connp->w, "%c%s", PKT_MODIFIERS, mods);
}

/*
 * Send all frame data related to the player self and his HUD.
 * NOTE: lock distance should be given in pixels
 */
int32_t Send_self(connection_t *connp, player_t *pl, player_t *lock_pl, int32_t lock_dist,
		int32_t lock_dir, int32_t autopilotlight, int32_t status, char *mods)
{
	int32_t n;
	uint8_t stat = (uint8_t) status;
	int32_t posx, posy, velx, vely;
	int32_t lock_id = -1;

	if (lock_pl) {
		lock_id = lock_pl->id;
	}

	if (Frame_is_real()) {
		posx = (int32_t) (pl->pos.x + 0.5);
		posy = (int32_t) (pl->pos.y + 0.5);
		velx = (int32_t) pl->vel.x;
		vely = (int32_t) pl->vel.y;
	}
	else {
		posx = (int32_t) (pl->pos_interp.x + 0.5);
		posy = (int32_t) (pl->pos_interp.y + 0.5);
		velx = (int32_t) pl->vel_interp.x;
		vely = (int32_t) pl->vel_interp.y;
	}

	n = Packet_printf(&connp->w, "%c"
		"%hd%hd%hd%hd%c"
		"%c%c%c"
		"%hd%hd%c%c"
		"%c%hd%hd"
		"%hd%hd%c"
		"%c%c", PKT_SELF, posx, posy, velx, vely, pl->dir,
			(int32_t) (pl->power + 0.5), (int32_t) (pl->turnspeed + 0.5),
			(int32_t) (pl->turnresistance * 255.0 + 0.5), lock_id,
			lock_dist, lock_dir, 0,
			0, pl->fuel.sum >> FUEL_SCALE_BITS, pl->fuel.max >> FUEL_SCALE_BITS,
			connp->view_width, connp->view_height, connp->debris_colors,
			stat, autopilotlight);
	if (n <= 0) {
		return n;
	}
	return Send_modifiers(connp, mods);
}

/*
 * Somebody is leaving the game.
 */
int32_t Send_leave(connection_t *connp, int32_t id)
{
	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for leave info (%d,%d)",
				connp->state, connp->pl);
		return 0;
	}
	return Packet_printf(&connp->c, "%c%hd", PKT_LEAVE, id);
}

/*
 * Somebody is declaring war.
 */
int32_t Send_war(connection_t *connp, player_t *war_src_pl, player_t *war_dst_pl)
{
	int32_t war_src_id = war_src_pl ? war_src_pl->id : -1;
	int32_t war_dst_id = war_dst_pl ? war_dst_pl->id : -1;

	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for war declaration (%d,%d,%d)",
				connp->cid, connp->state, connp->pl);
		return 0;
	}
	return Packet_printf(&connp->c, "%c%hd%hd", PKT_WAR, war_src_id,
			war_dst_id);
}

/*
 * Somebody is programming a robot to seek some player.
 */
int32_t Send_seek(connection_t *connp, int32_t programmer_id, int32_t robot_id,
		int32_t sought_id)
{
	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for seek declaration (%d,%d,%d)",
				connp->cid, connp->state, connp->pl);
		return 0;
	}
	return Packet_printf(&connp->c, "%c%hd%hd%hd", PKT_SEEK, programmer_id,
			robot_id, sought_id);
}

/*
 * Somebody is joining the game.
 */
int32_t Send_player(connection_t *connp, player_t *pl)
{
	int32_t n;
	char buf[MSG_LEN], ext[MSG_LEN];
	int32_t sbuf_len = connp->c.len;

	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for player info (%d,%d)",
				connp->state, connp->pl);
		return 0;
	}

	Convert_ship_2_string(pl->ship, buf, ext, 0x3200);
	n = Packet_printf(&connp->c, "%c%hd" "%c%c" "%s%s%s" "%S", PKT_PLAYER,
			pl->id, pl->team->Num, pl->mychar, pl->name, pl->realname,
			pl->hostname, buf);
	if (n > 0) {
		n = Packet_printf(&connp->c, "%S", ext);
		if (n <= 0) {
			connp->c.len = sbuf_len;
		}
	}

	return n;
}

/*
 * Send the new score for some player to a client.
 */
int32_t Send_score(connection_t *connp, player_t *pl)
{
	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for score(%d,%d)", connp->state,
				connp->pl);
		return 0;
	}
	return Packet_printf(&connp->c, "%c%hd%hd%hd%c", PKT_SCORE, pl->id, pl->score,
			pl->pl_life, pl->mychar);
}

/*
 * Send info about a player having which base.
 */
int32_t Send_base(connection_t *connp, player_t *pl)
{
	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for base info (%d,%d)",
				connp->state, connp->pl);
		return 0;
	}
	return Packet_printf(&connp->c, "%c%hd%hu", PKT_BASE, pl->id, pl->home_base->id);
}

/*
 * Send info about a base being released.
 */
int32_t Send_release_base(connection_t *connp, player_t *pl)
{
	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for base info (%d,%d)",
				connp->state, connp->pl);
		return 0;
	}
	return Packet_printf(&connp->c, "%c%hd%hu", PKT_BASE, NO_ID, pl->home_base->id);
}

/*
 * Send the amount of fuel in a fuelstation.
 */
int32_t Send_fuel(connection_t *connp, fuel_t *f)
{
	return Packet_printf(&connp->w, "%c%hu%hu", PKT_FUEL, f->id, f->fuel
			>> FUEL_SCALE_BITS);
}

int32_t Send_score_object(connection_t *connp, int32_t score, objposition_t *pos, const char *string)
{
	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for base info (%d,%d)", connp->state, connp->pl);
		return 0;
	}
	return Packet_printf(&connp->c, "%c%hd%hu%hu%s", PKT_SCORE_OBJECT, score, pos->bx, pos->by, string);
}

int32_t Send_destruct(connection_t *connp, int32_t count)
{
	return Packet_printf(&connp->w, "%c%hd", PKT_DESTRUCT, count);
}

int32_t Send_shutdown(connection_t *connp, int32_t count, int32_t delay)
{
	return Packet_printf(&connp->w, "%c%hd%hd", PKT_SHUTDOWN, count, delay);
}

int32_t Send_debris(connection_t *connp, int32_t type, uint8_t *p, int32_t n)
{
	int32_t avail;
	sockbuf_t *w = &connp->w;

	if ((n & 0xFF) != n) {
		errno = 0;
		error("Bad number of debris %d", n);
		return 0;
	}
	avail = w->size - w->len - SOCKBUF_WRITE_SPARE - 2;
	if (n * 2 >= avail) {
		if (avail > 2) {
			n = (avail - 1) / 2;
		}
		else {
			return 0;
		}
	}
	w->buf[w->len++] = PKT_DEBRIS + type;
	w->buf[w->len++] = n;
	memcpy(&w->buf[w->len], p, n * 2);
	w->len += n * 2;

	return n;
}

int32_t Send_wreckage(connection_t *connp, objposition_t *wr_pos, uint8_t wrtype,
		uint8_t size, uint8_t rot)
{
	if (connp->version < 0x3800) {
		return 1;
	}

	if (wreckageCollisionMayKill && connp->version > 0x4201) {
		/* Set the highest bit when wreckage is deadly. */
		wrtype |= 0x80;
	}
	else {
		wrtype &= ~0x80;
	}

	return Packet_printf(&connp->w, "%c%hd%hd%c%c%c", PKT_WRECKAGE, wr_pos->x, wr_pos->y,
			wrtype, size, rot);
}

int32_t Send_fastshot(connection_t *connp, int32_t type, uint8_t *p, int32_t n)
{
	int32_t avail;
	sockbuf_t *w = &connp->w;

	if ((n & 0xFF) != n) {
		errno = 0;
		error("Bad number of fastshot %d", n);
		return 0;
	}
	avail = w->size - w->len - SOCKBUF_WRITE_SPARE - 3;
	if (n * 2 >= avail) {
		if (avail > 2) {
			n = (avail - 1) / 2;
		}
		else {
			return 0;
		}
	}
	w->buf[w->len++] = PKT_FASTSHOT;
	w->buf[w->len++] = type;
	w->buf[w->len++] = n;
	memcpy(&w->buf[w->len], p, n * 2);
	w->len += n * 2;

	return n;
}

int32_t Send_ball(connection_t *connp, objposition_t *ball_pos, object_t *ball)
{
	int32_t id = -1;

	if (ball && Object_is_attached(ball)) {
		/* If player has the ball (ownership of a ball is not the same as actually carrying it),
		 * send his ID to the client */
		if (ball->owner) {
			id = ball->owner->id;
		}
	}

	return Packet_printf(&connp->w, "%c%hd%hd%hd", PKT_BALL, ball_pos->x, ball_pos->y, id);
}

int32_t Send_ship(connection_t *connp, objposition_t *pl_pos, int32_t id, int32_t dir, int32_t shield,
		int32_t cloak, int32_t eshield, int32_t phased, int32_t deflector)
{
	return Packet_printf(&connp->w, "%c%hd%hd%hd" "%c" "%c", PKT_SHIP, pl_pos->x,
			pl_pos->y, id, dir, (shield != 0));
}

int32_t Send_refuel(connection_t *connp, objposition_t *fs_pos, objposition_t *pl_pos)
{
	return Packet_printf(&connp->w, "%c%hd%hd%hd%hd", PKT_REFUEL, fs_pos->x, fs_pos->y, pl_pos->x, pl_pos->y);
}

int32_t Send_connector(connection_t *connp, objposition_t *ball_pos, objposition_t *pl_pos, int32_t tractor)
{
	return Packet_printf(&connp->w, "%c%hd%hd%hd%hd%c", PKT_CONNECTOR, ball_pos->x,
			ball_pos->y, pl_pos->x, pl_pos->y, tractor);
}

int32_t Send_radar(connection_t *connp, int32_t x, int32_t y, int32_t size)
{
	return Packet_printf(&connp->w, "%c%hd%hd%c", PKT_RADAR, x, y, size);
}

int32_t Send_fastradar(connection_t *connp, uint8_t *buf, int32_t n)
{
	int32_t avail;
	sockbuf_t *w = &connp->w;

	if ((n & 0xFF) != n) {
		errno = 0;
		error("Bad number of fastradar %d", n);
		return 0;
	}
	avail = w->size - w->len - SOCKBUF_WRITE_SPARE - 3;
	if (n * 3 >= avail) {
		if (avail > 3) {
			n = (avail - 2) / 3;
		}
		else {
			return 0;
		}
	}
	w->buf[w->len++] = PKT_FASTRADAR;
	w->buf[w->len++] = (uint8_t) (n & 0xFF);
	memcpy(&w->buf[w->len], buf, n * 3);
	w->len += n * 3;

	return (2 + (n * 3));
}

int32_t Send_damaged(connection_t *connp, int32_t damaged)
{
	return Packet_printf(&connp->w, "%c%c", PKT_DAMAGED, damaged);
}

int32_t Send_time_left(connection_t *connp, int32_t sec)
{
	return Packet_printf(&connp->w, "%c%ld", PKT_TIME_LEFT, sec);
}

int32_t Send_eyes(connection_t *connp, player_t *pl)
{
	return Packet_printf(&connp->w, "%c%hd", PKT_EYES, pl->id);
}

int32_t Send_message(connection_t *connp, const char *msg)
{
	if (!BIT(connp->state, CONN_PLAYING | CONN_READY)) {
		errno = 0;
		error("Connection not ready for message (%d,%d)", connp->state,
				connp->pl);
		return 0;
	}
	return Packet_printf(&connp->c, "%c%S", PKT_MESSAGE, msg);
}

int32_t Send_start_of_frame(connection_t *connp)
{
	struct timeval tv1;
	gettimeofday(&tv1, NULL);
	//printf("send start:%e %d\n",timeval_to_seconds(tv1), main_loops);


	if (connp->state != CONN_PLAYING) {
		if (connp->state != CONN_READY) {
			errno = 0;
			error("Connection not ready for frame (%d,%d)",
					connp->state, connp->pl);
		}
		return -1;
	}
	/*
	 * We tell the client which frame number this is and
	 * which keyboard update we have last received.
	 */
	Sockbuf_clear(&connp->w);
	if (Packet_printf(&connp->w, "%c%ld%ld", PKT_START, frame_loops,
			connp->last_key_change) <= 0) {
		Destroy_connection(connp, "write error");
		return -1;
	}

	/* Return ok */
	return 0;
}

int32_t Send_end_of_frame(connection_t *connp)
{
	int32_t n;
	extern int32_t last_packet_of_frame;

	last_packet_of_frame = 1;
	n = Packet_printf(&connp->w, "%c%ld", PKT_END, frame_loops);
	last_packet_of_frame = 0;
	if (n == -1) {
		Destroy_connection(connp, "write error");
		return -1;
	}
	if (n == 0) {
		/*
		 * Frame update size exceeded buffer size.
		 * Drop this packet.
		 */
		printf("ooops:send_end\n");
		fflush(stdout);
		Sockbuf_clear(&connp->w);
		return 0;
	}
	if (connp->c.len > 0 && connp->w.len < MAX_RELIABLE_DATA_PACKET_SIZE) {
		if (Send_reliable(connp) == -1) {
			return -1;
		}
		if (connp->w.len == 0) {
			return 1;
		}
	}
	if (Sockbuf_flush(&connp->w) == -1) {
		Destroy_connection(connp, "flush error");
		return -1;
	}
	Sockbuf_clear(&connp->w);
	return 0;
}

/* @brief Send the requested information about a player to a connected client
 *
 * @param connp	connection
 * @param pl	player
 * @param attr	requested attribute(s)
 */
void Send_info(connection_t *connp, player_t *pl, pl_send_t attr)
{
	if (BIT(attr, PL_SEND_GENERAL)) {
		Send_player(connp, pl);
	}

	if (BIT(attr, PL_SEND_SCORE)) {
		Send_score(connp, pl);
	}

	if (BIT(attr, PL_SEND_BASE) && pl->home_base) {
		Send_base(connp, pl);
	}
}

/*
 * Tell human player about himself.
 */
void Send_info_about_myself(player_t *pl, pl_send_t attr)
{
	ASSERT(pl->connp)
	Send_info(pl->connp, pl, attr);
}

/*
 * Tell a human player about others.
 */
void Send_info_about_others(player_t *pl, pl_send_t attr)
{
	int32_t i;
	player_t *pl2;

	ASSERT(pl->connp)

	for (i = 0; i < NumPlayers; i++) {
		pl2 = Players[i];

		if (pl2 == pl) {
			continue;
		}

		if (Player_is_connected(pl2) || Player_is_robot(pl2)) {
			Send_info(pl->connp, pl2, attr);
		}
	}
}

void Send_info_about_player(player_t *pl, pl_send_t attr)
{
	int32_t i;
	player_t *pl2;

	for (i = 0; i < NumPlayers; i++) {
		pl2 = Players[i];

		if (pl2 == pl) {
			continue;
		}

		/*
		 * And tell all the others about him.
		 */
		if (Player_is_connected(pl2)) {
			Send_info(pl2->connp, pl, attr);
		}

		/*
		 * And tell him about the relationships others have with eachother.
		 */
		// TODO this should be moved elsewhere
#if 0
		else if (Player_is_connected(pl) && Player_is_robot(pl2)) {
			player_t *pl_in_war;

			if ((pl_in_war = Robot_war_on_player(pl2)) != NULL) {
				if (BIT(attr, PL_SEND_WAR)) {
					Send_war(pl->connp, pl2, pl_in_war);
				}
			}
		}
#endif
	}
}

static int32_t Receive_keyboard(connection_t *connp)
{
	player_t *pl;
	int32_t change;
	uint8_t ch;
	int32_t size = KEYBOARD_SIZE;
	//struct timeval      tv1;

	//gettimeofday(&tv1, NULL);
	//printf("receive kb:%e %d\n",timeval_to_seconds(tv1), main_loops);
	if (connp->r.ptr - connp->r.buf + size + 1 + 4 > connp->r.len) {
		/*
		 * Incomplete client packet.
		 */
		return 0;
	}
	Packet_scanf(&connp->r, "%c%ld", &ch, &change);
	if (change <= connp->last_key_change) {
		/*
		 * We already have this key.
		 * Nothing to do.
		 */
		connp->r.ptr += size;
	}
	else {
		connp->last_key_change = change;
		pl = connp->pl;
		memcpy(pl->last_keyv, connp->r.ptr, size);
		connp->r.ptr += size;
		Handle_keyboard(pl);
	}
	if (connp->num_keyboard_updates++ && (connp->state & CONN_PLAYING)) {
		Destroy_connection(connp, "no macros");
		return -1;
	}

	return 1;
}

static int32_t Receive_quit(connection_t *connp)
{
	Destroy_connection(connp, "client quit");

	return -1;
}

static int32_t Receive_play(connection_t *connp)
{
	uint8_t ch;
	int32_t n;

	if ((n = Packet_scanf(&connp->r, "%c", &ch)) != 1) {
		errno = 0;
		error("Cannot receive play packet");
		Destroy_connection(connp, "receive error");
		return -1;
	}
	if (ch != PKT_PLAY) {
		errno = 0;
		error("Packet is not of play type");
		Destroy_connection(connp, "not play");
		return -1;
	}
	if (connp->state != CONN_LOGIN) {
		if (connp->state != CONN_PLAYING) {
			if (connp->state == CONN_READY) {
				connp->r.ptr = connp->r.buf + connp->r.len;
				return 0;
			}
			errno = 0;
			error("Connection not in login state (%02x)",
					connp->state);
			Destroy_connection(connp, "not login");
			return -1;
		}
		if (Send_reliable(connp) == -1) {
			return -1;
		}
		return 0;
	}
	Sockbuf_clear(&connp->w);
	if (!Handle_login(connp)) {
		Destroy_connection(connp, "login failed");
		return -1;
	}

	return 2;
}

static int32_t Receive_power(connection_t *connp)
{
	player_t *pl;
	uint8_t ch;
	int16_t tmp;
	int32_t n;
	DFLOAT power;

	if ((n = Packet_scanf(&connp->r, "%c%hd", &ch, &tmp)) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read error");
		}
		return n;
	}
	power = (DFLOAT) tmp / 256.0F;
	pl = connp->pl;

	switch (ch) {
	case PKT_POWER:
		pl->power = power;
		break;
	case PKT_POWER_S:
		pl->power_s = power;
		break;
	case PKT_TURNSPEED:
		pl->turnspeed = power;
		break;
	case PKT_TURNSPEED_S:
		pl->turnspeed_s = power;
		break;
	case PKT_TURNRESISTANCE:
		pl->turnresistance = power;
		break;
	case PKT_TURNRESISTANCE_S:
		pl->turnresistance_s = power;
		break;
	default:
		errno = 0;
		error("Not a power packet (%d,%02x)", ch, connp->state);
		Destroy_connection(connp, "not power");
		return -1;
	}
	return 1;
}

/*
 * Send the reliable data.
 * If the client is in the receive-frame-updates state then
 * all reliable data is piggybacked at the end of the
 * frame update packets.  (Except maybe for the MOTD data, which
 * could be transmitted in its own packets since MOTDs can be big.)
 * Otherwise if the client is not actively playing yet then
 * the reliable data is sent in its own packets since there
 * is no other data to combine it with.
 *
 * This thing still is not finished, but it works better than in 3.0.0 I hope.
 */
int32_t Send_reliable(connection_t *connp)
{
	char *read_buf;
	int32_t i, n, len, todo, max_todo;
	int32_t rel_off;
	const int32_t max_packet_size = MAX_RELIABLE_DATA_PACKET_SIZE,
			min_send_size = 1; /* was 4 in 3.0.7, 1 in 3.1.0 */

	if (connp->c.len <= 0 || connp->last_send_loops == main_loops) {
		connp->last_send_loops = main_loops;
		return 0;
	}
	read_buf = connp->c.buf;
	max_todo = connp->c.len;
	rel_off = connp->reliable_offset;
	if (connp->w.len > 0) {
		/* We are piggybacking on a frame update. */
		if (connp->w.len >= max_packet_size - min_send_size) {
			/* Frame already too big */
			return 0;
		}
		if (max_todo > max_packet_size - connp->w.len) {
			/* Do not exceed minimum fragment size. */
			max_todo = max_packet_size - connp->w.len;
		}
	}
	if (connp->retransmit_at_loop > main_loops) {
		/*
		 * It is no time to retransmit yet.
		 */
		if (max_todo <= connp->reliable_unsent - connp->reliable_offset
				+ min_send_size || connp->w.len == 0) {
			/*
			 * And we cannot send anything new either
			 * and we do not want to introduce a new packet.
			 */
			return 0;
		}
	}
	else if (connp->retransmit_at_loop != 0) {
		/*
		 * Timeout.
		 * Either our packet or the acknowledgement got lost,
		 * so retransmit.
		 */
		connp->acks >>= 1;
	}

	todo = max_todo;
	for (i = 0; i <= connp->acks && todo > 0; i++) {
		len = (todo > max_packet_size) ? max_packet_size : todo;
		if (Packet_printf(&connp->w, "%c%hd%ld%ld", PKT_RELIABLE, len,
				rel_off, main_loops) <= 0 || Sockbuf_write(
				&connp->w, read_buf, len) != len) {
			error("Cannot write reliable data");
			Destroy_connection(connp, "write error");
			return -1;
		}
		if ((n = Sockbuf_flush(&connp->w)) < len) {
			if (n == 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
				connp->acks = 0;
				break;
			}
			else {
				error("Cannot flush reliable data (%d)", n);
				Destroy_connection(connp, "flush error");
				return -1;
			}
		}
		todo -= len;
		rel_off += len;
		read_buf += len;
	}

	/*
	 * Drop rest of outgoing data packet if something remains at all.
	 */
	Sockbuf_clear(&connp->w);

	connp->last_send_loops = main_loops;

	if (max_todo - todo <= 0) {
		/*
		 * We have not transmitted anything at all.
		 */
		return 0;
	}

	/*
	 * Retransmission timer with exponential backoff.
	 */
	if (connp->rtt_retransmit > MAX_RETRANSMIT) {
		connp->rtt_retransmit = MAX_RETRANSMIT;
	}
	if (connp->retransmit_at_loop <= main_loops) {
		connp->retransmit_at_loop = main_loops + connp->rtt_retransmit;
		connp->rtt_retransmit <<= 1;
		connp->rtt_timeouts++;
	}
	else {
		connp->retransmit_at_loop = main_loops + connp->rtt_retransmit;
	}

	if (rel_off > connp->reliable_unsent) {
		connp->reliable_unsent = rel_off;
	}

	return (max_todo - todo);
}

static int32_t Receive_ack(connection_t *connp)
{
	int32_t n;
	uint8_t ch;
	int32_t rel, rtt, /* RoundTrip Time */
	diff, delta, rel_loops;

	if ((n = Packet_scanf(&connp->r, "%c%ld%ld", &ch, &rel, &rel_loops))
			<= 0) {
		errno = 0;
		error("Cannot read ack packet (%d)", n);
		Destroy_connection(connp, "read error");
		return -1;
	}
	if (ch != PKT_ACK) {
		errno = 0;
		error("Not an ack packet (%d)", ch);
		Destroy_connection(connp, "not ack");
		return -1;
	}
	rtt = main_loops - rel_loops;
	if (rtt > 0 && rtt <= MAX_RTT) {
		/*
		 * These roundtrip estimation calculations are derived from Comer's
		 * books "Internetworking with TCP/IP" parts I & II.
		 */
		if (connp->rtt_smoothed == 0) {
			/*
			 * Initialize the rtt estimator by this first measurement.
			 * The estimator is scaled by 3 bits.
			 */
			connp->rtt_smoothed = rtt << 3;
		}
		/*
		 * Scale the estimator back by 3 bits before calculating the error.
		 */
		delta = rtt - (connp->rtt_smoothed >> 3);
		/*
		 * Add one eigth of the error to the estimator.
		 */
		connp->rtt_smoothed += delta;
		/*
		 * Now we need the absolute value of the error.
		 */
		if (delta < 0) {
			delta = -delta;
		}
		/*
		 * The rtt deviation is scaled by 2 bits.
		 * Now we add one fourth of the difference between the
		 * error and the previous deviation to the deviation.
		 */
		connp->rtt_dev += delta - (connp->rtt_dev >> 2);
		/*
		 * The calculation of the retransmission timeout is what this is
		 * all about.  We take the smoothed rtt plus twice the deviation
		 * as the next retransmission timeout to use.  Because of the
		 * scaling used we get the following statement:
		 */
		connp->rtt_retransmit = ((connp->rtt_smoothed >> 2)
				+ connp->rtt_dev) >> 1;
		/*
		 * Now keep it within reasonable bounds.
		 */
		if (connp->rtt_retransmit < MIN_RETRANSMIT) {
			connp->rtt_retransmit = MIN_RETRANSMIT;
		}
	}
	diff = rel - connp->reliable_offset;
	if (diff > connp->c.len) {
		/* Impossible to ack data that has not been send */
		errno = 0;
		error("Bad ack (diff=%ld,cru=%ld,c=%ld,len=%d)", diff, rel,
				connp->reliable_offset, connp->c.len);
		Destroy_connection(connp, "bad ack");
		return -1;
	}
	else if (diff <= 0) {
		/* Late or duplicate ack of old data.  Discard. */
		return 1;
	}
	Sockbuf_advance(&connp->c, (int32_t) diff);
	connp->reliable_offset += diff;
	if ((n = ((diff + 512 - 1) / 512)) > connp->acks) {
		connp->acks = n;
	}
	else {
		connp->acks++;
	}
	if (connp->reliable_offset >= connp->reliable_unsent) {
		/*
		 * All reliable data has been sent and acked.
		 */
		connp->retransmit_at_loop = 0;
		if (connp->state == CONN_DRAIN) {
			Conn_set_state(connp, connp->drain_state,
					connp->drain_state);
		}
	}
	if (connp->state == CONN_READY && (connp->c.len <= 0
			|| (connp->c.buf[0] != PKT_REPLY && connp->c.buf[0]
					!= PKT_PLAY && connp->c.buf[0]
					!= PKT_SUCCESS && connp->c.buf[0]
					!= PKT_FAILURE))) {
		Conn_set_state(connp, connp->drain_state, connp->drain_state);
	}
	connp->rtt_timeouts = 0;

	return 1;
}

static int32_t Receive_discard(connection_t *connp)
{
	errno = 0;
	error("Discarding packet %d while in state %02x", connp->r.ptr[0],
			connp->state);
	connp->r.ptr = connp->r.buf + connp->r.len;

	return 0;
}

static int32_t Receive_undefined(connection_t *connp)
{
	errno = 0;
	error("Unknown packet type (%d,%02x)", connp->r.ptr[0], connp->state);
	Destroy_connection(connp, "undefined packet");
	return -1;
}

static int32_t Receive_ack_fuel(connection_t *connp)
{
	int32_t loops_ack;
	uint8_t ch;
	int32_t n;
	uint16_t num;

	if ((n = Packet_scanf(&connp->r, "%c%ld%hu", &ch, &loops_ack, &num))
			<= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read error");
		}
		return n;
	}
	if (num >= World.NumFuels) {
		Destroy_connection(connp, "bad fuel ack");
		return -1;
	}
	if (loops_ack > World.fuel[num].last_change) {
		SET_BIT(World.fuel[num].conn_mask, 1 << connp->cid);
	}
	return 1;
}

/*
 * If a message contains a colon then everything before that colon is
 * either a unique player name prefix, or a team number with players.
 * If the string does not match one team or one player the message is not sent.
 * If no colon, the message is general.
 */
static void Handle_talk(connection_t *connp, char *str)
{
	player_t *pl = connp->pl;
	player_t *pl2;
	int32_t i, sent, team;
	uint32_t len;
	char *content_ptr;

	/* Public message */
	if ((content_ptr = strchr(str, ':')) == NULL || content_ptr == str
			|| strchr("-_~)(/\\}{[]", content_ptr[1])) { /* smileys are smileys */
		Message_game_print("%s [%s]", str, pl->name);
		return;
	}

	*content_ptr++ = '\0';
	len = strlen(str);

	/* Determine the recipient */
	if (strspn(str, "0123456789") == len) { /* Team message */
		team = atoi(str);

		for (sent = i = 0; i < NumPlayers; i++) {
			pl2 = Players[i];

			if (pl2->team->Num == team) {
				sent++;
				Message_player_print(pl2, PL_MSG_NONE, "%s [%s]:[%d]", content_ptr, pl->name, team);
			}
		}

		if (sent) { /* Send to the sender as well */
			if (pl->team->Num != team) {
				Message_player_print(pl, PL_MSG_NONE, "%s [%s]:[%d]", content_ptr, pl->name, team);
			}
		}
		else {
			Message_player_print(pl, PL_MSG_REPLY, "Message not sent. Nobody in team %d.", team);
		}
	}
	else { /* Private message */
		const char *errmsg;
		player_t *other_pl = Player_get_by_name(str, NULL, &errmsg);

		if (!other_pl) {
			Message_player_print(pl, PL_MSG_REPLY, "Message not sent. %s", errmsg);
			return;
		}
		if (other_pl != pl) {
			Message_player_print(other_pl, PL_MSG_NONE, "%s [%s]:[%s]", content_ptr, pl->name, other_pl->name);
			Message_player_print(pl, PL_MSG_NONE, "%s [%s]:[%s]", content_ptr, pl->name, other_pl->name);
		}
	}
}

static int32_t Receive_talk(connection_t *connp)
{
	uint8_t ch;
	int32_t n;
	int32_t seq;
	char str[MAX_CHARS];

	if ((n = Packet_scanf(&connp->r, "%c%ld%s", &ch, &seq, str)) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read error");
		}
		return n;
	}
	if (seq > connp->talk_sequence_num) {
		if ((n = Packet_printf(&connp->c, "%c%ld", PKT_TALK_ACK, seq))
				<= 0) {
			if (n == -1) {
				Destroy_connection(connp, "write error");
			}
			return n;
		}
		connp->talk_sequence_num = seq;
		if (*str == '/') {
			Handle_player_command(connp->pl, str + 1);
		}
		else {
			Handle_talk(connp, str);
		}
	}
	return 1;
}

static int32_t Receive_display(connection_t *connp)
{
	uint8_t ch, debris_colors, spark_rand;
	int16_t width, height;
	int32_t n;

	if ((n = Packet_scanf(&connp->r, "%c%hd%hd%c%c", &ch, &width, &height,
			&debris_colors, &spark_rand)) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read error");
		}
		return n;
	}
	LIMIT(width, MIN_VIEW_SIZE, MAX_VIEW_SIZE);
	LIMIT(height, MIN_VIEW_SIZE, MAX_VIEW_SIZE);
	connp->view_width = width;
	connp->view_height = height;
	connp->debris_colors = debris_colors;
	connp->spark_rand = spark_rand;
	return 1;
}

static int32_t Receive_modifier_bank(connection_t *connp)
{
	char str[MAX_CHARS];
	uint8_t ch;
	uint8_t bank;
	int32_t n;

	if ((n = Packet_scanf(&connp->r, "%c%c%s", &ch, &bank, str)) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read modbank");
		}
		return n;
	}
	return 1;
}

void Get_display_parameters(connection_t *connp, int32_t *width, int32_t *height,
		int32_t *debris_colors, int32_t *spark_rand)
{
	*width = connp->view_width;
	*height = connp->view_height;
	*debris_colors = connp->debris_colors;
	*spark_rand = connp->spark_rand;
}

static int32_t Receive_shape(connection_t *connp)
{
	int32_t n;
	char ch;
	char str[2 * MSG_LEN];

	if ((n = Packet_scanf(&connp->r, "%c%S", &ch, str)) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read shape");
		}
		return n;
	}
	if ((n = Packet_scanf(&connp->r, "%S", &str[strlen(str)])) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read shape ext");
		}
		return n;
	}
	if (connp->state == CONN_LOGIN && connp->ship == NULL) {
		connp->ship = Parse_shape_str(str);
	}
	return 1;
}

static int32_t Receive_motd(connection_t *connp)
{
	uint8_t ch;
	int32_t offset;
	int32_t n;
	int32_t bytes;

	if ((n = Packet_scanf(&connp->r, "%c%ld%ld", &ch, &offset, &bytes))
			<= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read error");
		}
		return n;
	}

	/* Client requests MOTD. Don't care. */
	return 1;
}

static int32_t Receive_pointer_move(connection_t *connp)
{
	player_t *pl;
	uint8_t ch;
	int16_t movement;
	int32_t n;
	DFLOAT turnspeed, turndir;
	//long  last_loops;
	//struct timeval      tv1;
	//gettimeofday(&tv1, NULL);


	//if ((n = Packet_scanf(&connp->r, "%c%hd%ld", &ch, &movement, &last_loops)) <= 0){

	if ((n = Packet_scanf(&connp->r, "%c%hd", &ch, &movement)) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read error");
		}
		return n;
	}
	pl = connp->pl;

	//printf("receive_pointer:%e main_loops:%d\n",timeval_to_seconds(tv1),last_loops);

	turnspeed = (DFLOAT) movement * pl->turnspeed / (DFLOAT) MAX_PLAYER_TURNSPEED;

	if (turnspeed < 0) {
		turndir = -1.0;
		turnspeed = -turnspeed;
	}
	else {
		turndir = 1.0;
	}

	received_packets++;

	LIMIT(turnspeed, 0, 5 * ANGLE_RESOLUTION);
	pl->turnvel -= turndir * turnspeed;

	return 1;
}

static int32_t Receive_fps_request(connection_t *connp)
{
	player_t *pl;
	int32_t n;
	uint8_t ch;
	uint8_t wanted_fps;

	if ((n = Packet_scanf(&connp->r, "%c%c", &ch, &wanted_fps)) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read error");
		}
		return n;
	}
	if ((pl = Connection_is_occupied(connp))) {
		if (wanted_fps <= 0)
			wanted_fps = 1;
		if (wanted_fps == 20)
			wanted_fps = 254;
		pl->player_fps = wanted_fps;
		/* printf("receivefps:%d\n",fps);*/
	}

	return 1;
}

static int32_t Receive_audio_request(connection_t *connp)
{
	int32_t n;
	uint8_t ch;
	uint8_t onoff;

	if ((n = Packet_scanf(&connp->r, "%c%c", &ch, &onoff)) <= 0) {
		if (n == -1) {
			Destroy_connection(connp, "read error");
		}
		return n;
	}
	return 1;
}

const char *Player_get_addr(player_t *pl)
{
	connection_t *connp = NULL;

	if ((connp = Player_is_connected(pl))) {
		return connp->addr;
	}

	return NULL;
}
