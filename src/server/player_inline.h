/*
 * player_inline.h
 *
 *  Created on: Nov 15, 2009
 *      Author: rotunda
 */

#ifndef PLAYER_INLINE_H_
#define PLAYER_INLINE_H_

static inline player_t *Player_by_id(int32_t id) {
	return Players[GetInd[id]];
}

static inline player_t *Player_by_index(int32_t ind)
{
	return Players[ind];
}

// ==============================================================================
// type - from among USES_*
static inline bool Player_uses_property(player_t *pl, uint32_t type)
{
	return BIT((pl)->used, type) ? true : false;
}

// type - from among USES_*
static inline void Player_enable_property(player_t *pl, uint32_t type)
{
	SET_BIT(pl->used, type);
}

// type - from among USES_*
static inline void Player_disable_property(player_t *pl, uint32_t type)
{
	CLR_BIT(pl->used, type);
}

// type - from among USES_*
static inline void Player_toggle_property(player_t *pl, uint32_t type)
{
	TOGGLE_BIT(pl->used, type);
}

static inline bool Player_is_refueling(player_t *pl)
{
	if (Player_uses_property(pl, USES_REFUEL))
		return true;
	return false;
}

static inline bool Player_uses_connector(player_t *pl)
{
	if (Player_uses_property(pl, USES_CONNECTOR))
		return true;
	return false;
}

// ==============================================================================
// type - from among HAS_*
static inline bool Player_has_property(player_t *pl, uint32_t type)
{
	return BIT((pl)->have, type) ? true : false;
}

// type - from among HAS_*
static inline void Player_get_property(player_t *pl, uint32_t type)
{
	SET_BIT(pl->have, type);
}

// type - from among HAS_*
static inline void Player_lose_property(player_t *pl, uint32_t type)
{
	CLR_BIT(pl->have, type);
}

// ==============================================================================
static inline bool Player_is_state(player_t *pl, player_state_t state)
{
	return BIT(pl->pl_state, state) ? true : false;
}

static inline bool Player_is_active(player_t *pl)
{
	return (pl->pl_state == PL_STATE_ALIVE || pl->pl_state == PL_STATE_APPEARING || pl->pl_state == PL_STATE_WAITING
			|| pl->pl_state == PL_STATE_KILLED || pl->pl_state == PL_STATE_DEAD) ? true : false;
}

static inline bool Player_is_uninitialized(player_t *pl)
{
	return pl->pl_state == PL_STATE_UNDEFINED ? true : false;
}

static inline bool Player_is_waiting(player_t *pl)
{
	return pl->pl_state == PL_STATE_WAITING ? true : false;
}

static inline bool Player_is_appearing(player_t *pl)
{
	return pl->pl_state == PL_STATE_APPEARING ? true : false;
}

static inline bool Player_is_alive(player_t *pl)
{
	return pl->pl_state == PL_STATE_ALIVE ? true : false;
}

/* player was killed this frame ? */
static inline bool Player_is_killed(player_t *pl)
{
	return pl->pl_state == PL_STATE_KILLED ? true : false;
}
static inline bool Player_is_dead(player_t *pl)
{
	return pl->pl_state == PL_STATE_DEAD ? true : false;
}

static inline bool Player_is_paused(player_t *pl)
{
	return pl->pl_state == PL_STATE_PAUSED ? true : false;
}

//static inline bool Player_is_paused(player_t *pl)
//{
//	return pl->team->Num == PAUSE_TEAM_NUM ? true : false;
//}

// ==============================================================================
//static inline void Player_add_score(player_t *pl, double points)
//{
//	Add_Score(pl, points);
//	pl->update_scoreboard = true;
//	updateScores = true;
//}


//static inline void Player_set_score(player_t *pl, double points)
//{
//	Score_set(pl, points);
//}

static inline void Player_set_mychar(player_t *pl, char mychar)
{
	pl->mychar = mychar;
}

static inline void Player_set_life(player_t *pl, int life)
{
	pl->pl_life = life;
}

// ==============================================================================
static inline bool Player_is_human(player_t *pl)
{
	return pl->pl_type == PL_TYPE_HUMAN ? true : false;
}

static inline bool Player_is_robot(player_t *pl)
{
	return pl->pl_type == PL_TYPE_ROBOT ? true : false;
}

static inline bool Player_is_tank(player_t *pl)
{
	return pl->pl_type == PL_TYPE_TANK ? true : false;
}

// ==============================================================================
static inline bool Player_owns_tank(player_t *pl, player_t *tank)
{
	if (Player_is_tank(tank) && tank->lock.object /* kps - probably redundant */
	&& tank->lock.object == pl)
		return true;
	return false;
}

// ==============================================================================
static inline bool Player_is_self_destructing(player_t *pl)
{
	return (pl->self_destruct_count > 0.0) ? true : false;
}

static inline void Player_self_destruct(player_t *pl, bool on)
{
	if (on) {
		if (Player_is_self_destructing(pl))
			return;
		pl->self_destruct_count = SELF_DESTRUCT_DELAY_TICKS;
	}
	else {
		pl->self_destruct_count = 0.0;
	}
}

static inline bool Player_is_hoverpaused(player_t *pl)
{
	if (BIT(pl->pl_status, HOVERPAUSE))
		return true;
	return false;
}

static inline bool Player_is_thrusting(player_t *pl)
{
	if (BIT(pl->obj_status, THRUSTING))
		return true;
	return false;
}

/*
 * Used where we wish to know if a player is simply on the same team.
 */
static inline bool Players_are_teammates(player_t *pl1, player_t *pl2)
{
	if (BIT(World.rules->mode, TEAM_PLAY) && pl1->team && pl1->team == pl2->team) {
		return true;
	}
	return false;
}

#endif /* PLAYER_INLINE_H_ */
