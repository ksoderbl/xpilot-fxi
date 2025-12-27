/*
 * frame.h
 *
 *  Created on: Jun 13, 2009
 *      Author: rotunda
 */

#ifndef FRAME_H_
#define FRAME_H_

#include "types.h"

typedef enum player_msg_ {
	PL_MSG_GREETING = 0,
	PL_MSG_NOTICE = 1,
	PL_MSG_REPLY = 2,
	PL_MSG_NONE = 3,
} player_msg_t;

typedef enum server_msg_ {
	SRV_MSG_IMPORTANT = 0,	/* Public server messages displayed on topIn the client code they are */
	SRV_MSG_GAME = 1,
} server_msg_t;

static inline bool Frame_is_real(void)
{
	return (frame_cycle == 0) ? true : false;
}

void Frame_update(void);

void Message_game_important_print(const char *fmt, ...);
void Message_game_print(const char *fmt, ...);
void Message_player_print(player_t *pl, player_msg_t type, const char *fmt, ...);

#endif /* FRAME_H_ */
