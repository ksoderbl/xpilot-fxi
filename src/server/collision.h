/*
 * collision.h
 *
 *  Created on: Dec 6, 2009
 *      Author: rotunda
 */

#ifndef COLLISION_H_
#define COLLISION_H_

#include <stdint.h>
#include "player.h"

/* proposed edgewrap version: */
#define Collision_occured_old(o1, o2, r)                                   \
	(BIT(World.rules->mode, WRAP_PLAY)                                     \
		 ? ((((o1)->pos.x > (o2)->pos.x)                                   \
				 ? (((o1)->pos.x - (o2)->pos.x > (World.width >> 1))       \
						? ((o1)->pos.x - (o2)->pos.x > World.width - (r))  \
						: ((o1)->pos.x - (o2)->pos.x < (r)))               \
				 : (((o2)->pos.x - (o1)->pos.x > (World.width >> 1))       \
						? ((o2)->pos.x - (o1)->pos.x > World.width - (r))  \
						: ((o2)->pos.x - (o1)->pos.x < (r)))) &&           \
			(((o1)->pos.y > (o2)->pos.y)                                   \
				 ? (((o1)->pos.y - (o2)->pos.y > (World.height >> 1))      \
						? ((o1)->pos.y - (o2)->pos.y > World.height - (r)) \
						: ((o1)->pos.y - (o2)->pos.y < (r)))               \
				 : (((o2)->pos.y - (o1)->pos.y > (World.height >> 1))      \
						? ((o2)->pos.y - (o1)->pos.y > World.height - (r)) \
						: ((o2)->pos.y - (o1)->pos.y < (r)))))             \
		 : (DELTA((o1)->pos.x, (o2)->pos.x) < (r) && DELTA((o1)->pos.y, (o2)->pos.y) < (r)))

int32_t Collision_occured(int32_t p1x, int32_t p1y, int32_t p2x, int32_t p2y,
						  int32_t q1x, int32_t q1y, int32_t q2x, int32_t q2y, int32_t r);

void Collision_player_object_check(player_t *pl);
bool Collision_player_player_check(player_t *pl, player_t *pl2);
bool Collision_player_attach_ball(player_t *pl);
void Collision_players_check(void);

void Collision_cells_free(void);
void Collision_cells_allocate(void);
void Collision_cells_init(void);
void Collision_cells_cleanup(void);
void Collision_cells_objects_get(int32_t x, int32_t y, int32_t r, object_t ***list, int32_t *count);

void Collision_cells_print(void);

#endif /* COLLISION_H_ */
