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
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include <unistd.h>

#include "version.h"
#include "config.h"
#include "serverconst.h"
#include "global.h"
#include "proto.h"
#include "defaults.h"
#include "error.h"
#include "types.h"
#include "commonproto.h"

char option_version[] = VERSION;

/*
 * This module implements an in memory server option database.
 * Each option is made up by its names (one or two) and its
 * value.  The names are stored together with a pointer to
 * their value representation in a hash table.
 */

/* size of the hash table.  must be prime. */
#define	HASH_SIZE	317

/*
 * Define value representation structure which holds:
 *	a pointer to the string representation of an option value.
 *	a flag which is true if the value was set with override.
 *	an enum which represents the origin of the option value:
 *		where origins can be one of {map, defaultsfile, command line}.
 *	a pointer to an option description structure.
 *	a reference count which will be either zero, one or two.
 * This structure is automatically deallocated when its reference count
 * drops to zero.
 * The option description pointer may be NULL for undefined options.
 * The string value pointer is usually dynamically allocated, but
 * in theory (not yet in practice) could also point to a static string
 * if this option refers to a valString with a static default value.
 */
typedef struct _hash_value hash_value;
struct _hash_value {
	char *value;
	int32_t override;
	optOrigin origin;
	option_desc *desc;
	int32_t refcount;
};

/*
 * Define option name structure which holds a pointer
 * to the value structure.  More than one (two) different
 * name structures may point to the same value structure
 * if the same option has two different names.
 */
typedef struct _hash_node hash_node;
struct _hash_node {
	hash_node *next;
	char *name;
	hash_value *value;
};

/*
 * Define memory for hash table along with some statistics.
 */
static int32_t hash_nodes_allocated;
static int32_t hash_nodes_freed;
static int32_t hash_values_allocated;
static int32_t hash_values_freed;
static hash_node* Option_hash_array[HASH_SIZE];

/*
 * Compute a reasonable case-insensitive hash value across a character string.
 */
static int32_t Option_hash_string(const char *name)
{
	uint32_t hashVal = 0;
	const uint8_t *string = (const uint8_t *) name;
	int32_t i;

	for (i = 0; string[i] != '\0'; i++) {
		uint8_t c = string[i];

		if (isascii(c) && isalpha(c) && islower(c)) {
			c = toupper(c);
		}

		hashVal = (((hashVal << 3) + c) ^ i);

		while (hashVal > HASH_SIZE) {
			hashVal = (hashVal % HASH_SIZE) + (hashVal / HASH_SIZE);
		}
	}

	return (int32_t) (hashVal % HASH_SIZE);
}

/*
 * Free a hash value structure if its
 * reference count drops to zero.
 */
static void Option_free_value(hash_value* val)
{
	if (val->refcount > 0) {
		val->refcount--;
	}
	if (val->refcount == 0) {
		if (val->value) {
			if (!val->desc || val->value != val->desc->defaultValue) {
				free(val->value);
			}
			val->value = NULL;
		}
		free(val);
		hash_values_freed++;
	}
}

/*
 * Allocate a new option hash value and fill in its values.
 * The option value string representation is either derived
 * from the value parameter, or else from the option description
 * default value pointer.
 */
static hash_value *Option_allocate_value(const char *value, option_desc *desc,
		optOrigin origin)
{
	hash_value *tmp = (hash_value *) xp_safe_malloc(sizeof(hash_value));

	tmp->desc = desc;
	tmp->override = 0;
	tmp->origin = origin;
	tmp->refcount = 0;
	if (value == NULL) {
		if (desc != NULL && desc->defaultValue != NULL) {
			/* might also simply point to default value instead. */
			tmp->value = xp_safe_strdup(desc->defaultValue);
		}
		else {
			tmp->value = NULL;
		}
	}
	else {
		tmp->value = xp_safe_strdup(value);
	}

	if (tmp) {
		hash_values_allocated++;
	}

	return tmp;
}

/*
 * Free a hash node and its hash value.
 */
static void Option_free_node(hash_node* node)
{
	if (node->value) {
		Option_free_value(node->value);
		node->value = NULL;
	}
	if (node->name) {
		free(node->name);
		node->name = NULL;
	}
	node->next = NULL;
	free(node);
	hash_nodes_freed++;
}

/*
 * Allocate a new node for the hash table and fill in its values.
 */
static hash_node *Option_allocate_node(const char *name, hash_value *value)
{
	hash_node *tmp = (hash_node *) xp_safe_malloc(sizeof(hash_node));

	tmp->next = NULL;
	tmp->value = value;
	tmp->name = xp_safe_strdup(name);
	if (tmp->value) {
		tmp->value->refcount++;
	}

	if (tmp) {
		hash_nodes_allocated++;
	}

	return tmp;
}

/*
 * Add a hash node to the hash table.
 */
static void Option_add_node(hash_node *node)
{
	hash_node *np;
	int32_t ix = Option_hash_string(node->name);

	for (np = Option_hash_array[ix]; np; np = np->next) {
		if (!strcasecmp(node->name, np->name)) {
			fatal("Option_add_node node exists (%s, %s)\n",
					node->name, np->name);
		}
	}

	node->next = Option_hash_array[ix];
	Option_hash_array[ix] = node;
}

/*
 * Return the hash table node of a named option,
 * or NULL if there is no node for that option name.
 */
static hash_node *Get_hash_node_by_name(const char *name)
{
	hash_node *np;
	int32_t ix = Option_hash_string(name);

	for (np = Option_hash_array[ix]; np; np = np->next) {
		if (!strcasecmp(name, np->name)) {
			return np;
		}
	}

	return (hash_node *) NULL;
}

/*
 * Add an option description to the hash table.
 */
bool Option_add_desc(option_desc *desc)
{
	hash_value *val = Option_allocate_value(NULL, desc, OPT_INIT);
	hash_node *node1, *node2;

	if (!val) {
		return false;
	}

	node1 = Option_allocate_node(desc->name, val);
	if (!node1) {
		Option_free_value(val);
		return false;
	}

	node2 = NULL;
	if (strcasecmp(desc->name, desc->commandLineOption)) {
		node2 = Option_allocate_node(desc->commandLineOption, val);
		if (!node2) {
			Option_free_node(node1);
			return false;
		}
	}

	Option_add_node(node1);
	if (node2 != NULL) {
		Option_add_node(node2);
	}

	return true;
}

/*
 * Convert an option origin enumerated constant
 * to a character representation.
 */
static const char* Origin_name(optOrigin opt_origin)
{
	const char *source;

	switch (opt_origin) {
	case OPT_COMMAND:
		source = "command line";
		break;
	case OPT_PASSWORD:
		source = "password file";
		break;
	case OPT_DEFAULTS:
		source = "defaults file";
		break;
	case OPT_MAP:
		source = "map file";
		break;
	default:
		source = "unknown origin";
		break;
	}

	return source;
}

/*
 * Modify the value for a hash node if permissions allow us to do so.
 */
static void Option_change_node(hash_node *node, const char *value,
		int32_t override, optOrigin opt_origin)
{
	bool set_ok = false;

	if (node->value == NULL) {
		/* permit if option has no default value. */
		set_ok = true;
	}
	else {

		/* check option description permissions. */
		if (node->value->desc != NULL) {
			option_desc *desc = node->value->desc;
			if ((desc->flags & opt_origin) == 0) {
				warn(
						"Not allowed to change option '%s' from %s.",
						node->name, Origin_name(
								opt_origin));
				return;
			}
		}

		switch (opt_origin) {
		case OPT_COMMAND:
			/* command line always overrides */
			set_ok = true;
			break;

		case OPT_DEFAULTS:
			switch (node->value->origin) {
			case OPT_COMMAND:
				/* never modify command line arg. */
				break;

			case OPT_DEFAULTS:
				/* can't change if previous value has override. */
				if (!node->value->override) {
					set_ok = true;
				}
				break;

			case OPT_MAP:
				/* defaults file override wins over map. */
				if (override) {
					set_ok = true;
				}
				break;

			case OPT_PASSWORD:
				/* never modify if set by password file. */
				break;

			case OPT_INIT:
				set_ok = true;
				break;

			default:
				fatal("unknown node->value origin in set value");
			}
			break;

		case OPT_MAP:
			switch (node->value->origin) {
			case OPT_COMMAND:
				/* never modify command line arg. */
				break;

			case OPT_DEFAULTS:
				/* can't change if defaults value has override. */
				if (!node->value->override) {
					set_ok = true;
				}
				break;

			case OPT_MAP:
				/* can't change if previous value has override. */
				if (!node->value->override) {
					set_ok = true;
				}
				break;

			case OPT_PASSWORD:
				/* never modify if set by password file. */
				break;

			case OPT_INIT:
				set_ok = true;
				break;

			default:
				fatal("unknown node->value origin in set value");
			}
			break;

		case OPT_PASSWORD:
			switch (node->value->origin) {
			case OPT_COMMAND:
				/* never modify command line arg. */
				break;

			case OPT_DEFAULTS:
				/* password file always wins over defaults. */
				set_ok = true;
				break;

			case OPT_MAP:
				/* password file always wins over map. */
				set_ok = true;
				break;

			case OPT_PASSWORD:
				/* can't change if previous value has override. */
				if (!node->value->override) {
					set_ok = true;
				}
				break;

			case OPT_INIT:
				set_ok = true;
				break;

			default:
				fatal("unknown node->value origin in set value");
			}
			break;

		default:
			fatal("unknown opt_origin in set value");
		}
	}

	if (set_ok == true) {
		if (node->value == NULL) {
			node->value = Option_allocate_value(value, NULL,
					opt_origin);
			if (node->value == NULL) {
				fatal("Not enough memory.");
			}
			else {
				node->value->refcount++;
			}
		}
		else {
			if (node->value->value != NULL) {
				option_desc *desc = node->value->desc;
				if (!desc || node->value->value
						!= desc->defaultValue) {
					free(node->value->value);
				}
			}
			if (value == NULL) {
				node->value->value = NULL;
			}
			else {
				node->value->value = xp_safe_strdup(value);
			}
		}
		node->value->override = override;
		node->value->origin = opt_origin;
	}
#if DEVELOPMENT
	else {
		const char *old_value_origin_name = Origin_name(node->value->origin);
		const char *new_value_origin_name = Origin_name(opt_origin);
		warn("Not modifying %s option '%s' from %s\n",
				old_value_origin_name,
				node->name,
				new_value_origin_name);
	}
#endif
}

/*
 * Scan through the hash table of option name-value pairs looking for
 * an option with the specified name; if found call Option_change_node
 * to set option to the new value if permissions allow us to do so.
 */
void Option_set_value(const char *name, const char *value, int32_t override,
		optOrigin opt_origin)
{
	hash_node *np;
	hash_value *vp;
	int32_t ix = Option_hash_string(name);

	for (np = Option_hash_array[ix]; np; np = np->next) {
		if (!strcasecmp(name, np->name)) {
			Option_change_node(np, value, override, opt_origin);
			return;
		}
	}

	if (!value) {
		return;
	}

	vp = Option_allocate_value(value, NULL, opt_origin);
	if (!vp) {
		exit(1);
	}
	vp->override = override;

	np = Option_allocate_node(name, vp);
	if (!np) {
		exit(1);
	}

	np->next = Option_hash_array[ix];
	Option_hash_array[ix] = np;
}

/*
 * Return the value of the specified option,
 * or NULL if there is no value for that option.
 */
char *Option_get_value(const char *name, optOrigin *origin_ptr)
{
	hash_node *np = Get_hash_node_by_name(name);

	if (np != NULL) {
		if (origin_ptr != NULL) {
			*origin_ptr = np->value->origin;
		}
		return np->value->value;
	}

	return (char *) NULL;
}

/*
 * Free all option hash table related dynamically allocated memory.
 */
static void Options_hash_free(void)
{
	int32_t i;
	hash_node *np;

	for (i = 0; i < HASH_SIZE; i++) {
		while ((np = Option_hash_array[i]) != NULL) {
			Option_hash_array[i] = np->next;
			Option_free_node(np);
		}
	}

	if (hash_nodes_allocated != hash_nodes_freed) {
		errno = 0;
		error(
				"hash nodes alloc = %d, hash nodes free = %d, delta = %d\n",
				hash_nodes_allocated, hash_nodes_freed,
				hash_nodes_allocated - hash_nodes_freed);
	}

	if (hash_values_allocated != hash_values_freed) {
		errno = 0;
		error(
				"hash values alloc = %d, hash values free = %d, delta = %d\n",
				hash_values_allocated, hash_values_freed,
				hash_values_allocated - hash_values_freed);
	}
}

/*
 * Print info about our hashing function performance.
 */
static void Options_hash_performance(void)
{
#ifdef DEVELOPMENT
	int32_t bucket_use_count;
	int32_t i;
	hash_node *np;
	uint8_t histo[HASH_SIZE];
	char msg[MSG_LEN];

	if (getenv("XPILOTSHASHPERF") == NULL) {
		return;
	}

	memset(histo, 0, sizeof(histo));
	for (i = 0; i < HASH_SIZE; i++) {
		bucket_use_count = 0;
		for (np = Option_hash_array[i]; np; np = np->next) {
			bucket_use_count++;
		}
		histo[bucket_use_count]++;
	}

	sprintf(msg, "hash perf histo:");
	for (i = 0; i < NELEM(histo); i++) {
		sprintf(msg + strlen(msg), " %d", histo[i]);
		if (strlen(msg)> 75) {
			break;
		}
	}
	printf("%s\n", msg);
#endif
}

bool Convert_string_to_int(const char *value_str, int32_t *int_ptr)
{
	char *end_ptr = NULL;
	int32_t value;
	bool result;

	/* base 0 has special meaning. */
	value = strtol(value_str, (char **)(&end_ptr), 0);

	/* store value regardless of error. */
	*int_ptr = (int32_t) value;

	/* if at least one digit was found we're satisfied. */
	if (end_ptr > value_str) {
		result = true;
	}
	else {
		result = false;
	}

	return result;
}

bool Convert_string_to_float(const char *value_str, DFLOAT *float_ptr)
{
	char *end_ptr = NULL;
	double value;
	bool result;

	value = strtod(value_str, (char **)(&end_ptr));

	/* store value regardless of error. */
	*float_ptr = (DFLOAT) value;

	/* if at least one digit was found we're satisfied. */
	if (end_ptr > value_str) {
		result = true;
	}
	else {
		result = false;
	}

	return result;
}

bool Convert_string_to_bool(const char *value_str, bool *bool_ptr)
{
	bool result;

	if (!strcasecmp(value_str, "yes") || !strcasecmp(value_str, "on")
			|| !strcasecmp(value_str, "true")) {
		*bool_ptr = true;
		result = true;
	}
	else if (!strcasecmp(value_str, "no") || !strcasecmp(value_str, "off")
			|| !strcasecmp(value_str, "false")) {
		*bool_ptr = false;
		result = true;
	}
	else {
		result = false;
	}

	return result;
}

void Convert_list_to_string(TList list, char **string)
{
	TListIter iter;
	size_t size = 0;

	for (iter = LIST_GetFirstItem(list); iter != LIST_GetLastItem(list); LI_FORWARD(iter)) {
		size += 1 + strlen((const char *) LI_DATA(iter));
	}
	*string = (char *) xp_safe_malloc(size);
	**string = '\0';
	for (iter = LIST_GetFirstItem(list); iter != LIST_GetLastItem(list); LI_FORWARD(iter)) {
		if (iter != LIST_GetFirstItem(list)) {
			strlcat(*string, ",", size);
		}
		strlcat(*string, (const char *) LI_DATA(iter), size);
	}
}

void Convert_string_to_list(const char *value, TList *list_ptr)
{
	const char *start, *end;
	char *str;

	/* possibly allocate a new list. */
	if (NULL == *list_ptr) {
		*list_ptr = LIST_New();
		if (NULL == *list_ptr) {
			fatal("Not enough memory for list");
		}
	}

	/* make sure list is empty. */
	LIST_Clear(*list_ptr);

	/* copy comma separated list elements from value to list. */
	for (start = value; *start; start = end) {
		/* skip comma separators. */
		while (*start == ',') {
			start++;
		}
		/* search for end of list element. */
		end = start;
		while (*end && *end != ',') {
			end++;
		}
		/* copy non-zero results to list. */
		if (start < end) {
			str = (char *) xp_safe_malloc((end - start) + 1);
			memcpy(str, start, (end - start));
			str[(end - start)] = '\0';
			if (NULL == LIST_AddItemBottom(*list_ptr, str)) {
				fatal("Not enough memory for list element");
			}
		}
	}
}

/*
 * Set the option description variable.
 */
static void Option_parse_node(hash_node *np)
{
	option_desc *desc;
	const char *value;

	/* Does it have a description?   If so, get a pointer to it */
	if ((desc = np->value->desc) == NULL) {
		return;
	}

	/* get value from command line, defaults file or map file. */
	value = np->value->value;
	if (value == NULL) {
		/* no value has been set, so get the option default value. */
		value = desc->defaultValue;
		if (value == NULL) {
			/* no value at all.  (mapData or serverHost.) */
			return;
		}
	}

	if (!desc->variable) {
		if (desc->type == valVoid) {
			return;
		}
		else {
			dumpcore("Hashed option %s has no value", np->name);
		}
	}

	switch (desc->type) {

	case valVoid:
		break;

	case valInt: {
		int32_t *ptr = (int32_t *) desc->variable;

		if (Convert_string_to_int(value, ptr) != true) {
			warn("%s value '%s' not an integral number.", np->name,
					value);
			Convert_string_to_int(desc->defaultValue, ptr);
		}
		break;
	}

	case valReal: {
		DFLOAT *ptr = (DFLOAT *) desc->variable;

		if (Convert_string_to_float(value, ptr) != true) {
			warn("%s value '%s' not a number.", np->name, value);
			Convert_string_to_float(desc->defaultValue, ptr);
		}
		break;
	}

	case valBool: {
		bool *ptr = (bool *) desc->variable;

		if (Convert_string_to_bool(value, ptr) != true) {
			warn("%s value '%s' not a boolean.", np->name, value);
			Convert_string_to_bool(desc->defaultValue, ptr);
		}
		break;
	}

	case valString: {
		char **ptr = (char **) desc->variable;

		*ptr = xp_safe_strdup(value);
		break;
	}

	case valList: {
		TList *list_ptr = (TList *) desc->variable;

		Convert_string_to_list(value, list_ptr);
		break;
	}
	}
}

/*
 * Expand any "expand" arguments.
 */
static void Options_parse_expand(void)
{
	hash_node *np;

	np = Get_hash_node_by_name("expand");
	if (np == NULL) {
		dumpcore("Could not find option hash node for option '%s'.",
				"expand");
	}
	else {
		Option_parse_node(np);
	}

	if (expandList != NULL) {
		char *name;
		while ((name = (char *) LIST_RemoveFirstItem(expandList)) != NULL) {
			expandKeyword(name);
		}
		LIST_Delete(expandList);
		expandList = NULL;
	}
}

/*
 * Go through the hash table looking for name-value pairs that have defaults
 * assigned to them.   Process the defaults and, if possible, set the
 * associated variables.
 */
void Options_parse(void)
{
	int32_t i;
	hash_node *np;
	option_desc *options;
	int32_t option_count;

	options = Get_option_descs(&option_count);

	/*
	 * Expand a possible "-expand" option.
	 */
	Options_parse_expand();

	for (i = 0; i < option_count; i++) {
		np = Get_hash_node_by_name(options[i].name);
		if (np == NULL) {
			dumpcore(
					"Could not find option hash node for option '%s'.",
					options[i].name);
		}
		else {
			Option_parse_node(np);
		}
	}
}

/*
 * Free the option database memory.
 */
void Options_free(void)
{
	Options_hash_performance();
	Options_hash_free();
}

