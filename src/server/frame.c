/* $Id: frame.c,v 4.19 2000/10/15 13:09:55 bert Exp $
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

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "xpconfig.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "bit.h"
#include "netserver.h"
#include "error.h"

char frame_version[] = VERSION;

/*
 * Structure for calculating if a pixel is visible by a player.
 * The following always holds:
 *	(world.x >= realWorld.x && world.y >= realWorld.y)
 */
typedef struct {
    position	world;			/* Lower left hand corner is this */
					/* world coordinate */
    position	realWorld;		/* If the player is on the edge of
					   the screen, these are the world
					   coordinates before adjustment... */
} pixel_visibility_t;

/*
 * Structure with player position info measured in blocks instead of pixels.
 * Used for map state info updating.
 */
typedef struct {
    ipos		world;
    ipos		realWorld;
} block_visibility_t;

typedef struct {
    unsigned char	x, y;
} debris_t;

typedef struct {
    short		x, y, size;
} radar_t;


long			frame_loops = 1;
static long		last_frame_shuffle;
static unsigned short	object_shuffle[MAX_TOTAL_SHOTS];
static unsigned short	player_shuffle[64 + MAX_PSEUDO_PLAYERS];
static radar_t		*radar_ptr;
static int		num_radar, max_radar;

static pixel_visibility_t pv;
static int		view_width,
			view_height,
			horizontal_blocks,
			vertical_blocks,
			debris_x_areas,
			debris_y_areas,
			debris_areas,
			debris_colors,
			spark_rand;
static debris_t		*debris_ptr[DEBRIS_TYPES];
static unsigned		debris_num[DEBRIS_TYPES],
			debris_max[DEBRIS_TYPES];
static debris_t		*fastshot_ptr[DEBRIS_TYPES * 2];
static unsigned		fastshot_num[DEBRIS_TYPES * 2],
			fastshot_max[DEBRIS_TYPES * 2];
int gameSpeed;

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

#define inview(x_, y_) 								\
    (   (   ((x_) > pv.world.x && (x_) < pv.world.x + view_width)		\
	 || ((x_) > pv.realWorld.x && (x_) < pv.realWorld.x + view_width))	\
     && (   ((y_) > pv.world.y && (y_) < pv.world.y + view_height)		\
	 || ((y_) > pv.realWorld.y && (y_) < pv.realWorld.y + view_height)))

static int block_inview(block_visibility_t *bv, int x, int y)
{
    return ((x > bv->world.x && x < bv->world.x + horizontal_blocks)
	    || (x > bv->realWorld.x && x < bv->realWorld.x + horizontal_blocks))
	&& ((y > bv->world.y && y < bv->world.y + vertical_blocks)
	    || (y > bv->realWorld.y && y < bv->realWorld.y + vertical_blocks));
}

#define DEBRIS_STORE(xd,yd,color,offset) \
    int			i;						  \
    if (xd < 0) {							  \
	xd += World.width;						  \
    }									  \
    if (yd < 0) {							  \
	yd += World.height;						  \
    }									  \
    if ((unsigned) xd >= (unsigned)view_width || (unsigned) yd >= (unsigned)view_height) {	  \
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
    ptr_[num_].x = (unsigned char) xd;					  \
    ptr_[num_].y = (unsigned char) yd;					  \
    num_++;

static void fastshot_store(int xf, int yf, int color, int offset)
{
#define ptr_		(fastshot_ptr[i])
#define num_		(fastshot_num[i])
#define max_		(fastshot_max[i])
    DEBRIS_STORE(xf, yf, color, offset);
#undef ptr_
#undef num_
#undef max_
}

static void debris_store(int xf, int yf, int color)
{
#define ptr_		(debris_ptr[i])
#define num_		(debris_num[i])
#define max_		(debris_max[i])
    DEBRIS_STORE(xf, yf, color, 0);
#undef ptr_
#undef num_
#undef max_
}

static void fastshot_end(int conn)
{
    int			i;

    for (i = 0; i < DEBRIS_TYPES * 2; i++) {
	if (fastshot_num[i] != 0) {
	    Send_fastshot(conn, i,
			  (unsigned char *) fastshot_ptr[i],
			  fastshot_num[i]);
	    fastshot_num[i] = 0;
	}
    }
}

static void debris_end(int conn)
{
    int			i;

    for (i = 0; i < DEBRIS_TYPES; i++) {
	if (debris_num[i] != 0) {
	    Send_debris(conn, i,
			(unsigned char *) debris_ptr[i],
			debris_num[i]);
	    debris_num[i] = 0;
	}
    }
}

static void Frame_radar_buffer_reset(void)
{
    num_radar = 0;
}

static void Frame_radar_buffer_add(int x, int y, int s)
{
    radar_t		*p;

    EXPAND(radar_ptr, num_radar, max_radar, radar_t, 1);
    p = &radar_ptr[num_radar++];
    p->x = x;
    p->y = y;
    p->size = s;
}

static void Frame_radar_buffer_send(int conn)
{
    int			i;
    int			dest;
    int			tmp;
    radar_t		*p;
    const int		radar_width = 256;
    int			radar_height = (radar_width * World.y) / World.x;
    int			radar_x;
    int			radar_y;
    int			send_x;
    int			send_y;
    unsigned short	radar_shuffle[MAX_TOTAL_SHOTS];

    for (i = 0; i < num_radar; i++) {
	radar_shuffle[i] = i;
    }
    /* permute. */
    for (i = 0; i < num_radar; i++) {
	dest = (int)(rfrac() * num_radar);
	tmp = radar_shuffle[i];
	radar_shuffle[i] = radar_shuffle[dest];
	radar_shuffle[dest] = tmp;
    }

    for (i = 0; i < num_radar; i++) {
	p = &radar_ptr[radar_shuffle[i]];
	radar_x = (radar_width * p->x) / World.width;
	radar_y = (radar_height * p->y) / World.height;
	send_x = (World.width * radar_x) / radar_width;
	send_y = (World.height * radar_y) / radar_height;
	Send_radar(conn, send_x, send_y, p->size);
    }
}

static void Frame_radar_buffer_free(void)
{
    free(radar_ptr);
    radar_ptr = NULL;
    num_radar = 0;
    max_radar = 0;
}

static int Frame_status(int conn, int ind)
{
    static char		mods[MAX_CHARS];
    player		*pl = Players[ind];
    int			n,
			lock_ind,
			lock_id = -1,
			lock_dist = 0,
			lock_dir = 0,
			showautopilot;


    
    /*
     * Don't make lock visible during this frame if;
     * 0) we are not player locked or compass is not on.
     * 1) we have limited visibility and the player is out of range.
     * 2) the player is invisible and he's not in our team.
     * 3) he's not actively playing.
     * 4) we have blind mode and he's not on the visible screen.
     * 5) his distance is zero.
     */

    CLR_BIT(pl->lock.tagged, LOCK_VISIBLE);
    if (BIT(pl->lock.tagged, LOCK_PLAYER) && BIT(pl->used, OBJ_COMPASS)) {
	lock_id = pl->lock.pl_id;
	lock_ind = GetInd[lock_id];
	
	if ((pl->visibility[lock_ind].canSee || TEAM(ind, lock_ind))
	    && BIT(Players[lock_ind]->status, PLAYING|GAME_OVER) == PLAYING
	    && (playersOnRadar
		|| inview(Players[lock_ind]->pos.x, Players[lock_ind]->pos.y))
	    && pl->lock.distance != 0) {
	  SET_BIT(pl->lock.tagged, LOCK_VISIBLE);
	  lock_dir = (int)Wrap_findDir((int)(Players[lock_ind]->pos.x - pl->pos.x),
				       (int)(Players[lock_ind]->pos.y - pl->pos.y));
	  lock_dist = (int)pl->lock.distance;
	}
    }
    
    showautopilot = 0;

    /*
     * Don't forget to modify Receive_modifier_bank() in netserver.c
     */
    mods[0] = '\0';
    n = Send_self(conn,
		  pl,
		  lock_id,
		  lock_dist,
		  lock_dir,
		  showautopilot,
		  Players[GetInd[Get_player_id(conn)]]->status,
		  mods);
    if (n <= 0) {
	return 0;
    }
    if (BIT(pl->status, SELF_DESTRUCT) && pl->count > 0) {
	Send_destruct(conn, pl->count);
    }
    if (ShutdownServer != -1) {
	Send_shutdown(conn, ShutdownServer, ShutdownDelay);
    }

    return 1;
}

static void Frame_map(int conn, int ind)
{
    player		*pl = Players[ind];
    int			i,
			x,
			y,
			conn_bit = (1 << conn);
    block_visibility_t	bv;

    if ((main_loops % frameDivisor) == 0){
      x = pl->pos_interp.bx;
      y = pl->pos_interp.by;
    }

     if ((main_loops % frameDivisor) != 0){
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
	else if (bv.world.x > 0 && bv.world.x + horizontal_blocks > World.x) {
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
	if (BIT(World.fuel[i].conn_mask, conn_bit) == 0) {
	    if (World.block[World.fuel[i].blk_pos.x]
			   [World.fuel[i].blk_pos.y] == FUEL) {
		if (block_inview(&bv,
				 World.fuel[i].blk_pos.x,
				 World.fuel[i].blk_pos.y)) {
		    Send_fuel(conn, i, (int) World.fuel[i].fuel);
		}
	    }
	}
    }
}

static void Frame_shuffle(void)
{
    int			i;
    unsigned short	tmp, dest;

    if (last_frame_shuffle != frame_loops) {
	last_frame_shuffle = frame_loops;

	for (i = 0; i < NumObjs; i++) {
	    object_shuffle[i] = i;
	}
	/* permute. */
	for (i = 0; i < NumObjs; i++) {
	    dest = (int)(rfrac() * NumObjs);
	    tmp = object_shuffle[i];
	    object_shuffle[i] = object_shuffle[dest];
	    object_shuffle[dest] = tmp;
	}

	for (i = 0; i < NumPlayers; i++) {
	    player_shuffle[i] = i;
	}

	
	
    }
}

static void Frame_shots(int conn, int ind)
{
    player			*pl = Players[ind];
    int				i, color, x, y, fuzz = 0, teamshot;
    object			*shot;

    for (i = 0; i < NumObjs; i++) {
	shot = Obj[object_shuffle[i]];
	if ((main_loops % frameDivisor) == 0){
	  x = shot->pos.x;
	  y = shot->pos.y;
	}
	
	if ((main_loops % frameDivisor) != 0){
	  x = shot->pos_interp.x;
	  y = shot->pos_interp.y;
	}


	if (!inview(x, y)) {
	    continue;
	}
	if ((color = shot->color) == BLACK) {
	    xpprintf("black %d,%d\n", shot->type, shot->id);
	    color = WHITE;
	}
	switch (shot->type) {
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
			color = (shot->life >> 1);
		    } else {
			color = (shot->life >> 2);
		    }
		} else {
		    if (color == BLUE) {
			color = (shot->life >> 2);
		    } else {
			color = (shot->life >> 3);
		    }
		}
		if (color >= debris_colors) {
		    color = debris_colors - 1;
		}
	    }

	    debris_store((int)(x - pv.world.x),
			 (int)(y - pv.world.y),
			 color);
	    break;

	case OBJ_WRECKAGE:
	    if (spark_rand != 0 || wreckageCollisionMayKill) {
		Send_wreckage(conn, x, y,
			      (u_byte)shot->info, shot->size, shot->rotation);
	    }
	    break;

	case OBJ_SHOT:
	    if (BIT(World.rules->mode, TEAM_PLAY)
		&& teamImmunity
		&& shot->team == pl->team
		&& shot->id != pl->id) {
		color = BLUE;
		teamshot = DEBRIS_TYPES;
	    } else {
		teamshot = 0;
	    }

	    fastshot_store((int)(x - pv.world.x),
			   (int)(y - pv.world.y),
			   color, teamshot);
	   
	    break;

	case OBJ_BALL:
	    Send_ball(conn, x, y, shot->id);
	    break;
	default:
	    error("Frame_shots: Shot type %d not defined.", shot->type);
	    break;
	}
    }
}

static void Frame_ships(int conn, int ind)
{
    player			*pl = Players[ind],
				*pl_i;
    int				i, k;
    int                         pl_posx, pl_posy, ball_posx, ball_posy;
    for (k = 0; k < NumPlayers; k++) {
      i = player_shuffle[k];
      pl_i = Players[i];
      
      if ((main_loops % frameDivisor) == 0){
	pl_posx = pl_i->pos.x;
	pl_posy = pl_i->pos.y;
	if (pl_i->ball != NULL){
	  ball_posx = pl_i->ball->pos.x;
	  ball_posy = pl_i->ball->pos.y;
	}
      }
      
      if ((main_loops % frameDivisor) != 0){
	pl_posx = pl_i->pos_interp.x;
	pl_posy = pl_i->pos_interp.y;
	if (pl_i->ball != NULL){
	  ball_posx = pl_i->ball->pos_interp.x;
	  ball_posy = pl_i->ball->pos_interp.y;
	}
      }
      

      
      if (!BIT(pl_i->status, PLAYING|PAUSE)) {
	continue;
      }
      if (BIT(pl_i->status, GAME_OVER)) {
	continue;
      }
      if (!inview(pl_posx, pl_posy)) {
	continue;
      }
      if (BIT(pl_i->status, PAUSE)) {
	Send_paused(conn,
		    pl_posx,
		    pl_posy,
		    pl_i->count);
	continue;
      }
      
      /* Don't transmit information if fighter is invisible */
      if (pl->visibility[i].canSee
	  || i == ind
	    || TEAM(i, ind)) {
	/*
	 * Transmit ship information
	 */
	Send_ship(conn,
		  pl_posx,
		  pl_posy,
		  pl_i->id,
		  pl_i->dir,
		  BIT(pl_i->used, OBJ_SHIELD) != 0,
		  0 != 0,
		  0 != 0,
		  0 != 0,
		  0 != 0
		  );
      }
      
      if (BIT(pl_i->used, OBJ_REFUEL)) {
	if (inview(World.fuel[pl_i->fs].pix_pos.x,
		   World.fuel[pl_i->fs].pix_pos.y)) {
	  Send_refuel(conn,
		      (int)World.fuel[pl_i->fs].pix_pos.x,
		      (int)World.fuel[pl_i->fs].pix_pos.y,
		      pl_posx,
		      pl_posy);
	}
      }
	
      if (pl_i->ball != NULL
	  && inview(ball_posx, ball_posy)) {
	
	Send_connector(conn,
		       ball_posx,
		       ball_posy,
		       pl_posx,
		       pl_posy, 0);
      }
      
    }
}

    
static void Frame_radar(int conn, int ind)
{
    int			i, mask, s;
    player		*pl = Players[ind];
    object		*shot;
    DFLOAT		x, y;

    if (playersOnRadar || BIT(World.rules->mode, TEAM_PLAY)) {
      for (i = 0; i < NumPlayers; i++) {
	    /*
	     * Don't show on the radar:
	     *		Ourselves (not necessarily same as who we watch).
	     *		People who are not playing.
	     *		People in other teams if;
	     *			no playersOnRadar or if not visible
	     */
	    if (Players[i]->conn == conn
		|| BIT(Players[i]->status, PLAYING|PAUSE|GAME_OVER) != PLAYING
		|| (!TEAM(i, ind)
		    && (!playersOnRadar || !pl->visibility[i].canSee))) {
		continue;
	    }
	    x = Players[i]->pos.x;
	    y = Players[i]->pos.y;
	    if (BIT(pl->used, OBJ_COMPASS)
		&& BIT(pl->lock.tagged, LOCK_PLAYER)
		&& GetInd[pl->lock.pl_id] == i
		&& frame_loops % 5 >= 3) {
		continue;
	    }
	    s = 3;
	    if (TEAM(i, ind)) {
		s |= 0x80;
	    }
	   Frame_radar_buffer_add((int)x, (int)y, s);
	}
    }
}

static void Frame_parameters(int conn, int ind)
{
    player		*pl = Players[ind];

    Get_display_parameters(conn, &view_width, &view_height,
			   &debris_colors, &spark_rand);
    debris_x_areas = (view_width + 255) >> 8;
    debris_y_areas = (view_height + 255) >> 8;
    debris_areas = debris_x_areas * debris_y_areas;
    horizontal_blocks = (view_width + (BLOCK_SZ - 1)) / BLOCK_SZ;
    vertical_blocks = (view_height + (BLOCK_SZ - 1)) / BLOCK_SZ;

    if ((main_loops % frameDivisor) == 0){
      pv.world.x = pl->pos.x - view_width / 2;	/* Scroll */
      pv.world.y = pl->pos.y - view_height / 2;
    }

    if ((main_loops % frameDivisor) != 0){
      pv.world.x = pl->pos_interp.x - view_width / 2;	/* Scroll */
      pv.world.y = pl->pos_interp.y - view_height / 2;
    }


    pv.realWorld = pv.world;
    if (BIT (World.rules->mode, WRAP_PLAY)) {
	if (pv.world.x < 0 && pv.world.x + view_width < World.width) {
	    pv.world.x += World.width;
	}
	else if (pv.world.x > 0 && pv.world.x + view_width >= World.width) {
	    pv.realWorld.x -= World.width;
	}
	if (pv.world.y < 0 && pv.world.y + view_height < World.height) {
	    pv.world.y += World.height;
	}
	else if (pv.world.y > 0 && pv.world.y + view_height >= World.height) {
	    pv.realWorld.y -= World.height;
	}
    }
}

void Frame_update(void)
{
    int			i, conn, ind, player_fps;
    player		*pl;
    time_t		newTimeLeft = 0;
    static time_t	oldTimeLeft;
    static bool		game_over_called = false;

    if (++frame_loops >= LONG_MAX)	/* Used for misc. timing purposes */
	frame_loops = 0;

    Frame_shuffle();

    if (gameDuration > 0.0
	&& game_over_called == false
	&& oldTimeLeft != (newTimeLeft = gameOverTime - time(NULL))) {
	/*
	 * Do this once a second.
	 */
	if (newTimeLeft <= 0) {
	    Game_Over();
	    ShutdownServer = 30 * FPS;	/* Shutdown in 30 seconds */
	    game_over_called = true;
	}
    }

    for (i = 0; i < NumPlayers; i++) {
	pl = Players[i];
	conn = pl->conn;
	if (conn == NOT_CONNECTED) {
	    continue;
	}
	player_fps = internalFps;
	if (BIT(pl->status, PAUSE|GAME_OVER)
	    && !allowViewing
	    && !pl->isowner) {
	    /*
	     * Lower the frame rate for non-playing players
	     * to reduce network load.
	     * Owner always gets full framerate even if paused.
	     * With allowViewing on, everyone gets full framerate.
	     */
	    if (BIT(pl->status, PAUSE)) {
		if (frame_loops & 0x03) {
		    continue;
		}
	    } else {
		if (frame_loops & 0x01) {
		    continue;
		}
	    }
	}
	player_fps = MIN(player_fps, pl->player_fps);

	/*
	 * Reduce frame rate to player's own rate.
	 */
	if (player_fps < internalFps /*&& !ignoreMaxFPS*/) {
	    int divisor = (internalFps - 1) / player_fps + 1;
	    /*	    printf("divisor %d player %d player2 %d intern:%d\n",
		    divisor, player_fps, pl->player_fps,internalFps);
		    fflush(stdout);
	    */
	    if (frame_loops % divisor)
 		continue;
	}


	if (Send_start_of_frame(conn) == -1) {
	    continue;
	}
	if (newTimeLeft != oldTimeLeft) {
	    Send_time_left(conn, newTimeLeft);
	}
	/*
	 * If status is GAME_OVER or PAUSE'd, the user may look through the
	 * other players 'eyes'.  If PAUSE'd this only works on team members.
	 * We can't use TEAM() macro as PAUSE'd players are always on
	 * equivalent teams.
	 *
	 * This is done by using two indexes, one
	 * determining which data should be used (ind, set below) and
	 * one determining which connection to send it to (conn).
	 */
	if (BIT(pl->lock.tagged, LOCK_PLAYER)) {
	    if ((BIT(pl->status, (GAME_OVER|PLAYING)) == (GAME_OVER|PLAYING))
		|| (BIT(pl->status, PAUSE) &&
		    ((BIT(World.rules->mode, TEAM_PLAY)
		      && pl->team != TEAM_NOT_SET
		      && pl->team == Players[GetInd[pl->lock.pl_id]]->team)
		    || pl->isowner
		    || allowViewing))) {
		ind = GetInd[pl->lock.pl_id];
	    } else {
		ind = i;
	    }
	} else {
	    ind = i;
	}
	if (Players[ind]->damaged > 0) {
	    Send_damaged(conn, Players[ind]->damaged);
	} else {
	    Frame_parameters(conn, ind);
	    if (Frame_status(conn, ind) <= 0) {
		continue;
	    }
	    Frame_map(conn, ind);
	    Frame_ships(conn, ind);
	    Frame_shots(conn, ind);
	    Frame_radar_buffer_reset();
	    Frame_radar(conn, ind);
	    Frame_radar_buffer_send(conn);
	    debris_end(conn);
	    fastshot_end(conn);
	}
	Send_end_of_frame(conn);
    }
    oldTimeLeft = newTimeLeft;

    Frame_radar_buffer_free();
}

void Set_message(const char *message)
{
    player		*pl;
    int			i;
    const char		*msg;
    char		tmp[MSG_LEN];

    if ((i = strlen(message)) >= MSG_LEN) {
#ifndef SILENT
	errno = 0;
	error("Max message len exceed (%d,%s)", i, message);
#endif
	strncpy(tmp, message, MSG_LEN - 1);
	tmp[MSG_LEN - 1] = '\0';
	msg = tmp;
    } else {
	msg = message;
    }
    for (i = 0; i < NumPlayers; i++) {
	pl = Players[i];
	if (pl->conn != NOT_CONNECTED) {
	    Send_message(pl->conn, msg);
	}
    }
}

void Set_player_message(player *pl, const char *message)
{
    int			i;
    const char		*msg;
    char		tmp[MSG_LEN];

    if ((i = strlen(message)) >= MSG_LEN) {
#ifndef SILENT
	errno = 0;
	error("Max message len exceed (%d,%s)", i, message);
#endif
	memcpy(tmp, message, MSG_LEN - 1);
	tmp[MSG_LEN - 1] = '\0';
	msg = tmp;
    } else {
	msg = message;
    }
    if (pl->conn != NOT_CONNECTED) {
	Send_message(pl->conn, msg);
    }
    else if (IS_ROBOT_PTR(pl)) {
	Robot_message(GetInd[pl->id], msg);
    }
}

