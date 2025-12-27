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

#ifndef	NETSERVER_H
#define	NETSERVER_H

#include "net.h"
#include "object.h"


/*
 * Different states a connection can be in.
 */
#define CONN_FREE	0x00	/* free for use */
#define CONN_LISTENING	0x01	/* before connect() */
#define CONN_SETUP	0x02	/* after verification */
#define CONN_LOGIN	0x04	/* after setup info transferred */
#define CONN_PLAYING	0x08	/* when actively playing */
#define CONN_DRAIN	0x20	/* wait for all reliable data to be acked */
#define CONN_READY	0x40	/* draining after LOGIN and before PLAYING */

/*
 * In order to not let the server be locked by a collection
 * of idle connections we timeout a client if it doesn't
 * continue with logging in in a reasonable tempo.
 * Sorry, our resources are limited.
 * But the timeout should be easily configurable.
 * The timeout specifies the number of seconds each connection
 * state may last.
 */
#define LISTEN_TIMEOUT		4
#define SETUP_TIMEOUT		15
#define LOGIN_TIMEOUT		40
#define READY_TIMEOUT		40
#define IDLE_TIMEOUT		30

/*
 * Maximum roundtrip time taken as serious for rountrip time calculations.
 */
#define MAX_RTT			(fps + 1)

/* Check whether the connection is tied to any player */
#define Connection_is_occupied(connp)	((connp) ? (player_t *)((connp)->pl) : NULL)
/*
 * The retransmission timeout bounds in number of frames.
 */
#define MIN_RETRANSMIT		(fps / 8 + 1)
#define MAX_RETRANSMIT		(fps + 1)
#define DEFAULT_RETRANSMIT	(fps / 2)

#if !defined(MAXHOSTNAMELEN)
#define MAXHOSTNAMELEN 64
#endif

/*
 * Types of player attributes, that need to be sent to them
 */
typedef enum {
	PL_SEND_GENERAL = 1<<1,
	PL_SEND_SCORE = 1<<2,
	PL_SEND_BASE = 1<<3,
	PL_SEND_WAR = 1<<4,

	PL_SEND_ALL = 0xFFFFFFFF
} pl_send_t;

/*
 * All the connection state info.
 * Some of it is hardly ever used, if at all.
 */
struct _connection {
	int32_t cid; /* connection ID */
	int32_t state; /* state of connection */
	int32_t drain_state; /* state after draining done */
	uint32_t magic; /* magic cookie */
	sockbuf_t r; /* input buffer */
	sockbuf_t w; /* output buffer */
	sockbuf_t c; /* reliable data buffer */
	int32_t start; /* time of last state change */
	int32_t timeout; /* time when state timeouts */
	int32_t last_send_loops; /* last update of reliable */
	int32_t reliable_offset; /* amount of data acked */
	int32_t reliable_unsent; /* next unsend reliable byte */
	int32_t retransmit_at_loop; /* next retransmission time */
	int32_t rtt_smoothed; /* smoothed roundtrip time */
	int32_t rtt_dev; /* roundtrip time deviation */
	int32_t rtt_retransmit; /* retransmission time */
	int32_t rtt_timeouts; /* how many timeouts */
	int32_t acks; /* good acknowledgements */
	int32_t setup; /* amount of setup done */
	int32_t my_port; /* server port for this player */
	int32_t his_port; /* client port for this player */
	team_t *team; /* team number suggested by the incoming player */
	uint32_t version; /* XPilot version of client */
	int32_t last_key_change; /* last keyboard change */
	int32_t talk_sequence_num; /* talk acknowledgement */
	int32_t num_keyboard_updates; /* Keyboards in one packet */
	int32_t view_width, view_height;/* Viewable area dimensions */
	int32_t debris_colors; /* Max. debris intensities */
	int32_t spark_rand; /* Sparkling effect */
	char *real; /* real login name of player */
	char *nick; /* nickname of player */
	char *dpy; /* display of player */
	shipshape_t *ship; /* ship shape of player */
	char *addr; /* address of players host */
	char *host; /* hostname of players host */
	player_t *pl; /* pointer to the player's structure */
};

extern int32_t num_logins;
extern int32_t num_logouts;
extern int32_t num_pause;
extern int32_t num_unpause;

int32_t Get_motd(char *buf, int32_t offset, int32_t maxlen, int32_t *size_ptr);
int32_t Setup_net_server(void);
void Destroy_connection(connection_t *connp, const char *reason);
int32_t Check_connection(char *real, char *nick, char *dpy, char *addr);
int32_t Setup_connection(char *real, char *nick, char *dpy, team_t *team, char *addr,
		char *host, uint32_t version);

int32_t Input(void);

int32_t Handle_keyboard(player_t *pl);
void Handle_player_command(player_t *pl, char *cmd);

int32_t Send_reply(connection_t *connp, int32_t replyto, int32_t result);
int32_t Send_self(connection_t *connp, player_t *pl, player_t *lock_pl, int32_t lock_dist,
		int32_t lock_dir, int32_t autopilotlight, int32_t status, char *mods);
int32_t Send_leave(connection_t *connp, int32_t id);
int32_t Send_war(connection_t *connp, player_t *war_src_pl, player_t *war_dst_pl);
int32_t Send_seek(connection_t *connp, int32_t programmer_id, int32_t robot_id,
		int32_t sought_id);
int32_t Send_player(connection_t *connp, player_t *pl);
int32_t Send_score(connection_t *connp, player_t *pl);
int32_t Send_score_object(connection_t *connp, int32_t score, objposition_t *pos, const char *string);
int32_t Send_base(connection_t *connp, player_t *pl);
int32_t Send_release_base(connection_t *connp, player_t *pl);
int32_t Send_fuel(connection_t *connp, fuel_t *f);
int32_t Send_destruct(connection_t *connp, int32_t count);
int32_t Send_shutdown(connection_t *connp, int32_t count, int32_t delay);
int32_t Send_debris(connection_t *connp, int32_t type, uint8_t *p, int32_t n);
int32_t Send_wreckage(connection_t *connp, objposition_t *wr_pos, uint8_t wrtype, uint8_t size, uint8_t rot);
int32_t Send_fastshot(connection_t *connp, int32_t type, uint8_t *p, int32_t n);
int32_t Send_ball(connection_t *connp, objposition_t *ball_pos, object_t *ball);
int32_t Send_paused(connection_t *connp, int32_t x, int32_t y, int32_t count);
int32_t Send_ship(connection_t *connp, objposition_t *pl_pos, int32_t id, int32_t dir, int32_t shield,
		int32_t cloak, int32_t eshield, int32_t phased, int32_t deflector);
int32_t Send_refuel(connection_t *connp, objposition_t *fs_pos, objposition_t *pl_pos);
int32_t Send_connector(connection_t *connp, objposition_t *ball_pos, objposition_t *pl_pos, int32_t tractor);
int32_t Send_radar(connection_t *connp, int32_t x, int32_t y, int32_t size);
int32_t Send_fastradar(connection_t *connp, uint8_t *buf, int32_t n);
int32_t Send_damaged(connection_t *connp, int32_t damaged);
int32_t Send_message(connection_t *connp, const char *msg);
int32_t Send_start_of_frame(connection_t *connp);
int32_t Send_end_of_frame(connection_t *connp);
int32_t Send_reliable(connection_t *connp);
int32_t Send_time_left(connection_t *connp, int32_t sec);
int32_t Send_eyes(connection_t *connp, player_t *pl);
void Get_display_parameters(connection_t *connp, int32_t *width, int32_t *height, int32_t *debris_colors, int32_t *spark_rand);
int32_t Send_shape(connection_t *connp, int32_t shape);
const char *Player_get_addr(player_t *pl);

void Send_info(connection_t *connp, player_t *pl, pl_send_t attr);

void Send_info_about_myself(player_t *pl, pl_send_t attr);
void Send_info_about_others(player_t *pl, pl_send_t attr);
void Send_info_about_player(player_t *pl, pl_send_t attr);

#endif

