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

#ifndef OBJPOS_H
#define OBJPOS_H

#include "structs.h"


/*
 * The wall collision detection routines depend on repeatability
 * (getting the same result even after some "neutral" calculations)
 * and an exact determination whether a point is in space,
 * inside the wall (crash!) or on the edge.
 * This will be hard to achieve if only floating point would be used.
 * However, a resolution of a pixel is a bit rough and ugly.
 * Therefore a fixed point sub-pixel resolution is used called clicks.
 */
#define CLICK_SHIFT		6
#define CLICK			(1 << CLICK_SHIFT)
#define PIXEL_CLICKS		CLICK
#define BLOCK_CLICKS		(BLOCK_SZ << CLICK_SHIFT)

#define BLOCK_MIDDLE_TO_CLICK(B)	((int32_t)(((B) + 0.5f) * BLOCK_SZ) << CLICK_SHIFT)
#define BLOCK_MIDDLE_TO_PIXEL(B)	((int32_t)(((B) + 0.5f) * BLOCK_SZ))
#define BLOCK_TO_CLICK(B)	((int32_t)((B) * BLOCK_SZ) << CLICK_SHIFT)
#define BLOCK_TO_PIXEL(B)	((int32_t)((B) * BLOCK_SZ))
#define CLICK_TO_PIXEL(C)	((int32_t)((C) >> CLICK_SHIFT))
#define CLICK_TO_BLOCK(C)	((int32_t)((C) / (BLOCK_SZ << CLICK_SHIFT)))
#define CLICK_TO_FLOAT(C)	((DFLOAT)(C) * (1.0f / CLICK))
#define PIXEL_TO_CLICK(P)	((click_t)(P) << CLICK_SHIFT)
#define PIXEL_TO_BLOCK(P)	((P) / BLOCK_SZ)
#define FLOAT_TO_CLICK(F)	((click_t)((F) * CLICK))


typedef int32_t click_t;

typedef struct {
	click_t cx, cy;
} clpos_t;

typedef struct {
	click_t cx, cy;
} clvec_t;

/*
 * Object position
 *
 * NB: position in pixels used to be a float.
 */
struct _objposition {
	int32_t x, y; /* object position in pixels. */
	int32_t cx, cy; /* object position in clicks. */
	int32_t bx, by; /* object position in blocks. */
};

#define OBJ_X_IN_CLICKS(obj)	((obj)->pos.cx)
#define OBJ_Y_IN_CLICKS(obj)	((obj)->pos.cy)
#define OBJ_X_IN_PIXELS(obj)	((obj)->pos.x)
#define OBJ_Y_IN_PIXELS(obj)	((obj)->pos.y)
#define OBJ_X_IN_BLOCKS(obj)	((obj)->pos.bx)
#define OBJ_Y_IN_BLOCKS(obj)	((obj)->pos.by)

#define Object_position_remember(o_) \
	((o_)->prevpos.x = (o_)->pos.x, \
	 (o_)->prevpos.y = (o_)->pos.y)
#define Player_position_remember(p_) Object_position_remember(p_)

/** \brief Sets position in clicks
 *
 * NOTE: After execution the position can be outside map boundaries
 *
 * \param clpos		position in clicks
 * \param cx		X coordinate in clicks
 * \param cy		Y coordinate in clicks
 */
static inline void Position_simple_set(clpos_t *clpos, int32_t cx, int32_t cy)
{
	clpos->cx = cx;
	clpos->cy = cy;
}

/** \brief Adds the offsets to position
 *
 * NOTE: After execution the position can be outside map boundaries
 *
 * \param clpos		position in clicks
 * \param delta_cx	X coordinate in clicks
 * \param delta_cy	Y coordinate in clicks
 */
static inline void Position_simple_add(clpos_t *clpos, int32_t delta_cx, int32_t delta_cy)
{
	clpos->cx += delta_cx;
	clpos->cy += delta_cy;
}

void Player_position_debug(player_t *pl, const char *msg);
void Player_position_restore(player_t *pl);

void Position_copy(objposition_t *dstpos, objposition_t *srcpos);
void Position_set(objposition_t *pos, clpos_t *clpos);

#endif
