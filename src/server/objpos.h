/* $Id: objpos.h,v 1.3 2007/06/12 18:59:38 kps Exp $
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-98 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
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

#ifndef OBJPOS_H
#define OBJPOS_H

void Object_position_set_clicks(object_t *obj, int cx, int cy);
void Object_position_set_clicks_interpolation(object_t *obj, int cx, int cy);
void Object_position_init_clicks(object_t *obj, int cx, int cy);
void Object_position_init_clicks_interpolation(object_t *obj, int cx, int cy);
void Player_position_restore(player_t *pl);
void Player_position_set_clicks(player_t *pl, int cx, int cy);
void Player_position_set_clicks_interpolation(player_t *pl, int cx, int cy);
void Player_position_init_clicks(player_t *pl, int cx, int cy);
void Player_position_limit(player_t *pl);
void Player_position_debug(player_t *pl, const char *msg);

#define Object_position_remember(o_) \
	((o_)->prevpos.x = (o_)->pos.x, \
	 (o_)->prevpos.y = (o_)->pos.y)
#define Player_position_remember(p_) Object_position_remember(p_)

#endif
