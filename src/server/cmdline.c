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
/* Options parsing code contributed by Ted Lemon <mellon@ncd.com> */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "version.h"
#include "xpconfig.h"
#include "serverconst.h"
#include "global.h"
#include "proto.h"
#include "defaults.h"
#include "error.h"
#include "portability.h"
#include "checknames.h"
#include "tuner.h"
#include "list.h"
#include "map.h"


char cmdline_version[] = VERSION;

TList expandList; /* List of predefined settings. */
DFLOAT ShipMass; /* Default mass of ship */
DFLOAT ballMass; /* Default mass of balls */
DFLOAT ShotsMass; /* Default mass of shots */
DFLOAT ShotsSpeed; /* Default speed of shots */
int32_t ShotsLife; /* Default number of ticks */
/* each shot will live */

int32_t maxRobots; /* How many robots should enter */
int32_t minRobots; /* the game? */
char *robotFile; /* Filename for robot parameters */
int32_t robotsTalk; /* Do robots talk? */
int32_t robotsLeave; /* Do robots leave at all? */
int32_t robotLeaveLife; /* Max life per robot (0=off)*/
int32_t robotLeaveScore; /* Min score for robot to live (0=off)*/
int32_t robotLeaveRatio; /* Min ratio for robot to live (0=off)*/
int32_t robotTeam; /* Team for robots */
bool restrictRobots; /* Restrict robots to robotTeam? */
bool reserveRobotTeam; /* Allow only robots in robotTeam? */

int32_t ShotsMax; /* Max shots pr. player */
int32_t fireRepeatRate; /* Frames per autorepeat fire (0=off) */

bool RawMode; /* Let robots live even if there */
/* are no players logged in */
bool logRobots; /* log robots coming and going */
char *mapFileName; /* Name of mapfile... */
char *mapData; /* Raw map data... */
int32_t mapWidth; /* Width of the universe */
int32_t mapHeight; /* Height of the universe */
char *mapName; /* Name of the universe */
char *mapAuthor; /* Name of the creator */
int32_t contactPort; /* Contact port number */
char *serverHost; /* Host name (for multihomed hosts) */
char *greeting; /* Greeting for joining players */

bool crashWithPlayer; /* Can players overrun other players? */
bool bounceWithPlayer; /* Can players bounce other players? */
bool playerKillings; /* Can players kill each other? */
bool playerShielding = false; /* Can players use shields? */
bool playerStartsShielded = true; /* Players start with shields up? */
bool shotsWallBounce; /* Do shots bounce off walls? */
bool sparksWallBounce; /* Do sparks bounce off walls? */
bool debrisWallBounce; /* Do sparks bounce off walls? */
bool ballCollisions; /* Do balls participate in colls.? */
DFLOAT maxObjectWallBounceSpeed; /* max object bounce speed */
DFLOAT maxShieldedWallBounceSpeed; /* max shielded bounce speed */
DFLOAT maxUnshieldedWallBounceSpeed; /* max unshielded bounce speed */
DFLOAT playerWallBrakeFactor; /* wall lowers speed if less than 1 */
DFLOAT objectWallBrakeFactor; /* wall lowers speed if less than 1 */
DFLOAT objectWallBounceLifeFactor; /* reduce object life */
bool limitedLives; /* Are lives limited? */
int32_t worldLives; /* If so, what's the max? */
bool endOfRoundReset; /* Reset the world when round ends? */
int32_t resetOnHuman; /* Last human to reset round for */
bool teamPlay; /* Are teams allowed? */
bool teamFuel; /* Do fuelstations belong to teams? */
bool keepShots; /* Keep shots when player leaves? */

bool extraBorder; /* Give map an extra border? */

char *defaultsFileName; /* Name of defaults file... */
char *passwordFileName; /* Name of password file... */
char *motdFileName; /* Name of motd file */
char *scoreTableFileName; /* Name of score table file */
char *adminMessageFileName; /* Name of admin message file */
int32_t adminMessageFileSizeLimit;/* Limit on admin message file size */

bool allowShipShapes;

bool playersOnRadar; /* Are players visible on radar? */

bool reportToMetaServer; /* Send status to meta-server? */
bool searchDomainForXPilot; /* Do a DNS lookup for XPilot.domain? */
char *denyHosts; /* Computers which are denied service */

bool teamAssign; /* Assign player to team if not set? */

/* Are team members immune to effects of
 * collisions with objects owned by a team mate?
 * NOTE: balls are not subject to teamImmunity */
bool teamImmunity;

bool treasureCollisionDestroys; /* Do balls pop after collision with players? */
bool treasureCollisionMayKill; /* Do unshielded players always die after collision with balls? */
bool wreckageCollisionMayKill; /* Do unshielded players always die after collision with wreckage? */

DFLOAT ballConnectorSpringConstant;
DFLOAT ballConnectorDamping;
DFLOAT maxBallConnectorRatio;
DFLOAT ballConnectorLength;

bool lockOtherTeam; /* lock ply from other teams when dead? */
bool useWreckage; /* destroyed ships leave wreckage? */

int32_t roundsToPlay; /* # of rounds to play. */
int32_t roundsPlayed; /* # of rounds played sofar. */

int32_t maxVisibleObject; /* how many objects a player can see */
bool pLockServer; /* Is server swappable out of memory?  */
char *password; /* password for operator status */
int32_t clientPortStart; /* First UDP port for clients */
int32_t clientPortEnd; /* Last one (these are for firewalls) */

char *robotRealName; /* Real name for robot */
char *robotHostName; /* Host name for robot */

bool selfImmunity; /* Are players immune to their own weapons? */

char *defaultShipShape; /* What ship shape is used for players */
/* who do not define their own? */
int32_t maxPauseTime; /* Max. time you can stay paused for */

float mainLoopTime;

extern char conf_logfile_string[]; /* Default name of log file */

/*
 ** Two functions which can be used if an option
 ** does not have its own function which should
 ** be called after the option value has been
 ** changed during runtime.  The tuner_none
 ** function should be specified when an option
 ** cannot be changed at all during runtime.
 ** The tuner_dummy can be specified if it
 ** is OK to modify the option during runtime
 ** and no follow up action is needed.
 */
void tuner_none(void)
{
}
void tuner_dummy(void)
{
}

static void Tune_robot_real_name(void)
{
	Fix_real_name(robotRealName);
}
static void Tune_robot_host_name(void)
{
	Fix_host_name(robotHostName);
}

static option_desc
		options[] =
			{
						{
								"help",
								"help",
								"0",
								NULL,
								valVoid,
								tuner_none,
								"Print out this help message.\n",
								OPT_NONE },
						{
								"version",
								"version",
								"0",
								NULL,
								valVoid,
								tuner_none,
								"Print version information.\n",
								OPT_NONE },
						{
								"dump",
								"dump",
								"0",
								NULL,
								valVoid,
								tuner_none,
								"Print all options with their default values in defaultsfile format.\n",
								OPT_NONE },
						{
								"expand",
								"expand",
								"",
								&expandList,
								valList,
								tuner_none,
								"Expand a comma separated list of predefined settings.\n"
									"These settings can be defined in either the defaults file\n"
									"or in a map file using the \"define:\" operator.\n",
								OPT_COMMAND },
						{
								"shipMass",
								"shipMass",
								"20.0",
								&ShipMass,
								valReal,
								tuner_shipmass,
								"Mass of fighters.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"ballMass",
								"ballMass",
								"50.0",
								&ballMass,
								valReal,
								tuner_ballmass,
								"Mass of balls.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"shotMass",
								"shotMass",
								"0.1",
								&ShotsMass,
								valReal,
								tuner_dummy,
								"Mass of bullets.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"shotSpeed",
								"shotSpeed",
								"21.0",
								&ShotsSpeed,
								valReal,
								tuner_dummy,
								"Maximum speed of bullets.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"shotLife",
								"shotLife",
								"60",
								&ShotsLife,
								valInt,
								tuner_dummy,
								"Life of bullets in ticks.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"fireRepeatRate",
								"fireRepeat",
								"2",
								&fireRepeatRate,
								valInt,
								tuner_dummy,
								"Number of frames per automatic fire (0=off).\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"maxRobots",
								"robots",
								"4",
								&maxRobots,
								valInt,
								tuner_maxrobots,
								"The maximum number of robots wanted.\n"
									"Adds robots if there are less than maxRobots players.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"robotFile",
								"robotFile",
								NULL,
								&robotFile,
								valString,
								tuner_none,
								"The file containing parameters for robot details.\n",
								OPT_COMMAND
										| OPT_DEFAULTS },
						{
								"robotsTalk",
								"robotsTalk",
								"false",
								&robotsTalk,
								valBool,
								tuner_dummy,
								"Do robots talk when they kill, die etc.?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"robotsLeave",
								"robotsLeave",
								"true",
								&robotsLeave,
								valBool,
								tuner_dummy,
								"Do robots leave the game?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"robotLeaveLife",
								"robotLeaveLife",
								"50",
								&robotLeaveLife,
								valInt,
								tuner_dummy,
								"Max life per robot (0=off).\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"robotLeaveScore",
								"robotLeaveScore",
								"-90",
								&robotLeaveScore,
								valInt,
								tuner_dummy,
								"Min score for robot to play (0=off).\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"robotLeaveRatio",
								"robotLeaveRatio",
								"-5",
								&robotLeaveRatio,
								valInt,
								tuner_dummy,
								"Min ratio for robot to play (0=off).\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"robotTeam",
								"robotTeam",
								"-1",
								&robotTeam,
								valInt,
								tuner_robotteam,
								"Team to use for robots.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"restrictRobots",
								"restrictRobots",
								"true",
								&restrictRobots,
								valBool,
								tuner_dummy,
								"Are robots restricted to their own team?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"reserveRobotTeam",
								"reserveRobotTeam",
								"true",
								&reserveRobotTeam,
								valBool,
								tuner_dummy,
								"Is the robot team only for robots?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"robotRealName",
								"robotRealName",
								"robot",
								&robotRealName,
								valString,
								Tune_robot_real_name,
								"What is the robots' apparent real name?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"robotHostName",
								"robotHostName",
								"xpilot.org",
								&robotHostName,
								valString,
								Tune_robot_host_name,
								"What is the robots' apparent host name?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"selfImmunity",
								"selfImmunity",
								"false",
								&selfImmunity,
								valBool,
								tuner_dummy,
								"Are players immune to their own weapons?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"defaultShipShape",
								"defaultShipShape",
								"(NM:Default)(AU:Unknown)(SH: 15,0 -9,8 -9,-8)(MG: 15,0)(LG: 15,0)"
									"(RG: 15,0)(EN: -9,0)(LR: -9,8)(RR: -9,-8)(LL: -9,8)(RL: -9,-8)"
									"(MR: 15,0)",
								&defaultShipShape,
								valString,
								tuner_none,
								"What is the default ship shape for people who do not have a ship\n"
									"shape defined?",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"maxPlayerShots",
								"shots",
								"256",
								&ShotsMax,
								valInt,
								tuner_shotsmax,
								"Maximum allowed bullets per player.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"idleRun",
								"rawMode",
								"false",
								&RawMode,
								valBool,
								tuner_dummy,
								"Do robots keep on playing even if all human players quit?\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"logRobots",
								"logRobots",
								"true",
								&logRobots,
								valBool,
								tuner_dummy,
								"Log the comings and goings of robots.\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"mapWidth",
								"mapWidth",
								"100",
								&mapWidth,
								valInt,
								tuner_none,
								"Width of the world in blocks.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"mapHeight",
								"mapHeight",
								"100",
								&mapHeight,
								valInt,
								tuner_none,
								"Height of the world in blocks.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"mapFileName",
								"map",
								NULL,
								&mapFileName,
								valString,
								tuner_none,
								"The filename of the map to use.\n"
									"Or \"wild\" if you want a map by The Wild Map Generator.\n"
									"The geometry of a \"wild\" map is given by the -mapWidth\n"
									"and the -mapHeight options.\n",
								OPT_COMMAND
										| OPT_DEFAULTS },
						{
								"mapName",
								"mapName",
								"unknown",
								&mapName,
								valString,
								tuner_none,
								"The title of the map.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"mapAuthor",
								"mapAuthor",
								"anonymous",
								&mapAuthor,
								valString,
								tuner_none,
								"The name of the map author.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"contactPort",
								"port",
								"15345",
								&contactPort,
								valInt,
								tuner_none,
								"The server contact port number.\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"serverHost",
								"serverHost",
								NULL,
								&serverHost,
								valString,
								tuner_none,
								"The server's fully qualified domain name (for multihomed hosts).\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"greeting",
								"greeting",
								NULL,
								&greeting,
								valString,
								tuner_dummy,
								"Short greeting string for players when they login to server.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"mapData",
								"mapData",
								NULL,
								&mapData,
								valString,
								tuner_none,
								"The map's topology.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"allowPlayerCrashes",
								"allowPlayerCrashes",
								"yes",
								&crashWithPlayer,
								valBool,
								Set_world_rules,
								"Can players overrun other players?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"allowPlayerBounces",
								"allowPlayerBounces",
								"yes",
								&bounceWithPlayer,
								valBool,
								Set_world_rules,
								"Can players bounce with other players?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"allowPlayerKilling",
								"killings",
								"yes",
								&playerKillings,
								valBool,
								Set_world_rules,
								"Should players be allowed to kill one other?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"shotsWallBounce",
								"shotsWallBounce",
								"no",
								&shotsWallBounce,
								valBool,
								Move_init,
								"Do shots bounce off walls?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"ballCollisions",
								"ballCollisions",
								"no",
								&ballCollisions,
								valBool,
								tuner_dummy,
								"Can balls collide with other objects?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"sparksWallBounce",
								"sparksWallBounce",
								"no",
								&sparksWallBounce,
								valBool,
								Move_init,
								"Do thrust spark particles bounce off walls to give reactive thrust?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"debrisWallBounce",
								"debrisWallBounce",
								"no",
								&debrisWallBounce,
								valBool,
								Move_init,
								"Do explosion debris particles bounce off walls?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"maxObjectWallBounceSpeed",
								"maxObjectBounceSpeed",
								"40",
								&maxObjectWallBounceSpeed,
								valReal,
								Move_init,
								"The maximum allowed speed for objects to bounce off walls.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"maxShieldedWallBounceSpeed",
								"maxShieldedBounceSpeed",
								"50",
								&maxShieldedWallBounceSpeed,
								valReal,
								Move_init,
								"The maximum allowed speed for a shielded player to bounce off walls.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"maxUnshieldedWallBounceSpeed",
								"maxUnshieldedBounceSpeed",
								"20",
								&maxUnshieldedWallBounceSpeed,
								valReal,
								Move_init,
								"Maximum allowed speed for an unshielded player to bounce off walls.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"playerWallBounceBrakeFactor",
								"playerWallBrake",
								"0.89",
								&playerWallBrakeFactor,
								valReal,
								Move_init,
								"Factor to slow down players when they hit the wall (between 0 and 1).\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"objectWallBounceBrakeFactor",
								"objectWallBrake",
								"0.95",
								&objectWallBrakeFactor,
								valReal,
								Move_init,
								"Factor to slow down objects when they hit the wall (between 0 and 1).\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"objectWallBounceLifeFactor",
								"objectWallBounceLifeFactor",
								"0.80",
								&objectWallBounceLifeFactor,
								valReal,
								Move_init,
								"Factor to reduce the life of objects after bouncing (between 0 and 1).\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"reportToMetaServer",
								"reportMeta",
								"yes",
								&reportToMetaServer,
								valBool,
								tuner_none,
								"Keep the meta server informed about our game?\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"searchDomainForXPilot",
								"searchDomainForXPilot",
								"no",
								&searchDomainForXPilot,
								valBool,
								tuner_none,
								"Search the local domain for the existence of xpilot.domain?\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"denyHosts",
								"denyHosts",
								"",
								&denyHosts,
								valString,
								Set_deny_hosts,
								"List of network addresses of computers which are denied service.\n"
									"Each address may optionally be followed by a slash and a network mask.\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"limitedLives",
								"limitedLives",
								"no",
								&limitedLives,
								valBool,
								tuner_none,
								"Should players have limited lives?\n"
									"See also worldLives.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"worldLives",
								"lives",
								"0",
								&worldLives,
								valInt,
								tuner_worldlives,
								"Number of lives each player has (no sense without limitedLives).\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"reset",
								"reset",
								"yes",
								&endOfRoundReset,
								valBool,
								tuner_dummy,
								"Does the world reset when the end of round is reached?\n"
									"When true all mines, missiles, shots and explosions will be\n"
									"removed from the world and all players including the winner(s)\n"
									"will be transported back to their homebases.\n"
									"This option is only effective when limitedLives is turned on.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"resetOnHuman",
								"humanReset",
								"0",
								&resetOnHuman,
								valInt,
								tuner_dummy,
								"Normally, new players have to wait until a round is finished\n"
									"before they can start playing. With this option, the first N\n"
									"human players to enter will cause the round to be restarted.\n"
									"In other words, if this option is set to 0, nothing special\n"
									"happens and you have to wait as usual until the round ends (if\n"
									"there are rounds at all, otherwise this option doesn't do\n"
									"anything). If it is set to 1, the round is ended when the first\n"
									"human player enters. This is useful if the robots have already\n"
									"started a round and you don't want to wait for them to finish.\n"
									"If it is set to 2, this also happens for the second human player.\n"
									"This is useful when you got bored waiting for another player to\n"
									"show up and have started playing against the robots. When someone\n"
									"finally joins you, they won't have to wait for you to finish the\n"
									"round before they can play too. For higher numbers it works the\n"
									"same. So this option gives the last human player for whom the\n"
									"round is restarted. Anyone who enters after that does have to\n"
									"wait until the round is over.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"teamPlay",
								"teams",
								"no",
								&teamPlay,
								valBool,
								tuner_none,
								"Is the map a team play map?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"teamFuel",
								"teamFuel",
								"no",
								&teamFuel,
								valBool,
								tuner_dummy,
								"Are fuelstations only available to team members?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"keepShots",
								"keepShots",
								"no",
								&keepShots,
								valBool,
								tuner_dummy,
								"Do shots, mines and missiles remain after their owner leaves?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"teamAssign",
								"teamAssign",
								"yes",
								&teamAssign,
								valBool,
								tuner_dummy,
								"If players have not specified which team they like to join\n"
									"should the server choose a team for them automatically?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"teamImmunity",
								"teamImmunity",
								"yes",
								&teamImmunity,
								valBool,
								tuner_dummy,
								"Should other team members be immune to various shots thrust etc.?\n"
									"This works for alliances too.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"treasureCollisionDestroys",
								"treasureCollisionDestroy",
								"yes",
								&treasureCollisionDestroys,
								valBool,
								tuner_dummy,
								"Are balls destroyed when a player touches it?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"ballConnectorSpringConstant",
								"ballConnectorSpringConstant",
								"1500.0",
								&ballConnectorSpringConstant,
								valReal,
								tuner_dummy,
								"What is the spring constant for connectors?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"ballConnectorDamping",
								"ballConnectorDamping",
								"2.0",
								&ballConnectorDamping,
								valReal,
								tuner_dummy,
								"What is the damping force on connectors?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"maxBallConnectorRatio",
								"maxBallConnectorRatio",
								"0.32",
								&maxBallConnectorRatio,
								valReal,
								tuner_dummy,
								"How much longer or shorter can a connecter get before it breaks?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"ballConnectorLength",
								"ballConnectorLength",
								"120",
								&ballConnectorLength,
								valReal,
								tuner_dummy,
								"How long is a normal connector string?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"treasureCollisionMayKill",
								"treasureUnshieldedCollisionKills",
								"no",
								&treasureCollisionMayKill,
								valBool,
								tuner_dummy,
								"Does a ball kill a player when the player touches it unshielded?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"wreckageCollisionMayKill",
								"wreckageUnshieldedCollisionKills",
								"no",
								&wreckageCollisionMayKill,
								valBool,
								tuner_dummy,
								"Can ships be destroyed when hit by wreckage?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"extraBorder",
								"extraBorder",
								"no",
								&extraBorder,
								valBool,
								tuner_none,
								"Give map an extra border of solid rock.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"defaultsFileName",
								"defaults",
								NULL,
								&defaultsFileName,
								valString,
								tuner_none,
								"The filename of the defaults file to read on startup.\n",
								OPT_COMMAND, },
						{
								"passwordFileName",
								"passwordFileName",
								NULL,
								&passwordFileName,
								valString,
								tuner_none,
								"The filename of the password file to read on startup.\n",
								OPT_COMMAND
										| OPT_DEFAULTS, },
						{
								"motdFileName",
								"motd",
								NULL,
								&motdFileName,
								valString,
								tuner_none,
								"The filename of the MOTD file to show to clients when they join.\n",
								OPT_COMMAND
										| OPT_DEFAULTS, },
						{
								"scoreTableFileName",
								"scoretable",
								NULL,
								&scoreTableFileName,
								valString,
								tuner_none,
								"The filename for the score table to be dumped to.\n"
									"This is a placeholder option which doesn't do anything.\n",
								OPT_COMMAND
										| OPT_DEFAULTS },
						{
								"adminMessageFileName",
								"adminMessage",
								conf_logfile_string,
								&adminMessageFileName,
								valString,
								tuner_none,
								"The name of the file where player messages for the\n"
									"server administrator will be saved.  For the messages\n"
									"to be saved the file must already exist.  Players can\n"
									"send these messages by writing to god.",
								OPT_COMMAND
										| OPT_DEFAULTS },
						{
								"adminMessageFileSizeLimit",
								"adminMessageLimit",
								"20202",
								&adminMessageFileSizeLimit,
								valInt,
								tuner_none,
								"The maximum size in bytes of the admin message file.\n",
								OPT_COMMAND
										| OPT_DEFAULTS },
#if 0
					{
						"intGameSpeed",
						"intGameSpeed",
						"12",
						&intGameSpeed,
						valInt,
						tuner_dummy,
						"The number of timings per second the server uses for timing purposes.\n",
						OPT_ORIGIN_ANY | OPT_VISIBLE
					},
					{
						"gameSpeed",
						"gameSpeed",
						"12.0",
						&gameSpeed,
						valReal,
						tuner_gamespeed,
						"The number of ticks per second for timing purposes.\n",
						OPT_ORIGIN_ANY | OPT_VISIBLE
					},
#endif
						{
								"allowShipShapes",
								"ShipShapes",
								"True",
								&allowShipShapes,
								valBool,
								tuner_none,
								"Are players allowed to define their own ship shape?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"playersOnRadar",
								"playersRadar",
								"True",
								&playersOnRadar,
								valBool,
								tuner_dummy,
								"Are players visible on the radar.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"initialFuel",
								"initialFuel",
								"800",
								&World.items[ITEM_FUEL].initial,
								valInt,
								Set_initial_resources,
								"How much fuel players start with, or the minimum after being killed.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"initialAfterburners",
								"initialAfterburners",
								"0",
								&World.items[ITEM_AFTERBURNER].initial,
								valInt,
								Set_initial_resources,
								"How many afterburners players start with.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"lockOtherTeam",
								"lockOtherTeam",
								"true",
								&lockOtherTeam,
								valBool,
								tuner_dummy,
								"Can you lock on players from other teams when you're dead.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"useWreckage",
								"useWreckage",
								"false",
								&useWreckage,
								valBool,
								tuner_dummy,
								"Do destroyed ships leave wreckage?\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"roundsToPlay",
								"roundsToPlay",
								"0",
								&roundsToPlay,
								valInt,
								tuner_dummy,
								"The number of rounds to play.  Unlimited if 0.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"maxVisibleObject",
								"maxVisibleObjects",
								"1000",
								&maxVisibleObject,
								valInt,
								tuner_dummy,
								"What is the maximum number of objects a player can see.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"pLockServer",
								"pLockServer",
#ifdef PLOCKSERVER
								"true",
#else
								"false",
#endif
								&pLockServer,
								valBool,
								tuner_plock,
								"Whether the server is prevented from being swapped out of memory.\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"password",
								"password",
								NULL,
								&password,
								valString,
								tuner_dummy,
								"The password needed to obtain operator privileges.\n"
									"If specified on the command line, on many systems other\n"
									"users will be able to see the password.  Therefore, using\n"
									"the password file instead is recommended.",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_PASSWORD },
						{
								"clientPortStart",
								"clientPortStart",
								"0",
								&clientPortStart,
								valInt,
								tuner_dummy,
								"Use UDP ports clientPortStart - clientPortEnd (for firewalls)\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
						{
								"clientPortEnd",
								"clientPortEnd",
								"0",
								&clientPortEnd,
								valInt,
								tuner_dummy,
								"Use UDP ports clientPortStart - clientPortEnd (for firewalls)\n",
								OPT_COMMAND
										| OPT_DEFAULTS
										| OPT_VISIBLE },
#if 0
					{
						"maxPauseTime",
						"maxPauseTime",
						"3600", /* can pause 1 hour by default */
						&maxPauseTime,
						valSec,
						tuner_dummy,
						"The maximum time a player can stay paused for, in seconds.\n"
						"After being paused this long, the player will be kicked off.\n"
						"Setting this option to 0 disables the feature.\n",
						OPT_ORIGIN_ANY | OPT_VISIBLE
					},
#endif
						{
								"frameDivisor",
								"frameDivisor",
								"4",
								&frameDivisor,
								valInt,
								tuner_framedivisor,
								"Set frameDivisor so that fps/frameDivisor is close to 12.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },

						{
								"fps",
								"fps",
								"50",
								&fps,
								valInt,
								tuner_fps,
								"The number of frames the server handles.\n",
								OPT_ORIGIN_ANY
										| OPT_VISIBLE },
						{
								"mainLoopTime",
								"mainLoopTime",
								"0",
								&mainLoopTime,
								valReal,
								tuner_none,
								"Duration of last Main_loop() function call (in milliseconds).\n"
									"This option is read only.\n",
								OPT_COMMAND
										| OPT_VISIBLE },
						{
								"rankFileName",
								"rankFileName",
								NULL,
								&rankFileName,
								valString,
								tuner_none,
								"The filename for the XML file to hold server ranking data.\n"
									"To reset the ranking, delete this file.\n",
								OPT_COMMAND
										| OPT_DEFAULTS },
						{
								"rankWebpageFileName",
								"rankWebpage",
								NULL,
								&rankWebpageFileName,
								valString,
								tuner_none,
								"The filename for the webpage with the server ranking list.\n",
								OPT_COMMAND
										| OPT_DEFAULTS },
						{
								"rankWebpageCSS",
								"rankCSS",
								NULL,
								&rankWebpageCSS,
								valString,
								tuner_none,
								"The URL of an optional style sheet for the ranking webpage.\n",
								OPT_COMMAND
										| OPT_DEFAULTS },
			};

static bool options_inited = false;

option_desc* Get_option_descs(int32_t *count_ptr)
{
	if (options_inited != true) {
		dumpcore("options not initialized");
	}

	*count_ptr = NELEM(options);
	return &options[0];
}

static void Init_default_options(void)
{
	option_desc* desc;

	if ((desc = Find_option_by_name("mapFileName")) == NULL) {
		dumpcore("Could not find map file option");
	}
	desc->defaultValue = Conf_default_map();

	if ((desc = Find_option_by_name("motdFileName")) == NULL) {
		dumpcore("Could not find motd file option");
	}
	desc->defaultValue = Conf_servermotdfile();

	if ((desc = Find_option_by_name("robotFile")) == NULL) {
		dumpcore("Could not find robot file option");
	}
	desc->defaultValue = Conf_robotfile();

	if ((desc = Find_option_by_name("defaultsFileName")) == NULL) {
		dumpcore("Could not find defaults file option");
	}
	desc->defaultValue = Conf_defaults_file_name();

	if ((desc = Find_option_by_name("passwordFileName")) == NULL) {
		dumpcore("Could not find password file option");
	}
	desc->defaultValue = Conf_password_file_name();
}

bool Init_options(void)
{
	int32_t i;
	int32_t option_count = NELEM(options);

	if (options_inited != false) {
		dumpcore("Can't init options twice.");
	}

	Init_default_options();

	for (i = 0; i < option_count; i++) {
		if (Option_add_desc(&options[i]) == false) {
			return false;
		}
	}

	options_inited = true;

	return true;
}

void Free_options(void)
{
	int32_t i;
	int32_t option_count = NELEM(options);

	if (options_inited == true) {
		options_inited = false;
		for (i = 0; i < option_count; i++) {
			if (options[i].type == valString) {
				char **str_ptr = (char **) options[i].variable;
				char *str = *str_ptr;
				if (str != NULL && str
						!= options[i].defaultValue) {
					free(str);
					*str_ptr = NULL;
				}
			}
		}
	}
}

option_desc* Find_option_by_name(const char* name)
{
	int32_t j;
	int32_t option_count = NELEM(options);

	for (j = 0; j < option_count; j++) {
		if (!strcasecmp(options[j].commandLineOption, name)
				|| !strcasecmp(options[j].name, name)) {
			return (&options[j]);
		}
	}
	return NULL;
}

