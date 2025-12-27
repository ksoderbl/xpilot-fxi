/* $Id: player.c,v 4.15 2000/03/24 14:14:58 bert Exp $
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

#define SERVER
#include "version.h"
#include "commonproto.h"
#include "config.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "map.h"
#include "score.h"
#include "bit.h"
#include "netserver.h"
#include "error.h"
#include "objpos.h"

char player_version[] = VERSION;

extern int Rate(int winner, int loser);

bool		updateScores = true;


/********* **********
 * Functions on player array.
 */

void Pick_startpos(int ind)
{
    player	*pl = Players[ind];
    int		i, num_free;
    int		pick = 0, seen = 0;
    static int	prev_num_bases = 0;
    static char	*free_bases = NULL;

    if (prev_num_bases != World.NumBases) {
	prev_num_bases = World.NumBases;
	if (free_bases != NULL) {
	    free(free_bases);
	}
	free_bases = (char *) malloc(World.NumBases * sizeof(*free_bases));
	if (free_bases == NULL) {
	    error("Can't allocate memory for free_bases");
	    End_game();
	}
    }

    num_free = 0;
    for (i = 0; i < World.NumBases; i++) {
	if (World.base[i].team == pl->team) {
	    num_free++;
	    free_bases[i] = 1;
	} else {
	    free_bases[i] = 0;	/* other team */
	}
    }

    for (i = 0; i < NumPlayers; i++) {
	if (i != ind
	    && free_bases[Players[i]->home_base]) {
	    free_bases[Players[i]->home_base] = 0;	/* occupado */
	    num_free--;
	}
    }

    {
	pick = (int)(rfrac() * num_free);
	seen = 0;
	for (i = 0; i < World.NumBases; i++) {
	    if (free_bases[i] != 0) {
		if (seen < pick) {
		    seen++;
		} else {
		    break;
		}
	    }
	}
    }

    if (i == World.NumBases) {
	error("Can't pick startpos (ind=%d,num=%d,free=%d,pick=%d,seen=%d)",
	      ind, World.NumBases, num_free, pick, seen);
	End_game();
    } else {
	pl->home_base = i;
	if (ind < NumPlayers) {
	    for (i = 0; i < NumPlayers; i++) {
		if (Players[i]->conn != NOT_CONNECTED) {
		    Send_base(Players[i]->conn,
			      pl->id,
			      pl->home_base);
		}
	    }
	    if (BIT(pl->status, PLAYING) == 0) {
		pl->count = RECOVERY_DELAY;
	    }
	    else if (BIT(pl->status, PAUSE|GAME_OVER)) {
		Go_home(ind);
	    }
	}
    }
}


void Go_home(int ind)
{
    player		*pl = Players[ind];
    int			i, x, y, dir;
    DFLOAT		vx, vy, velo;

    x = World.base[pl->home_base].pos.x;
    y = World.base[pl->home_base].pos.y;
    dir = World.base[pl->home_base].dir;
    vx = vy = velo = 0;

    pl->dir = dir;
    pl->float_dir = dir;
    Player_position_init_pixels(pl,
				(x + 0.5) * BLOCK_SZ + vx,
				(y + 0.5) * BLOCK_SZ + vy);
    pl->vel.x = vx;
    pl->vel.y = vy;
    pl->velocity = velo;
    pl->acc.x = pl->acc.y = 0.0;
    pl->turnacc = pl->turnvel = 0.0;
    memset(pl->last_keyv, 0, sizeof(pl->last_keyv));
    memset(pl->prev_keyv, 0, sizeof(pl->prev_keyv));
    pl->used &= ~USED_KILL;

    if (playerStartsShielded != 0) {
	SET_BIT(pl->used, OBJ_SHIELD);
	if (playerShielding == 0) {
	    pl->shield_time = 2 * FPS;
	    SET_BIT(pl->have, OBJ_SHIELD);
	}
    }
    CLR_BIT(pl->status, THRUSTING);
    pl->updateVisibility = 1;
    for (i = 0; i < NumPlayers; i++) {
	pl->visibility[i].lastChange = 0;
	Players[i]->visibility[ind].lastChange = 0;
    }

    if (IS_ROBOT_PTR(pl)) {
	Robot_go_home(ind);
    }
}


/*
 * Give ship one more tank, if possible.
 */
void Player_add_tank(int ind, long tank_fuel)
{
    player		*pl = Players[ind];
    long		tank_cap, add_fuel;

    if (pl->fuel.num_tanks < MAX_TANKS) {
	pl->fuel.num_tanks++;
	tank_cap = TANK_CAP(pl->fuel.num_tanks);
	add_fuel = tank_fuel;
	LIMIT(add_fuel, 0, tank_cap);
	pl->fuel.sum += add_fuel;
	pl->fuel.max += tank_cap;
	pl->fuel.tank[pl->fuel.num_tanks] = add_fuel;
	pl->emptymass += TANK_MASS;
	pl->item[ITEM_TANK] = pl->fuel.num_tanks;
    }
}

/*
 * Remove a tank from a ship, if possible.
 */
void Player_remove_tank(int ind, int which_tank)
{
    player		*pl = Players[ind];
    int			i, tank_ind;
    long		tank_fuel, tank_cap;

    if (pl->fuel.num_tanks > 0) {
	tank_ind = which_tank;
	LIMIT(tank_ind, 1, pl->fuel.num_tanks);
	pl->emptymass -= TANK_MASS;
	tank_fuel = pl->fuel.tank[tank_ind];
	tank_cap = TANK_CAP(tank_ind);
	pl->fuel.max -= tank_cap;
	pl->fuel.sum -= tank_fuel;
	pl->fuel.num_tanks--;
	if (pl->fuel.current > pl->fuel.num_tanks) {
	    pl->fuel.current = 0;
	} else {
	    for (i = tank_ind; i <= pl->fuel.num_tanks; i++) {
		pl->fuel.tank[i] = pl->fuel.tank[i + 1];
	    }
	}
	pl->item[ITEM_TANK] = pl->fuel.num_tanks;
    }
}

/*
 * Give player the initial number of tanks and amount of fuel.
 * Upto the maximum allowed.
 */
static void Player_init_fuel(int ind, long total_fuel)
{
    player		*pl = Players[ind];
    long		fuel = total_fuel;
    int			i;

    pl->fuel.num_tanks  = 0;
    pl->fuel.current    = 0;
    pl->fuel.max	= TANK_CAP(0);
    pl->fuel.sum	= MIN(fuel, pl->fuel.max);
    pl->fuel.tank[0]	= pl->fuel.sum;
    pl->emptymass	= ShipMass;
    pl->item[ITEM_TANK]	= pl->fuel.num_tanks;

    fuel -= pl->fuel.sum;

    for (i = 1; i <= World.items[ITEM_TANK].initial; i++) {
	Player_add_tank(ind, fuel);
	fuel -= pl->fuel.tank[i];
    }
}

int Init_player(int ind, wireobj *ship)
{
    player		*pl = Players[ind];
    bool		too_late = false;
    int			i;


    pl->vel.x	= pl->vel.y	= 0.0;
    pl->acc.x	= pl->acc.y	= 0.0;
    pl->float_dir = pl->dir	= DIR_UP;
    pl->turnvel		= 0.0;
    pl->turnacc		= 0.0;
    pl->mass		= ShipMass;
    pl->emptymass	= ShipMass;

    for (i = 0; i < NUM_ITEMS; i++) {
	if (!BIT(1U << i, ITEM_BIT_FUEL | ITEM_BIT_TANK)) {
	    pl->item[i] = World.items[i].initial;
	}
    }

    pl->fuel.sum        = World.items[ITEM_FUEL].initial << FUEL_SCALE_BITS;
    Player_init_fuel(ind, pl->fuel.sum);

    if (allowShipShapes == true && ship) {
	pl->ship = ship;
    }
    else {
	pl->ship = Default_ship();
    }

    pl->power			= 45.0;
    pl->turnspeed		= 30.0;
    pl->turnresistance		= 0.12;
    pl->power_s			= 35.0;
    pl->turnspeed_s		= 25.0;
    pl->turnresistance_s	= 0.12;

    pl->count		= -1;
    pl->shield_time	= 0;

    pl->type		= OBJ_PLAYER;
    pl->type_ext	= 0;		/* assume human player */
    pl->shots		= 0;
    pl->shot_speed	= ShotsSpeed;
    pl->max_speed	= SPEED_LIMIT - pl->shot_speed;
    pl->shot_max	= ShotsMax;
    pl->shot_life	= ShotsLife;
    pl->shot_mass	= ShotsMass;
    pl->shot_time	= 0;
    pl->color		= WHITE;
    pl->score		= 0;
    pl->prev_score	= 0;
    pl->fs		= 0;
    pl->name[0]		= '\0';
    pl->damaged 	= 0;
    pl->stunned		= 0;

    pl->status		= PLAYING | DEF_BITS;
    pl->have		= DEF_HAVE;
    pl->used		= DEF_USED;
	
    for (i = 0; i < LOCKBANK_MAX; i++)
	pl->lockbank[i] = NOT_CONNECTED;

    pl->mychar		= ' ';
    pl->prev_mychar	= pl->mychar;
    pl->life		= World.rules->lives;
    pl->prev_life	= pl->life;
    pl->ball 		= NULL;
    pl->player_fps	= FPS;

    pl->kills		= 0;
    pl->deaths		= 0;

    /*
     * If limited lives and if nobody has lost a life yet, you may enter
     * now, otherwise you will have to wait 'til everyone gets GAME OVER.
     */
    if (BIT(World.rules->mode, LIMITED_LIVES)) {
	for (i = 0; i < NumPlayers; i++) {
	    /* If a non-team member has lost a life,
	     * then it's too late to join. */
	    if (Players[i]->life < World.rules->lives && !TEAM(ind, i)) {
		too_late = true;
		break;
	    }
	}
	if (too_late) {
	    pl->mychar	= 'W';
	    pl->prev_life = pl->life = 0;
	    SET_BIT(pl->status, GAME_OVER);
	}
    }

    pl->team = TEAM_NOT_SET;

    pl->lock.tagged	= LOCK_NONE;
    pl->lock.pl_id	= 0;

    pl->robot_data_ptr	= NULL;

    pl->id		= peek_ID();
    GetInd[pl->id]	= ind;
    pl->conn		= NOT_CONNECTED;

    pl->shove_next = 0;
    for (i = 0; i < MAX_RECORDED_SHOVES; i++) {
	pl->shove_record[i].pusher_id = -1;
    }

    pl->frame_last_busy	= frame_loops; /*timing ok, used only to determine pausing -pgm*/

    pl->isowner = 0;
    pl->isoperator = 0;

    return pl->id;
}


static player			*playerArray;
static struct _visibility	*visibilityArray;

void Alloc_players(int number)
{
    player *p;
    struct _visibility *t;
    int i;


    /* Allocate space for pointers */
    Players = (player **) calloc(number + 1, sizeof(player *));

    /* Allocate space for all entries, all player structs */
    p = playerArray = (player *) calloc(number, sizeof(player));

    /* Allocate space for all visibility arrays, n arrays of n entries */
    t = visibilityArray =
	(struct _visibility *) calloc(number * number,
				      sizeof(struct _visibility));

    if (!Players || !playerArray || !visibilityArray) {
	error("Not enough memory for Players.");
	exit(1);
    }

    /* Players[-1] should evaluate to NULL. */
    Players++;

    for (i = 0; i < number; i++) {
	Players[i] = p++;
	Players[i]->visibility = t;
	/* Advance to next block/array */
	t += number;
    }
}



void Free_players(void)
{
    if (Players) {
	--Players;
	free(Players);
	Players = NULL;

	free(playerArray);
	free(visibilityArray);
    }
}



void Update_score_table(void)
{
    int			i, j;
    player		*pl;

    for (j = 0; j < NumPlayers; j++) {
	pl = Players[j];
	if (pl->score != pl->prev_score
	    || pl->life != pl->prev_life
	    || pl->mychar != pl->prev_mychar) {
	    pl->prev_score = pl->score;
	    pl->prev_life = pl->life;
	    pl->prev_mychar = pl->mychar;
	    for (i = 0; i < NumPlayers; i++) {
		if (Players[i]->conn != NOT_CONNECTED) {
		    Send_score(Players[i]->conn, pl->id,
			       pl->score, pl->life, pl->mychar);
		}
	    }
	}
    }
    updateScores = false;
}


void Reset_all_players(void)
{
    player		*pl;
    int			i, j;

    updateScores = true;

    for (i = 0; i < NumPlayers; i++) {
	pl = Players[i];
	if (endOfRoundReset) {
	    if (!BIT(pl->status, PAUSE)) {
		Kill_player(i);
		if (pl != Players[i]) {
		    /* player was deleted. */
		    i--;
		    continue;
		}
	    }
	}
	CLR_BIT(pl->status, GAME_OVER);
	CLR_BIT(pl->have, OBJ_BALL);
	pl->kills = 0;
	pl->deaths = 0;
	pl->frame_last_busy = frame_loops; /*timing ok, used only to determine pausing -pgm */
	if (!BIT(pl->status, PAUSE)) {
	    pl->mychar = ' ';
	    pl->life = World.rules->lives;
	}
	if (IS_ROBOT_PTR(pl))
	    pl->mychar = 'R';
    }
    if (BIT(World.rules->mode, TEAM_PLAY)) {

	/* Detach any balls and kill ball */
	/* We are starting all over again */
	for (j = NumObjs - 1; j >= 0 ; j--) {
	    if (BIT(Obj[j]->type, OBJ_BALL)) {
		Obj[j]->id = -1;
		Obj[j]->life = 0;
		Obj[j]->owner = 0;	/* why not -1 ??? */
		CLR_BIT(Obj[j]->status, RECREATE);
		Delete_shot(j);
	    }
	}

	/* Reset the treasures */
	for (i = 0; i < World.NumTreasures; i++) {
	    World.treasures[i].destroyed = 0;
	    World.treasures[i].have = false;
	    Make_treasure_ball(i);
	}

	/* Reset the teams */
	for (i = 0; i < MAX_TEAMS; i++) {
	    World.teams[i].TreasuresDestroyed = 0;
	    World.teams[i].TreasuresLeft = World.teams[i].NumTreasures;
	}
    }

    if (endOfRoundReset) {
	for (i = 0; i < NumObjs; i++) {
	    object *obj = Obj[i];
	    if (BIT(obj->type, OBJ_SHOT|OBJ_DEBRIS|OBJ_SPARK))
		obj->life = 0;
	}
    }

    roundtime = maxRoundTime * FPS;

    Update_score_table();
}


void Check_team_members(int team)
{
    player		*pl;
    int			members, i;

    if (! BIT(World.rules->mode, TEAM_PLAY))
	return;

    for (members = i = 0; i < NumPlayers; i++) {
	pl = Players[i];
	if (pl->team != TEAM_NOT_SET
	    && pl->team == team)
	    members++;
    }
    if (World.teams[team].NumMembers != members) {
	error ("Server has reset team %d members from %d to %d",
	       team, World.teams[team].NumMembers, members);
	for (i = 0; i < NumPlayers; i++) {
	    pl = Players[i];
	    if (pl->team != TEAM_NOT_SET
		&& pl->team == team)
		error ("Team %d currently has player %d: \"%s\"",
		       team, i+1, pl->name);
	}
	World.teams[team].NumMembers = members;
    }
}


void Check_team_treasures(int team)
{
    int 		i, j, ownerind, idind;
    treasure_t		*t;
    object		*obj;

    if (! BIT(World.rules->mode, TEAM_PLAY))
	return;

    for (i = 0; i < World.NumTreasures; i++) {
	t = &(World.treasures[i]);

	if (t->team != team)
	    continue;

	for (j = 0; j < NumObjs; j++) {
	    obj = Obj[j];

	    if (! BIT(obj->type, OBJ_BALL)
		|| obj->treasure != i)
		continue;
	    ownerind = (obj->owner == -1 ? -1 : GetInd[obj->owner]);
	    idind = (obj->id == -1 ? -1 : GetInd[obj->id]);
	}
    }
}


static void Compute_end_of_round_values(int *average_score,
					int *num_best_players,
					DFLOAT *best_ratio,
					int best_players[])
{
    int			i;
    DFLOAT		ratio;

    /* Initialize everything */
    *average_score = 0;
    *num_best_players = 0;
    *best_ratio = -1.0;

    /* Figure out what the average score is and who has the best kill/death */
    /* ratio for this round */
    for (i = 0; i < NumPlayers; i++) {
	if ((BIT(Players[i]->status, PAUSE)
	     && Players[i]->count <= 0)) {
	    continue;
	}
	*average_score += Players[i]->score;
	ratio = (DFLOAT) Players[i]->kills / (Players[i]->deaths + 1);
	if (ratio > *best_ratio) {
	    *best_ratio = ratio;
	    best_players[0] = i;
	    *num_best_players = 1;
	}
	else if (ratio == *best_ratio) {
	    best_players[(*num_best_players)++] = i;
	}
    }
    *average_score /= NumPlayers;
}


static void Give_best_player_bonus(int average_score,
				   int num_best_players,
				   DFLOAT best_ratio,
				   int best_players[])
{
    int			i;
    int			points;
    char		msg[MSG_LEN];


    if (best_ratio == 0) {
	sprintf(msg, "There is no Deadly Player");
    }
    else if (num_best_players == 1) {
	player *bp = Players[best_players[0]];

	sprintf(msg,
		"%s is the Deadliest Player with a kill ratio of %d/%d.",
		bp->name,
		bp->kills, bp->deaths);
	points = (int) (best_ratio * Rate(bp->score, average_score));
	SCORE(best_players[0], points,
	      OBJ_X_IN_BLOCKS(bp),
	      OBJ_Y_IN_BLOCKS(bp),
	      "[Deadliest]");
    }
    else {
	msg[0] = '\0';
	for (i = 0; i < num_best_players; i++) {
	    player	*bp = Players[best_players[i]];
	    int		ratio = Rate(bp->score, average_score);
	    DFLOAT	score = (DFLOAT) (ratio + num_best_players)
				/ num_best_players;

	    if (msg[0]) {
		if (i == num_best_players - 1)
		    strcat(msg, " and ");
		else
		    strcat(msg, ", ");
	    }
	    if (strlen(msg) + 8 + strlen(bp->name) >= sizeof(msg)) {
		Set_message(msg);
		msg[0] = '\0';
	    }
	    strcat(msg, bp->name);
	    points = (int) (best_ratio * score);
	    SCORE(best_players[i], points,
		  OBJ_X_IN_BLOCKS(bp),
		  OBJ_Y_IN_BLOCKS(bp),
		  "[Deadly]");
	}
	if (strlen(msg) + 64 >= sizeof(msg)) {
	    Set_message(msg);
	    msg[0] = '\0';
	}
	sprintf(msg + strlen(msg),
		" are the Deadly Players with kill ratios of %d/%d.",
		Players[best_players[0]]->kills,
		Players[best_players[0]]->deaths);
    }
    Set_message(msg);
}

static void Give_individual_bonus(int ind, int average_score)
{
    DFLOAT		ratio;
    int			points;

    ratio = (DFLOAT) Players[ind]->kills / (Players[ind]->deaths + 1);
    points = (int) (ratio * Rate(Players[ind]->score, average_score));
    SCORE(ind, points,
	  OBJ_X_IN_BLOCKS(Players[ind]),
	  OBJ_Y_IN_BLOCKS(Players[ind]),
	  "[Winner]");
}


static void Count_rounds(void)
{
    char		msg[MSG_LEN];

    if (!roundsToPlay) {
	return;
    }

    ++roundsPlayed;

    sprintf(msg, " < Round %d out of %d completed. >",
	    roundsPlayed, roundsToPlay);
    Set_message(msg);
    if (roundsPlayed >= roundsToPlay) {
	Game_Over();
    }
}


void Team_game_over(int winning_team, const char *reason)
{
    int			i, j;
    int			average_score;
    int			num_best_players;
    int			*best_players;
    DFLOAT		best_ratio;
    char		msg[MSG_LEN];

    if (!(best_players = (int *)malloc(NumPlayers * sizeof(int)))) {
	error("no mem");
	End_game();
    }

    /* Figure out the average score and who has the best kill/death ratio */
    /* ratio for this round */
    Compute_end_of_round_values(&average_score,
				&num_best_players,
				&best_ratio,
				best_players);

    /* Print out the results of the round */
    if (winning_team != -1) {
	sprintf(msg, " < Team %d has won the game%s! >", winning_team,
		reason);
    } else {
	sprintf(msg, " < We have a draw%s! >", reason);
    }
    Set_message(msg);

    /* Give bonus to the best player */
    Give_best_player_bonus(average_score,
			   num_best_players,
			   best_ratio,
			   best_players);

    /* Give bonuses to the winning team */
    if (winning_team != -1) {
	for (i = 0; i < NumPlayers; i++) {
	    if (Players[i]->team != winning_team) {
		continue;
	    }
	    if ((BIT(Players[i]->status, PAUSE)
		    && Players[i]->count <= 0)
		|| (BIT(Players[i]->status, GAME_OVER)
		    && Players[i]->mychar == 'W'
		    && Players[i]->score == 0)) {
		continue;
	    }
	    for (j = 0; j < num_best_players; j++) {
		if (i == best_players[j]) {
		    break;
		}
	    }
	    if (j == num_best_players) {
		Give_individual_bonus(i, average_score);
	    }
	}
    }

    Reset_all_players();

    Count_rounds();

    free(best_players);
}

void Individual_game_over(int winner)
{
    int			i, j;
    int			average_score;
    int			num_best_players;
    int			*best_players;
    DFLOAT		best_ratio;
    char		msg[MSG_LEN];

    if (!(best_players = (int *)malloc(NumPlayers * sizeof(int)))) {
	error("no mem");
	End_game();
    }

    /* Figure out what the average score is and who has the best kill/death */
    /* ratio for this round */
    Compute_end_of_round_values(&average_score, &num_best_players,
				&best_ratio, best_players);

    /* Print out the results of the round */
    if (winner == -1) {
	Set_message(" < We have a draw! >");
    }
    else if (winner == -2) {
	Set_message(" < The robots have won the game! >");
	/* Perhaps this should be a different sound? */
    } else {
	sprintf(msg, " < %s has won the game! >", Players[winner]->name);
	Set_message(msg);
    }

    /* Give bonus to the best player */
    Give_best_player_bonus(average_score,
			   num_best_players,
			   best_ratio,
			   best_players);

    /* Give bonus to the winning player */
    if (winner >= 0) {
	for (i = 0; i < num_best_players; i++) {
	    if (winner == best_players[i]) {
		break;
	    }
	}
	if (i == num_best_players) {
	    Give_individual_bonus(winner, average_score);
	}
    }
    else if (winner == -2) {
	for (j = 0; j < NumPlayers; j++) {
	    if (IS_ROBOT_IND(j)) {
		for (i = 0; i < num_best_players; i++) {
		    if (j == best_players[i]) {
			break;
		    }
		}
		if (i == num_best_players) {
		    Give_individual_bonus(j, average_score);
		}
	    }
	}
    }

    Reset_all_players();

    free(best_players);
}


void Compute_game_status(void)
{
    int			i;
    char		msg[MSG_LEN];

    if (roundtime > 0) {
	roundtime--;
    }
    
    
    if (BIT(World.rules->mode, TEAM_PLAY)) {
	/* Do we have a winning team ? */

	enum TeamState {
	    TeamEmpty,
	    TeamDead,
	    TeamAlive
	}	team_state[MAX_TEAMS];
	int	num_dead_teams = 0;
	int	num_alive_teams = 0;
	int	winning_team = -1;
	for (i = 0; i < MAX_TEAMS; i++) {
	    team_state[i] = TeamEmpty;
	}

	for (i = 0; i < NumPlayers; i++) {
	  if (BIT(Players[i]->status, PAUSE)) {
	    /* Ignore paused players. */
	    continue;
	  }
#if 0
	    /* not all teammode maps have treasures. */
	  else if (World.teams[Players[i]->team].NumTreasures == 0) {
	    /* Ignore players with no treasure troves */
	    continue;
	  }
#endif
	  else if (BIT(Players[i]->status, GAME_OVER)) {
	    if (team_state[Players[i]->team] == TeamEmpty) {
	      /* Assume all teammembers are dead. */
	      num_dead_teams++;
	      team_state[Players[i]->team] = TeamDead;
	    }
	  }
	     /*
	     * If the player is not paused and he is not in the
	     * game over mode and his team owns treasures then he is
	     * considered alive.
	     * But he may not be playing though if the rest of the team
	     * was genocided very quickly after game reset, while this
	     * player was still being transported back to his homebase.
	     */
	    else if (team_state[Players[i]->team] != TeamAlive) {
		if (team_state[Players[i]->team] == TeamDead) {
		    /* Oops!  Not all teammembers are dead yet. */
		    num_dead_teams--;
		}
		team_state[Players[i]->team] = TeamAlive;
		++num_alive_teams;
		/* Remember a team which was alive. */
		winning_team = Players[i]->team;
	    }
	}

	if (num_alive_teams > 1) {
	    char	*bp;
	    int		teams_with_treasure = 0;
	    int		team_win[MAX_TEAMS];
	    int		team_score[MAX_TEAMS];
	    int		winners;
	    int		max_destroyed = 0;
	    int		max_left = 0;
	    int		max_score = 0;
	    /*
	     * Game is not over if more than one team which have treasures
	     * still have one remaining in play.  Note that it is possible
	     * for max_destroyed to be zero, in the case where a team
	     * destroys some treasures and then all quit, and the remaining
	     * teams did not destroy any.
	     */

	    for (i = 0; i < MAX_TEAMS; i++) {
		team_score[i] = 0;
		if (team_state[i] != TeamAlive) {
		    team_win[i] = 0;
		    continue;
		}
		team_win[i] = 1;

		if (World.teams[i].TreasuresDestroyed > max_destroyed)
		  max_destroyed = World.teams[i].TreasuresDestroyed;
                if (World.teams[i].TreasuresLeft)
		  teams_with_treasure++;
	    }


	    /*
	     * Game is not over if more than one team has treasure.
	     */
	    
	    if ((teams_with_treasure > 1 || !max_destroyed)
		&& (roundtime != 0 || maxRoundTime <= 0)) {
		return;
	    }

	    if (maxRoundTime > 0 && roundtime == 0) {
		Set_message("Timer expired. Round ends now.");
	    }

	    /*
	     * Find the winning team;
	     *	Team destroying most number of treasures;
	     *	If drawn; the one with most saved treasures,
	     *	If drawn; the team with the most points,
	     *	If drawn; an overall draw.
	     */
	    for (winners = i = 0; i < MAX_TEAMS; i++) {
		if (!team_win[i])
		    continue;
		if (World.teams[i].TreasuresDestroyed == max_destroyed) {
		    if (World.teams[i].TreasuresLeft > max_left)
			max_left = World.teams[i].TreasuresLeft;
		    winning_team = i;
		    winners++;
		} else {
		    team_win[i] = 0;
		}
	    }
	    if (winners == 1) {
		sprintf(msg, " by destroying %d treasures", max_destroyed);
		Team_game_over(winning_team, msg);
		return;
	    }

	    for (i = 0; i < NumPlayers; i++) {
		if (BIT(Players[i]->status, PAUSE))
		    continue;
		team_score[Players[i]->team] += Players[i]->score;
	    }

	    for (winners = i = 0; i < MAX_TEAMS; i++) {
		if (!team_win[i])
		    continue;
		if (World.teams[i].TreasuresLeft == max_left) {
		    if (team_score[i] > max_score)
			max_score = team_score[i];
		    winning_team = i;
		    winners++;
		} else {
		    team_win[i] = 0;
		}
	    }
	    if (winners == 1) {
		sprintf(msg, " by destroying %d treasures and successfully defending %d",
			max_destroyed, max_left);
		Team_game_over(winning_team, msg);
		return;
	    }

	    for (winners = i = 0; i < MAX_TEAMS; i++) {
		if (!team_win[i])
		    continue;
		if (team_score[i] == max_score) {
		    winning_team = i;
		    winners++;
		} else {
		    team_win[i] = 0;
		}
	    }
	    if (winners == 1) {
		sprintf(msg, " by destroying %d treasures, saving %d, and "
			"scoring %d points",
			max_destroyed, max_left, max_score);
		Team_game_over(winning_team, msg);
		return;
	    }

	    /* Highly unlikely */

	    sprintf(msg, " between teams ");
	    bp = msg + strlen(msg);
	    for (i = 0; i < MAX_TEAMS; i++) {
		if (!team_win[i])
		    continue;
		*bp++ = "0123456789"[i]; *bp++ = ','; *bp++ = ' ';
	    }
	    bp -= 2;
	    *bp = '\0';
	    Team_game_over(-1, msg);

	}
	else if (num_dead_teams > 0) {
	    if (num_alive_teams == 1)
		Team_game_over(winning_team, " by staying alive");
	    else
		Team_game_over(-1, " as everyone died");
	}
	else {
	    /*
	     * num_alive_teams <= 1 && num_dead_teams == 0
	     *
	     * There is a possibility that the game has ended because players
	     * quit, the game over state is needed to reset treasures.  We
	     * must count how many treasures are missing, if there are any
	     * the playing team (if any) wins.
	     */
	    int	i, treasures_destroyed;

	    for (treasures_destroyed = i = 0; i < MAX_TEAMS; i++)
		treasures_destroyed += (World.teams[i].NumTreasures
					- World.teams[i].TreasuresLeft);
	    if (treasures_destroyed)
		Team_game_over(winning_team, " by staying in the game");
	}

    } else {

    /* Do we have a winner ? (No team play) */
	int num_alive_players = 0;
	int num_active_players = 0;
	int num_alive_robots = 0;
	int num_active_humans = 0;
	int winner = -1;

	for (i=0; i<NumPlayers; i++)  {
	    if (BIT(Players[i]->status, PAUSE))
		continue;

	    if (!BIT(Players[i]->status, GAME_OVER)) {
		num_alive_players++;
		if (IS_ROBOT_IND(i)) {
		    num_alive_robots++;
		}
		winner = i; 	/* Tag player that's alive */
	    }
	    else if (IS_HUMAN_IND(i)) {
		num_active_humans++;
	    }
	    num_active_players++;
	}

	if (num_alive_players == 1 && num_active_players > 1) {
	    Individual_game_over(winner);
	}
	else if (num_alive_players == 0 && num_active_players >= 1) {
	    Individual_game_over(-1);
	}
	else if (num_alive_robots > 1
	    && num_alive_players == num_alive_robots
	    && num_active_humans > 0) {
	    Individual_game_over(-2);
	}
	else if (maxRoundTime > 0 && roundtime == 0) {
	    Set_message("Timer expired. Round ends now.");
	    Individual_game_over(-1);
	}
    }
}

void Delete_player(int ind)
{
    player		*pl = Players[ind];
    object		*obj;
    int			i, j,
			id = pl->id;

    if (IS_ROBOT_PTR(pl)) {
	Robot_destroy(ind);
    }

    /* Delete remaining shots */
    for (i = NumObjs - 1; i >= 0; i--) {
	obj = Obj[i];
	if (obj->id == id) {
	    if (obj->type == OBJ_BALL) {
		Delete_shot(i);
		obj->owner = -1;
	    }
	    else if (BIT(obj->type, OBJ_DEBRIS | OBJ_SPARK)) {
		/* Okay, so you want robot explosions to exist,
		 * even if the robot left the game. */
		obj->id = -1;
	    }
	    else {
		if (!keepShots)
		    obj->life = 0;
	        obj->id = -1;
	    }
	}
	else if (obj->owner == id) {
	    obj->owner = -1;
	}
    }

    Free_ship_shape(pl->ship);

    NumPlayers--;

    if (pl->team != TEAM_NOT_SET) {
	World.teams[pl->team].NumMembers--;
	if (IS_ROBOT_PTR(pl))
	    World.teams[pl->team].NumRobots--;
    }

    if (IS_ROBOT_PTR(pl))
	NumRobots--;

    /*
     * Swap entry no 'ind' with the last one.
     *
     * Change the Players[] pointer array to have Players[ind] point to
     * a valid player and move our leaving player to Players[NumPlayers].
     */
    pl			= Players[NumPlayers];	/* Swap pointers... */
    Players[NumPlayers]	= Players[ind];
    Players[ind]	= pl;
    pl			= Players[NumPlayers];	/* Restore pointer. */

    GetInd[Players[ind]->id] = ind;
    GetInd[Players[NumPlayers]->id] = NumPlayers;

    Check_team_members(pl->team);

    for (i = NumPlayers - 1; i >= 0; i--) {
	if (BIT(Players[i]->lock.tagged, LOCK_PLAYER|LOCK_VISIBLE)
	    && (Players[i]->lock.pl_id == id || NumPlayers <= 1)) {
	    CLR_BIT(Players[i]->lock.tagged, LOCK_PLAYER|LOCK_VISIBLE);
	}
	if (IS_ROBOT_IND(i)
	    && Robot_war_on_player(i) == id) {
	    Robot_reset_war(i);
	}
	for (j = 0; j < LOCKBANK_MAX; j++) {
	    if (Players[i]->lockbank[j] == id)
		Players[i]->lockbank[j] = NOT_CONNECTED;
	}
	for (j = 0; j < MAX_RECORDED_SHOVES; j++) {
	    if (Players[i]->shove_record[j].pusher_id == id) {
		Players[i]->shove_record[j].pusher_id = -1;
	    }
	}
    }

    for (i = NumPlayers - 1; i >= 0; i--) {
	if (Players[i]->conn != NOT_CONNECTED) {
	    Send_leave(Players[i]->conn, id);
	}
    }

    release_ID(id);
}

void Detach_ball(int ind, int obj)
{
    int			i, cnt;

    if (obj == -1 || Obj[obj] == Players[ind]->ball) {
	Players[ind]->ball = NULL;
	CLR_BIT(Players[ind]->used, OBJ_CONNECTOR);
    }

    if (BIT(Players[ind]->have, OBJ_BALL)) {
	for (cnt = i = 0; i < NumObjs; i++) {
	    if (Obj[i]->type == OBJ_BALL && Obj[i]->id == Players[ind]->id) {
		if (obj == -1 || obj == i) {
		    Obj[i]->id = -1;
		    /* Don't reset owner so you can throw balls */
		} else {
		    cnt++;
		}
	    }
	}
	if (cnt == 0)
	    CLR_BIT(Players[ind]->have, OBJ_BALL);
    }
}

void Kill_player(int ind)
{
    Explode_fighter(ind);
    Player_death_reset(ind);
}

void Player_death_reset(int ind)
{
    player		*pl = Players[ind];
    int			i;

    Detach_ball(ind, -1);

    pl->vel.x		= pl->vel.y	= 0.0;
    pl->acc.x		= pl->acc.y	= 0.0;
    pl->emptymass	= pl->mass	= ShipMass;
    pl->status		|= DEF_BITS;
    pl->status		&= ~(KILL_BITS);

    for (i = 0; i < NUM_ITEMS; i++) {
	if (!BIT(1U << i, ITEM_BIT_FUEL | ITEM_BIT_TANK)) {
	    pl->item[i] = World.items[i].initial;
	}
    }

    pl->shot_speed	= ShotsSpeed;
    pl->shot_max	= ShotsMax;
    pl->shot_life	= ShotsLife;
    pl->shot_mass	= ShotsMass;
    pl->count		= RECOVERY_DELAY;
    pl->damaged 	= 0;
    pl->stunned		= 0;
    pl->lock.distance	= 0;

    Player_init_fuel(ind, World.items[ITEM_FUEL].initial * FUEL_SCALE_FACT);

    /*-BA Handle the combination of limited life games and
     *-BA robotLeaveLife by making a robot leave iff it gets
     *-BA eliminated in any round.  Means that robotLeaveLife
     *-BA is ignored, but that robotsLeave is still respected.
     *-BD Added check on race mode. Since in race mode everyone
     *-BD gets killed at the end of the round, all robots would
     *-BD be replaced in the next round. I don't think that's
     *-BD the Right Thing to do.
     *-BD Also, only check a robot's score at the end of the round.
     *-BD 27-2-98 Check on team mode too. It's very confusing to
     *-BD have different robots in your team every round.
     */

    if (BIT(World.rules->mode, LIMITED_LIVES)) { 
	pl->life--;
	if (pl->life == -1) {
	    if (IS_ROBOT_PTR(pl)) {
	      if (!BIT(World.rules->mode, TEAM_PLAY)
		  || (robotsLeave && pl->score < robotLeaveScore)) {
		Robot_delete(ind, false);
		return;
	      }
	    }
	    pl->life = 0;
	    SET_BIT(pl->status, GAME_OVER);
	    pl->mychar = 'D';
	    Player_lock_closest(ind, 0);
	}
    }
    else {
	pl->life++;
    }

    pl->deaths++;

    pl->have	= DEF_HAVE;
    pl->used	|= DEF_USED;
    pl->used	&= ~(USED_KILL);
    pl->used	&= pl->have;
}
