/*
 * player.h
 *
 *  Created on: May 21, 2009
 *      Author: rotunda
 */

#ifndef PLAYER_H_
#define PLAYER_H_

#include "structs.h"
#include "object.h"
#include "score.h"

/*
 * Different types of attributes a player can have.
 * These are the bits of the player->have and player->used fields.
 */
#define HAS_NOTHING (0)
#define HAS_EMERGENCY_THRUST (1U << 30)
#define HAS_AUTOPILOT (1U << 29)
#define HAS_TRACTOR_BEAM (1U << 28)
#define HAS_LASER (1U << 27)
#define HAS_CLOAKING_DEVICE (1U << 26)
#define HAS_SHIELD (1U << 25)
#define HAS_REFUEL (1U << 24)
#define HAS_REPAIR (1U << 23)
#define HAS_COMPASS (1U << 22)
#define HAS_AFTERBURNER (1U << 21)
#define HAS_CONNECTOR (1U << 20)
#define HAS_EMERGENCY_SHIELD (1U << 19)
#define HAS_DEFLECTOR (1U << 18)
#define HAS_PHASING_DEVICE (1U << 17)
#define HAS_MIRROR (1U << 16)
#define HAS_ARMOR (1U << 15)
#define HAS_SHOT (1U << 4)
#define HAS_BALL (1U << 3)

#define USES_NOTHING HAS_NOTHING
#define USES_EMERGENCY_THRUST HAS_EMERGENCY_THRUST
#define USES_AUTOPILOT HAS_AUTOPILOT
#define USES_TRACTOR_BEAM HAS_TRACTOR_BEAM
#define USES_LASER HAS_LASER
#define USES_CLOAKING_DEVICE HAS_CLOAKING_DEVICE
#define USES_SHIELD HAS_SHIELD
#define USES_REFUEL HAS_REFUEL
#define USES_REPAIR HAS_REPAIR
#define USES_COMPASS HAS_COMPASS
#define USES_AFTERBURNER HAS_AFTERBURNER
#define USES_CONNECTOR HAS_CONNECTOR
#define USES_EMERGENCY_SHIELD HAS_EMERGENCY_SHIELD
#define USES_DEFLECTOR HAS_DEFLECTOR
#define USES_PHASING_DEVICE HAS_PHASING_DEVICE
#define USES_MIRROR HAS_MIRROR
#define USES_ARMOR HAS_ARMOR
#define USES_SHOT HAS_SHOT
#define USES_BALL HAS_BALL

/*
 * Possible player status bits.
 * The bits that the client needs must fit into a byte,
 * so the first 8 bitvalues are reserved for that purpose,
 * those are defined in common/rules.h.
 */
#define HOVERPAUSE (1U << 9) /* Hovering pause */
#define REPROGRAM (1U << 10) /* Player reprogramming */
#define FINISH (1U << 11)	 /* Finished a lap this frame */
#define RACE_OVER (1U << 12) /* After finished and score */

#define LOCK_NONE 0x00	  /* No lock */
#define LOCK_PLAYER 0x01  /* Locked on player */
#define LOCK_VISIBLE 0x02 /* Lock information was on HUD */
						  /* computed just before frame shown */
						  /* and client input checked */
#define LOCKBANK_MAX 4	  /* Maximum number of locks in bank */

#define MAX_FUEL_DISTANCE 100 /* maximum distance from which a player can connect to a fuel station */

/*
 * Player type. These values are set in the player->pl_type field.
 */
enum player_type_
{
	PL_TYPE_HUMAN = 1U << 0,
	PL_TYPE_ROBOT = 1U << 1,
	PL_TYPE_TANK = 1U << 2
};

/*
 * These values are set in the player->pl_state field.
 */
enum player_state_
{
	PL_STATE_UNDEFINED = 1U << 0,
	PL_STATE_WAITING = 1U << 1,
	PL_STATE_APPEARING = 1U << 2,
	PL_STATE_ALIVE = 1U << 3,
	PL_STATE_KILLED = 1U << 4, /* temporary state; should transform to APPEARING, DEAD or PAUSED in the same frame */
	PL_STATE_DEAD = 1U << 5,
	PL_STATE_PAUSED = 1U << 6
};

/*
 * Fuel structure, used by player
 */
typedef struct
{
	int32_t sum; /* Sum of fuel in all tanks */
	int32_t max; /* How much fuel can you take? */
	int32_t l1;	 /* Fuel critical level */
	int32_t l2;	 /* Fuel warning level */
	int32_t l3;	 /* Fuel notify level */
} pl_fuel_t;

/* IMPORTANT
 *
 * This is the player structure, the first part MUST be similar to object_t,
 * this makes it possible to use the same basic operations on both of them
 * (mainly used in update.c).
 */
struct _player
{
	OBJECT_BASE

	/* up to here the player type should be the same as an object. */

	int32_t pl_type;		/* extended type info (tank, robot) */
	char pl_type_mychar;	/* Special char for player type */
	char mychar;			/* Special char for player state */
	uint32_t pl_old_status; /* OLD_PLAYING etc. */
	uint16_t pl_status;		/* HOVERPAUSE etc. */
	uint16_t pl_state;		/* one of PL_STATE_*; ONLY ONE can be set at a time */
	int32_t pl_life;		/* Lives left (if lives limited) */

	double pause_count;			/* ticks until unpause possible */
	double recovery_count;		/* ticks to recovery */
	double self_destruct_count; /* if > 0, ticks before boom */
	int32_t shot_delay_count;	/* ticks between two consecutive shots fired by one player */
	double shield_count;		/* Shields if no playerShielding */

	DFLOAT velocity; /* Absolute speed */
	DFLOAT velocity_interp;

	/*
	 * Number of kills this round. A kill is accounted to a player iff (s)he kills another
	 * player and is not killed in the process by the same event. This excludes e.g. crashes
	 * between players.
	 */
	int32_t kills;
	int32_t deaths;		 /* Number of deaths this round */
	int32_t self_deaths; /* Number of deaths caused by the player himself */

	int32_t used; /** Items you use **/
	int32_t have; /** Items you have **/

	pl_fuel_t fuel;			 /* ship tanks and the stored fuel */
	DFLOAT emptymass;		 /* Mass of empty ship */
	DFLOAT float_dir;		 /* Direction, in float var */
	DFLOAT turnvel;			 /* Current velocity of turn (right) */
	DFLOAT turnacc;			 /* Current acceleration of turn */
	DFLOAT power;			 /* Force of thrust */
	DFLOAT power_s;			 /* Saved power fiks */
	DFLOAT turnspeed;		 /* How fast player acc-turns */
	DFLOAT turnspeed_s;		 /* Saved turnspeed */
	DFLOAT turnresistance;	 /* How much is lost in % */
	DFLOAT turnresistance_s; /* Saved (see above) */

	int32_t score; /* Current score of player */

	shipshape_t *ship; /* wire model of ship shape */

	int32_t item[NUM_ITEMS]; /* for each item type how many */

	int32_t shots;	   /* Number of active shots by player */
	int32_t shot_max;  /* Maximum number of shots active */
	DFLOAT shot_speed; /* Speed of shots fired by player */
	int32_t shot_time; /* Time (in ticks) of last shot fired by player */

	fuel_t *fs; /* Connected to fuel station fs */

	base_t *home_base; /* Pointer to the home base */

	double survival_time; /* time player has survived unshielded*/

	struct
	{
		int32_t flags;	 /* Flag, what is tagged? */
		void *object;	 /* Tagged object pointer */
		DFLOAT distance; /* Distance to object in pixels */
	} lock;
	void *lockbank[LOCKBANK_MAX]; /* Saved player locks */

	char name[MAX_CHARS];	  /* Nick-name of player */
	char realname[MAX_CHARS]; /* Real name of player */
	char hostname[MAX_CHARS]; /* Hostname of client player uses */

	object_t *ball_tmp; /* Ball that is BEING attached (string not solid yet) */

	/*
	 * Pointer to robot private data (dynamically allocated).
	 * Only used in robot code.
	 */
	struct robot_data *robot_data_ptr;

	/*
	 * A record of who's been pushing me (a circular buffer).
	 */
	shove_t shove_record[MAX_RECORDED_SHOVES];
	int32_t shove_next;

	connection_t *connp; /* pointer to the connection structure */

	uint32_t version; /* XPilot version number of client */

	BITV_DECL(last_keyv, NUM_KEYS); /* Keyboard state */
	BITV_DECL(prev_keyv, NUM_KEYS); /* Keyboard state */

	int32_t frame_last_busy; /* When player touched keyboard. */

	int32_t player_fps; /* FPS that this player can do */

	bool isoperator;		/* If player has operator privileges. */
	bool update_scoreboard; /* If score table info needs to be sent when updateScores is set to true */
	struct ranknode *rank;
	bool oldturn; /* Use old turn player code */
};

// Returns pointer to the player's connection structure, or NULL if
// the supplied pointer is invalid or if the player is disconnected.
#define Player_is_connected(pl) ((pl) ? ((pl)->connp) : NULL)

#define Player_tanks_are_full(pl) ((pl->fuel.sum >= pl->fuel.max) ? true : false)

void Player_set_state(player_t *pl, int state);
player_state_t Player_compute_entry_state(player_t *pl, team_t *team);

int32_t Players_kick_paused(void);
void Player_pause_forced(player_t *pl);
void Player_pause_self(player_t *pl);
void Player_unpause(player_t *pl, team_t *team);
bool Player_is_close_to_base(player_t *pl);

void Player_swap_team(player_t *pl, team_t *team);
void Players_swap_teams(player_t *pl1, player_t *pl2);
void Set_swapper_state(player_t *pl);

void Player_add_to_team(player_t *pl, team_t *team);
void Player_remove_from_team(player_t *pl);

bool Round_count_killed_players(team_t *team);

bool Player_lock_is_allowed(player_t *pl, player_t *pl_lock);
bool Player_lock_closest(player_t *pl, bool next);
void Player_lock_next(player_t *pl, bool forwards);
bool Player_lock_is_initialized(player_t *pl);
void Player_lock_set(player_t *pl, player_t *pl_target);
void Player_lock_clear(player_t *pl);

void Player_fuel_init(player_t *pl, int32_t fuel);
void Player_fuel_add(player_t *pl, int32_t fuel);
void Player_refuel_start(player_t *pl);
bool Player_refuel_advance(player_t *pl);
void Player_refuel_stop(player_t *pl);

void Player_change_home(player_t *pl, base_t *base);
player_t *Player_owns_base(base_t *base);

void Players_shoot(void);

void Players_handle_after_kill(void);
void Players_remove(void);

object_t *Player_get_ball_object(player_t *pl);

void Player_go_home(player_t *pl);
bool Player_is_home(player_t *pl);
void Player_proceed_return_home(player_t *pl);

player_t *Player_add(void);
void Player_remove(player_t *pl);
void Players_allocate(int32_t number);
void Players_free(void);

void Players_send_score(void);
void Players_reset(void);

void Player_destroy(player_t *pl);
void Player_reset(player_t *pl);

void Individual_game_over(int32_t winner);

bool Player_idle_timed_out(player_t *pl);
bool Player_is_recovered(player_t *pl);

void Players_interpolation_init(void);
void Player_interpolation_reset(player_t *pl);

#endif /* PLAYER_H_ */
