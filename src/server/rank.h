#ifndef RANK_H
#define RANK_H

#include <time.h>
#include <sys/time.h>

#include "pack.h"
#include "types.h"
#include "object.h"

/* #include "version.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "object.h"
#include "objpos.h"
*/

typedef struct ranknode {

    char name[MAX_NAME_LEN];
    char user[MAX_NAME_LEN];
    char host[MAX_HOST_LEN];

    time_t timestamp;

    int kills, deaths;
    int rounds, shots;
    int ballsCashed, ballsSaved;
    int ballsWon, ballsLost;
    int bestball;
    int score;
    player *pl;
} ranknode_t;

bool Rank_get_stats(const char *name, char *buf);
ranknode_t *Rank_get_by_name(const char *name);
void Rank_init_saved_scores(void);
void Rank_get_saved_score(player *pl);
void Rank_save_score(player *pl);
void Rank_write_rankfile(void);
void Rank_write_webpage(void);
void Rank_show_ranks(void);

inline void Rank_add_kill(int ind);
inline void Rank_add_death(player *pl);
inline void Rank_add_round(player *pl);
inline void Rank_add_score(player *pl, int points);
#endif
