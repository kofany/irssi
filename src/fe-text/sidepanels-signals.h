#ifndef IRSSI_FE_TEXT_SIDEPANELS_SIGNALS_H
#define IRSSI_FE_TEXT_SIDEPANELS_SIGNALS_H

#include <glib.h>
#include <irssi/src/common.h>
#include <irssi/src/core/servers.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/core/queries.h>
#include <irssi/src/core/nicklist.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/fe-common/core/window-items.h>
#include <irssi/src/fe-text/mainwindows.h>
#include "sidepanels-types.h"

/* Window and item change handlers */
void sig_window_changed(WINDOW_REC *old, WINDOW_REC *new);
void sig_window_item_changed(WINDOW_REC *w, WI_ITEM_REC *item);
void sig_window_created(WINDOW_REC *window);
void sig_window_destroyed(WINDOW_REC *window);
void sig_window_item_new(WI_ITEM_REC *item);
void sig_query_created(QUERY_REC *query);

/* Channel handlers */
void sig_channel_joined(CHANNEL_REC *channel);
void sig_channel_sync(CHANNEL_REC *channel);
void sig_channel_wholist(CHANNEL_REC *channel);

/* Nicklist handlers */
void sig_nicklist_new(CHANNEL_REC *ch, NICK_REC *nick);
void sig_nicklist_changed(CHANNEL_REC *channel, NICK_REC *nick, const char *old_nick);
void sig_nicklist_remove(void);
void sig_nicklist_gone_changed(void);
void sig_nicklist_serverop_changed(void);
void sig_nicklist_host_changed(void);
void sig_nicklist_account_changed(void);

/* Message event handlers */
void sig_message_join(SERVER_REC *server, const char *channel, const char *nick,
                     const char *address, const char *account, const char *realname);
void sig_message_part(SERVER_REC *server, const char *channel, const char *nick,
                     const char *address, const char *reason);
void sig_message_quit(SERVER_REC *server, const char *nick, const char *address,
                     const char *reason);
void sig_message_nick(SERVER_REC *server, const char *newnick, const char *oldnick,
                     const char *address);
void sig_message_kick(void);
void sig_message_own_nick(void);

/* Mode change handlers */
void sig_nick_mode_filter(CHANNEL_REC *channel, NICK_REC *nick,
                         const char *setby, const char *modestr, const char *typestr);

/* Main window handlers */
void sig_mainwindow_resized(MAIN_WINDOW_REC *mw);

/* Signal registration/removal */
void sidepanels_signals_register(void);
void sidepanels_signals_unregister(void);

/* Initialization */
void sidepanels_signals_init(void);
void sidepanels_signals_deinit(void);

#endif
