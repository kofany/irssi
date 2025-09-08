#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/sidepanels.h>
#include <irssi/src/fe-text/sidepanels-signals.h>
#include <irssi/src/fe-text/sidepanels-render.h>
#include <irssi/src/fe-text/sidepanels-activity.h>
#include <irssi/src/core/servers.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/core/queries.h>
#include <irssi/src/core/nicklist.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/fe-common/core/window-items.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/fe-text/term.h>
#include <irssi/src/fe-common/core/formats.h>
#include <irssi/src/fe-text/module-formats.h>
#include <irssi/src/fe-common/core/themes.h>
#include <irssi/src/fe-text/gui-printtext.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <stdarg.h>
#include <stdlib.h>

/* External functions we need */
extern void sp_logf(const char *fmt, ...);
extern void update_left_selection_to_active(void);

void sig_window_changed(WINDOW_REC *old, WINDOW_REC *new)
{
	(void) old;

	/* Reset priority when user opens/switches to window */
	if (new) {
		/* Debug: Window switching - tracks user navigation */
		// sp_logf("SIGNAL: window_changed from %d to %d '%s' (USER SWITCHED)",
		//        old ? old->refnum : -1, new->refnum, item_name);
		reset_window_priority(new);
	}

	update_left_selection_to_active();
	redraw_both_panels_only("window_changed"); /* Window change affects both activity (left) and nicklist (right) */
}

void sig_window_item_changed(WINDOW_REC *w, WI_ITEM_REC *item)
{
	(void) w;
	(void) item;
	redraw_both_panels_only("window_item_changed");
}

void sig_window_created(WINDOW_REC *window)
{
	(void) window;
	redraw_both_panels_only("window_created"); /* Window creation affects both panels */
}

void sig_window_destroyed(WINDOW_REC *window)
{
	/* Clean up activity state for destroyed window */
	if (window && window_priorities) {
		g_hash_table_remove(window_priorities, window);
	}
	redraw_both_panels_only("window_destroyed"); /* Window destruction affects both panels */
}

void sig_window_item_new(WI_ITEM_REC *item)
{
	(void) item;
	redraw_both_panels_only("window_item_new");
}

void sig_query_created(QUERY_REC *query)
{
	(void) query;
	redraw_both_panels_only("query_created"); /* Query creation affects both panels */
}

void sig_channel_joined(CHANNEL_REC *channel)
{
	(void) channel;
	redraw_both_panels_only("channel_joined");
}

void sig_channel_sync(CHANNEL_REC *channel)
{
	(void) channel;
	redraw_right_panels_only("channel_sync");
}

void sig_channel_wholist(CHANNEL_REC *channel)
{
	(void) channel;
	redraw_right_panels_only("channel_wholist");
}

void sig_nicklist_new(CHANNEL_REC *ch, NICK_REC *nick)
{
	(void) ch;
	(void) nick;
	schedule_batched_redraw("nicklist_new");
}

void sig_nicklist_changed(CHANNEL_REC *channel, NICK_REC *nick, const char *old_nick)
{
	/* Nicklist changes don't trigger activity - just redraw right panels */
	(void) channel;
	(void) nick;
	(void) old_nick;
	schedule_batched_redraw("nicklist_changed");
}

/* Wrapper functions for signals that don't provide event names */
void sig_nicklist_remove(void) { schedule_batched_redraw("nicklist_remove"); }
void sig_nicklist_gone_changed(void) { schedule_batched_redraw("nicklist_gone_changed"); }
void sig_nicklist_serverop_changed(void) { schedule_batched_redraw("nicklist_serverop_changed"); }
void sig_nicklist_host_changed(void) { schedule_batched_redraw("nicklist_host_changed"); }
void sig_nicklist_account_changed(void) { schedule_batched_redraw("nicklist_account_changed"); }
void sig_message_kick(void) { schedule_batched_redraw("message_kick"); }
void sig_message_own_nick(void) { schedule_batched_redraw("message_own_nick"); }

/* Event signal handlers for join/part/quit/nick - priority 1 events */
void sig_message_join(SERVER_REC *server, const char *channel, const char *nick,
                             const char *address, const char *account, const char *realname)
{
	WINDOW_REC *window = window_find_item(server, channel);
	if (window) {
		/* Debug: Join events - tracks channel join activity */
		sp_logf("SIGNAL: message_join %s on %s (Window %d) active_win=%d", nick, channel,
		        window->refnum, active_win ? active_win->refnum : -1);
		handle_new_activity(window, DATA_LEVEL_EVENT);
	}
	redraw_both_panels_only("message_join"); /* Join affects both activity (left) and nicklist (right) */
}

void sig_message_part(SERVER_REC *server, const char *channel, const char *nick,
                             const char *address, const char *reason)
{
	WINDOW_REC *window = window_find_item(server, channel);
	if (window) {
		/* Debug: Part events - tracks channel part activity */
		sp_logf("SIGNAL: message_part %s from %s (Window %d) active_win=%d", nick, channel,
		        window->refnum, active_win ? active_win->refnum : -1);
		handle_new_activity(window, DATA_LEVEL_EVENT);
	}
	redraw_both_panels_only("message_part"); /* Part affects both activity (left) and nicklist (right) */
}

void sig_message_quit(SERVER_REC *server, const char *nick, const char *address,
                             const char *reason)
{
	/* Handle quit for all windows where this nick was present */
	GSList *tmp;
	for (tmp = windows; tmp != NULL; tmp = tmp->next) {
		WINDOW_REC *window = tmp->data;
		if (window->active && window->active->server == server) {
			handle_new_activity(window, DATA_LEVEL_EVENT);
		}
	}
	redraw_both_panels_only("message_quit"); /* Quit affects activity (left) and nicklists (right) across channels */
}

void sig_message_nick(SERVER_REC *server, const char *newnick, const char *oldnick,
                             const char *address)
{
	/* Handle nick change for all windows on this server */
	GSList *tmp;
	for (tmp = windows; tmp != NULL; tmp = tmp->next) {
		WINDOW_REC *window = tmp->data;
		if (window->active && window->active->server == server) {
			handle_new_activity(window, DATA_LEVEL_EVENT);
		}
	}
	redraw_both_panels_only("message_nick"); /* Nick change affects activity (left) and nicklists (right) */
}

void sig_nick_mode_filter(CHANNEL_REC *channel, NICK_REC *nick,
					  const char *setby, const char *modestr, const char *typestr)
{
	/* Only redraw for nick prefixes @ and + (op and voice) */
	(void) channel;
	(void) nick; 
	(void) setby;
	(void) typestr;
	
	if (modestr && (strchr(modestr, '@') || strchr(modestr, '+'))) {
		redraw_right_panels_only("nick_mode_changed");
	}
}

void sig_mainwindow_resized(MAIN_WINDOW_REC *mw)
{
	redraw_one(mw);
}

void sidepanels_signals_register(void)
{
	signal_add("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized);
	signal_add("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_add("window item changed", (SIGNAL_FUNC) sig_window_item_changed);
	signal_add("window created", (SIGNAL_FUNC) sig_window_created);
	signal_add("window destroyed", (SIGNAL_FUNC) sig_window_destroyed);
	signal_add("window item new", (SIGNAL_FUNC) sig_window_item_new);
	signal_add("query created", (SIGNAL_FUNC) sig_query_created);
	signal_add("channel joined", (SIGNAL_FUNC) sig_channel_joined);
	signal_add("nicklist changed", (SIGNAL_FUNC) sig_nicklist_changed);
	signal_add("nicklist new", (SIGNAL_FUNC) sig_nicklist_new);
	signal_add("nicklist remove", (SIGNAL_FUNC) sig_nicklist_remove);
	signal_add("nicklist gone changed", (SIGNAL_FUNC) sig_nicklist_gone_changed);
	signal_add("nicklist serverop changed", (SIGNAL_FUNC) sig_nicklist_serverop_changed);
	signal_add("nicklist host changed", (SIGNAL_FUNC) sig_nicklist_host_changed);
	signal_add("nicklist account changed", (SIGNAL_FUNC) sig_nicklist_account_changed);
	/* Message events for live nicklist updates */
	signal_add("message join", (SIGNAL_FUNC) sig_message_join);
	signal_add("message part", (SIGNAL_FUNC) sig_message_part);
	signal_add("message quit", (SIGNAL_FUNC) sig_message_quit);
	signal_add("message kick", (SIGNAL_FUNC) sig_message_kick);
	signal_add("message nick", (SIGNAL_FUNC) sig_message_nick);
	signal_add("message own_nick", (SIGNAL_FUNC) sig_message_own_nick);
	/* Channel state changes */
	signal_add("channel created", (SIGNAL_FUNC) redraw_both_panels_only); /* Affects both window list and nicklist */
	signal_add("channel destroyed", (SIGNAL_FUNC) redraw_both_panels_only); /* Affects both window list and nicklist */
	signal_add("channel topic changed", (SIGNAL_FUNC) redraw_left_panels_only);
	signal_add("channel sync", (SIGNAL_FUNC) sig_channel_sync);
	signal_add("channel wholist", (SIGNAL_FUNC) sig_channel_wholist);
	/* Mode changes affecting nicklist */
	signal_add("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_filter);
	/* Live activity notifications */
	signal_add("print text", (SIGNAL_FUNC) sig_print_text);
}

void sidepanels_signals_unregister(void)
{
	signal_remove("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized);
	signal_remove("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_remove("window item changed", (SIGNAL_FUNC) sig_window_item_changed);
	signal_remove("window created", (SIGNAL_FUNC) sig_window_created);
	signal_remove("window destroyed", (SIGNAL_FUNC) sig_window_destroyed);
	signal_remove("window item new", (SIGNAL_FUNC) sig_window_item_new);
	signal_remove("query created", (SIGNAL_FUNC) sig_query_created);
	signal_remove("channel joined", (SIGNAL_FUNC) sig_channel_joined);
	signal_remove("nicklist changed", (SIGNAL_FUNC) sig_nicklist_changed);
	signal_remove("nicklist new", (SIGNAL_FUNC) sig_nicklist_new);
	signal_remove("nicklist remove", (SIGNAL_FUNC) sig_nicklist_remove);
	signal_remove("nicklist gone changed", (SIGNAL_FUNC) sig_nicklist_gone_changed);
	signal_remove("nicklist serverop changed", (SIGNAL_FUNC) sig_nicklist_serverop_changed);
	signal_remove("nicklist host changed", (SIGNAL_FUNC) sig_nicklist_host_changed);
	signal_remove("nicklist account changed", (SIGNAL_FUNC) sig_nicklist_account_changed);
	/* Message events for live nicklist updates */
	signal_remove("message join", (SIGNAL_FUNC) sig_message_join);
	signal_remove("message part", (SIGNAL_FUNC) sig_message_part);
	signal_remove("message quit", (SIGNAL_FUNC) sig_message_quit);
	signal_remove("message kick", (SIGNAL_FUNC) sig_message_kick);
	signal_remove("message nick", (SIGNAL_FUNC) sig_message_nick);
	signal_remove("message own_nick", (SIGNAL_FUNC) sig_message_own_nick);
	/* Channel state changes */
	signal_remove("channel created", (SIGNAL_FUNC) redraw_both_panels_only);
	signal_remove("channel destroyed", (SIGNAL_FUNC) redraw_both_panels_only);
	signal_remove("channel topic changed", (SIGNAL_FUNC) redraw_left_panels_only);
	signal_remove("channel sync", (SIGNAL_FUNC) sig_channel_sync);
	signal_remove("channel wholist", (SIGNAL_FUNC) sig_channel_wholist);
	/* Mode changes affecting nicklist */
	signal_remove("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_filter);
	/* Live activity notifications */
	signal_remove("print text", (SIGNAL_FUNC) sig_print_text);
}

void sidepanels_signals_init(void)
{
	/* Nothing to initialize */
}

void sidepanels_signals_deinit(void)
{
	/* Nothing to clean up */
}
