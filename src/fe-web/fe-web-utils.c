/*
 fe-web-utils.c : Utility functions for fe-web module

    Copyright (C) 2025

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "module.h"
#include "fe-web.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Generate unique message ID (timestamp-counter format) */
char *fe_web_generate_message_id(void)
{
	static int counter = 0;
	time_t now;
	char *id;

	now = time(NULL);
	id = g_strdup_printf("%ld-%04d", (long)now, counter);

	counter++;
	if (counter >= 10000) {
		counter = 0;
	}

	return id;
}

/* Escape JSON string (simple version for common cases) */
char *fe_web_escape_json(const char *str)
{
	GString *result;
	const char *p;

	if (str == NULL) {
		return g_strdup("");
	}

	result = g_string_new("");

	for (p = str; *p != '\0'; p++) {
		switch (*p) {
		case '"':
			g_string_append(result, "\\\"");
			break;
		case '\\':
			g_string_append(result, "\\\\");
			break;
		case '\b':
			g_string_append(result, "\\b");
			break;
		case '\f':
			g_string_append(result, "\\f");
			break;
		case '\n':
			g_string_append(result, "\\n");
			break;
		case '\r':
			g_string_append(result, "\\r");
			break;
		case '\t':
			g_string_append(result, "\\t");
			break;
		default:
			if (*p < 32) {
				g_string_append_printf(result, "\\u%04x", (unsigned char)*p);
			} else {
				g_string_append_c(result, *p);
			}
			break;
		}
	}

	return g_string_free(result, FALSE);
}

/* Convert message type enum to string */
static const char *fe_web_type_to_string(WEB_MESSAGE_TYPE type)
{
	switch (type) {
	case WEB_MSG_AUTH_OK:
		return "auth_ok";
	case WEB_MSG_MESSAGE:
		return "message";
	case WEB_MSG_SERVER_STATUS:
		return "server_status";
	case WEB_MSG_CHANNEL_JOIN:
		return "channel_join";
	case WEB_MSG_CHANNEL_PART:
		return "channel_part";
	case WEB_MSG_CHANNEL_KICK:
		return "channel_kick";
	case WEB_MSG_USER_QUIT:
		return "user_quit";
	case WEB_MSG_TOPIC:
		return "topic";
	case WEB_MSG_CHANNEL_MODE:
		return "channel_mode";
	case WEB_MSG_NICKLIST:
		return "nicklist";
	case WEB_MSG_NICK_CHANGE:
		return "nick_change";
	case WEB_MSG_USER_MODE:
		return "user_mode";
	case WEB_MSG_AWAY:
		return "away";
	case WEB_MSG_WHOIS:
		return "whois";
	case WEB_MSG_CHANNEL_LIST:
		return "channel_list";
	case WEB_MSG_STATE_DUMP:
		return "state_dump";
	case WEB_MSG_ERROR:
		return "error";
	case WEB_MSG_PONG:
		return "pong";
	default:
		return "unknown";
	}
}

/* Create new message structure */
WEB_MESSAGE_REC *fe_web_message_new(WEB_MESSAGE_TYPE type)
{
	WEB_MESSAGE_REC *msg;

	msg = g_new0(WEB_MESSAGE_REC, 1);
	msg->type = type;
	msg->timestamp = time(NULL);
	msg->extra_data = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                        g_free, g_free);
	msg->is_own = FALSE;

	return msg;
}

/* Free message structure */
void fe_web_message_free(WEB_MESSAGE_REC *msg)
{
	if (msg == NULL) {
		return;
	}

	g_free(msg->id);
	g_free(msg->server_tag);
	g_free(msg->target);
	g_free(msg->nick);
	g_free(msg->text);
	g_free(msg->response_to);

	if (msg->extra_data != NULL) {
		g_hash_table_destroy(msg->extra_data);
	}

	g_free(msg);
}

/* Serialize message to JSON */
char *fe_web_message_to_json(WEB_MESSAGE_REC *msg)
{
	GString *json;
	char *escaped;

	json = g_string_new("{");

	/* id */
	if (msg->id != NULL) {
		escaped = fe_web_escape_json(msg->id);
		g_string_append_printf(json, "\"id\":\"%s\",", escaped);
		g_free(escaped);
	}

	/* type */
	g_string_append_printf(json, "\"type\":\"%s\"", fe_web_type_to_string(msg->type));

	/* response_to (for WHOIS, channel_list) */
	if (msg->response_to != NULL) {
		escaped = fe_web_escape_json(msg->response_to);
		g_string_append_printf(json, ",\"response_to\":\"%s\"", escaped);
		g_free(escaped);
	}

	/* server */
	if (msg->server_tag != NULL) {
		escaped = fe_web_escape_json(msg->server_tag);
		g_string_append_printf(json, ",\"server\":\"%s\"", escaped);
		g_free(escaped);
	}

	/* channel/target */
	if (msg->target != NULL) {
		escaped = fe_web_escape_json(msg->target);
		g_string_append_printf(json, ",\"channel\":\"%s\"", escaped);
		g_free(escaped);
	}

	/* nick */
	if (msg->nick != NULL) {
		escaped = fe_web_escape_json(msg->nick);
		g_string_append_printf(json, ",\"nick\":\"%s\"", escaped);
		g_free(escaped);
	}

	/* text */
	if (msg->text != NULL) {
		escaped = fe_web_escape_json(msg->text);
		g_string_append_printf(json, ",\"text\":\"%s\"", escaped);
		g_free(escaped);
	}

	/* timestamp */
	g_string_append_printf(json, ",\"timestamp\":%ld", (long)msg->timestamp);

	/* level (for message types) */
	if (msg->level != 0) {
		g_string_append_printf(json, ",\"level\":%d", msg->level);
	}

	/* is_own */
	if (msg->type == WEB_MSG_MESSAGE) {
		g_string_append_printf(json, ",\"is_own\":%s",
		                      msg->is_own ? "true" : "false");
	}

	/* extra_data - serialize hash table if not empty */
	if (msg->extra_data != NULL && g_hash_table_size(msg->extra_data) > 0) {
		GHashTableIter iter;
		gpointer key;
		gpointer value;
		int first;

		g_string_append(json, ",\"extra\":{");

		first = 1;
		g_hash_table_iter_init(&iter, msg->extra_data);
		while (g_hash_table_iter_next(&iter, &key, &value)) {
			char *escaped_key;
			char *escaped_value;

			if (!first) {
				g_string_append_c(json, ',');
			}
			first = 0;

			escaped_key = fe_web_escape_json((const char *)key);
			escaped_value = fe_web_escape_json((const char *)value);
			g_string_append_printf(json, "\"%s\":\"%s\"", escaped_key, escaped_value);
			g_free(escaped_key);
			g_free(escaped_value);
		}

		g_string_append_c(json, '}');
	}

	g_string_append_c(json, '}');

	return g_string_free(json, FALSE);
}

/* Send message to specific client */
void fe_web_send_message(WEB_CLIENT_REC *client, WEB_MESSAGE_REC *msg)
{
	char *json;

	if (client == NULL || !client->authenticated || !client->handshake_done) {
		return;
	}

	json = fe_web_message_to_json(msg);

	/* TODO: Actually send via WebSocket (will be implemented in fe-web-server.c) */
	/* For now, just append to buffer */
	if (client->output_buffer != NULL) {
		g_string_append(client->output_buffer, json);
		g_string_append_c(client->output_buffer, '\n');
	}

	g_free(json);
	client->messages_sent++;
}

/* Send message to all clients synced with specific server */
void fe_web_send_to_server_clients(IRC_SERVER_REC *server, WEB_MESSAGE_REC *msg)
{
	GSList *tmp;

	if (server == NULL) {
		return;
	}

	for (tmp = web_clients; tmp != NULL; tmp = tmp->next) {
		WEB_CLIENT_REC *client = tmp->data;

		/* Send ONLY to clients synced with this server or all servers */
		if (client->authenticated &&
		    (client->server == server || client->wants_all_servers)) {
			fe_web_send_message(client, msg);
		}
	}
}

/* Send message to all authenticated clients */
void fe_web_send_to_all_clients(WEB_MESSAGE_REC *msg)
{
	GSList *tmp;

	for (tmp = web_clients; tmp != NULL; tmp = tmp->next) {
		WEB_CLIENT_REC *client = tmp->data;

		if (client->authenticated) {
			fe_web_send_message(client, msg);
		}
	}
}
