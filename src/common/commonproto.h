/*
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-2001 by
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

#ifndef	COMMONPROTO_H
#define	COMMONPROTO_H

#include "types.h"
#include "config.h"

#define SWAP(_a, _b)	    {DFLOAT _tmp = _a; _a = _b; _b = _tmp;}

/* randommt.c */
void seedMT(uint32_t seed);
uint32_t reloadMT(void);
uint32_t randomMT(void);

/* math.c */
double rfrac(void);
int32_t mod(int32_t x, int32_t y);
void make_trig_table(void);

/* strdup.c */
char *xp_strdup(const char *);
char *xp_safe_strdup(const char *old_string);

/* default.c */
uint32_t String_hash(const char *s);

/* strlcpy.c, unnecessary on FreeBSD */
#if (!__BSD_VISIBLE)
size_t strlcpy(char *dest, const char *src, size_t size);
size_t strlcat(char *dest, const char *src, size_t size);
#endif

#if (!HAVE_STRINGS_H)
int32_t strcasecmp(const char *str1, const char *str2);
int32_t strncasecmp(const char *str1, const char *str2, size_t n);
#endif

/* xpmemory.c */
void *xp_malloc(size_t size);
void *xp_realloc(void *oldptr, size_t size);
void *xp_calloc(size_t nmemb, size_t size);
void xp_free(void *p);
void *xp_safe_malloc(size_t size);
void *xp_safe_realloc(void *oldptr, size_t size);
void *xp_safe_calloc(size_t nmemb, size_t size);
void xp_safe_free(void *p);

#endif
