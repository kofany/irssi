/*
 fe-web-signals.c : Signal handlers for web frontend

    Copyright (C) 2025 irssip project
*/

#include "module.h"
#include "fe-web.h"
#include <irssip/src/core/signals.h>
#include <irssip/src/core/servers.h>
#include <irssip/src/core/channels.h>
#include <irssip/src/core/queries.h>
#include <irssip/src/fe-common/core/printtext.h>
#include <irssip/src/fe-common/core/window-items.h>

/* Signal handlers */

static void sig_print_text(WINDOW_REC *window, void *server, const char *target,
                          gpointer level_p, const char *text)
{
	WEB_MESSAGE_REC *msg;
	int level = GPOINTER_TO_INT(level_p);
	
	if (g_slist_length(web_clients) == 0) {
		return; /* No clients connected */
	}
	
	msg = fe_web_message_new(WEB_MSG_CHAT);
	msg->server_tag = server ? SERVER(server)->tag : NULL;
	msg->target = g_strdup(target);
	msg->text = g_strdup(text);
	msg->level = level;
	msg->timestamp = time(NULL);
	
	/* Add window information */
	if (window) {
		g_hash_table_insert(msg->extra_data, g_strdup("window_refnum"), 
		                   g_strdup_printf("%d", window->refnum));
		g_hash_table_insert(msg->extra_data, g_strdup("window_name"), 
		                   g_strdup(window->name ? window->name : ""));
	}
	
	fe_web_broadcast_message(msg);
	fe_web_message_free(msg);
}

static void sig_message_public(SERVER_REC *server, const char *msg, const char *nick,
                              const char *address, const char *target)
{
	WEB_MESSAGE_REC *web_msg;
	
	if (g_slist_length(web_clients) == 0) {
		return;
	}
	
	web_msg = fe_web_message_new(WEB_MSG_CHAT);
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(target);
	web_msg->nick = g_strdup(nick);
	web_msg->text = g_strdup(msg);
	web_msg->level = MSGLEVEL_PUBLIC;
	web_msg->timestamp = time(NULL);
	
	/* Add extra data */
	if (address) {
		g_hash_table_insert(web_msg->extra_data, g_strdup("address"), g_strdup(address));
	}
	
	fe_web_broadcast_message(web_msg);
	fe_web_message_free(web_msg);
}

static void sig_message_private(SERVER_REC *server, const char *msg, const char *nick,
                               const char *address)
{
	WEB_MESSAGE_REC *web_msg;
	
	if (g_slist_length(web_clients) == 0) {
		return;
	}
	
	web_msg = fe_web_message_new(WEB_MSG_CHAT);
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(nick); /* Private message target is the nick */
	web_msg->nick = g_strdup(nick);
	web_msg->text = g_strdup(msg);
	web_msg->level = MSGLEVEL_MSGS;
	web_msg->timestamp = time(NULL);
	
	if (address) {
		g_hash_table_insert(web_msg->extra_data, g_strdup("address"), g_strdup(address));
	}
	
	fe_web_broadcast_message(web_msg);
	fe_web_message_free(web_msg);
}

static void sig_server_connected(SERVER_REC *server)
{
	WEB_MESSAGE_REC *msg;
	
	if (g_slist_length(web_clients) == 0) {
		return;
	}
	
	msg = fe_web_message_new(WEB_MSG_SERVER_STATUS);
	msg->server_tag = g_strdup(server->tag);
	msg->text = g_strdup("connected");
	msg->timestamp = time(NULL);
	
	g_hash_table_insert(msg->extra_data, g_strdup("server_address"), 
	                   g_strdup(server->connrec->address));
	g_hash_table_insert(msg->extra_data, g_strdup("server_port"), 
	                   g_strdup_printf("%d", server->connrec->port));
	g_hash_table_insert(msg->extra_data, g_strdup("nick"), 
	                   g_strdup(server->nick));
	
	fe_web_broadcast_message(msg);
	fe_web_message_free(msg);
}

static void sig_server_disconnected(SERVER_REC *server)
{
	WEB_MESSAGE_REC *msg;
	
	if (g_slist_length(web_clients) == 0) {
		return;
	}
	
	msg = fe_web_message_new(WEB_MSG_SERVER_STATUS);
	msg->server_tag = g_strdup(server->tag);
	msg->text = g_strdup("disconnected");
	msg->timestamp = time(NULL);
	
	fe_web_broadcast_message(msg);
	fe_web_message_free(msg);
}

static void sig_channel_joined(CHANNEL_REC *channel)
{
	WEB_MESSAGE_REC *msg;
	
	if (g_slist_length(web_clients) == 0) {
		return;
	}
	
	msg = fe_web_message_new(WEB_MSG_CHAT);
	msg->server_tag = g_strdup(channel->server->tag);
	msg->target = g_strdup(channel->name);
	msg->text = g_strdup_printf("You have joined %s", channel->name);
	msg->level = MSGLEVEL_JOINS;
	msg->timestamp = time(NULL);
	
	/* Send channel topic if available */
	if (channel->topic) {
		g_hash_table_insert(msg->extra_data, g_strdup("topic"), g_strdup(channel->topic));
	}
	
	fe_web_broadcast_message(msg);
	fe_web_message_free(msg);
}

static void sig_nicklist_new(CHANNEL_REC *channel, NICK_REC *nick)
{
	WEB_MESSAGE_REC *msg;
	
	if (g_slist_length(web_clients) == 0) {
		return;
	}
	
	msg = fe_web_message_new(WEB_MSG_NICK_LIST);
	msg->server_tag = g_strdup(channel->server->tag);
	msg->target = g_strdup(channel->name);
	msg->nick = g_strdup(nick->nick);
	msg->text = g_strdup("join");
	msg->timestamp = time(NULL);
	
	/* Add nick modes */
	if (nick->op) g_hash_table_insert(msg->extra_data, g_strdup("op"), g_strdup("1"));
	if (nick->voice) g_hash_table_insert(msg->extra_data, g_strdup("voice"), g_strdup("1"));
	if (nick->halfop) g_hash_table_insert(msg->extra_data, g_strdup("halfop"), g_strdup("1"));
	
	fe_web_broadcast_message(msg);
	fe_web_message_free(msg);
}

static void sig_nicklist_remove(CHANNEL_REC *channel, NICK_REC *nick)
{
	WEB_MESSAGE_REC *msg;
	
	if (g_slist_length(web_clients) == 0) {
		return;
	}
	
	msg = fe_web_message_new(WEB_MSG_NICK_LIST);
	msg->server_tag = g_strdup(channel->server->tag);
	msg->target = g_strdup(channel->name);
	msg->nick = g_strdup(nick->nick);
	msg->text = g_strdup("part");
	msg->timestamp = time(NULL);
	
	fe_web_broadcast_message(msg);
	fe_web_message_free(msg);
}

void fe_web_signals_init(void)
{
	/* Core message signals */
	signal_add("print text", (SIGNAL_FUNC) sig_print_text);
	signal_add("message public", (SIGNAL_FUNC) sig_message_public);
	signal_add("message private", (SIGNAL_FUNC) sig_message_private);
	
	/* Server signals */
	signal_add("server connected", (SIGNAL_FUNC) sig_server_connected);
	signal_add("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
	
	/* Channel signals */
	signal_add("channel joined", (SIGNAL_FUNC) sig_channel_joined);
	
	/* Nick list signals */
	signal_add("nicklist new", (SIGNAL_FUNC) sig_nicklist_new);
	signal_add("nicklist remove", (SIGNAL_FUNC) sig_nicklist_remove);
}

void fe_web_signals_deinit(void)
{
	signal_remove("print text", (SIGNAL_FUNC) sig_print_text);
	signal_remove("message public", (SIGNAL_FUNC) sig_message_public);
	signal_remove("message private", (SIGNAL_FUNC) sig_message_private);
	signal_remove("server connected", (SIGNAL_FUNC) sig_server_connected);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
	signal_remove("channel joined", (SIGNAL_FUNC) sig_channel_joined);
	signal_remove("nicklist new", (SIGNAL_FUNC) sig_nicklist_new);
	signal_remove("nicklist remove", (SIGNAL_FUNC) sig_nicklist_remove);
}
