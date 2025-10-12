/*
 fe-web-signals.c : IRC signal handlers for fe-web

    Copyright (C) 2025

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "module.h"
#include "fe-web.h"

#include <irssi/src/core/signals.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/core/nicklist.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/irc/core/irc-servers.h>
#include <irssi/src/irc/core/irc-channels.h>
#include <irssi/src/irc/core/irc-nicklist.h>

/* Signal: "message public" */
static void sig_message_public(IRC_SERVER_REC *server, const char *msg,
                                const char *nick, const char *address,
                                const char *target)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_MESSAGE);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(target);
	web_msg->nick = g_strdup(nick);
	web_msg->text = g_strdup(msg);
	web_msg->level = MSGLEVEL_PUBLIC;
	web_msg->is_own = FALSE;

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message own_public" */
static void sig_message_own_public(IRC_SERVER_REC *server, const char *msg,
                                    const char *target)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_MESSAGE);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(target);
	web_msg->nick = g_strdup(server->nick);
	web_msg->text = g_strdup(msg);
	web_msg->level = MSGLEVEL_PUBLIC;
	web_msg->is_own = TRUE;

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message private" */
static void sig_message_private(IRC_SERVER_REC *server, const char *msg,
                                 const char *nick, const char *address)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_MESSAGE);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(nick); /* Private message - target is the sender */
	web_msg->nick = g_strdup(nick);
	web_msg->text = g_strdup(msg);
	web_msg->level = MSGLEVEL_MSGS;
	web_msg->is_own = FALSE;

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message own_private" */
static void sig_message_own_private(IRC_SERVER_REC *server, const char *msg,
                                     const char *target, const char *orig_target)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_MESSAGE);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(target);
	web_msg->nick = g_strdup(server->nick);
	web_msg->text = g_strdup(msg);
	web_msg->level = MSGLEVEL_MSGS;
	web_msg->is_own = TRUE;

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message join" */
static void sig_message_join(IRC_SERVER_REC *server, const char *channel,
                              const char *nick, const char *address)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_CHANNEL_JOIN);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(channel);
	web_msg->nick = g_strdup(nick);

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message part" */
static void sig_message_part(IRC_SERVER_REC *server, const char *channel,
                              const char *nick, const char *address,
                              const char *reason)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_CHANNEL_PART);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(channel);
	web_msg->nick = g_strdup(nick);
	if (reason != NULL && *reason != '\0') {
		web_msg->text = g_strdup(reason);
	}

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message kick" */
static void sig_message_kick(IRC_SERVER_REC *server, const char *channel,
                              const char *nick, const char *kicker,
                              const char *address, const char *reason)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_CHANNEL_KICK);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(channel);
	web_msg->nick = g_strdup(nick);
	if (reason != NULL && *reason != '\0') {
		web_msg->text = g_strdup(reason);
	}

	/* Add kicker to extra_data */
	g_hash_table_insert(web_msg->extra_data, g_strdup("kicker"),
	                   g_strdup(kicker));

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message quit" */
static void sig_message_quit(IRC_SERVER_REC *server, const char *nick,
                              const char *address, const char *reason)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_USER_QUIT);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->nick = g_strdup(nick);
	if (reason != NULL && *reason != '\0') {
		web_msg->text = g_strdup(reason);
	}

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message topic" */
static void sig_message_topic(IRC_SERVER_REC *server, const char *channel,
                               const char *topic, const char *nick,
                               const char *address)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_TOPIC);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(channel);
	web_msg->nick = g_strdup(nick);
	web_msg->text = g_strdup(topic);

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message irc mode" */
static void sig_message_irc_mode(IRC_SERVER_REC *server, const char *channel,
                                  const char *nick, const char *address,
                                  const char *mode)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_CHANNEL_MODE);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(channel);
	web_msg->nick = g_strdup(nick);
	web_msg->text = g_strdup(mode);

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "nick mode changed" */
static void sig_nick_mode_changed(IRC_CHANNEL_REC *channel, NICK_REC *nick)
{
	WEB_MESSAGE_REC *web_msg;
	IRC_SERVER_REC *server;
	char mode_str[32];

	if (channel == NULL || nick == NULL) {
		return;
	}

	server = IRC_SERVER(channel->server);
	if (server == NULL) {
		return;
	}

	/* Build mode string */
	mode_str[0] = '\0';
	if (nick->op) {
		strcat(mode_str, "@");
	}
	if (nick->halfop) {
		strcat(mode_str, "%");
	}
	if (nick->voice) {
		strcat(mode_str, "+");
	}

	web_msg = fe_web_message_new(WEB_MSG_CHANNEL_MODE);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->target = g_strdup(channel->name);
	web_msg->nick = g_strdup(nick->nick);
	web_msg->text = g_strdup(mode_str);

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "message nick" */
static void sig_message_nick(IRC_SERVER_REC *server, const char *newnick,
                              const char *oldnick, const char *address)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_NICK_CHANGE);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->nick = g_strdup(oldnick);
	web_msg->text = g_strdup(newnick);

	fe_web_send_to_server_clients(server, web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "server connected" */
static void sig_server_connected(IRC_SERVER_REC *server)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_SERVER_STATUS);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->text = g_strdup("connected");

	fe_web_send_to_all_clients(web_msg);
	fe_web_message_free(web_msg);
}

/* Signal: "server disconnected" */
static void sig_server_disconnected(IRC_SERVER_REC *server)
{
	WEB_MESSAGE_REC *web_msg;

	if (server == NULL) {
		return;
	}

	web_msg = fe_web_message_new(WEB_MSG_SERVER_STATUS);
	web_msg->id = fe_web_generate_message_id();
	web_msg->server_tag = g_strdup(server->tag);
	web_msg->text = g_strdup("disconnected");

	fe_web_send_to_all_clients(web_msg);
	fe_web_message_free(web_msg);
}

/* Initialize signal handlers */
void fe_web_signals_init(void)
{
	/* Message signals */
	signal_add("message public", (SIGNAL_FUNC) sig_message_public);
	signal_add("message own_public", (SIGNAL_FUNC) sig_message_own_public);
	signal_add("message private", (SIGNAL_FUNC) sig_message_private);
	signal_add("message own_private", (SIGNAL_FUNC) sig_message_own_private);

	/* Channel events */
	signal_add("message join", (SIGNAL_FUNC) sig_message_join);
	signal_add("message part", (SIGNAL_FUNC) sig_message_part);
	signal_add("message kick", (SIGNAL_FUNC) sig_message_kick);
	signal_add("message quit", (SIGNAL_FUNC) sig_message_quit);

	/* Channel info */
	signal_add("message topic", (SIGNAL_FUNC) sig_message_topic);
	signal_add("message irc mode", (SIGNAL_FUNC) sig_message_irc_mode);
	signal_add("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_changed);

	/* Nick changes */
	signal_add("message nick", (SIGNAL_FUNC) sig_message_nick);

	/* Server events */
	signal_add("server connected", (SIGNAL_FUNC) sig_server_connected);
	signal_add("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
}

/* Deinitialize signal handlers */
void fe_web_signals_deinit(void)
{
	/* Message signals */
	signal_remove("message public", (SIGNAL_FUNC) sig_message_public);
	signal_remove("message own_public", (SIGNAL_FUNC) sig_message_own_public);
	signal_remove("message private", (SIGNAL_FUNC) sig_message_private);
	signal_remove("message own_private", (SIGNAL_FUNC) sig_message_own_private);

	/* Channel events */
	signal_remove("message join", (SIGNAL_FUNC) sig_message_join);
	signal_remove("message part", (SIGNAL_FUNC) sig_message_part);
	signal_remove("message kick", (SIGNAL_FUNC) sig_message_kick);
	signal_remove("message quit", (SIGNAL_FUNC) sig_message_quit);

	/* Channel info */
	signal_remove("message topic", (SIGNAL_FUNC) sig_message_topic);
	signal_remove("message irc mode", (SIGNAL_FUNC) sig_message_irc_mode);
	signal_remove("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_changed);

	/* Nick changes */
	signal_remove("message nick", (SIGNAL_FUNC) sig_message_nick);

	/* Server events */
	signal_remove("server connected", (SIGNAL_FUNC) sig_server_connected);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
}

/* Helper function to dump single server state */
static void fe_web_dump_server_state(WEB_CLIENT_REC *client, IRC_SERVER_REC *server)
{
	GSList *tmp;
	WEB_MESSAGE_REC *state_msg;

	if (server == NULL) {
		return;
	}

	/* Send state_dump marker message first */
	state_msg = fe_web_message_new(WEB_MSG_STATE_DUMP);
	state_msg->id = fe_web_generate_message_id();
	state_msg->server_tag = g_strdup(server->tag);
	fe_web_send_message(client, state_msg);
	fe_web_message_free(state_msg);

	/* Dump channels */
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		IRC_CHANNEL_REC *channel = tmp->data;
		WEB_MESSAGE_REC *msg;
		GString *nicklist;

		/* Send channel join */
		msg = fe_web_message_new(WEB_MSG_CHANNEL_JOIN);
		msg->id = fe_web_generate_message_id();
		msg->server_tag = g_strdup(server->tag);
		msg->target = g_strdup(channel->name);
		msg->nick = g_strdup(server->nick);
		fe_web_send_message(client, msg);
		fe_web_message_free(msg);

		/* Send topic */
		if (channel->topic != NULL && *channel->topic != '\0') {
			msg = fe_web_message_new(WEB_MSG_TOPIC);
			msg->id = fe_web_generate_message_id();
			msg->server_tag = g_strdup(server->tag);
			msg->target = g_strdup(channel->name);
			msg->text = g_strdup(channel->topic);
			fe_web_send_message(client, msg);
			fe_web_message_free(msg);
		}

		/* Send nicklist */
		msg = fe_web_message_new(WEB_MSG_NICKLIST);
		msg->id = fe_web_generate_message_id();
		msg->server_tag = g_strdup(server->tag);
		msg->target = g_strdup(channel->name);

		/* Build nicklist JSON */
		nicklist = g_string_new("[");
		{
			GSList *nicks;
			GSList *nick_tmp;

			nicks = nicklist_getnicks(CHANNEL(channel));
			for (nick_tmp = nicks; nick_tmp != NULL; nick_tmp = nick_tmp->next) {
				NICK_REC *nick = nick_tmp->data;
				char *escaped_nick;
				char prefix[8];

				if (nicklist->len > 1) {
					g_string_append_c(nicklist, ',');
				}

				/* Build prefix string (@, +, etc) */
				prefix[0] = '\0';
				if (nick->op) {
					strcat(prefix, "@");
				}
				if (nick->halfop) {
					strcat(prefix, "%");
				}
				if (nick->voice) {
					strcat(prefix, "+");
				}

				escaped_nick = fe_web_escape_json(nick->nick);
				g_string_append_printf(nicklist, "{\"nick\":\"%s\",\"prefix\":\"%s\"}",
				                      escaped_nick, prefix);
				g_free(escaped_nick);
			}
			g_slist_free(nicks);
		}
		g_string_append_c(nicklist, ']');

		msg->text = g_string_free(nicklist, FALSE);
		fe_web_send_message(client, msg);
		fe_web_message_free(msg);
	}
}

/* Dump current state to client */
void fe_web_dump_state(WEB_CLIENT_REC *client)
{
	IRC_SERVER_REC *server;
	GSList *tmp;
	extern GSList *servers;

	if (client == NULL) {
		return;
	}

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: [%s] Dumping state (all_servers: %d)",
	          client->id, client->wants_all_servers);

	/* If wants all servers, dump all */
	if (client->wants_all_servers) {
		int count = 0;
		for (tmp = servers; tmp != NULL; tmp = tmp->next) {
			server = IRC_SERVER(tmp->data);
			if (server != NULL && server->connected) {
				printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
				          "fe-web: [%s] Dumping server: %s",
				          client->id, server->tag);
				fe_web_dump_server_state(client, server);
				count++;
			}
		}
		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		          "fe-web: [%s] Dumped %d servers", client->id, count);
		return;
	}

	/* Dump specific server */
	server = client->server;
	if (server == NULL) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
		          "fe-web: [%s] ERROR: No server assigned for state dump",
		          client->id);
		return;
	}

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: [%s] Dumping server: %s",
	          client->id, server->tag);
	fe_web_dump_server_state(client, server);
	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: [%s] State dump completed", client->id);
}
