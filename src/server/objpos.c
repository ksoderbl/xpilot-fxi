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
#include <stdio.h>
#include <string.h>

#include "version.h"
#include "config.h"
#include "const.h"
#include "debug.h"

#include "objpos.h"
#include "map.h"

char objpos_version[] = VERSION;


void Position_copy(objposition_t *dstpos, objposition_t *srcpos)
{
	memcpy(dstpos, srcpos, sizeof(objposition_t));
}

void Position_set(objposition_t *pos, clpos_t *clpos)
{
	int32_t cx, cy;

	cx = WRAP_XCLICK(clpos->cx);
	cy = WRAP_YCLICK(clpos->cy);

#if (DEBUG > 0)
	ASSERT(cx >= 0 && cx < World.cwidth && cy >= 0 && cy < World.cheight)
#endif

	pos->cx = cx;
	pos->x = CLICK_TO_PIXEL(cx);
	pos->bx = PIXEL_TO_BLOCK(pos->x);
	pos->cy = cy;
	pos->y = CLICK_TO_PIXEL(cy);
	pos->by = PIXEL_TO_BLOCK(pos->y);
}
