/*
 fe-web-api.c : JSON API for web frontend

    Copyright (C) 2025 irssi project
*/

#include "module.h"
#include "fe-web.h"
#include <irssi/src/core/servers.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/core/queries.h>
#include <irssi/src/core/nicklist.h>

char *fe_web_api_serialize_server(SERVER_REC *server)
{
	GString *json;
	GSList *tmp;
	char *escaped_tag, *escaped_address, *escaped_nick;
	gboolean first_channel;

	if (server == NULL) return g_strdup("{}");

	json = g_string_new("{");

	escaped_tag = fe_web_escape_json_string(server->tag);
	escaped_address = fe_web_escape_json_string(server->connrec->address);
	escaped_nick = fe_web_escape_json_string(server->nick);

	g_string_append_printf(json,
		"\"tag\":%s,"
		"\"address\":%s,"
		"\"port\":%d,"
		"\"connected\":%s,"
		"\"nick\":%s,"
		"\"channels\":[",
		escaped_tag,
		escaped_address,
		server->connrec->port,
		server->connected ? "true" : "false",
		escaped_nick
	);

	/* Add channels */
	first_channel = TRUE;
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *channel = tmp->data;
		char *channel_json = fe_web_api_serialize_channel(channel);
		
		if (!first_channel) {
			g_string_append_c(json, ',');
		}
		g_string_append(json, channel_json);
		first_channel = FALSE;
		
		g_free(channel_json);
	}
	
	g_string_append(json, "]}");
	
	g_free(escaped_tag);
	g_free(escaped_address);
	g_free(escaped_nick);
	
	return g_string_free(json, FALSE);
}

char *fe_web_api_serialize_channel(CHANNEL_REC *channel)
{
	GString *json;
	char *escaped_name, *escaped_topic;

	if (channel == NULL) return g_strdup("{}");

	json = g_string_new("{");

	escaped_name = fe_web_escape_json_string(channel->name);
	escaped_topic = fe_web_escape_json_string(channel->topic);

	g_string_append_printf(json,
		"\"name\":%s,"
		"\"server\":\"%s\","
		"\"topic\":%s,"
		"\"nicks\":[",
		escaped_name,
		channel->server->tag,
		escaped_topic ? escaped_topic : "null"
	);

	/* Add nicks - TODO: Implement proper nicklist iteration */
	/* channel->nicks is a GHashTable, not GSList - need different approach */
	
	g_string_append_printf(json,
		"],\"messages\":[],"
		"\"unread\":0,"
		"\"highlight\":false}"
	);
	
	g_free(escaped_name);
	g_free(escaped_topic);
	
	return g_string_free(json, FALSE);
}

char *fe_web_api_serialize_window(WINDOW_REC *window)
{
	/* TODO: Implement when WINDOW_REC structure is available */
	return g_strdup("{\"error\":\"not_implemented\"}");
}

static void fe_web_api_send_server_list(WEB_CLIENT_REC *client)
{
	WEB_MESSAGE_REC *msg;
	GString *servers_json;
	GSList *tmp;
	gboolean first = TRUE;

	if (client == NULL) return;

	msg = fe_web_message_new(WEB_MSG_SERVER_STATUS);
	if (msg == NULL) return;

	msg->text = g_strdup("server_list");

	servers_json = g_string_new("[");

	/* Check if servers list is available */
	if (servers == NULL) {
		g_string_append(servers_json, "]");
		if (msg->extra_data != NULL) {
			g_hash_table_insert(msg->extra_data, g_strdup("servers"),
			                   g_string_free(servers_json, FALSE));
		} else {
			g_string_free(servers_json, TRUE);
		}
		fe_web_client_send_message(client, msg);
		fe_web_message_free(msg);
		return;
	}

	for (tmp = servers; tmp != NULL; tmp = tmp->next) {
		SERVER_REC *server = tmp->data;
		char *server_json = fe_web_api_serialize_server(server);
		
		if (!first) {
			g_string_append_c(servers_json, ',');
		}
		g_string_append(servers_json, server_json);
		first = FALSE;
		
		g_free(server_json);
	}
	
	g_string_append_c(servers_json, ']');

	if (msg->extra_data != NULL) {
		g_hash_table_insert(msg->extra_data, g_strdup("servers"),
		                   g_string_free(servers_json, FALSE));
	} else {
		g_string_free(servers_json, TRUE);
	}
	
	fe_web_client_send_message(client, msg);
	fe_web_message_free(msg);
}

static void fe_web_api_send_window_list(WEB_CLIENT_REC *client)
{
	WEB_MESSAGE_REC *msg;

	msg = fe_web_message_new(WEB_MSG_WINDOW_CHANGE);
	msg->text = g_strdup("window_list_not_implemented");

	fe_web_client_send_message(client, msg);
	fe_web_message_free(msg);
}

void fe_web_api_handle_request(WEB_CLIENT_REC *client, const char *request)
{
	if (client == NULL || request == NULL) return;
	
	/* Handle API requests from web client */
	if (strstr(request, "\"type\":\"get_servers\"")) {
		fe_web_api_send_server_list(client);
	} else if (strstr(request, "\"type\":\"get_windows\"")) {
		fe_web_api_send_window_list(client);
	}
}

/* sig_client_connected removed - unused function */

void fe_web_api_init(void)
{
	/* API is ready */
}

void fe_web_api_deinit(void)
{
	/* Cleanup API */
}
