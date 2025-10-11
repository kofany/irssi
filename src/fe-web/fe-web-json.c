/*
 fe-web-json.c : Simple JSON parser for fe-web

    Copyright (C) 2025

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "module.h"
#include "fe-web.h"

#include <string.h>
#include <ctype.h>

/* Simple JSON value extraction - finds "key":"value" or "key":123 */
char *fe_web_json_get_string(const char *json, const char *key)
{
	char *search_str;
	char *pos;
	char *start;
	char *end;
	char *result;

	if (json == NULL || key == NULL) {
		return NULL;
	}

	/* Build search string: "key": */
	search_str = g_strdup_printf("\"%s\"", key);
	pos = strstr(json, search_str);
	g_free(search_str);

	if (pos == NULL) {
		return NULL;
	}

	/* Skip past the key and colon */
	pos = strchr(pos + strlen(key), ':');
	if (pos == NULL) {
		return NULL;
	}
	pos++;

	/* Skip whitespace */
	while (*pos != '\0' && isspace(*pos)) {
		pos++;
	}

	/* Check if it's a string (starts with ") */
	if (*pos != '"') {
		return NULL;
	}

	start = pos + 1;

	/* Find end of string (handle escaped quotes) */
	end = start;
	while (*end != '\0') {
		if (*end == '\\' && *(end + 1) != '\0') {
			end += 2;
			continue;
		}
		if (*end == '"') {
			break;
		}
		end++;
	}

	if (*end != '"') {
		return NULL;
	}

	/* Extract string (without unescaping for now) */
	result = g_strndup(start, end - start);
	return result;
}

/* Get integer value from JSON */
int fe_web_json_get_int(const char *json, const char *key, int default_value)
{
	char *search_str;
	char *pos;
	int value;

	if (json == NULL || key == NULL) {
		return default_value;
	}

	/* Build search string: "key": */
	search_str = g_strdup_printf("\"%s\"", key);
	pos = strstr(json, search_str);
	g_free(search_str);

	if (pos == NULL) {
		return default_value;
	}

	/* Skip past the key and colon */
	pos = strchr(pos + strlen(key), ':');
	if (pos == NULL) {
		return default_value;
	}
	pos++;

	/* Skip whitespace */
	while (*pos != '\0' && isspace(*pos)) {
		pos++;
	}

	/* Parse integer */
	if (sscanf(pos, "%d", &value) == 1) {
		return value;
	}

	return default_value;
}

/* Check if JSON has a key */
int fe_web_json_has_key(const char *json, const char *key)
{
	char *search_str;
	char *pos;

	if (json == NULL || key == NULL) {
		return 0;
	}

	search_str = g_strdup_printf("\"%s\"", key);
	pos = strstr(json, search_str);
	g_free(search_str);

	return pos != NULL;
}
