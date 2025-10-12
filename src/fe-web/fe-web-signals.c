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
#include <irssi/src/core/queries.h>
#include <irssi/src/core/misc.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/irc/core/irc.h>
#include <irssi/src/irc/core/irc-servers.h>
#include <irssi/src/irc/core/irc-channels.h>
#include <irssi/src/irc/core/irc-nicklist.h>
/* Forward declarations for WHOIS handlers used in dispatch table */
static void event_whois(IRC_SERVER_REC *server, const char *data);
static void event_whois_server(IRC_SERVER_REC *server, const char *data);
static void event_whois_idle(IRC_SERVER_REC *server, const char *data);
static void event_whois_channels(IRC_SERVER_REC *server, const char *data);
static void event_whois_account(IRC_SERVER_REC *server, const char *data);
static void event_whois_secure(IRC_SERVER_REC *server, const char *data);

/* fe-web WHOIS event dispatch table (file-scope) */
typedef void (*FEWEB_WHOIS_HANDLER)(IRC_SERVER_REC *server, const char *data);
static struct { int num; FEWEB_WHOIS_HANDLER func; } feweb_whois_events[] = {
	{ 311, event_whois },
	{ 312, event_whois_server },
	{ 317, event_whois_idle },
	{ 319, event_whois_channels },
	{ 330, event_whois_account },
	{ 671, event_whois_secure },
	{ 0, NULL }
};


/* Global hash table for tracking active WHOIS requests */
/* Key: "server_tag:nick", Value: WHOIS_REC* */
static GHashTable *active_whois = NULL;

/* Helper: Create WHOIS key */
static char *whois_key(IRC_SERVER_REC *server, const char *nick)
{
	return g_strdup_printf("%s:%s", server->tag, nick);
}

/* Helper: Create new WHOIS record */
static WHOIS_REC *whois_rec_new(const char *nick)
{
	WHOIS_REC *rec = g_new0(WHOIS_REC, 1);
	rec->nick = g_strdup(nick);
	rec->timestamp = time(NULL);
	return rec;
}

/* Helper: Free WHOIS record */
static void whois_rec_free(WHOIS_REC *rec)
{
	if (rec == NULL)
		return;

	g_free(rec->nick);
	g_free(rec->user);
	g_free(rec->host);
	g_free(rec->realname);
	g_free(rec->server);
	g_free(rec->server_info);
	g_free(rec->idle);
	g_free(rec->signon);
	g_free(rec->channels);
	g_free(rec->account);

	/* Free special list */
	if (rec->special != NULL) {
		g_slist_free_full(rec->special, g_free);
	}

	g_free(rec);
}

/* Helper: Get or create WHOIS record */
static WHOIS_REC *whois_get_or_create(IRC_SERVER_REC *server, const char *nick)
{
	char *key;
	WHOIS_REC *rec;

	if (active_whois == NULL)
		return NULL;

	key = whois_key(server, nick);
	rec = g_hash_table_lookup(active_whois, key);

	if (rec == NULL) {
		rec = whois_rec_new(nick);
		g_hash_table_insert(active_whois, key, rec);
	} else {
		g_free(key);
	}

	return rec;
}

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

/* Signal: "channel joined" - YOU joined a channel */
static void sig_channel_joined(IRC_CHANNEL_REC *channel)
{
	IRC_SERVER_REC *server;
	WEB_MESSAGE_REC *msg;
	GString *nicklist;

	if (channel == NULL) {
		return;
	}

	server = IRC_SERVER(channel->server);
	if (server == NULL) {
		return;
	}

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: YOU joined %s on %s - sending nicklist",
	          channel->name, server->tag);

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
	fe_web_send_to_server_clients(server, msg);
	fe_web_message_free(msg);

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: Sent nicklist for %s (%d users)",
	          channel->name, g_slist_length(nicklist_getnicks(CHANNEL(channel))));
}

/* Signal: "query created" - Query window opened */
static void sig_query_created(QUERY_REC *query, gpointer automatic)
{
	IRC_SERVER_REC *server;
	WEB_MESSAGE_REC *msg;

	if (query == NULL) {
		return;
	}

	server = IRC_SERVER(query->server);
	if (server == NULL) {
		return;
	}

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: Query opened with %s on %s (automatic: %d)",
	          query->name, server->tag, GPOINTER_TO_INT(automatic));

	/* Send query_opened event */
	msg = fe_web_message_new(WEB_MSG_QUERY_OPENED);
	msg->id = fe_web_generate_message_id();
	msg->server_tag = g_strdup(server->tag);
	msg->nick = g_strdup(query->name);

	fe_web_send_to_server_clients(server, msg);
	fe_web_message_free(msg);

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: Sent query_opened for %s", query->name);
}

/* Signal: "query destroyed" - Query window closed */
static void sig_query_destroyed(QUERY_REC *query)
{
	IRC_SERVER_REC *server;
	WEB_MESSAGE_REC *msg;

	if (query == NULL) {
		return;
	}

	server = IRC_SERVER(query->server);
	if (server == NULL) {
		return;
	}

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: Query closed with %s on %s",
	          query->name, server->tag);

	/* Send query_closed event */
	msg = fe_web_message_new(WEB_MSG_QUERY_CLOSED);
	msg->id = fe_web_generate_message_id();
	msg->server_tag = g_strdup(server->tag);
	msg->nick = g_strdup(query->name);

	fe_web_send_to_server_clients(server, msg);
	fe_web_message_free(msg);

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: Sent query_closed for %s", query->name);
}

/* Signal: "event 311" - WHOIS user/host */
static void event_whois(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *user, *host, *realname;
	WHOIS_REC *rec;

	if (server == NULL || data == NULL)
		return;

	params = event_get_params(data, 6, NULL, &nick, &user,
	                          &host, NULL, &realname);

	rec = whois_get_or_create(server, nick);
	if (rec != NULL) {
		g_free(rec->user);
		g_free(rec->host);
		g_free(rec->realname);
		rec->user = g_strdup(user);
		rec->host = g_strdup(host);
		rec->realname = g_strdup(realname);
	}

	g_free(params);
}

/* Signal: "event 312" - WHOIS server */
static void event_whois_server(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *whoserver, *desc;
	WHOIS_REC *rec;

	if (server == NULL || data == NULL)
		return;

	params = event_get_params(data, 4, NULL, &nick, &whoserver, &desc);

	rec = whois_get_or_create(server, nick);
	if (rec != NULL) {
		g_free(rec->server);
		g_free(rec->server_info);
		rec->server = g_strdup(whoserver);
		rec->server_info = g_strdup(desc);

		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		          "fe-web: WHOIS server for %s: %s [%s]", nick, whoserver, desc);
	}

	g_free(params);
}

/* Signal: "event 317" - WHOIS idle */
static void event_whois_idle(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *secstr, *signonstr, *rest;
	WHOIS_REC *rec;

	if (server == NULL || data == NULL)
		return;

	params = event_get_params(data, 5 | PARAM_FLAG_GETREST, NULL,
	                          &nick, &secstr, &signonstr, &rest);

	rec = whois_get_or_create(server, nick);
	if (rec != NULL) {
		g_free(rec->idle);
		g_free(rec->signon);
		rec->idle = g_strdup(secstr);
		/* Only set signon if "signon time" is in rest */
		if (strstr(rest, "signon time") != NULL) {
			rec->signon = g_strdup(signonstr);
		}

		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		          "fe-web: WHOIS idle for %s: idle=%s signon=%s", nick, secstr,
		          rec->signon ? rec->signon : "none");
	}

	g_free(params);
}

/* Signal: "event 319" - WHOIS channels */
static void event_whois_channels(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *chans;
	WHOIS_REC *rec;

	if (server == NULL || data == NULL)
		return;

	params = event_get_params(data, 3, NULL, &nick, &chans);

	rec = whois_get_or_create(server, nick);
	if (rec != NULL) {
		g_free(rec->channels);
		rec->channels = g_strdup(chans);

		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		          "fe-web: WHOIS channels for %s: %s", nick, chans);
	}

	g_free(params);
}

/* Signal: "event 330" - WHOIS account */
static void event_whois_account(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *account;
	WHOIS_REC *rec;

	if (server == NULL || data == NULL)
		return;

	params = event_get_params(data, 3, NULL, &nick, &account);

	rec = whois_get_or_create(server, nick);
	if (rec != NULL) {
		g_free(rec->account);
		rec->account = g_strdup(account);

		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		          "fe-web: WHOIS account for %s: %s", nick, account);
	}

	g_free(params);
}

/* Signal: "event 671" - WHOIS secure connection */
static void event_whois_secure(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick;
	WHOIS_REC *rec;

	if (server == NULL || data == NULL)
		return;

	params = event_get_params(data, 2, NULL, &nick);

	rec = whois_get_or_create(server, nick);
	if (rec != NULL) {
		rec->secure = TRUE;

		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		          "fe-web: WHOIS secure for %s: true", nick);
	}

	g_free(params);
}

/* Signal: "user mode changed" - User mode change */
static void sig_user_mode_changed(IRC_SERVER_REC *server, const char *oldmode)
{
	WEB_MESSAGE_REC *msg;

	if (server == NULL || server->usermode == NULL)
		return;

	msg = fe_web_message_new(WEB_MSG_USER_MODE);
	msg->id = fe_web_generate_message_id();
	msg->server_tag = g_strdup(server->tag);
	msg->nick = g_strdup(server->nick);
	msg->text = g_strdup(server->usermode);

	fe_web_send_to_server_clients(server, msg);
	fe_web_message_free(msg);
}

/* Signal: "event 301" - AWAY status */
static void event_away_status(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *awaymsg;
	WEB_MESSAGE_REC *msg;

	if (server == NULL || data == NULL)
	return;

	params = event_get_params(data, 3, NULL, &nick, &awaymsg);

	msg = fe_web_message_new(WEB_MSG_AWAY);
	msg->id = fe_web_generate_message_id();
	msg->server_tag = g_strdup(server->tag);
	msg->nick = g_strdup(nick);
	msg->text = g_strdup(awaymsg);

	fe_web_send_to_server_clients(server, msg);
	fe_web_message_free(msg);

	g_free(params);
}

/* Signal: "whois default event" - Catch-all for non-standard WHOIS events */
static void event_whois_default(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *text;
	WHOIS_REC *rec;
	int num;

	if (server == NULL || data == NULL)
		return;

	/* Get event number from current_server_event */
num = atoi(current_server_event);

	/* Dispatch standard WHOIS numerics via our handlers (redirect sends them here) */
	for (int i = 0; feweb_whois_events[i].num != 0; i++) {
		if (feweb_whois_events[i].num == num) {
			feweb_whois_events[i].func(server, data);
			return;
		}
	}

	params = event_get_params(data, 3 | PARAM_FLAG_GETREST, NULL, &nick, &text);

	rec = whois_get_or_create(server, nick);
	if (rec != NULL && text != NULL && *text != '\0') {
		/* Add to special list - only non-standard events */
		rec->special = g_slist_append(rec->special, g_strdup(text));

		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		          "fe-web: WHOIS special (event %d) for %s: %s", num, nick, text);
	}

	g_free(params);
}

/* Signal: "event 318" - End of WHOIS */
static void event_end_of_whois(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *key;
	WHOIS_REC *rec;
	WEB_MESSAGE_REC *msg;

	if (server == NULL || data == NULL)
		return;

	params = event_get_params(data, 2, NULL, &nick);
	key = whois_key(server, nick);

	rec = g_hash_table_lookup(active_whois, key);
	/* Check if we have any WHOIS data (don't rely on whois_found flag) */
	if (rec != NULL && (rec->user != NULL || rec->channels != NULL ||
	                    rec->idle != NULL || rec->account != NULL ||
	                    rec->special != NULL)) {
		/* Send WHOIS message to clients */
		msg = fe_web_message_new(WEB_MSG_WHOIS);
		msg->id = fe_web_generate_message_id();
		msg->server_tag = g_strdup(server->tag);
		msg->nick = g_strdup(rec->nick);

		/* Add extra data */
		if (rec->user != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("user"),
			                   g_strdup(rec->user));
		if (rec->host != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("host"),
			                   g_strdup(rec->host));
		if (rec->realname != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("realname"),
			                   g_strdup(rec->realname));
		if (rec->server != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("server"),
			                   g_strdup(rec->server));
		if (rec->server_info != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("server_info"),
			                   g_strdup(rec->server_info));
		if (rec->channels != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("channels"),
			                   g_strdup(rec->channels));
		if (rec->idle != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("idle"),
			                   g_strdup(rec->idle));
		if (rec->signon != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("signon"),
			                   g_strdup(rec->signon));
		if (rec->account != NULL)
			g_hash_table_insert(msg->extra_data, g_strdup("account"),
			                   g_strdup(rec->account));
		if (rec->secure)
			g_hash_table_insert(msg->extra_data, g_strdup("secure"),
			                   g_strdup("true"));

		/* Add special WHOIS lines as JSON array */
		if (rec->special != NULL) {
			GString *special_json = g_string_new("[");
			GSList *tmp;
			gboolean first = TRUE;

			for (tmp = rec->special; tmp != NULL; tmp = tmp->next) {
				char *escaped;
				if (!first) g_string_append(special_json, ",");
				escaped = fe_web_escape_json((char *)tmp->data);
				g_string_append_printf(special_json, "\"%s\"", escaped);
				g_free(escaped);
				first = FALSE;
			}
			g_string_append(special_json, "]");

			g_hash_table_insert(msg->extra_data, g_strdup("special"),
			                   g_string_free(special_json, FALSE));
		}

		fe_web_send_to_server_clients(server, msg);
		fe_web_message_free(msg);

		printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		          "fe-web: Sent WHOIS response for %s on %s",
		          nick, server->tag);

		/* Remove from active_whois */
		g_hash_table_remove(active_whois, key);
	}

	g_free(key);
	g_free(params);
}

/* Initialize signal handlers */
void fe_web_signals_init(void)
{
	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: Initializing signal handlers (including WHOIS handlers)");

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

	/* Channel lifecycle (YOU joined/parted) */
	signal_add("channel joined", (SIGNAL_FUNC) sig_channel_joined);

	/* Query lifecycle */
	signal_add("query created", (SIGNAL_FUNC) sig_query_created);
	signal_add("query destroyed", (SIGNAL_FUNC) sig_query_destroyed);

	/* WHOIS events - use signal_add_last to run after fe-common/irc handlers */
	signal_add_last("whois event", (SIGNAL_FUNC) event_whois);
	signal_add_last("event 311", (SIGNAL_FUNC) event_whois);
	signal_add_last("event 312", (SIGNAL_FUNC) event_whois_server);
	signal_add_last("event 317", (SIGNAL_FUNC) event_whois_idle);
	signal_add_last("event 319", (SIGNAL_FUNC) event_whois_channels);
	signal_add_last("event 330", (SIGNAL_FUNC) event_whois_account);
	signal_add_last("whois account", (SIGNAL_FUNC) event_whois_account);
	signal_add_last("event 671", (SIGNAL_FUNC) event_whois_secure);
	signal_add_last("whois default event", (SIGNAL_FUNC) event_whois_default);
	signal_add_last("whois end", (SIGNAL_FUNC) event_end_of_whois);
	signal_add_last("event 318", (SIGNAL_FUNC) event_end_of_whois);

	/* User mode and away */
	signal_add("user mode changed", (SIGNAL_FUNC) sig_user_mode_changed);
	signal_add("event 301", (SIGNAL_FUNC) event_away_status);

	/* Initialize active_whois hash table */
	active_whois = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                      g_free, (GDestroyNotify) whois_rec_free);
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

	/* Channel lifecycle */
	signal_remove("channel joined", (SIGNAL_FUNC) sig_channel_joined);

	/* Query lifecycle */
	signal_remove("query created", (SIGNAL_FUNC) sig_query_created);
	signal_remove("query destroyed", (SIGNAL_FUNC) sig_query_destroyed);

	/* WHOIS events */
	signal_remove("whois event", (SIGNAL_FUNC) event_whois);
	signal_remove("event 311", (SIGNAL_FUNC) event_whois);
	signal_remove("event 312", (SIGNAL_FUNC) event_whois_server);
	signal_remove("event 317", (SIGNAL_FUNC) event_whois_idle);
	signal_remove("event 319", (SIGNAL_FUNC) event_whois_channels);
	signal_remove("event 330", (SIGNAL_FUNC) event_whois_account);
	signal_remove("whois account", (SIGNAL_FUNC) event_whois_account);
	signal_remove("event 671", (SIGNAL_FUNC) event_whois_secure);
	signal_remove("whois default event", (SIGNAL_FUNC) event_whois_default);
	signal_remove("whois end", (SIGNAL_FUNC) event_end_of_whois);
	signal_remove("event 318", (SIGNAL_FUNC) event_end_of_whois);

	/* User mode and away */
	signal_remove("user mode changed", (SIGNAL_FUNC) sig_user_mode_changed);
	signal_remove("event 301", (SIGNAL_FUNC) event_away_status);

	/* Cleanup active_whois hash table */
	if (active_whois != NULL) {
		g_hash_table_destroy(active_whois);
		active_whois = NULL;
	}
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
