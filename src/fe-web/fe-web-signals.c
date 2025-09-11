/*
 fe-web-signals.c : Signal handlers for web frontend

    Copyright (C) 2025 irssi project
*/

#include "module.h"
#include "fe-web.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/servers.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/core/queries.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/core/nicklist.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/fe-common/core/window-items.h>
#include <irssi/src/fe-common/core/formats.h>

/* Signal handlers */

static void sig_print_text(TEXT_DEST_REC *dest, const char *text, const char *stripped)
{
	WEB_MESSAGE_REC *msg;

	/* Check client count */
	if (g_slist_length(web_clients) == 0) {
		return; /* No clients connected */
	}

	/* Check for NULL parameters */
	if (!dest || !text) {
		return; /* Skip NULL parameters */
	}

	msg = fe_web_message_new(WEB_MSG_CHAT);

	/* Extract all available information from TEXT_DEST_REC */
	msg->server_tag = dest->server_tag ? g_strdup(dest->server_tag) : NULL;
	msg->target = dest->target ? g_strdup(dest->target) : g_strdup("");
	msg->nick = dest->nick ? g_strdup(dest->nick) : NULL;
	msg->text = g_strdup(text);
	msg->level = dest->level;
	msg->timestamp = time(NULL);

	/* Add window information */
	if (dest->window) {
		g_hash_table_insert(msg->extra_data, g_strdup("window_refnum"),
		                   g_strdup_printf("%d", dest->window->refnum));
		g_hash_table_insert(msg->extra_data, g_strdup("window_name"),
		                   g_strdup(dest->window->name ? dest->window->name : ""));
	}

	/* Add additional context from TEXT_DEST_REC */
	if (dest->address) {
		g_hash_table_insert(msg->extra_data, g_strdup("address"), g_strdup(dest->address));
	}

	if (dest->hilight_color) {
		g_hash_table_insert(msg->extra_data, g_strdup("hilight_color"), g_strdup(dest->hilight_color));
	}

	if (dest->hilight_priority > 0) {
		g_hash_table_insert(msg->extra_data, g_strdup("hilight_priority"),
		                   g_strdup_printf("%d", dest->hilight_priority));
	}

	/* Add stripped text for logging/search purposes */
	if (stripped && g_strcmp0(text, stripped) != 0) {
		g_hash_table_insert(msg->extra_data, g_strdup("stripped_text"), g_strdup(stripped));
	}

	fe_web_broadcast_message(msg);
	fe_web_message_free(msg);
}

/* Removed sig_message_public and sig_message_private functions
 * These are now handled by sig_print_text which captures all displayed text
 * with full context from TEXT_DEST_REC, avoiding duplicates */

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
	/* Main signal - captures ALL text output from irssi */
	signal_add("print text", (SIGNAL_FUNC) sig_print_text);

	/* Server signals - these don't duplicate print text */
	signal_add("server connected", (SIGNAL_FUNC) sig_server_connected);
	signal_add("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);

	/* Channel signals - these don't duplicate print text */
	signal_add("channel joined", (SIGNAL_FUNC) sig_channel_joined);

	/* Nick list signals - these don't duplicate print text */
	signal_add("nicklist new", (SIGNAL_FUNC) sig_nicklist_new);
	signal_add("nicklist remove", (SIGNAL_FUNC) sig_nicklist_remove);

	/* Note: Removed message_public and message_private signals to avoid duplicates
	 * since print text already captures all displayed messages with full context */
}

void fe_web_signals_deinit(void)
{
	signal_remove("print text", (SIGNAL_FUNC) sig_print_text);
	signal_remove("server connected", (SIGNAL_FUNC) sig_server_connected);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
	signal_remove("channel joined", (SIGNAL_FUNC) sig_channel_joined);
	signal_remove("nicklist new", (SIGNAL_FUNC) sig_nicklist_new);
	signal_remove("nicklist remove", (SIGNAL_FUNC) sig_nicklist_remove);
}
