/* $Id: xpconfig.c,v 1.5 2008/08/16 21:07:33 rotunda_pk Exp $
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

int8_t xpconfig_version[] = VERSION;


int8_t *Conf_contactaddress(void);
int8_t *Conf_datadir(void);
int8_t *Conf_defaults_file_name(void);
int8_t *Conf_password_file_name(void);
int8_t *Conf_mapdir(void);
int8_t *Conf_default_map(void);
int8_t *Conf_servermotdfile(void);
int8_t *Conf_localmotdfile(void);
int8_t *Conf_logfile(void);
int8_t *Conf_ship_file(void);
int8_t *Conf_localguru(void);
int8_t *Conf_robotfile(void);
int8_t *Conf_zcat_ext(void);
int8_t *Conf_zcat_format(void);


int8_t *Conf_datadir(void)
{
	static int8_t conf[] = CONF_DATADIR;

	return conf;
}

int8_t *Conf_defaults_file_name(void)
{
	static int8_t conf[] = DEFAULTS_FILE_NAME;

	return conf;
}

int8_t *Conf_password_file_name(void)
{
	static int8_t conf[] = PASSWORD_FILE_NAME;

	return conf;
}

int8_t *Conf_mapdir(void)
{
	static int8_t conf[] = MAPDIR;

	return conf;
}

static int8_t conf_default_map_string[] = DEFAULT_MAP;

int8_t *Conf_default_map(void)
{
	return conf_default_map_string;
}

int8_t *Conf_servermotdfile(void)
{
	static int8_t conf[] = SERVERMOTDFILE;
	static int8_t env[] = "XPILOTSERVERMOTD";
	int8_t *filename;

	filename = getenv(env);
	if (filename == NULL) {
		filename = conf;
	}

	return filename;
}

int8_t *Conf_localmotdfile(void)
{
	static int8_t conf[] = LOCALMOTDFILE;

	return conf;
}

int8_t conf_logfile_string[] = LOGFILE;

int8_t *Conf_logfile(void)
{
	return conf_logfile_string;
}

/* needed by client/default.c */
int8_t conf_ship_file_string[] = SHIP_FILE;

int8_t *Conf_ship_file(void)
{
	return conf_ship_file_string;
}

int8_t *Conf_localguru(void)
{
	static int8_t conf[] = LOCALGURU;

	return conf;
}

int8_t *Conf_contactaddress(void)
{
	static int8_t conf[] = CONTACTADDRESS;

	return conf;
}

static int8_t conf_robotfile_string[] = ROBOTFILE;

int8_t *Conf_robotfile(void)
{
	return conf_robotfile_string;
}

int8_t *Conf_zcat_ext(void)
{
	static int8_t conf[] = ZCAT_EXT;

	return conf;
}

int8_t *Conf_zcat_format(void)
{
	static int8_t conf[] = ZCAT_FORMAT;

	return conf;
}
