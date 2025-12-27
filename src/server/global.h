/* $Id: global.h,v 1.12 2008/08/26 20:51:06 rotunda_pk Exp $
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

#ifndef	GLOBAL_H
#define	GLOBAL_H

#include "object.h"
#include "map.h"
#include "list.h"


#ifndef MAX
#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#endif

// TODO: Temporary solution for checking whether player's structure
// is within the allowed range
//#define PL_STRUCT_OUT_OF_BOUNDS(pl)		(pl < Players[0] || pl >= Players[NumPlayers]) ? true : false

#define	STR80	(80)

typedef struct {
	int8_t owner[STR80];
	int8_t host[STR80];
} server_t;

/*
 * Global data.
 */

/*#define NumObjs		(ObjCount + 0)*/

extern player_t **Players;
extern object_t *Obj[];
extern int32_t frame_loops;
extern int32_t NumPlayers;
extern int32_t NumPseudoPlayers;
extern int32_t NumQueuedPlayers;
extern int32_t NumObjs;
extern int32_t ObjCount;
extern int32_t NumRobots, maxRobots, minRobots;
extern int32_t login_in_progress;

extern int8_t *robotFile;
extern int32_t robotsTalk, robotsLeave, robotLeaveLife;
extern int32_t robotLeaveScore, robotLeaveRatio;
extern int32_t robotTeam;
extern bool restrictRobots, reserveRobotTeam;
extern World_map World;
extern server_t Server;
extern list_t expandList;
extern DFLOAT ShotsMass, ShipMass, ShotsSpeed;
extern DFLOAT ballMass;
extern int32_t ShotsMax, ShotsLife;
extern int32_t fireRepeatRate;
extern int32_t DEF_BITS, KILL_BITS, DEF_HAVE, DEF_USED, USED_KILL;
extern int32_t GetInd[];
extern bool RawMode;
extern bool logRobots;
extern int32_t main_loops;
extern int32_t main_loops_slow;
extern int32_t NumOperators;
extern int8_t *mapFileName;
extern int32_t mapRule;
extern int8_t *mapData;
extern int32_t mapWidth;
extern int32_t mapHeight;
extern int8_t *mapName;
extern int8_t *mapAuthor;
extern int32_t contactPort;
extern int8_t *serverHost;
extern int8_t *greeting;
extern int8_t *serverAddr;
extern bool crashWithPlayer;
extern bool bounceWithPlayer;
extern bool playerKillings;
extern bool playerShielding;
extern bool playerStartsShielded;
extern bool shotsWallBounce;
extern bool ballCollisions;
extern bool sparksWallBounce;
extern bool debrisWallBounce;
extern bool cloakedExhaust;
extern bool cloakedShield;
extern DFLOAT maxObjectWallBounceSpeed;
extern DFLOAT maxShieldedWallBounceSpeed;
extern DFLOAT maxUnshieldedWallBounceSpeed;
extern DFLOAT playerWallBrakeFactor;
extern DFLOAT objectWallBrakeFactor;
extern DFLOAT objectWallBounceLifeFactor;

extern bool limitedLives;
extern int32_t worldLives;
extern bool endOfRoundReset;
extern int32_t resetOnHuman;
extern bool teamPlay;
extern bool teamFuel;
extern bool keepShots;
extern bool teamAssign;
extern bool teamImmunity;
extern bool teamShareScore;
extern bool edgeWrap;
extern bool edgeBounce;
extern bool extraBorder;

extern bool updateScores;
extern bool allowShipShapes;

extern bool reportToMetaServer;
extern bool searchDomainForXPilot;
extern int8_t *denyHosts;
extern int32_t maxClientsPerIP;

extern bool playersOnRadar;

extern bool treasureCollisionDestroys;
extern bool treasureCollisionMayKill;
extern bool wreckageCollisionMayKill;

extern DFLOAT ballConnectorSpringConstant;
extern DFLOAT ballConnectorDamping;
extern DFLOAT maxBallConnectorRatio;
extern DFLOAT ballConnectorLength;
extern bool connectorIsString;

extern int32_t game_lock;

extern int8_t *motdFileName;
extern int8_t *scoreTableFileName;
extern int8_t *adminMessageFileName;
extern int32_t adminMessageFileSizeLimit;

extern bool lockOtherTeam;

extern int32_t maxVisibleObject;
extern bool pLockServer;

extern int32_t roundDelaySeconds;
extern int32_t round_delay;
extern int32_t round_delay_send;

extern int32_t roundsToPlay;
extern int32_t roundsPlayed;

extern bool useWreckage;
extern int8_t *password;

extern int8_t *robotRealName;
extern int8_t *robotHostName;

extern bool selfImmunity;

extern int8_t *defaultShipShape;
extern int8_t *tankShipShape;

extern int32_t clientPortStart;
extern int32_t clientPortEnd;

extern int32_t maxPauseTime;
extern DFLOAT mainLoopTime;

extern int32_t KILLING_SHOTS;
extern uint32_t SPACE_BLOCKS;

/* new as of xph */

extern int32_t frameDivisor;
extern int32_t fps;
extern int32_t intGameSpeed;
extern float ticksPerFrame;
extern DFLOAT gameSpeed;
extern int8_t *rankFileName;
extern int8_t *rankWebpageFileName;

#endif /* GLOBAL_H */
