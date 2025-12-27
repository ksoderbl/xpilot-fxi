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
#include <sys/types.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "version.h"
#include "commonproto.h"
#include "xpconfig.h"
#include "debug.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "bit.h"

#include "frame.h"
#include "netserver.h"
#include "error.h"

#include "player.h"
#include "map.h"
#include "player_inline.h"

#include "robot.h"

char frame_version[] = VERSION;

/*
 * Structure for calculating if a pixel is visible by a player.
 * The following always holds:
 *	(world.x >= realWorld.x && world.y >= realWorld.y)
 */
typedef struct {
	position_t world; /* Lower left hand corner is this */
	/* world coordinate */
	position_t realWorld; /* If the player is on the edge of
	 the screen, these are the world
	 coordinates before adjustment... */
} pixel_visibility_t;

/*
 * Structure with player position info measured in blocks instead of pixels.
 * Used for map state info updating.
 */
typedef struct {
	ipos_t world;
	ipos_t realWorld;
} block_visibility_t;

typedef struct {
	uint8_t x, y;
} debris_t;

typedef struct {
	int16_t x, y, size;
} radar_t;

int32_t frame_loops = 1;
static int32_t last_frame_shuffle;
uint16_t object_shuffle[MAX_TOTAL_OBJECTS];
static uint16_t player_shuffle[64 + MAX_PSEUDO_PLAYERS];
static radar_t *radar_ptr;
static int32_t num_radar, max_radar;

static pixel_visibility_t pv;
static int32_t view_width, view_height, horizontal_blocks, vertical_blocks,
		debris_x_areas, debris_y_areas, debris_areas, debris_colors,
		spark_rand;
static debris_t *debris_ptr[DEBRIS_TYPES];
static uint32_t debris_num[DEBRIS_TYPES], debris_max[DEBRIS_TYPES];
static debris_t *fastshot_ptr[DEBRIS_TYPES * 2];
static uint32_t fastshot_num[DEBRIS_TYPES * 2], fastshot_max[DEBRIS_TYPES * 2];

static char message[MSG_LEN];	// message sent to a player

static const char server_greeting_str[] = " [*Server greeting*]";
static const char server_notice_str[] = " [*Server notice*]";
static const char server_reply_str[] = " [*Server reply*]";

void Frame_shots(connection_t *connp, player_t *pl);
void Frame_shuffle(void);


/*
 * Macro to make room in a given dynamic array for new elements.
 * P is the pointer to the array memory.
 * N is the current number of elements in the array.
 * M is the current size of the array.
 * T is the type of the elements.
 * E is the number of new elements to store in the array.
 * The goal is to keep the number of malloc/realloc calls low
 * while not wasting too much memory because of over-allocation.
 */
#define EXPAND(P,N,M,T,E)						\
    if ((N) + (E) > (M)) {						\
	if ((M) <= 0) {							\
	    M = (E) + 2;						\
	    P = (T *) malloc((M) * sizeof(T));				\
	    N = 0;							\
	} else {							\
	    M = ((M) << 1) + (E);					\
	    P = (T *) realloc(P, (M) * sizeof(T));			\
	}								\
	if (P == NULL) {						\
	    error("No memory");						\
	    N = M = 0;							\
	    return;	/* ! */						\
	}								\
    }

#define inview(pos) 								\
    (   (   (((pos)->x) > pv.world.x && ((pos)->x) < pv.world.x + view_width)		\
	 || (((pos)->x) > pv.realWorld.x && ((pos)->x) < pv.realWorld.x + view_width))	\
     && (   (((pos)->y) > pv.world.y && ((pos)->y) < pv.world.y + view_height)		\
	 || (((pos)->y) > pv.realWorld.y && ((pos)->y) < pv.realWorld.y + view_height)))

static int32_t block_inview(block_visibility_t *bv, objposition_t *pos)
{
	return ((pos->bx > bv->world.x && pos->bx < bv->world.x + horizontal_blocks) || (pos->bx
			> bv->realWorld.x && pos->bx < bv->realWorld.x
			+ horizontal_blocks)) && ((pos->by > bv->world.y && pos->by
			< bv->world.y + vertical_blocks) || (pos->by
			> bv->realWorld.y && pos->by < bv->realWorld.y
			+ vertical_blocks));
}

#define DEBRIS_STORE(xd,yd,color,offset) \
    int32_t			i;						  \
    if (xd < 0) {							  \
	xd += World.width;						  \
    }									  \
    if (yd < 0) {							  \
	yd += World.height;						  \
    }									  \
    if ((uint32_t) xd >= (uint32_t)view_width || (uint32_t) yd >= (uint32_t)view_height) {	  \
	/*								  \
	 * There's some rounding error or so somewhere.			  \
	 * Should be possible to resolve it.				  \
	 */								  \
	return;								  \
    }									  \
									  \
    i = offset + color * debris_areas					  \
	+ (((yd >> 8) % debris_y_areas) * debris_x_areas)		  \
	+ ((xd >> 8) % debris_x_areas);					  \
									  \
    if (num_ >= 255) {							  \
	return;								  \
    }									  \
    if (num_ >= max_) {							  \
	if (num_ == 0) {							  \
	    ptr_ = (debris_t *) malloc((max_ = 16) * sizeof(*ptr_));	  \
	} else {							  \
	    ptr_ = (debris_t *) realloc(ptr_, (max_ += max_) * sizeof(*ptr_)); \
	}								  \
	if (ptr_ == 0) {							  \
	    error("No memory for debris");				  \
	    num_ = 0;							  \
	    return;							  \
	}								  \
    }									  \
    ptr_[num_].x = (uint8_t) xd;					  \
    ptr_[num_].y = (uint8_t) yd;					  \
    num_++;

static void fastshot_store(int32_t xf, int32_t yf, int32_t color, int32_t offset)
{
#define ptr_		(fastshot_ptr[i])
#define num_		(fastshot_num[i])
#define max_		(fastshot_max[i])
	DEBRIS_STORE(xf, yf, color, offset);
#undef ptr_
#undef num_
#undef max_
}

static void debris_store(int32_t xf, int32_t yf, int32_t color)
{
#define ptr_		(debris_ptr[i])
#define num_		(debris_num[i])
#define max_		(debris_max[i])
	DEBRIS_STORE(xf, yf, color, 0);
#undef ptr_
#undef num_
#undef max_
}

static void fastshot_end(connection_t *connp)
{
	int32_t i;

	for (i = 0; i < DEBRIS_TYPES * 2; i++) {
		if (fastshot_num[i] != 0) {
			Send_fastshot(connp, i,
					(uint8_t *) fastshot_ptr[i],
					fastshot_num[i]);
			fastshot_num[i] = 0;
		}
	}
}

static void debris_end(connection_t *connp)
{
	int32_t i;

	for (i = 0; i < DEBRIS_TYPES; i++) {
		if (debris_num[i] != 0) {
			Send_debris(connp, i, (uint8_t *) debris_ptr[i],
					debris_num[i]);
			debris_num[i] = 0;
		}
	}
}

static void Frame_radar_buffer_reset(void)
{
	num_radar = 0;
}

static void Frame_radar_buffer_add(int32_t x, int32_t y, int32_t s)
{
	radar_t *p;

	EXPAND(radar_ptr, num_radar, max_radar, radar_t, 1);
	p = &radar_ptr[num_radar++];
	p->x = x;
	p->y = y;
	p->size = s;
}

static void Frame_radar_buffer_send(connection_t *connp)
{
	int32_t i;
	int32_t dest;
	int32_t tmp;
	radar_t *p;
	const int32_t radar_width = 256;
	int32_t radar_height = (radar_width * World.y) / World.x;
	int32_t radar_x;
	int32_t radar_y;
	int32_t send_x;
	int32_t send_y;
	uint16_t radar_shuffle[MAX_TOTAL_OBJECTS];

	for (i = 0; i < num_radar; i++) {
		radar_shuffle[i] = i;
	}
	/* permute. */
	for (i = 0; i < num_radar; i++) {
		dest = (int32_t) (rfrac() * num_radar);
		tmp = radar_shuffle[i];
		radar_shuffle[i] = radar_shuffle[dest];
		radar_shuffle[dest] = tmp;
	}

	if (connp->version <= 0x4400) {
		for (i = 0; i < num_radar; i++) {
			p = &radar_ptr[radar_shuffle[i]];
			radar_x = (radar_width * p->x) / World.width;
			radar_y = (radar_height * p->y) / World.height;
			send_x = (World.width * radar_x) / radar_width;
			send_y = (World.height * radar_y) / radar_height;
			Send_radar(connp, send_x, send_y, p->size);
		}
	}
	else {
		uint8_t buf[3 * 256];
		int32_t buf_index = 0;
		int32_t fast_count = 0;

		if (num_radar > 256) {
			num_radar = 256;
		}
		for (i = 0; i < num_radar; i++) {
			p = &radar_ptr[radar_shuffle[i]];
			radar_x = (radar_width * p->x) / World.width;
			radar_y = (radar_height * p->y) / World.height;
			if (radar_y >= 1024) {
				continue;
			}
			buf[buf_index++] = (uint8_t) (radar_x);
			buf[buf_index++] = (uint8_t) (radar_y & 0xFF);
			buf[buf_index]
					= (uint8_t) ((radar_y >> 2)
							& 0xC0);
			if (p->size & 0x80) {
				buf[buf_index] |= (uint8_t) (0x20);
			}
			buf[buf_index] |= (uint8_t) (p->size & 0x07);
			buf_index++;
			fast_count++;
		}
		if (fast_count > 0) {
			Send_fastradar(connp, buf, fast_count);
		}
	}
}

static void Frame_radar_buffer_free(void)
{
	free(radar_ptr);
	radar_ptr = NULL;
	num_radar = 0;
	max_radar = 0;
}

static int32_t Frame_status(connection_t *connp, player_t *pl)
{
	player_t *lock_pl = NULL;
	static char mods[MAX_CHARS];
	int32_t n;
	int32_t lock_dist = 0, lock_dir = 0;
	int32_t showautopilot;

	/*
	 * Don't make lock visible during this frame if;
	 * 0) we are not player locked or compass is not on.
	 * 1) we have limited visibility and the player is out of range.
	 * 2) the player is invisible and he's not in our team.
	 * 3) he's not actively playing.
	 * 4) we have blind mode and he's not on the visible screen.
	 * 5) his distance is zero.
	 */

	CLR_BIT(pl->lock.flags, LOCK_VISIBLE);
	if (BIT(pl->lock.flags, LOCK_PLAYER) && Player_uses_property(pl, USES_COMPASS)) {
		lock_pl = pl->lock.object;

		if (Player_is_alive(lock_pl) && (playersOnRadar || inview(&lock_pl->pos)) && pl->lock.distance != 0) {
			SET_BIT(pl->lock.flags, LOCK_VISIBLE);
			lock_dir = (int32_t) Map_get_discrete_angle(&pl->pos, &lock_pl->pos);
			lock_dist = (int32_t) pl->lock.distance;
		}
	}

	showautopilot = 0;

	/*
	 * Don't forget to modify Receive_modifier_bank() in netserver.c
	 */
	mods[0] = '\0';
	n = Send_self(connp, pl, lock_pl, lock_dist, lock_dir, showautopilot,
			connp->pl->pl_old_status, mods);
	if (n <= 0) {
		return 0;
	}

//	if (BIT(pl->pl_old_status, SELF_DESTRUCT) && pl->self_destruct_count > 0) {
	if (Player_is_self_destructing(pl)) {
		Send_destruct(connp, pl->self_destruct_count);
	}

	return 1;
}

static void Frame_map(connection_t *connp, player_t *pl)
{
	int32_t i, x, y, conn_bit = (1 << connp->cid);
	block_visibility_t bv;

	if (Frame_is_real()) {
		x = pl->pos.bx;
		y = pl->pos.by;
	}
	else {
		x = pl->pos_interp.bx;
		y = pl->pos_interp.by;
	}

	bv.world.x = x - (horizontal_blocks >> 1);
	bv.world.y = y - (vertical_blocks >> 1);
	bv.realWorld = bv.world;
	if (BIT(World.rules->mode, WRAP_PLAY)) {
		if (bv.world.x < 0 && bv.world.x + horizontal_blocks < World.x) {
			bv.world.x += World.x;
		}
		else if (bv.world.x > 0 && bv.world.x + horizontal_blocks
				> World.x) {
			bv.realWorld.x -= World.x;
		}
		if (bv.world.y < 0 && bv.world.y + vertical_blocks < World.y) {
			bv.world.y += World.y;
		}
		else if (bv.world.y > 0 && bv.world.y + vertical_blocks > World.y) {
			bv.realWorld.y -= World.y;
		}
	}

	for (i = 0; i < World.NumFuels; i++) {
		if (!BIT(World.fuel[i].conn_mask, conn_bit)) {
			if (block_inview(&bv, &World.fuel[i].pos)) {
				Send_fuel(connp, &World.fuel[i]);
			}
		}
	}
}

void Frame_shuffle(void)
{
	int32_t i;
	uint16_t tmp, dest;

	if (last_frame_shuffle != frame_loops) {
		last_frame_shuffle = frame_loops;

		for (i = 0; i < NumObjs; i++) {
			object_shuffle[i] = i;
		}
		/* permute. */
		for (i = 0; i < NumObjs; i++) {
			dest = (int32_t) (rfrac() * NumObjs);
			tmp = object_shuffle[i];
			object_shuffle[i] = object_shuffle[dest];
			object_shuffle[dest] = tmp;
		}

		for (i = 0; i < NumPlayers; i++) {
			player_shuffle[i] = i;
		}

	}
}

void Frame_shots(connection_t *connp, player_t *pl)
{
	int32_t i, color, fuzz = 0, teamshot;
	object_t *obj;
	objposition_t *pos;

	for (i = 0; i < NumObjs; i++) {
		obj = Obj[object_shuffle[i]];

		if (Frame_is_real()) {
			pos = &obj->pos;
		}
		else {
			pos = &obj->pos_interp;
		}

		if (!inview(pos)) {
			continue;
		}
		if ((color = obj->color) == BLACK) {
			xpprintf("black %d\n", obj->type);
			color = WHITE;
		}

		switch (obj->type) {
		case OBJ_SPARK:
		case OBJ_DEBRIS:
			if ((fuzz >>= 7) < 0x40) {
				fuzz = rand();
			}
			if ((fuzz & 0x7F) >= spark_rand) {
				/*
				 * produce a sparkling effect by not displaying
				 * particles every frame.
				 */
				break;
			}
			/*
			 * The number of colors which the client
			 * uses for displaying debris is bigger than 2
			 * then the color used denotes the temperature
			 * of the debris particles.
			 * Higher color number means hotter debris.
			 */
			if (debris_colors >= 3) {
				if (debris_colors > 4) {
					if (color == BLUE) {
						color = (obj->obj_life >> 1);
					}
					else {
						color = (obj->obj_life >> 2);
					}
				}
				else {
					if (color == BLUE) {
						color = (obj->obj_life >> 2);
					}
					else {
						color = (obj->obj_life >> 3);
					}
				}
				if (color >= debris_colors) {
					color = debris_colors - 1;
				}
			}

			debris_store((int32_t) (pos->x - pv.world.x), (int32_t) (pos->y - pv.world.y), color);
			break;

		case OBJ_WRECKAGE:
			if (spark_rand != 0 || wreckageCollisionMayKill) {
				Send_wreckage(connp, pos, (uint8_t) obj->info, obj->size, obj->rotation);
			}
			break;

		case OBJ_SHOT:
			if (BIT(World.rules->mode, TEAM_PLAY) && teamImmunity
					&& obj->team == pl->team && obj->owner != pl) {
				color = BLUE;
				teamshot = DEBRIS_TYPES;
			}
			else {
				teamshot = 0;
			}

			fastshot_store((int32_t) (pos->x - pv.world.x), (int32_t) (pos->y - pv.world.y), color, teamshot);

			break;

		case OBJ_BALL:
			Send_ball(connp, pos, obj);
			break;
		default:
			error("Frame_shots: Shot type %d not defined.", obj->type);
			break;
		}
	}
}

static void Frame_ships(connection_t *connp, player_t *pl)
{
	player_t *pl_i;
	int32_t i, k;
	objposition_t *ball_pos;
	objposition_t *pl_pos;

	for (k = 0; k < NumPlayers; k++) {
		i = player_shuffle[k];
		pl_i = Players[i];

		ball_pos = NULL;

		if (Frame_is_real()) {
			pl_pos = &pl_i->pos;
		}
		else {
			pl_pos = &pl_i->pos_interp;
		}

		if (pl_i->ball_tmp) {
			if (Frame_is_real()) {
				ball_pos = &pl_i->ball_tmp->pos;
			}
			else {
				ball_pos = &pl_i->ball_tmp->pos_interp;
			}
		}

		if (!Player_is_alive(pl_i)) {
			continue;
		}

		if (!inview(pl_pos)) {
			continue;
		}

		/*
		 * Transmit ship information
		 */
		Send_ship(connp, pl_pos, pl_i->id, pl_i->dir,
				Player_uses_property(pl_i, USES_SHIELD), 0 != 0, 0 != 0, 0 != 0, 0 != 0);

		if (Player_uses_property(pl_i, USES_REFUEL)) {
			if (inview(&pl_i->fs->pos)) {
				Send_refuel(connp, &pl_i->fs->pos, pl_pos);
			}
		}

		if (ball_pos && inview(ball_pos)) {
			Send_connector(connp, ball_pos, pl_pos, 0);
		}
	}
}

static void Frame_radar(connection_t *connp, player_t *pl)
{
	player_t *pl2;
	int32_t i, s;
	int32_t x, y;

	if (playersOnRadar || BIT(World.rules->mode, TEAM_PLAY)) {
		for (i = 0; i < NumPlayers; i++) {
			pl2 = Players[i];

			/*
			 * Don't show on the radar:
			 *		Ourselves (not necessarily same as who we watch).
			 *		People who are not playing.
			 *		People in other teams if;
			 *			no playersOnRadar or if not visible
			 */
			if (pl2->connp == connp || !Player_is_alive(pl2) || (!TEAM(pl2, pl) && (!playersOnRadar))) {
				continue;
			}
                        
			if (Frame_is_real()) {
				x = pl2->pos.x;
				y = pl2->pos.y;
			}
			else {
				x = pl2->pos_interp.x;
				y = pl2->pos_interp.y;
			}

			if (Player_uses_property(pl, USES_COMPASS)
					&& BIT(pl->lock.flags, LOCK_PLAYER)
					&& pl->lock.object == pl2
					&& ((frame_loops / frameDivisor) % 5 >= 3)) {
				continue;
			}
			s = 3;
			if (TEAM(pl2, pl)) {
				s |= 0x80;
			}
			Frame_radar_buffer_add(x, y, s);
		}
	}
}

static void Frame_parameters(connection_t *connp, player_t *pl)
{
	Get_display_parameters(connp, &view_width, &view_height, &debris_colors, &spark_rand);
	debris_x_areas = (view_width + 255) >> 8;
	debris_y_areas = (view_height + 255) >> 8;
	debris_areas = debris_x_areas * debris_y_areas;
	horizontal_blocks = (view_width + (BLOCK_SZ - 1)) / BLOCK_SZ;
	vertical_blocks = (view_height + (BLOCK_SZ - 1)) / BLOCK_SZ;

	if (Frame_is_real()) {
		pv.world.x = pl->pos.x - view_width / 2; /* Scroll */
		pv.world.y = pl->pos.y - view_height / 2;
	}
	else {
		pv.world.x = pl->pos_interp.x - view_width / 2; /* Scroll */
		pv.world.y = pl->pos_interp.y - view_height / 2;
	}

	pv.realWorld = pv.world;
	if (BIT(World.rules->mode, WRAP_PLAY)) {
		if (pv.world.x < 0 && pv.world.x + view_width < World.width) {
			pv.world.x += World.width;
		}
		else if (pv.world.x > 0 && pv.world.x + view_width
				>= World.width) {
			pv.realWorld.x -= World.width;
		}
		if (pv.world.y < 0 && pv.world.y + view_height < World.height) {
			pv.world.y += World.height;
		}
		else if (pv.world.y > 0 && pv.world.y + view_height
				>= World.height) {
			pv.realWorld.y -= World.height;
		}
	}
}

void Frame_update(void)
{
	player_t *pl;
	player_t *pl2;
	connection_t *connp;
	int32_t i, player_fps;

	if (++frame_loops >= INT_MAX) /* Used for misc. timing purposes */
		frame_loops = 0;

	Frame_shuffle();

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if (!Player_is_connected(pl) || Player_is_robot(pl)) {
			continue;
		}

		connp = pl->connp;

		/*
		 * kps - with this implementation player fps can never
		 * be less than "real" fps, that is the number of "real"
		 * frames per second, typically about 12.
		 */
		player_fps = MIN(fps, pl->player_fps);
		if (player_fps < fps) {
			int32_t divisor = 2;

			while (fps > player_fps * divisor) {
				divisor *= 2;
			}

			if ((frame_cycle % divisor) != 0) {
				continue;
			}
		}

#if 0
		player_fps = MIN(fps, pl->player_fps);

		/*
		 * Reduce frame rate to player's own rate.
		 */
		if (player_fps < fps /*&& !ignoreMaxFPS*/) {
			int32_t divisor = (fps - 1) / player_fps + 1;
			/*	    printf("divisor %d player %d player2 %d intern:%d\n",
			 //	    divisor, player_fps, pl->player_fps,fps);
			 //	    fflush(stdout);
			 */
			if (frame_cycle % divisor != 0)
			continue;
		}

#endif

		//gettimeofday(&tv1, NULL);
		//printf("start:%e %d\n",timeval_to_seconds(tv1));

		if (Send_start_of_frame(connp) == -1) {
			continue;
		}

		/*
		 * Choose the correct player for generation of the frame content.
		 * Usually it will be the currently processed player (pl), unless he is
		 * in WAITING, DEAD or PAUSED state, has an active lock and his
		 * recovery time hasn't passed yet.
		 */
		if (BIT(pl->lock.flags, LOCK_PLAYER)) {
			/*
			 * An important consequence of the recovery timer here is that
			 * DEAD players will see themselves traveling back to home base
			 * first, and then (as soon as the timer expires) the view will
			 * switch to the locked player.
			 * Players in the WAITING and PAUSED state do not get the recovery
			 * time, so they see the locked player immediately after entering
			 * the state.
			 */
			if (Player_is_recovered(pl) &&
					(Player_is_waiting(pl) || Player_is_dead(pl) || Player_is_paused(pl))) {
				pl2 = pl->lock.object;
			}
			else {
				pl2 = pl;
			}
		}
		else {
			pl2 = pl;
		}

		//printf("%d\n",frame_cycle);
		Frame_parameters(connp, pl2);
		if (Frame_status(connp, pl2) <= 0) {
			continue;
		}
		Frame_map(connp, pl2);
		Frame_ships(connp, pl2);
		Frame_shots(connp, pl2);
		Frame_radar_buffer_reset();
		Frame_radar(connp, pl2);
		Frame_radar_buffer_send(connp);
		debris_end(connp);
		fastshot_end(connp);
		Send_end_of_frame(connp);
		//gettimeofday(&tv1, NULL);
		//printf("end:%e %d\n\n",timeval_to_seconds(tv1));
	}

	Frame_radar_buffer_free();
}

/*
 * Handles high-profile game-related messages (e.g. reason for round end) meant to be seen by all players.
 * NOTE: the maximum message length is MSG_LEN - 5.
 * The string will look like this: " < original_messsage >".
 */
void Message_game_important_print(const char *fmt, ...)
{
	player_t *pl;
	int32_t i;
	int32_t written;
	va_list ap;

	va_start(ap, fmt);

	written = vsnprintf(message, sizeof(message) - 1, fmt, ap);
	message[written] = 0;

	/* Make sure the extended string will fit */
	message[sizeof(message) - 5] = 0;

	/* Move the whole message to make room for the " < " prefix */
	i = written;
	while (i >= 0) {
		message[i + 3] = message[i];
		i--;
	}

	/* Insert the prefix and suffix */
	message[0] = ' '; message[1] = '<'; message[2] = ' ';
	strcat(message, " >");

	/* Send to all human players */
	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];

		if (Player_is_connected(pl)) {
			Send_message(pl->connp, message);
		}
	}

	va_end(ap);
}

/*
 * Handles low-profile game-related messages (e.g. joins, leaves, kills) meant to be seen by all players.
 * NOTE: the maximum message length is MSG_LEN.
 */
void Message_game_print(const char *fmt, ...)
{
	player_t *pl;
	int32_t i;
	int32_t written;
	va_list ap;

	va_start(ap, fmt);

	written = vsnprintf(message, sizeof(message) - 1, fmt, ap);
	message[written] = 0;

	for (i = 0; i < NumPlayers; i++) {
		pl = Players[i];
		if (Player_is_connected(pl)) {
			Send_message(pl->connp, message);
		}
	}

	va_end(ap);
}

/*
 * Handles a message sent privately to one player (a private talk message, a team message
 * or the server reply to player's action).
 * NOTE: the maximum message length is MSG_LEN - length_of_the_server_string - 1.
 */
void Message_player_print(player_t *pl, player_msg_t type, const char *fmt, ...)
{
	int32_t written;
	va_list ap;
	char *server_str;

	va_start(ap, fmt);

	written = vsnprintf(message, sizeof(message) - 1, fmt, ap);
	message[written] = 0;

	/* choose and add the server string */
	if (type == PL_MSG_GREETING) {
		server_str = (char *) server_greeting_str;
	}
	else if (type == PL_MSG_NOTICE) {
		server_str = (char *) server_notice_str;
	}
	else if (type == PL_MSG_REPLY) {
		server_str = (char *) server_reply_str;
	}
	else {
		server_str = NULL;
	}

	/* attach add the server string at the end of the message */
	if (server_str) {
		if (strlen(message) + strlen(server_str) + 1 > sizeof(message)) {
			message[sizeof(message) - 1 - strlen(server_str)] = 0;
		}
		strcat(message, server_str);
	}

	if (Player_is_connected(pl)) {
		Send_message(pl->connp, message);
	}
	else if (Player_is_robot(pl)) {
		Robot_message(pl, message);
	}

	va_end(ap);
}
