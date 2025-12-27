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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "version.h"
#include "config.h"

/*
 * Configure these, that's what they're here for.
 * Explanation about all these compile time configuration options
 * is in the Makefile.std and in the Imakefile.
 */
#ifndef LOCALGURU
#    define LOCALGURU		"xpilot@xpilot.org"
#endif

#ifndef	DEFAULT_MAP
#    define DEFAULT_MAP		"teamcup.xp"
#endif

#ifndef CONF_DATADIR
#    error "Please define CONF_DATADIR!!!"
#endif

#ifndef DEFAULTS_FILE_NAME
#    define DEFAULTS_FILE_NAME	CONF_DATADIR "defaults.txt"
#endif
#ifndef PASSWORD_FILE_NAME
#    define PASSWORD_FILE_NAME	CONF_DATADIR "password.txt"
#endif
#ifndef ROBOTFILE
#    define ROBOTFILE		CONF_DATADIR "robots.txt"
#endif
#ifndef SERVERMOTDFILE
#    define	SERVERMOTDFILE	CONF_DATADIR "servermotd.txt"
#endif
#ifndef LOCALMOTDFILE
#    define	LOCALMOTDFILE	CONF_DATADIR "localmotd.txt"
#endif
#ifndef LOGFILE
#    define	LOGFILE		CONF_DATADIR "log.txt"
#endif
#ifndef MAPDIR
#    define MAPDIR		CONF_DATADIR "maps/"
#endif
#ifndef SHIP_FILE
#    define SHIP_FILE		CONF_DATADIR "shipshapes.txt"
#endif

#ifndef ZCAT_EXT
#    define ZCAT_EXT	".gz"
#endif

#ifndef ZCAT_FORMAT
#    define ZCAT_FORMAT "gzip -d -c < %s"
#endif

/*
 * Please don't change this one.
 */
#ifndef CONTACTADDRESS
#    define CONTACTADDRESS	"xpilot@xpilot.org"
#endif

char xpconfig_version[] = VERSION;


char *Conf_contactaddress(void);
char *Conf_datadir(void);
char *Conf_defaults_file_name(void);
char *Conf_password_file_name(void);
char *Conf_mapdir(void);
char *Conf_default_map(void);
char *Conf_servermotdfile(void);
char *Conf_localmotdfile(void);
char *Conf_logfile(void);
char *Conf_ship_file(void);
char *Conf_localguru(void);
char *Conf_robotfile(void);
char *Conf_zcat_ext(void);
char *Conf_zcat_format(void);


char *Conf_datadir(void)
{
	static char conf[] = CONF_DATADIR;

	return conf;
}

char *Conf_defaults_file_name(void)
{
	static char conf[] = DEFAULTS_FILE_NAME;

	return conf;
}

char *Conf_password_file_name(void)
{
	static char conf[] = PASSWORD_FILE_NAME;

	return conf;
}

char *Conf_mapdir(void)
{
	static char conf[] = MAPDIR;

	return conf;
}

static char conf_default_map_string[] = DEFAULT_MAP;

char *Conf_default_map(void)
{
	return conf_default_map_string;
}

char *Conf_servermotdfile(void)
{
	static char conf[] = SERVERMOTDFILE;
	static char env[] = "XPILOTSERVERMOTD";
	char *filename;

	filename = getenv(env);
	if (filename == NULL) {
		filename = conf;
	}

	return filename;
}

char *Conf_localmotdfile(void)
{
	static char conf[] = LOCALMOTDFILE;

	return conf;
}

char conf_logfile_string[] = LOGFILE;

char *Conf_logfile(void)
{
	return conf_logfile_string;
}

/* needed by client/default.c */
char conf_ship_file_string[] = SHIP_FILE;

char *Conf_ship_file(void)
{
	return conf_ship_file_string;
}

char *Conf_localguru(void)
{
	static char conf[] = LOCALGURU;

	return conf;
}

char *Conf_contactaddress(void)
{
	static char conf[] = CONTACTADDRESS;

	return conf;
}

static char conf_robotfile_string[] = ROBOTFILE;

char *Conf_robotfile(void)
{
	return conf_robotfile_string;
}

char *Conf_zcat_ext(void)
{
	static char conf[] = ZCAT_EXT;

	return conf;
}

char *Conf_zcat_format(void)
{
	static char conf[] = ZCAT_FORMAT;

	return conf;
}
