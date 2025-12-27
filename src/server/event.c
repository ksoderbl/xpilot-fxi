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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "version.h"
#include "config.h"
#include "serverconst.h"
#include "global.h"
#include "commonproto.h"
#include "proto.h"
#include "score.h"
#include "map.h"
#include "bit.h"
#include "netserver.h"
#include "player.h"
#include "player_inline.h"
#include "rules.h"
#include "frame.h"
#include "debug.h"

/*
 * Globals.
 */
char event_version[] = VERSION;


int32_t Handle_keyboard(player_t *pl)
{
	int32_t key;
	bool pressed;

	for (key = 0; key < NUM_KEYS; key++) {
		if (pl->last_keyv[key / BITV_SIZE] == pl->prev_keyv[key / BITV_SIZE]) {
			key |= (BITV_SIZE - 1); /* Skip to next keyv element */
			continue;
		}
		while (BITV_ISSET(pl->last_keyv, key) == BITV_ISSET(pl->prev_keyv, key)) {
			if (++key >= NUM_KEYS) {
				break;
			}
		}
		if (key >= NUM_KEYS) {
			break;
		}
		pressed = BITV_ISSET(pl->last_keyv, key) != 0;
		BITV_TOGGLE(pl->prev_keyv, key);
		if (key != KEY_SHIELD) /* would interfere with auto-idle-pause.. */
			pl->frame_last_busy = frame_loops; /* ok -pgm due to client auto-shield */

		/*
		 * Allow these functions while you're 'dead'.
		 */
		if (!Player_is_alive(pl)) {
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
			case KEY_REPROGRAM:
			case KEY_SWAP_SETTINGS:
			case KEY_INCREASE_POWER:
			case KEY_DECREASE_POWER:
			case KEY_INCREASE_TURNSPEED:
			case KEY_DECREASE_TURNSPEED:
			case KEY_TANK_NEXT:
			case KEY_TANK_PREV:
			case KEY_TURN_LEFT: /* Needed so that we don't get */
			case KEY_TURN_RIGHT: /* out-of-sync with the turnacc */
			case KEY_THRUST:	/* to capture (in particular) key releases */
			case KEY_FIRE_SHOT:	/* to capture (in particular) key releases */
			case KEY_REFUEL:	/* to capture (in particular) key releases */
			case KEY_CONNECTOR:	/* to capture (in particular) key releases */
			case KEY_DROP_BALL:	/* to capture (in particular) key releases */
				break;
			default:
				continue;
			}
		}

		if (pressed) { /* --- KEYPRESS --- */
			switch (key) {

			case KEY_TANK_NEXT:
			case KEY_TANK_PREV:
#if 0
				if (pl->fuel.num_tanks) {
					pl->fuel.current += (key == KEY_TANK_NEXT) ? 1 : -1;
					if (pl->fuel.current < 0) {
						pl->fuel.current = pl->fuel.num_tanks;
					}
					else if (pl->fuel.current > pl->fuel.num_tanks) {
						pl->fuel.current = 0;
					}
				}
#endif
				break;

			case KEY_LOCK_NEXT:
				Player_lock_next(pl, true);
				break;

			case KEY_LOCK_PREV:
				Player_lock_next(pl, false);
				break;

			case KEY_TOGGLE_COMPASS:
				if (!Player_has_property(pl, HAS_COMPASS)) {
					break;
				}
				Player_toggle_property(pl, USES_COMPASS);
				if (!Player_uses_property(pl, USES_COMPASS)) {
					break;
				}

				if (!Player_lock_is_initialized(pl)) {
					Player_lock_closest(pl, false);
				}

				break;

			case KEY_LOCK_NEXT_CLOSE:
				if (!Player_lock_closest(pl, true)) {
					Player_lock_closest(pl, false);
				}
				break;

			case KEY_LOCK_CLOSE:
				Player_lock_closest(pl, false);
				break;

			case KEY_CHANGE_HOME:
				if (Block_is_base(OBJ_X_IN_BLOCKS(pl), OBJ_Y_IN_BLOCKS(pl))) {
					Player_change_home(pl, Map_get_base_by_pos(&pl->pos));
				}
				break;

			case KEY_DROP_BALL:
				Ball_detach(pl, NULL);
				break;

			case KEY_FIRE_SHOT:
				if (Player_has_property(pl, HAS_SHOT)) {
					Player_disable_property(pl, USES_SHIELD);
					Player_enable_property(pl, USES_SHOT);
				}
				break;

			case KEY_REPROGRAM:
				SET_BIT(pl->pl_status, REPROGRAM);
				break;

			case KEY_LOAD_LOCK_1:
			case KEY_LOAD_LOCK_2:
			case KEY_LOAD_LOCK_3:
			case KEY_LOAD_LOCK_4: {
				player_t *lock_pl = pl->lockbank[key - KEY_LOAD_LOCK_1];

				if (BIT(pl->pl_status, REPROGRAM)) {
					if (BIT(pl->lock.flags, LOCK_PLAYER)) {
						pl->lockbank[key - KEY_LOAD_LOCK_1] = pl->lock.object;
					}
				}
				else {
					if (lock_pl != NULL && Player_lock_is_allowed(pl, lock_pl)) {
						pl->lock.object = lock_pl;
						SET_BIT(pl->lock.flags, LOCK_PLAYER);
					}
				}
				break;
			}

			case KEY_TURN_LEFT:
			case KEY_TURN_RIGHT:
				pl->turnacc = 0;
				if (BITV_ISSET(pl->last_keyv, KEY_TURN_LEFT)) {
					pl->turnacc += pl->turnspeed * ticksPerFrame;
				}
				if (BITV_ISSET(pl->last_keyv, KEY_TURN_RIGHT)) {
					pl->turnacc -= pl->turnspeed * ticksPerFrame;
				}
				break;

			case KEY_SELF_DESTRUCT:
				if (Player_is_self_destructing(pl)) {
					Player_self_destruct(pl, false);
				}
				else {
					Player_self_destruct(pl, true);
				}
				break;

			case KEY_PAUSE:
				if (!Player_is_paused(pl)) {
					Player_pause_self(pl);
				}
				else {
					Player_unpause(pl, NULL);
				}

				if (!Player_is_paused(pl)) {
					BITV_SET(pl->last_keyv, key);
					BITV_SET(pl->prev_keyv, key);
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
				Player_refuel_start(pl);
				break;

			case KEY_CONNECTOR:
				if (Player_has_property(pl, HAS_CONNECTOR))
					Player_enable_property(pl, USES_CONNECTOR);
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
				if (pl->turnacc == 0.0) {
					pl->turnspeed *= 1.05;
				}
				pl->turnspeed = MIN(pl->turnspeed, MAX_PLAYER_TURNSPEED);
				break;

			case KEY_DECREASE_TURNSPEED:
				if (pl->turnacc == 0.0) {
					pl->turnspeed *= 0.95;
				}
				pl->turnspeed = MAX(pl->turnspeed, MIN_PLAYER_TURNSPEED);
				break;

			case KEY_THRUST:
				SET_BIT(pl->obj_status, THRUSTING);
				break;

			default:
				break;
			}
		}
		else {
			/* --- KEYRELEASE --- */
			switch (key) {
			case KEY_TURN_LEFT:
			case KEY_TURN_RIGHT:
				pl->turnacc = 0;
				if (BITV_ISSET(pl->last_keyv, KEY_TURN_LEFT)) {
					pl->turnacc += pl->turnspeed * ticksPerFrame;
				}
				if (BITV_ISSET(pl->last_keyv, KEY_TURN_RIGHT)) {
					pl->turnacc -= pl->turnspeed * ticksPerFrame;
				}
				break;

			case KEY_REFUEL:
				Player_refuel_stop(pl);
				break;

			case KEY_CONNECTOR:
				Player_disable_property(pl, USES_CONNECTOR);
				break;

			case KEY_SHIELD:
				if (Player_uses_property(pl, USES_SHIELD)) {
					Player_disable_property(pl, USES_SHIELD);
					/*
					 * Insert the default fireRepeatRate between lowering
					 * shields and firing in order to prevent macros
					 * and hacked clients.
					 */
					pl->shot_time = main_loops_slow;
				}
				break;

			case KEY_FIRE_SHOT:
				Player_disable_property(pl, USES_SHOT);
				break;

			case KEY_THRUST:
				CLR_BIT(pl->obj_status, THRUSTING);
				break;

			case KEY_REPROGRAM:
				CLR_BIT(pl->pl_status, REPROGRAM);
				break;

			default:
				break;
			}
		}
	}
	memcpy(pl->prev_keyv, pl->last_keyv, sizeof(pl->last_keyv));

	return 1;
}
