#ifndef RANK_H
#define RANK_H

#include <time.h>
#include <sys/time.h>

#include "pack.h"
#include "types.h"
#include "object.h"
#include "global.h"

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
    player_t *pl;
} ranknode_t;

bool Rank_get_stats(const char *name, char *buf);
ranknode_t *Rank_get_by_name(const char *name);
void Rank_init_saved_scores(void);
void Rank_get_saved_score(player_t *pl);
void Rank_save_score(player_t *pl);
void Rank_write_rankfile(void);
void Rank_write_webpage(void);
void Rank_show_ranks(void);

/* static routines from rank.h + player.h in ng47pre */


static inline void Player_add_score(player_t *pl, int points)
{
    pl->score += points;
    pl->update_score = true;
    updateScores = true;
}

static inline void Player_set_score(player_t *pl, int points)
{
    pl->score = points;
    pl->update_score = true;
    updateScores = true;
}

static inline void Rank_add_score(player_t *pl, int points)
{
    if (pl->rank)
        pl->rank->score += points;
}

static inline void Rank_set_score(int ind, int points)
{
    player_t *pl = Players[ind];
    pl->score = points;
    pl->update_score = true;
    if (pl->rank)
        pl->rank->score = points;
}

static inline void Rank_fire_shot(player_t *pl)
{
    pl->shots++;
    if (pl->rank)
        pl->rank->shots++;
}

static inline void Rank_add_kill(player_t *pl)
{
    pl->kills++;
    if (pl->rank)
        pl->rank->kills++;
}

static inline void Rank_add_death(player_t *pl)
{
    pl->deaths++;
    if (pl->rank)
        pl->rank->deaths++;
}

static inline void Rank_add_round(player_t *pl)
{
    if (pl->rank)
	pl->rank->rounds++;
}

static inline void Rank_cashed_ball(player_t *pl)
{
    if (pl->rank)
        pl->rank->ballsCashed++; 
}

static inline void Rank_saved_ball(player_t *pl)
{
    if (pl->rank)
        pl->rank->ballsSaved++;
}

static inline void Rank_won_ball(player_t *pl)
{
    if (pl->rank)
        pl->rank->ballsWon++;
}

static inline void Rank_lost_ball(player_t *pl)
{
    if (pl->rank)
        pl->rank->ballsLost++;
}

static inline void Rank_ballrun(player_t *pl, int tim)
{
    if (pl->rank) {
        if (pl->rank->bestball == 0 || tim < pl->rank->bestball)
            pl->rank->bestball = tim;
    }
}



static inline int Rank_get_best_ballrun(player_t *pl)
{
    return pl->rank ? pl->rank->bestball : 0;
}


#endif
