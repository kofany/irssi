/*
 fe-web-api.c : JSON API for web frontend

    Copyright (C) 2025 irssip project
*/

#include "module.h"
#include "fe-web.h"
#include <src/core/servers.h>
#include <src/core/channels.h>
#include <src/core/queries.h>
#include <src/core/nicklist.h>

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
	GSList *tmp;
	char *escaped_name, *escaped_topic;
	gboolean first_nick;
	GSList *nick_tmp;

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

	/* Add nicks */
	first_nick = TRUE;
	for (nick_tmp = channel->nicks; nick_tmp != NULL; nick_tmp = nick_tmp->next) {
		NICK_REC *nick = nick_tmp->data;
		char *escaped_nick = fe_web_escape_json_string(nick->nick);
		char *mode = "";
		
		if (nick->op) mode = "@";
		else if (nick->halfop) mode = "%";
		else if (nick->voice) mode = "+";
		
		if (!first_nick) {
			g_string_append_c(json, ',');
		}
		
		g_string_append_printf(json,
			"{\"nick\":%s,\"mode\":\"%s\",\"away\":%s}",
			escaped_nick,
			mode,
			nick->gone ? "true" : "false"
		);
		
		first_nick = FALSE;
		g_free(escaped_nick);
	}
	
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
	
	msg = fe_web_message_new(WEB_MSG_SERVER_STATUS);
	msg->text = g_strdup("server_list");
	
	servers_json = g_string_new("[");
	
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
	
	g_hash_table_insert(msg->extra_data, g_strdup("servers"), 
	                   g_string_free(servers_json, FALSE));
	
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

static void sig_client_connected(WEB_CLIENT_REC *client)
{
	/* Send initial data to newly connected client */
	fe_web_api_send_server_list(client);
	fe_web_api_send_window_list(client);
}

void fe_web_api_init(void)
{
	/* API is ready */
}

void fe_web_api_deinit(void)
{
	/* Cleanup API */
}
