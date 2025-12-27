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

#ifndef RULES_H
#define RULES_H

/*
 * Bitfield definitions for playing mode.
 */
#define CRASH_WITH_PLAYER	(1<<0)
#define BOUNCE_WITH_PLAYER	(1<<1)
#define PLAYER_KILLINGS		(1<<2)
#define LIMITED_LIVES		(1<<3)
#define TIMING                  (1<<4)
#define ONE_PLAYER_ONLY		(1<<5)
#define PLAYER_SHIELDING	(1<<6)
#define TEAM_PLAY		(1<<8)
#define WRAP_PLAY		(1<<9)
#define PRACTISE_PLAY		(1<<10)

/*
 * Client uses only a subset of them:
 */
#define CLIENT_RULES_MASK	(WRAP_PLAY|TEAM_PLAY|TIMING|LIMITED_LIVES)

/*
 * Possible object and player status bits.
 * Needed here because client needs them too.
 * The bits that the client needs must fit into a byte,
 * so the first 8 bitvalues are reserved for that purpose.
 */
#define OLD_PLAYING			(1L<<0)		/* alive or killed */
#define OLD_PAUSE			(1L<<1) 	/* paused */
#define OLD_GAME_OVER			(1L<<2)		/* waiting or dead */
#define OLD_KILLED			(1L<<10)

typedef struct {
	int32_t lives;
	int32_t mode;
} rules_t;

extern int32_t KILL_OBJ_BITS;

#endif
