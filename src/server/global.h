/* $Id: global.h,v 1.3 2007/02/09 23:23:35 pgma Exp $
 *
 * XPilot, a multiplayer gravity war game.  Copyright (C) 1991-2001 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
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

#ifndef OBJECT_H
/* need player */
#include "object.h"
#endif

#ifndef MAP_H
/* need World_map */
#include "map.h"
#endif

#ifndef LIST_H_INCLUDED
/* need list_t */
#include "list.h"
#endif

#ifndef MAX
#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#endif

#define	STR80	(80)

typedef struct {
    char	owner[STR80];
    char	host[STR80];
} server_t;

/*
 * Global data.
 */
#define FPS		gameSpeed


/*#define NumObjs		(ObjCount + 0)*/


extern player		**Players;
extern object		*Obj[];
extern long		frame_loops;
extern int		NumPlayers;
extern int		NumPseudoPlayers;
extern int		NumQueuedPlayers;
extern int		NumObjs;
extern int		ObjCount;
extern int		NumRobots, maxRobots, minRobots;
extern int		login_in_progress;

extern char		*robotFile;
extern int		robotsTalk, robotsLeave, robotLeaveLife;
extern int		robotLeaveScore, robotLeaveRatio;
extern int		robotTeam;
extern bool		restrictRobots, reserveRobotTeam;
extern World_map	World;
extern server_t		Server;
extern list_t		expandList;
extern DFLOAT		ShotsMass, ShipMass, ShotsSpeed;
extern DFLOAT		ballMass;
extern int		ShotsMax, ShotsLife;
extern int		fireRepeatRate;
extern long		DEF_BITS, KILL_BITS, DEF_HAVE, DEF_USED, USED_KILL;
extern int		GetInd[];
extern bool		RawMode;
extern bool		NoQuit;
extern bool		logRobots;
extern long		main_loops;
extern long		main_loops_slow;
extern int		NumOperators;
extern char		*mapFileName;
extern int		mapRule;
extern char		*mapData;
extern int		mapWidth;
extern int		mapHeight;
extern char		*mapName;
extern char		*mapAuthor;
extern int 		contactPort;
extern char		*serverHost;
extern char		*greeting;
extern char		*serverAddr;
extern bool		crashWithPlayer;
extern bool		bounceWithPlayer;
extern bool		playerKillings;
extern bool		playerShielding;
extern bool		playerStartsShielded;
extern bool		shotsWallBounce;
extern bool		ballsWallBounce;
extern bool		ballCollisions;
extern bool		ballSparkCollisions;
extern bool		sparksWallBounce;
extern bool		debrisWallBounce;
extern bool		cloakedExhaust;
extern bool		cloakedShield;
extern DFLOAT		maxObjectWallBounceSpeed;
extern DFLOAT		maxShieldedWallBounceSpeed;
extern DFLOAT		maxUnshieldedWallBounceSpeed;
extern DFLOAT		maxShieldedWallBounceAngle;
extern DFLOAT		maxUnshieldedWallBounceAngle;
extern DFLOAT		playerWallBrakeFactor;
extern DFLOAT		objectWallBrakeFactor;
extern DFLOAT		objectWallBounceLifeFactor;

extern bool		limitedLives;
extern int		worldLives;
extern bool		endOfRoundReset;
extern int		resetOnHuman;
extern bool		teamPlay;
extern bool		teamFuel;
extern bool		keepShots;
extern bool		teamAssign;
extern bool		teamImmunity;
extern bool		teamShareScore;
extern bool		timing;
extern bool		edgeWrap;
extern bool		edgeBounce;
extern bool		extraBorder;

extern DFLOAT		shotKillScoreMult;
extern DFLOAT		tankKillScoreMult;
extern DFLOAT		runoverKillScoreMult;
extern DFLOAT		ballKillScoreMult;
extern DFLOAT		explosionKillScoreMult;
extern DFLOAT		shoveKillScoreMult;
extern DFLOAT		crashScoreMult;
extern DFLOAT		selfKillScoreMult;
extern DFLOAT		selfDestructScoreMult;
extern DFLOAT		unownedKillScoreMult;

extern bool		updateScores;
extern bool		allowShipShapes;

extern bool		reportToMetaServer;
extern bool		searchDomainForXPilot;
extern char		*denyHosts;
extern int		maxClientsPerIP;

extern bool		playersOnRadar;
extern bool		treasureKillTeam;

extern bool		treasureCollisionDestroys;
extern bool		treasureCollisionMayKill;
extern bool		wreckageCollisionMayKill;

extern DFLOAT		ballConnectorSpringConstant;
extern DFLOAT		ballConnectorDamping;
extern DFLOAT		maxBallConnectorRatio;
extern DFLOAT		ballConnectorLength;
extern bool		connectorIsString;

extern bool		allowViewing;
extern int		game_lock;

extern char		*motdFileName;
extern char	       	*scoreTableFileName;
extern char		*adminMessageFileName;
extern int		adminMessageFileSizeLimit;

extern bool		lockOtherTeam;

extern int		maxVisibleObject;
extern bool		pLockServer;

extern int		roundDelaySeconds;
extern int		round_delay;
extern int		round_delay_send;

extern int		roundsToPlay;
extern int		roundsPlayed;

extern bool		useWreckage;
extern char		*password;

extern char		*robotRealName;
extern char		*robotHostName;

extern bool		selfImmunity;

extern char		*defaultShipShape;
extern char		*tankShipShape;

extern int		clientPortStart;
extern int		clientPortEnd;

extern int		maxPauseTime;
extern DFLOAT		mainLoopTime;

extern long		KILLING_SHOTS;
extern unsigned		SPACE_BLOCKS;


/* new as of xph */ 

extern int 	frameDivisor;
extern int	fps;
extern int	gameSpeed;
extern char	*rankFileName;
extern char	*rankWebpageFileName;

#endif /* GLOBAL_H */
