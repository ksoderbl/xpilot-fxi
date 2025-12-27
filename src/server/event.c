/* $Id: event.c,v 4.14 2000/03/22 21:29:53 bert Exp $
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-98 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
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
#include <string.h>
#include <stdio.h>

#define SERVER
#include "version.h"
#include "config.h"
#include "serverconst.h"
#include "global.h"
#include "proto.h"
#include "score.h"
#include "map.h"
#include "bit.h"
#include "netserver.h"

char event_version[] = VERSION;

#define SWAP(_a, _b)	    {DFLOAT _tmp = _a; _a = _b; _b = _tmp;}

/*
 * Globals.
 */
static char		msg[MSG_LEN];


/* rewrote this (was a real ugly hack) -pgm */
static void Refuel(int ind)
{
    player *pl = Players[ind];
    int i;
    int xpos, ypos;
    DFLOAT l, dist = 100;
    
    if ((main_loops % frameDivisor) == 0){
      xpos = pl->pos.x;
      ypos = pl->pos.y;
    }
    
    if ((main_loops % frameDivisor) != 0){
      xpos = pl->pos_interp.x;
      ypos = pl->pos_interp.y;
    }



    CLR_BIT(pl->used, OBJ_REFUEL);
    for (i = 0; i < World.NumFuels; i++) {
	if (World.block[World.fuel[i].blk_pos.x]
		       [World.fuel[i].blk_pos.y] == FUEL) {
	    l = Wrap_length(xpos - World.fuel[i].pix_pos.x,
			    ypos - World.fuel[i].pix_pos.y);
	    if (l < dist) {
		SET_BIT(pl->used, OBJ_REFUEL);
		pl->fs = i;
	    }
	}
    }
}


bool team_dead(int team)
{
    int i;
    bool alive = false;

    for (i = 0; i < NumPlayers; i++) {
	if (Players[i]->team == team &&
	    BIT(Players[i]->status, PLAYING|GAME_OVER) == PLAYING) {
	    alive = true;
	    break;
	}
    }
    return (!alive);
}

/*
 * Return true if a lock is allowed.
 */
bool Player_lock_allowed(int ind, int lock)
{
    player		*pl = Players[ind];

    /* we can never lock on ourselves, nor on -1. */
    if (ind == lock || lock == -1) {
	return false;
    }

    /* if we are actively playing then we can lock since we are not viewing. */
    if (BIT(pl->status, PLAYING|PAUSE|GAME_OVER) == PLAYING) {
	return true;
    }

    /* if there is no team play then we can always lock on anyone. */
    if (!BIT(World.rules->mode, TEAM_PLAY)) {
	return true;
    }

    /* we can always lock on players from our own team. */
    if (TEAM(ind, lock)) {
	return true;
    }

    /* if lockOtherTeam is true then we can always lock on other teams. */
    if (lockOtherTeam) {
	return true;
    }

    /* if our own team is dead then we can lock on anyone. */
    if (team_dead(pl->team)) {
	return true;
    }

    /* can't find any reason why this lock should be allowed. */
    return false;
}

/*
 * Sven Mascheck:
 * If all _opponents are paused, then even LOCK_NEXT (ot LOCK_PREV)
 * might not lock_next (or lock_prev), as Player_lock_closest() might
 * be called  [ "event.c" line 466 ] :
 * This happens when the player is not locked on any one anymore -
 * and this happens if he tried to lock_closest before (if all
 * opponents are paused).
 * Player_lock_closest() is called with (ind, 0) and that means that
 * the lock is cleared in _any case_ with the current code - that could
 * be done without calling Player_lock_closest().
 * (btw, code in Player_lock_closest() looks like 'evolutionary code :)
 * I am not sure where to fix that locking problem
 * ( in "case KEY_LOCK_NEXT" or in Player_lock_closest() ).
 * I tried to find a solution but now i am bit screwed up..  :)
 */
int Player_lock_closest(int ind, int next)
{
    player *pl = Players[ind];
    int lock, i, newpl;
    DFLOAT dist, best, l;

    if (!next) {
	CLR_BIT(pl->lock.tagged, LOCK_PLAYER);
    }

    if (BIT(pl->lock.tagged, LOCK_PLAYER)) {
	lock = GetInd[pl->lock.pl_id];
	dist = Wrap_length(Players[lock]->pos.x - pl->pos.x,
			   Players[lock]->pos.y - pl->pos.y);
    } else {
	lock = -1;
	dist = 0.0;
    }
    newpl = -1;
    best = FLT_MAX;
    for (i = 0; i < NumPlayers; i++) {
	if (i == lock
	    || (BIT(Players[i]->status, PLAYING|PAUSE|GAME_OVER) != PLAYING)
	    || !Player_lock_allowed(ind, i)
	    || TEAM(ind,i)) {
	    continue;
	}
	l = Wrap_length(Players[i]->pos.x - pl->pos.x,
			Players[i]->pos.y - pl->pos.y);
	if (l >= dist && l < best) {
	    best = l;
	    newpl = i;
	}
    }
    if (newpl == -1) {
	return 0;
    }

    SET_BIT(pl->lock.tagged, LOCK_PLAYER);
    pl->lock.pl_id = Players[newpl]->id;

    return 1;
}


void Pause_player(int ind, int onoff)
{
    player		*pl = Players[ind];
    int			i;

    if (onoff != 0 && !BIT(pl->status, PAUSE)) { /* Turn pause mode on */
	pl->count = 10*FPS;
	pl->updateVisibility = 1;
	CLR_BIT(pl->status, SELF_DESTRUCT|PLAYING);
	SET_BIT(pl->status, PAUSE);
	pl->mychar = 'P';
	updateScores = true;
	if (BIT(pl->have, OBJ_BALL))
	    Detach_ball(ind, -1);
    }
    else if (onoff == 0 && BIT(pl->status, PAUSE)) { /* Turn pause mode off */
	if (pl->count <= 0) {
	    bool toolate = false;

	    CLR_BIT(pl->status, PAUSE);
	    updateScores = true;
	    if (BIT(World.rules->mode, LIMITED_LIVES)) {
		for (i = 0; i < NumPlayers; i++) {
		    /* If a non-team member has lost a life,
		     * then it's too late to join. */
		    if (i == ind) {
			continue;
		    }
		    if (Players[i]->life < World.rules->lives && !TEAM(ind, i)) {
			toolate = true;
			break;
		    }
		}
	    }
	    if (toolate) {
		pl->life = 0;
		pl->mychar = 'W';
		SET_BIT(pl->status, GAME_OVER);
	    } else {
		pl->mychar = ' ';
		Go_home(ind);
		SET_BIT(pl->status, PLAYING);
		if (BIT(World.rules->mode, LIMITED_LIVES)) {
		    pl->life = World.rules->lives;
		}
	    }
	}
    }
}


int Handle_keyboard(int ind)
{
    player  	*pl = Players[ind];
    int	    	i, j, k, key, pressed, xi, yi;
    DFLOAT  	minv;


    for (key = 0; key < NUM_KEYS; key++) {
	if (pl->last_keyv[key / BITV_SIZE] == pl->prev_keyv[key / BITV_SIZE]) {
	    key |= (BITV_SIZE - 1);	/* Skip to next keyv element */
	    continue;
	}
	while (BITV_ISSET(pl->last_keyv, key)
	       == BITV_ISSET(pl->prev_keyv, key)) {
	    if (++key >= NUM_KEYS) {
		break;
	    }
	}
	if (key >= NUM_KEYS) {
	    break;
	}
	pressed = BITV_ISSET(pl->last_keyv, key) != 0;
	BITV_TOGGLE(pl->prev_keyv, key);
	if (key != KEY_SHIELD)	/* would interfere with auto-idle-pause.. */
	    pl->frame_last_busy = frame_loops; /* ok -pgm due to client auto-shield */

	/*
	 * Allow these functions while you're 'dead'.
	 */
	if (BIT(pl->status, PLAYING|GAME_OVER|PAUSE) != PLAYING) {
	    switch (key) {
	    case KEY_PAUSE:
	    case KEY_LOCK_NEXT:
	    case KEY_LOCK_PREV:
	    case KEY_LOCK_CLOSE:
	    case KEY_LOCK_NEXT_CLOSE:
	    case KEY_TOGGLE_VELOCITY:
	    case KEY_TOGGLE_POWER:
	    case KEY_TOGGLE_COMPASS:
	    case KEY_LOAD_MODIFIERS_1:
	    case KEY_LOAD_MODIFIERS_2:
	    case KEY_LOAD_MODIFIERS_3:
	    case KEY_LOAD_MODIFIERS_4:
	    case KEY_LOAD_LOCK_1:
	    case KEY_LOAD_LOCK_2:
	    case KEY_LOAD_LOCK_3:
	    case KEY_LOAD_LOCK_4:
	    case KEY_SWAP_SETTINGS:
	    case KEY_INCREASE_POWER:
	    case KEY_DECREASE_POWER:
	    case KEY_INCREASE_TURNSPEED:
	    case KEY_DECREASE_TURNSPEED:
	    case KEY_TANK_NEXT:
	    case KEY_TANK_PREV:
	    case KEY_TURN_LEFT:		/* Needed so that we don't get */
	    case KEY_TURN_RIGHT:	/* out-of-sync with the turnacc */
		break;
	    default:
		continue;
	    }
	}

	if (pressed) { /* --- KEYPRESS --- */
	    switch (key) {

	    case KEY_TANK_NEXT:
	    case KEY_TANK_PREV:
		if (pl->fuel.num_tanks) {
		    pl->fuel.current += (key==KEY_TANK_NEXT) ? 1 : -1;
		    if (pl->fuel.current < 0)
			pl->fuel.current = pl->fuel.num_tanks;
		    else if (pl->fuel.current > pl->fuel.num_tanks)
			pl->fuel.current = 0;
		}
		break;

	    case KEY_LOCK_NEXT:
	    case KEY_LOCK_PREV:
		j = i = GetInd[pl->lock.pl_id];
		if (!BIT(pl->lock.tagged, LOCK_PLAYER)
		    || j < 0 || j >= NumPlayers) {
		    /* better jump to KEY_LOCK_CLOSE... */
		    Player_lock_closest(ind, 0);
		    break;
		}
		do {
		    if (key == KEY_LOCK_PREV) {
			if (--i < 0) {
			    i = NumPlayers - 1;
			}
		    } else {
			if (++i >= NumPlayers) {
			    i = 0;
			}
		    }
		    if (i == j)
			break;
		} while (i == ind
			 || BIT(Players[i]->status, GAME_OVER|PAUSE)
			 || !Player_lock_allowed(ind, i));
		if (i == ind) {
		    CLR_BIT(pl->lock.tagged, LOCK_PLAYER);
		}
		else {
		    pl->lock.pl_id = Players[i]->id;
		    SET_BIT(pl->lock.tagged, LOCK_PLAYER);
		}
		break;

	    case KEY_TOGGLE_COMPASS:
		if (!BIT(pl->have, OBJ_COMPASS))
		    break;
		TOGGLE_BIT(pl->used, OBJ_COMPASS);
		if (BIT(pl->used, OBJ_COMPASS) == 0) {
		    break;
		}
		/*
		 * Verify if the lock has ever been initialized at all
		 * and if the lock is still valid.
		 */
		if (BIT(pl->lock.tagged, LOCK_PLAYER)
		    && NumPlayers > 1
		    && (k = pl->lock.pl_id) > 0
		    && (i = GetInd[k]) > 0
		    && i < NumPlayers
		    && Players[i]->id == k
		    && i != ind) {
		    break;
		}
		Player_lock_closest(ind, 0);
		break;

	    case KEY_LOCK_NEXT_CLOSE:
		if (!Player_lock_closest(ind, 1)) {
		    Player_lock_closest(ind, 0);
		}
		break;

	    case KEY_LOCK_CLOSE:
		Player_lock_closest(ind, 0);
		break;

	    case KEY_CHANGE_HOME:
		xi = OBJ_X_IN_BLOCKS(pl);
		yi = OBJ_Y_IN_BLOCKS(pl);
		if (World.block[xi][yi] == BASE) {
		    msg[0] = '\0';
		    for (i=0; i<World.NumBases; i++) {
			if (World.base[i].pos.x == xi
			    && World.base[i].pos.y == yi) {

			    if (i == pl->home_base) {
				break;
			    }
			    if (World.base[i].team != TEAM_NOT_SET
				&& World.base[i].team != pl->team)
				break;
			    pl->home_base = i;
			    sprintf(msg, "%s has changed home base.",
				    pl->name);
			    break;
			}
		    }
		    for (i=0; i<NumPlayers; i++)
			if (i != ind
			    && pl->home_base == Players[i]->home_base) {
			    Pick_startpos(i);
			    sprintf(msg, "%s has taken over %s's home base.",
				    pl->name, Players[i]->name);
			}
		    if (msg[0]) {
			Set_message(msg);
		    }
		    for (i = 0; i < NumPlayers; i++) {
			if (Players[i]->conn != NOT_CONNECTED) {
			    Send_base(Players[i]->conn,
				      pl->id,
				      pl->home_base);
			}
		    }
		}
		break;

	    case KEY_DROP_BALL:
		Detach_ball(ind, -1);
		break;

	    case KEY_FIRE_SHOT:
		if (!BIT(pl->used, OBJ_SHIELD|OBJ_SHOT)
		    && BIT(pl->have, OBJ_SHOT)) {
		    SET_BIT(pl->used, OBJ_SHOT);
		    Fire_normal_shots(ind);
		}
		break;

	    case KEY_LOAD_LOCK_1:
	    case KEY_LOAD_LOCK_2:
	    case KEY_LOAD_LOCK_3:
	    case KEY_LOAD_LOCK_4: {
	      int *l = &(pl->lockbank[key - KEY_LOAD_LOCK_1]);
	      if (*l != NOT_CONNECTED
		  && Player_lock_allowed(ind, GetInd[*l])) {
		pl->lock.pl_id = *l;
		SET_BIT(pl->lock.tagged, LOCK_PLAYER);
	      }
	      break;
	    }
	      
	    case KEY_TURN_LEFT:
	    case KEY_TURN_RIGHT:
	        pl->turnacc = 0;
		if (BITV_ISSET(pl->last_keyv, KEY_TURN_LEFT)) {
		    pl->turnacc += pl->turnspeed;
		}
		if (BITV_ISSET(pl->last_keyv, KEY_TURN_RIGHT)) {
		    pl->turnacc -= pl->turnspeed;
		}
		break;

	    case KEY_SELF_DESTRUCT:
		TOGGLE_BIT(pl->status, SELF_DESTRUCT);
		if (BIT(pl->status, SELF_DESTRUCT))
		    pl->count = 150;
		break;

	    case KEY_PAUSE:
		if (BIT(pl->status, PAUSE)) {
		    i = PAUSE;
		}
		else {
		    xi = OBJ_X_IN_BLOCKS(pl);
		    yi = OBJ_Y_IN_BLOCKS(pl);
		    j = World.base[pl->home_base].pos.x;
		    k = World.base[pl->home_base].pos.y;
		    if (j == xi && k == yi) {
			minv = 3.0f;
			i = PAUSE;
		    } else {
			/*
			 * Hover pause doesn't work within two squares of the
			 * players home base, they would want the better pause.
			 */
			if (ABS(j - xi) <= 2 && ABS(k - yi) <= 2)
			    break;
			minv = 5.0f;
		    }
		    if (pl->velocity > minv)
			break;
		}
		
		/* toggle pause mode */
		Pause_player(ind, !BIT(pl->status, PAUSE));
		if (BIT(pl->status, PLAYING)) {
		  BITV_SET(pl->last_keyv, key);
		  BITV_SET(pl->prev_keyv, key);
		  return 1;
		}
		break;
		
		
	    case KEY_SWAP_SETTINGS:
		if (pl->turnacc == 0.0) {
		    SWAP(pl->power, pl->power_s);
		    SWAP(pl->turnspeed, pl->turnspeed_s);
		    SWAP(pl->turnresistance, pl->turnresistance_s);
		}
		break;

	    case KEY_REFUEL:
		Refuel(ind);
		break;

	    case KEY_CONNECTOR:
		if (BIT(pl->have, OBJ_CONNECTOR))
		    SET_BIT(pl->used, OBJ_CONNECTOR);
		break;

	    case KEY_INCREASE_POWER:
		pl->power *= 1.10;
		pl->power = MIN(pl->power, MAX_PLAYER_POWER);
		break;

	    case KEY_DECREASE_POWER:
		pl->power *= 0.90;
		pl->power = MAX(pl->power, MIN_PLAYER_POWER);
		break;

	    case KEY_INCREASE_TURNSPEED:
		if (pl->turnacc == 0.0)
		    pl->turnspeed *= 1.05;
		pl->turnspeed = MIN(pl->turnspeed, MAX_PLAYER_TURNSPEED);
		break;

	    case KEY_DECREASE_TURNSPEED:
		if (pl->turnacc == 0.0)
		    pl->turnspeed *= 0.95;
		pl->turnspeed = MAX(pl->turnspeed, MIN_PLAYER_TURNSPEED);
		break;

	    case KEY_THRUST:
		SET_BIT(pl->status, THRUSTING);
		break;

	    default:
		break;
	    }
	} else {
	    /* --- KEYRELEASE --- */
	    switch (key) {
	    case KEY_TURN_LEFT:
	    case KEY_TURN_RIGHT:
		pl->turnacc = 0;
		if (BITV_ISSET(pl->last_keyv, KEY_TURN_LEFT)) {
		    pl->turnacc += pl->turnspeed;
		}
		if (BITV_ISSET(pl->last_keyv, KEY_TURN_RIGHT)) {
		    pl->turnacc -= pl->turnspeed;
		}
		break;

	    case KEY_REFUEL:
		CLR_BIT(pl->used, OBJ_REFUEL);
		break;

	    case KEY_CONNECTOR:
		CLR_BIT(pl->used, OBJ_CONNECTOR);
		break;

	    case KEY_SHIELD:
		if (BIT(pl->used, OBJ_SHIELD)) {
		    CLR_BIT(pl->used, OBJ_SHIELD);
		    /*
		     * Insert the default fireRepeatRate between lowering
		     * shields and firing in order to prevent macros
		     * and hacked clients.
		     */
		    pl->shot_time = main_loops_slow;
		}
		break;

	    case KEY_FIRE_SHOT:
		CLR_BIT(pl->used, OBJ_SHOT);
		break;

	    case KEY_THRUST:
		CLR_BIT(pl->status, THRUSTING);
		break;

	    default:
		break;
	    }
	}
    }
    memcpy(pl->prev_keyv, pl->last_keyv, sizeof(pl->last_keyv));

    return 1;
}
