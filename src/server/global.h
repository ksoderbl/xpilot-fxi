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

#ifndef	GLOBAL_H
#define	GLOBAL_H

#include "serverconst.h"
#include "list.h"
#include "structs.h"
//#include "map.h"

#ifndef MAX
#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#endif

#define	STR80	(80)

typedef struct {
	char owner[STR80];
	char host[STR80];
} server_t;

/*
 * Global data.
 */

/* from map.c */
extern World_map World;

/* from player.c */
extern player_t **Players;
extern int32_t NumPlayers;
extern int32_t NumPaused;
extern int32_t GetInd[NUM_IDS + 1];

/* from object.c */
extern object_t *Obj[];
extern int32_t NumObjs;

/* from player.c */
extern bool updateScores;


extern int32_t NumQueuedPlayers;
extern int32_t frame_loops;
extern int32_t NumRobots;
extern int32_t maxRobots;
extern int32_t minRobots;
extern int32_t login_in_progress;

extern char *robotFile;
extern int32_t robotsTalk;
extern int32_t robotsLeave;
extern int32_t robotLeaveLife;
extern int32_t robotLeaveScore;
extern int32_t robotLeaveRatio;
extern int32_t robotTeam;
extern bool restrictRobots;
extern bool reserveRobotTeam;
extern server_t Server;
extern TList expandList;
extern DFLOAT ShotsMass, ShipMass, ShotsSpeed;
extern DFLOAT ballMass;
extern int32_t ShotsMax, ShotsLife;
extern int32_t fireRepeatRate;
extern int32_t HAS_DEFAULT, USES_DEFAULT, USED_KILL;
extern bool RawMode;
extern bool logRobots;
extern int32_t main_loops;
extern int32_t main_loops_slow;
extern int32_t NumOperators;
extern char *mapFileName;
extern int32_t mapRule;
extern char *mapData;
extern int32_t mapWidth;
extern int32_t mapHeight;
extern char *mapName;
extern char *mapAuthor;
extern int32_t contactPort;
extern char *serverHost;
extern char *greeting;
extern char *serverAddr;
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
extern bool extraBorder;

//extern bool updateScores;
extern bool allowShipShapes;

extern bool reportToMetaServer;
extern bool searchDomainForXPilot;
extern char *denyHosts;
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

extern char *motdFileName;
extern char *scoreTableFileName;
extern char *adminMessageFileName;
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
extern char *password;

extern char *robotRealName;
extern char *robotHostName;

extern bool selfImmunity;

extern char *defaultShipShape;

extern int32_t clientPortStart;
extern int32_t clientPortEnd;

extern int32_t maxPauseTime;
extern float mainLoopTime;

extern int32_t KILLING_SHOTS;
extern uint32_t SPACE_BLOCKS;

extern char *rankFileName;
extern char *rankWebpageFileName;
extern char *rankWebpageCSS;

/* from server.c */
extern int32_t frameDivisor;
extern int32_t fps;
extern int32_t intGameSpeed;
extern float ticksPerFrame;
extern double realTimeStep;
extern double gameSpeed;
extern int32_t frame_cycle;

#endif /* GLOBAL_H */
