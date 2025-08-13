#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/commands.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/core/servers.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/core/queries.h>
#include <irssi/src/core/nicklist.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/fe-common/core/window-items.h>
#include <irssi/src/fe-common/core/themes.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/term.h>
#include <irssi/src/fe-text/module-formats.h>
#include <irssi/src/fe-text/gui-printtext.h>

/* Settings */
static int sp_left_width;
static int sp_right_width;
static int sp_enable_left;
static int sp_enable_right;

/* Channel list item */
typedef struct {
	char *name;              /* Channel/query name */
	SERVER_REC *server;      /* Associated server */
	WINDOW_REC *window;      /* Associated window */
	WI_ITEM_REC *item;       /* Window item (channel/query) */
	int activity_level;      /* Activity indicator level */
	gboolean is_active;      /* Currently active window */
} ChannelListItem;

/* Nicklist item */
typedef struct {
	char *nick;              /* Nickname */
	char *prefix;            /* Mode prefix (@, +, etc.) */
	int level;               /* User level/mode */
	gboolean is_away;        /* Away status */
	NICK_REC *nick_rec;      /* Original nick record */
} NicklistItem;

/* Panel context per main window */
typedef struct {
	/* Selection and scroll state */
	int left_selected_index;
	int left_scroll_offset;
	int right_selected_index;
	int right_scroll_offset;
	
	/* Channel list data */
	GList *channel_list;     /* List of ChannelListItem */
	gboolean channel_list_dirty;
	
	/* Nicklist data */
	GList *nicklist;         /* List of NicklistItem */
	gboolean nicklist_dirty;
	CHANNEL_REC *nicklist_channel; /* Current channel for nicklist */
	
	/* Cached content for dirty checking */
	char **left_content_cache;
	char **right_content_cache;
	int left_content_lines;
	int right_content_lines;
} SP_MAINWIN_CTX;

static GHashTable *mw_to_ctx;

/* Forward declarations */
static void sidepanels_redraw_all(MAIN_WINDOW_REC *mw);
static void adjust_panel_widths_for_terminal_size(void);
static void update_left_selection(SP_MAINWIN_CTX *ctx, int new_selection);
static void update_right_selection(SP_MAINWIN_CTX *ctx, int new_selection);
static void adjust_left_scroll_to_selection(SP_MAINWIN_CTX *ctx, MAIN_WINDOW_REC *mw);
static void adjust_right_scroll_to_selection(SP_MAINWIN_CTX *ctx, MAIN_WINDOW_REC *mw);
static void scroll_left_panel(SP_MAINWIN_CTX *ctx, MAIN_WINDOW_REC *mw, int delta);
static void scroll_right_panel(SP_MAINWIN_CTX *ctx, MAIN_WINDOW_REC *mw, int delta);

/* Nicklist management */
static void free_nicklist_item(NicklistItem *item)
{
	if (!item) return;
	g_free(item->nick);
	g_free(item->prefix);
	g_free(item);
}

static void clear_nicklist(SP_MAINWIN_CTX *ctx)
{
	if (!ctx->nicklist) return;
	
	g_list_foreach(ctx->nicklist, (GFunc) free_nicklist_item, NULL);
	g_list_free(ctx->nicklist);
	ctx->nicklist = NULL;
	ctx->nicklist_dirty = TRUE;
	ctx->nicklist_channel = NULL;
}

static void populate_nicklist(SP_MAINWIN_CTX *ctx)
{
	WINDOW_REC *active_window = active_win;
	WI_ITEM_REC *item;
	CHANNEL_REC *channel;
	GSList *nicks, *tmp;
	char *selected_nick = NULL;
	int old_selection = ctx->right_selected_index;
	
	/* Remember currently selected nick */
	if (ctx->nicklist && old_selection >= 0) {
		NicklistItem *selected_item = g_list_nth_data(ctx->nicklist, old_selection);
		if (selected_item) {
			selected_nick = g_strdup(selected_item->nick);
		}
	}
	
	/* Clear existing nicklist */
	clear_nicklist(ctx);
	
	/* Get active window item */
	if (!active_window || !active_window->active) return;
	item = active_window->active;
	
	/* Only show nicklist for channels */
	if (!IS_CHANNEL(item)) return;
	channel = CHANNEL(item);
	
	/* Skip disconnected servers */
	if (!channel->server || !channel->server->connected) return;
	
	/* Store current channel */
	ctx->nicklist_channel = channel;
	
	/* Get sorted nicklist */
	nicks = nicklist_getnicks(channel);
	
	/* Populate nicklist */
	for (tmp = nicks; tmp != NULL; tmp = tmp->next) {
		NICK_REC *nick = tmp->data;
		NicklistItem *list_item;
		char prefix[10] = "";
		
		/* Build prefix string */
		if (nick->op) strcat(prefix, "@");
		if (nick->halfop) strcat(prefix, "%");
		if (nick->voice) strcat(prefix, "+");
		
		/* Create nicklist item */
		list_item = g_new0(NicklistItem, 1);
		list_item->nick = g_strdup(nick->nick);
		list_item->prefix = g_strdup(prefix);
		list_item->level = nick->op ? 4 : (nick->halfop ? 3 : (nick->voice ? 2 : 1));
		list_item->is_away = nick->gone;
		list_item->nick_rec = nick;
		
		ctx->nicklist = g_list_append(ctx->nicklist, list_item);
	}
	
	g_slist_free(nicks);
	
	/* Restore selection if possible */
	if (selected_nick) {
		GList *item_tmp;
		int index = 0;
		gboolean found = FALSE;
		for (item_tmp = ctx->nicklist; item_tmp != NULL; item_tmp = item_tmp->next) {
			NicklistItem *item = item_tmp->data;
			if (g_strcmp0(item->nick, selected_nick) == 0) {
				ctx->right_selected_index = index;
				found = TRUE;
				break;
			}
			index++;
		}
		g_free(selected_nick);
		
		/* If previously selected nick not found, try to keep similar position */
		if (!found && old_selection >= 0) {
			int list_length = g_list_length(ctx->nicklist);
			if (list_length > 0) {
				/* Keep selection at similar relative position */
				ctx->right_selected_index = (old_selection < list_length) ? old_selection : list_length - 1;
			}
		}
	}
	
	/* If selection is invalid, reset to first item */
	if (ctx->right_selected_index >= g_list_length(ctx->nicklist)) {
		ctx->right_selected_index = ctx->nicklist ? 0 : -1;
	}
	
	/* Ensure scroll offset is still valid after list changes */
	if (ctx->nicklist) {
		int list_length = g_list_length(ctx->nicklist);
		if (ctx->right_scroll_offset >= list_length) {
			ctx->right_scroll_offset = (list_length > 0) ? list_length - 1 : 0;
		}
	} else {
		ctx->right_scroll_offset = 0;
	}
	
	ctx->nicklist_dirty = TRUE;
}

/* Channel list management */
static void free_channel_list_item(ChannelListItem *item)
{
	if (!item) return;
	g_free(item->name);
	g_free(item);
}

static void clear_channel_list(SP_MAINWIN_CTX *ctx)
{
	if (!ctx->channel_list) return;
	
	g_list_foreach(ctx->channel_list, (GFunc) free_channel_list_item, NULL);
	g_list_free(ctx->channel_list);
	ctx->channel_list = NULL;
	ctx->channel_list_dirty = TRUE;
}

static void populate_channel_list(SP_MAINWIN_CTX *ctx)
{
	GSList *tmp;
	WINDOW_REC *active_window = active_win;
	char *selected_name = NULL;
	int old_selection = ctx->left_selected_index;
	
	/* Remember currently selected item name */
	if (ctx->channel_list && old_selection >= 0) {
		ChannelListItem *selected_item = g_list_nth_data(ctx->channel_list, old_selection);
		if (selected_item) {
			selected_name = g_strdup(selected_item->name);
		}
	}
	
	/* Clear existing list */
	clear_channel_list(ctx);
	
	/* Iterate through all windows and collect channels/queries */
	for (tmp = windows; tmp != NULL; tmp = tmp->next) {
		WINDOW_REC *window = tmp->data;
		WI_ITEM_REC *item;
		ChannelListItem *list_item;
		
		/* Skip windows without items */
		if (!window->items) continue;
		
		/* Get the active item in this window */
		item = window->active;
		if (!item) continue;
		
		/* Skip items with disconnected servers */
		if (item->server && !item->server->connected) continue;
		
		/* Create channel list item */
		list_item = g_new0(ChannelListItem, 1);
		list_item->name = g_strdup(item->visible_name ? item->visible_name : item->name);
		list_item->server = item->server;
		list_item->window = window;
		list_item->item = item;
		list_item->activity_level = item->data_level;
		list_item->is_active = (window == active_window);
		
		ctx->channel_list = g_list_append(ctx->channel_list, list_item);
	}
	
	/* Restore selection if possible */
	if (selected_name) {
		GList *item_tmp;
		int index = 0;
		gboolean found = FALSE;
		for (item_tmp = ctx->channel_list; item_tmp != NULL; item_tmp = item_tmp->next) {
			ChannelListItem *item = item_tmp->data;
			if (g_strcmp0(item->name, selected_name) == 0) {
				ctx->left_selected_index = index;
				found = TRUE;
				break;
			}
			index++;
		}
		g_free(selected_name);
		
		/* If previously selected item not found, try to keep similar position */
		if (!found && old_selection >= 0) {
			int list_length = g_list_length(ctx->channel_list);
			if (list_length > 0) {
				/* Keep selection at similar relative position */
				ctx->left_selected_index = (old_selection < list_length) ? old_selection : list_length - 1;
			}
		}
	}
	
	/* If selection is invalid, reset to first item */
	if (ctx->left_selected_index >= g_list_length(ctx->channel_list)) {
		ctx->left_selected_index = ctx->channel_list ? 0 : -1;
	}
	
	/* Ensure scroll offset is still valid after list changes */
	if (ctx->channel_list) {
		int list_length = g_list_length(ctx->channel_list);
		if (ctx->left_scroll_offset >= list_length) {
			ctx->left_scroll_offset = (list_length > 0) ? list_length - 1 : 0;
		}
	} else {
		ctx->left_scroll_offset = 0;
	}
	
	ctx->channel_list_dirty = TRUE;
}

static void read_settings(void)
{
	int left_width, right_width;
	
	left_width = settings_get_int("sidepanel_left_width");
	right_width = settings_get_int("sidepanel_right_width");
	
	/* Validate width settings */
	if (left_width < 0 || left_width > 50) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, 
			"Invalid sidepanel_left_width %d, using default 14", left_width);
		left_width = 14;
		settings_set_int("sidepanel_left_width", left_width);
	}
	if (right_width < 0 || right_width > 50) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, 
			"Invalid sidepanel_right_width %d, using default 16", right_width);
		right_width = 16;
		settings_set_int("sidepanel_right_width", right_width);
	}
	
	sp_left_width = left_width;
	sp_right_width = right_width;
	sp_enable_left = settings_get_bool("sidepanel_left");
	sp_enable_right = settings_get_bool("sidepanel_right");
}

static SP_MAINWIN_CTX *get_ctx(MAIN_WINDOW_REC *mw, gboolean create)
{
	SP_MAINWIN_CTX *ctx = g_hash_table_lookup(mw_to_ctx, mw);
	if (!ctx && create) {
		ctx = g_new0(SP_MAINWIN_CTX, 1);
		ctx->left_selected_index = -1;
		ctx->left_scroll_offset = 0;
		ctx->right_selected_index = -1;
		ctx->right_scroll_offset = 0;
		ctx->channel_list = NULL;
		ctx->channel_list_dirty = TRUE;
		ctx->nicklist = NULL;
		ctx->nicklist_dirty = TRUE;
		ctx->nicklist_channel = NULL;
		ctx->left_content_cache = NULL;
		ctx->right_content_cache = NULL;
		ctx->left_content_lines = 0;
		ctx->right_content_lines = 0;
		g_hash_table_insert(mw_to_ctx, mw, ctx);
		
		/* Populate initial data */
		populate_channel_list(ctx);
		populate_nicklist(ctx);
	}
	return ctx;
}

static void destroy_ctx(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = g_hash_table_lookup(mw_to_ctx, mw);
	if (!ctx) return;
	
	/* Free channel list and nicklist */
	clear_channel_list(ctx);
	clear_nicklist(ctx);
	
	/* Free cached content */
	if (ctx->left_content_cache) {
		int i;
		for (i = 0; i < ctx->left_content_lines; i++) {
			g_free(ctx->left_content_cache[i]);
		}
		g_free(ctx->left_content_cache);
	}
	if (ctx->right_content_cache) {
		int i;
		for (i = 0; i < ctx->right_content_lines; i++) {
			g_free(ctx->right_content_cache[i]);
		}
		g_free(ctx->right_content_cache);
	}
	
	g_hash_table_remove(mw_to_ctx, mw);
	g_free(ctx);
}

static void apply_reservations_all(void)
{
	GSList *tmp;
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		
		/* Create context for this mainwindow if it doesn't exist */
		get_ctx(mw, TRUE);
		
		/* Reset previous reservations if any by setting negative, then apply new */
		if (mw->statusbar_columns_left)
			mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, 0);
		if (mw->statusbar_columns_right)
			mainwindow_set_statusbar_columns(mw, 0, -mw->statusbar_columns_right);
		if (sp_enable_left && sp_left_width > 0)
			mainwindow_set_statusbar_columns(mw, sp_left_width, 0);
		if (sp_enable_right && sp_right_width > 0)
			mainwindow_set_statusbar_columns(mw, 0, sp_right_width);
	}
}

static void sig_mainwindow_created(MAIN_WINDOW_REC *mw)
{
	/* Create context for this mainwindow */
	get_ctx(mw, TRUE);
	
	if (sp_enable_left && sp_left_width > 0)
		mainwindow_set_statusbar_columns(mw, sp_left_width, 0);
	if (sp_enable_right && sp_right_width > 0)
		mainwindow_set_statusbar_columns(mw, 0, sp_right_width);
}

static void sig_mainwindow_destroyed(MAIN_WINDOW_REC *mw)
{
	/* Destroy context for this mainwindow */
	destroy_ctx(mw);
}

static void sig_setup_changed(void)
{
	read_settings();
	apply_reservations_all();
}

static void sig_terminal_resized(void)
{
	/* Adjust panel widths based on new terminal size */
	adjust_panel_widths_for_terminal_size();
}

static void sig_mainwindow_resized(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx;
	
	/* Panel windows are already moved by mainwindow_resize_windows() */
	
	/* Preserve selection state and adjust scroll offsets for new size */
	ctx = get_ctx(mw, FALSE);
	if (ctx) {
		/* Adjust scroll offsets to account for new panel height */
		adjust_left_scroll_to_selection(ctx, mw);
		adjust_right_scroll_to_selection(ctx, mw);
		
		/* Mark panels as dirty to trigger redraw */
		ctx->channel_list_dirty = TRUE;
		ctx->nicklist_dirty = TRUE;
	}
	
	/* Trigger panel redraw after resize */
	sidepanels_redraw_all(mw);
}

static void sig_mainwindow_moved(MAIN_WINDOW_REC *mw)
{
	/* Panel windows are already moved by mainwindow_resize_windows() */
	/* Trigger panel redraw after move */
	sidepanels_redraw_all(mw);
}

static void sig_window_changed(WINDOW_REC *old, WINDOW_REC *new)
{
	GSList *tmp;
	
	/* Update channel list and nicklist for all mainwindows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (ctx) {
			populate_channel_list(ctx);
			populate_nicklist(ctx);
			sidepanels_redraw_all(mw);
		}
	}
}

static void sig_window_created(WINDOW_REC *window)
{
	GSList *tmp;
	
	/* Update channel list for all mainwindows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (ctx) {
			populate_channel_list(ctx);
		}
	}
}

static void sig_window_destroyed(WINDOW_REC *window)
{
	GSList *tmp;
	
	/* Update channel list for all mainwindows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (ctx) {
			populate_channel_list(ctx);
		}
	}
}

static void sig_window_activity(WINDOW_REC *window, int old_level)
{
	GSList *tmp;
	
	/* Update activity levels in channel list for all mainwindows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (ctx) {
			GList *item_tmp;
			/* Update activity level for this window */
			for (item_tmp = ctx->channel_list; item_tmp != NULL; item_tmp = item_tmp->next) {
				ChannelListItem *item = item_tmp->data;
				if (item->window == window && item->item) {
					item->activity_level = item->item->data_level;
					ctx->channel_list_dirty = TRUE;
					break;
				}
			}
		}
	}
}

static void sig_window_refnum_changed(WINDOW_REC *window, gpointer old_refnum)
{
	GSList *tmp;
	
	/* Update channel list for all mainwindows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (ctx) {
			populate_channel_list(ctx);
		}
	}
}

static void sig_nicklist_changed(CHANNEL_REC *channel, NICK_REC *nick, const char *old_nick)
{
	GSList *tmp;
	
	/* Update nicklist for all mainwindows if this is the active channel */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (ctx && ctx->nicklist_channel == channel) {
			populate_nicklist(ctx);
		}
	}
}

static void sig_nick_mode_changed(CHANNEL_REC *channel, NICK_REC *nick, const char *setby, const char *mode, const char *type)
{
	GSList *tmp;
	
	/* Update nicklist for all mainwindows if this is the active channel */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (ctx && ctx->nicklist_channel == channel) {
			populate_nicklist(ctx);
		}
	}
}

/* Command handlers */
static void cmd_panel(const char *data, SERVER_REC *server, void *item)
{
	char *params, *arg1, *arg2;
	int width;
	
	params = g_strdup(data);
	arg1 = cmd_get_param(&params);
	arg2 = cmd_get_param(&params);
	
	if (*arg1 == '\0') {
		/* Show current settings */
		printtext(NULL, NULL, MSGLEVEL_CRAP, 
			"Sidepanels: left=%s (width=%d), right=%s (width=%d)",
			sp_enable_left ? "ON" : "OFF", sp_left_width,
			sp_enable_right ? "ON" : "OFF", sp_right_width);
		goto cleanup;
	}
	
	if (g_ascii_strcasecmp(arg1, "left") == 0) {
		if (*arg2 == '\0') {
			printtext(NULL, NULL, MSGLEVEL_CRAP, 
				"Left panel: %s (width=%d)", 
				sp_enable_left ? "ON" : "OFF", sp_left_width);
			goto cleanup;
		}
		
		width = atoi(arg2);
		if (width < 0 || width > 50) {
			printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, 
				"Panel width must be between 0 and 50");
			goto cleanup;
		}
		
		settings_set_int("sidepanel_left_width", width);
		settings_set_bool("sidepanel_left", width > 0);
		printtext(NULL, NULL, MSGLEVEL_CRAP, 
			"Left panel width set to %d", width);
			
	} else if (g_ascii_strcasecmp(arg1, "right") == 0) {
		if (*arg2 == '\0') {
			printtext(NULL, NULL, MSGLEVEL_CRAP, 
				"Right panel: %s (width=%d)", 
				sp_enable_right ? "ON" : "OFF", sp_right_width);
			goto cleanup;
		}
		
		width = atoi(arg2);
		if (width < 0 || width > 50) {
			printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, 
				"Panel width must be between 0 and 50");
			goto cleanup;
		}
		
		settings_set_int("sidepanel_right_width", width);
		settings_set_bool("sidepanel_right", width > 0);
		printtext(NULL, NULL, MSGLEVEL_CRAP, 
			"Right panel width set to %d", width);
			
	} else if (g_ascii_strcasecmp(arg1, "on") == 0) {
		settings_set_bool("sidepanel_left", TRUE);
		settings_set_bool("sidepanel_right", TRUE);
		printtext(NULL, NULL, MSGLEVEL_CRAP, "Sidepanels enabled");
		
	} else if (g_ascii_strcasecmp(arg1, "off") == 0) {
		settings_set_bool("sidepanel_left", FALSE);
		settings_set_bool("sidepanel_right", FALSE);
		printtext(NULL, NULL, MSGLEVEL_CRAP, "Sidepanels disabled");
		
	} else {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, 
			"Usage: /panel [left|right <width>] [on|off]");
	}
	
cleanup:
	g_free(params);
}

/* Panel rendering functions */
static void sidepanels_redraw_left(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx;
	GList *tmp;
	int y, max_height;
	int visible_start, visible_end;
	
	if (!mw->left_panel_win) return;
	
	ctx = get_ctx(mw, FALSE);
	if (!ctx || !ctx->channel_list_dirty) return;
	
	/* Adjust scroll offset to keep selection visible */
	adjust_left_scroll_to_selection(ctx, mw);
	
	/* Handle empty channel list */
	if (!ctx->channel_list) {
		term_window_clear(mw->left_panel_win);
		term_set_color2(mw->left_panel_win, 0, 8, 0); /* Dark gray */
		term_move(mw->left_panel_win, 0, 0);
		term_addstr(mw->left_panel_win, "(no channels)");
		term_set_color2(mw->left_panel_win, 0, 0, 0);
		term_refresh(mw->left_panel_win);
		ctx->channel_list_dirty = FALSE;
		return;
	}
	
	/* Clear panel */
	term_window_clear(mw->left_panel_win);
	
	/* Calculate visible range */
	max_height = mw->height - mw->statusbar_lines;
	visible_start = ctx->left_scroll_offset;
	visible_end = visible_start + max_height;
	
	/* Draw channel list */
	y = 0;
	for (tmp = ctx->channel_list; tmp != NULL && y < max_height; tmp = tmp->next) {
		ChannelListItem *item = tmp->data;
		int item_index = g_list_position(ctx->channel_list, tmp);
		
		/* Skip items before visible range */
		if (item_index < visible_start) continue;
		if (item_index >= visible_end) break;
		
		/* Set colors based on selection, activity and active state */
		if (item_index == ctx->left_selected_index) {
			/* Selected item - reverse */
			term_set_color2(mw->left_panel_win, ATTR_REVERSE, 15, 0);
		} else if (item->is_active) {
			/* Active channel - white, bold */
			term_set_color2(mw->left_panel_win, ATTR_BOLD, 15, 0);
		} else if (item->activity_level > 0) {
			/* Activity - yellow, bold */
			term_set_color2(mw->left_panel_win, ATTR_BOLD, 11, 0);
		} else {
			/* Normal - default colors */
			term_set_color2(mw->left_panel_win, 0, 7, 0);
		}
		
		/* Position cursor and draw item */
		term_move(mw->left_panel_win, 0, y);
		term_clrtoeol(mw->left_panel_win);
		
		/* Draw activity indicator */
		if (item->activity_level > 0) {
			term_addstr(mw->left_panel_win, "*");
		} else {
			term_addstr(mw->left_panel_win, " ");
		}
		
		/* Draw channel name (truncate if too long) */
		if (strlen(item->name) > mw->statusbar_columns_left - 2) {
			char *truncated = g_strndup(item->name, mw->statusbar_columns_left - 3);
			term_addstr(mw->left_panel_win, truncated);
			g_free(truncated);
		} else {
			term_addstr(mw->left_panel_win, item->name);
		}
		
		y++;
	}
	
	/* Reset colors and refresh */
	term_set_color2(mw->left_panel_win, 0, 0, 0);
	term_refresh(mw->left_panel_win);
	
	ctx->channel_list_dirty = FALSE;
}

static void sidepanels_redraw_right(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx;
	GList *tmp;
	int y, max_height;
	int visible_start, visible_end;
	
	if (!mw->right_panel_win) return;
	
	ctx = get_ctx(mw, FALSE);
	if (!ctx || !ctx->nicklist_dirty) return;
	
	/* Adjust scroll offset to keep selection visible */
	adjust_right_scroll_to_selection(ctx, mw);
	
	/* Handle empty nicklist or non-channel */
	if (!ctx->nicklist || !ctx->nicklist_channel) {
		term_window_clear(mw->right_panel_win);
		term_set_color2(mw->right_panel_win, 0, 8, 0); /* Dark gray */
		term_move(mw->right_panel_win, 0, 0);
		if (!ctx->nicklist_channel) {
			term_addstr(mw->right_panel_win, "(not a channel)");
		} else {
			term_addstr(mw->right_panel_win, "(no users)");
		}
		term_set_color2(mw->right_panel_win, 0, 0, 0);
		term_refresh(mw->right_panel_win);
		ctx->nicklist_dirty = FALSE;
		return;
	}
	
	/* Clear panel */
	term_window_clear(mw->right_panel_win);
	
	/* Calculate visible range */
	max_height = mw->height - mw->statusbar_lines;
	visible_start = ctx->right_scroll_offset;
	visible_end = visible_start + max_height;
	
	/* Draw nicklist */
	y = 0;
	for (tmp = ctx->nicklist; tmp != NULL && y < max_height; tmp = tmp->next) {
		NicklistItem *item = tmp->data;
		int item_index = g_list_position(ctx->nicklist, tmp);
		int prefix_len, available_width;
		
		/* Skip items before visible range */
		if (item_index < visible_start) continue;
		if (item_index >= visible_end) break;
		
		/* Set colors based on selection and nick status */
		if (item_index == ctx->right_selected_index) {
			/* Selected nick - reverse */
			term_set_color2(mw->right_panel_win, ATTR_REVERSE, 15, 0);
		} else if (item->is_away) {
			/* Away nick - dark gray */
			term_set_color2(mw->right_panel_win, 0, 8, 0);
		} else if (item->level >= 4) {
			/* Op - red */
			term_set_color2(mw->right_panel_win, ATTR_BOLD, 9, 0);
		} else if (item->level >= 2) {
			/* Voice - green */
			term_set_color2(mw->right_panel_win, ATTR_BOLD, 10, 0);
		} else {
			/* Normal - default */
			term_set_color2(mw->right_panel_win, 0, 7, 0);
		}
		
		/* Position cursor and draw item */
		term_move(mw->right_panel_win, 0, y);
		term_clrtoeol(mw->right_panel_win);
		
		/* Draw prefix and nick */
		if (strlen(item->prefix) > 0) {
			term_addstr(mw->right_panel_win, item->prefix);
		}
		
		/* Draw nick (truncate if too long) */
		prefix_len = strlen(item->prefix);
		available_width = mw->statusbar_columns_right - prefix_len;
		if (strlen(item->nick) > available_width) {
			char *truncated = g_strndup(item->nick, available_width);
			term_addstr(mw->right_panel_win, truncated);
			g_free(truncated);
		} else {
			term_addstr(mw->right_panel_win, item->nick);
		}
		
		y++;
	}
	
	/* Reset colors and refresh */
	term_set_color2(mw->right_panel_win, 0, 0, 0);
	term_refresh(mw->right_panel_win);
	
	ctx->nicklist_dirty = FALSE;
}

static void sidepanels_redraw_all(MAIN_WINDOW_REC *mw)
{
	if (!sp_enable_left && !sp_enable_right) return;
	
	term_refresh_freeze();
	
	if (sp_enable_left && mw->left_panel_win) {
		sidepanels_redraw_left(mw);
	}
	
	if (sp_enable_right && mw->right_panel_win) {
		sidepanels_redraw_right(mw);
	}
	
	term_refresh_thaw();
}

/* Edge case handling and resize stability */
static void adjust_panel_widths_for_terminal_size(void)
{
	GSList *tmp;
	int terminal_width = term_width;
	int effective_left_width = sp_left_width;
	int effective_right_width = sp_right_width;
	gboolean left_enabled = sp_enable_left;
	gboolean right_enabled = sp_enable_right;
	
	/* Apply auto-hide logic based on terminal width */
	if (terminal_width < 40) {
		/* Very narrow - hide both panels */
		left_enabled = FALSE;
		right_enabled = FALSE;
	} else if (terminal_width < 60) {
		/* Narrow - hide right panel, shrink left */
		right_enabled = FALSE;
		if (effective_left_width > 8) {
			effective_left_width = 8; /* Minimum left width */
		}
	} else if (terminal_width < 80) {
		/* Medium - hide right panel only */
		right_enabled = FALSE;
	}
	
	/* Ensure minimum widths */
	if (left_enabled && effective_left_width < 8) {
		effective_left_width = 8;
	}
	if (right_enabled && effective_right_width < 6) {
		effective_right_width = 6;
	}
	
	/* Apply to all mainwindows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		
		/* Reset current reservations */
		if (mw->statusbar_columns_left > 0) {
			mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, 0);
		}
		if (mw->statusbar_columns_right > 0) {
			mainwindow_set_statusbar_columns(mw, 0, -mw->statusbar_columns_right);
		}
		
		/* Apply new reservations */
		if (left_enabled && effective_left_width > 0) {
			mainwindow_set_statusbar_columns(mw, effective_left_width, 0);
		}
		if (right_enabled && effective_right_width > 0) {
			mainwindow_set_statusbar_columns(mw, 0, effective_right_width);
		}
		
		/* Trigger redraw */
		sidepanels_redraw_all(mw);
	}
}

/* Selection and scroll management */
static void adjust_left_scroll_to_selection(SP_MAINWIN_CTX *ctx, MAIN_WINDOW_REC *mw)
{
	int max_height, list_length;
	
	if (!ctx->channel_list || ctx->left_selected_index < 0) return;
	
	max_height = mw->height - mw->statusbar_lines;
	list_length = g_list_length(ctx->channel_list);
	
	/* If selection is above visible area, scroll up */
	if (ctx->left_selected_index < ctx->left_scroll_offset) {
		ctx->left_scroll_offset = ctx->left_selected_index;
	}
	/* If selection is below visible area, scroll down */
	else if (ctx->left_selected_index >= ctx->left_scroll_offset + max_height) {
		ctx->left_scroll_offset = ctx->left_selected_index - max_height + 1;
	}
	
	/* Ensure scroll offset is within valid bounds */
	if (ctx->left_scroll_offset < 0) {
		ctx->left_scroll_offset = 0;
	}
	if (ctx->left_scroll_offset > list_length - max_height && list_length > max_height) {
		ctx->left_scroll_offset = list_length - max_height;
	}
}

static void adjust_right_scroll_to_selection(SP_MAINWIN_CTX *ctx, MAIN_WINDOW_REC *mw)
{
	int max_height, list_length;
	
	if (!ctx->nicklist || ctx->right_selected_index < 0) return;
	
	max_height = mw->height - mw->statusbar_lines;
	list_length = g_list_length(ctx->nicklist);
	
	/* If selection is above visible area, scroll up */
	if (ctx->right_selected_index < ctx->right_scroll_offset) {
		ctx->right_scroll_offset = ctx->right_selected_index;
	}
	/* If selection is below visible area, scroll down */
	else if (ctx->right_selected_index >= ctx->right_scroll_offset + max_height) {
		ctx->right_scroll_offset = ctx->right_selected_index - max_height + 1;
	}
	
	/* Ensure scroll offset is within valid bounds */
	if (ctx->right_scroll_offset < 0) {
		ctx->right_scroll_offset = 0;
	}
	if (ctx->right_scroll_offset > list_length - max_height && list_length > max_height) {
		ctx->right_scroll_offset = list_length - max_height;
	}
}

static void update_left_selection(SP_MAINWIN_CTX *ctx, int new_selection)
{
	int list_length;
	
	if (!ctx->channel_list) {
		ctx->left_selected_index = -1;
		ctx->left_scroll_offset = 0;
		return;
	}
	
	list_length = g_list_length(ctx->channel_list);
	if (list_length == 0) {
		ctx->left_selected_index = -1;
		ctx->left_scroll_offset = 0;
		return;
	}
	
	/* Clamp selection to valid range */
	if (new_selection < 0) {
		new_selection = 0;
	} else if (new_selection >= list_length) {
		new_selection = list_length - 1;
	}
	
	ctx->left_selected_index = new_selection;
	ctx->channel_list_dirty = TRUE;
}

static void update_right_selection(SP_MAINWIN_CTX *ctx, int new_selection)
{
	int list_length;
	
	if (!ctx->nicklist) {
		ctx->right_selected_index = -1;
		ctx->right_scroll_offset = 0;
		return;
	}
	
	list_length = g_list_length(ctx->nicklist);
	if (list_length == 0) {
		ctx->right_selected_index = -1;
		ctx->right_scroll_offset = 0;
		return;
	}
	
	/* Clamp selection to valid range */
	if (new_selection < 0) {
		new_selection = 0;
	} else if (new_selection >= list_length) {
		new_selection = list_length - 1;
	}
	
	ctx->right_selected_index = new_selection;
	ctx->nicklist_dirty = TRUE;
}

/* Scroll management functions */
static void scroll_left_panel(SP_MAINWIN_CTX *ctx, MAIN_WINDOW_REC *mw, int delta)
{
	int max_height, list_length;
	int new_offset;
	
	if (!ctx->channel_list) return;
	
	max_height = mw->height - mw->statusbar_lines;
	list_length = g_list_length(ctx->channel_list);
	
	/* Calculate new scroll offset */
	new_offset = ctx->left_scroll_offset + delta;
	
	/* Clamp to valid range */
	if (new_offset < 0) {
		new_offset = 0;
	}
	if (new_offset > list_length - max_height && list_length > max_height) {
		new_offset = list_length - max_height;
	}
	
	/* Update if changed */
	if (new_offset != ctx->left_scroll_offset) {
		ctx->left_scroll_offset = new_offset;
		ctx->channel_list_dirty = TRUE;
	}
}

static void scroll_right_panel(SP_MAINWIN_CTX *ctx, MAIN_WINDOW_REC *mw, int delta)
{
	int max_height, list_length;
	int new_offset;
	
	if (!ctx->nicklist) return;
	
	max_height = mw->height - mw->statusbar_lines;
	list_length = g_list_length(ctx->nicklist);
	
	/* Calculate new scroll offset */
	new_offset = ctx->right_scroll_offset + delta;
	
	/* Clamp to valid range */
	if (new_offset < 0) {
		new_offset = 0;
	}
	if (new_offset > list_length - max_height && list_length > max_height) {
		new_offset = list_length - max_height;
	}
	
	/* Update if changed */
	if (new_offset != ctx->right_scroll_offset) {
		ctx->right_scroll_offset = new_offset;
		ctx->nicklist_dirty = TRUE;
	}
}

/* Public API functions for selection and scroll management */
void sidepanels_move_left_selection(MAIN_WINDOW_REC *mw, int delta)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
	if (!ctx) return;
	
	update_left_selection(ctx, ctx->left_selected_index + delta);
	sidepanels_redraw_left(mw);
}

void sidepanels_move_right_selection(MAIN_WINDOW_REC *mw, int delta)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
	if (!ctx) return;
	
	update_right_selection(ctx, ctx->right_selected_index + delta);
	sidepanels_redraw_right(mw);
}

void sidepanels_scroll_left(MAIN_WINDOW_REC *mw, int delta)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
	if (!ctx) return;
	
	scroll_left_panel(ctx, mw, delta);
	sidepanels_redraw_left(mw);
}

void sidepanels_scroll_right(MAIN_WINDOW_REC *mw, int delta)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
	if (!ctx) return;
	
	scroll_right_panel(ctx, mw, delta);
	sidepanels_redraw_right(mw);
}

int sidepanels_get_left_selection(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
	return ctx ? ctx->left_selected_index : -1;
}

int sidepanels_get_right_selection(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
	return ctx ? ctx->right_selected_index : -1;
}

void sidepanels_init(void)
{
	settings_add_int("lookandfeel", "sidepanel_left_width", 14);
	settings_add_int("lookandfeel", "sidepanel_right_width", 16);
	settings_add_bool("lookandfeel", "sidepanel_left", TRUE);
	settings_add_bool("lookandfeel", "sidepanel_right", TRUE);

	/* Initialize context hash table */
	mw_to_ctx = g_hash_table_new(g_direct_hash, g_direct_equal);

	read_settings();
	
	/* Mainwindow signals */
	signal_add("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
	signal_add("mainwindow destroyed", (SIGNAL_FUNC) sig_mainwindow_destroyed);
	signal_add("terminal resized", (SIGNAL_FUNC) sig_terminal_resized);
	signal_add("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized);
	signal_add("mainwindow moved", (SIGNAL_FUNC) sig_mainwindow_moved);
	
	/* Window signals */
	signal_add("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_add("window created", (SIGNAL_FUNC) sig_window_created);
	signal_add("window destroyed", (SIGNAL_FUNC) sig_window_destroyed);
	signal_add("window activity", (SIGNAL_FUNC) sig_window_activity);
	signal_add("window refnum changed", (SIGNAL_FUNC) sig_window_refnum_changed);
	
	/* Window item signals */
	signal_add("window item new", (SIGNAL_FUNC) sig_window_created);
	signal_add("window item remove", (SIGNAL_FUNC) sig_window_destroyed);
	signal_add("window item changed", (SIGNAL_FUNC) sig_window_changed);
	
	/* Nicklist signals */
	signal_add("nicklist changed", (SIGNAL_FUNC) sig_nicklist_changed);
	signal_add("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_changed);
	
	/* Settings signal */
	signal_add("setup changed", (SIGNAL_FUNC) sig_setup_changed);
	
	/* Register commands */
	command_bind("panel", NULL, (SIGNAL_FUNC) cmd_panel);
	
	/* Apply to existing mainwindows with size adjustment */
	adjust_panel_widths_for_terminal_size();
}

void sidepanels_deinit(void)
{
	GSList *tmp;
	
	/* Remove all signal handlers */
	signal_remove("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
	signal_remove("mainwindow destroyed", (SIGNAL_FUNC) sig_mainwindow_destroyed);
	signal_remove("terminal resized", (SIGNAL_FUNC) sig_terminal_resized);
	signal_remove("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized);
	signal_remove("mainwindow moved", (SIGNAL_FUNC) sig_mainwindow_moved);
	signal_remove("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_remove("window created", (SIGNAL_FUNC) sig_window_created);
	signal_remove("window destroyed", (SIGNAL_FUNC) sig_window_destroyed);
	signal_remove("window activity", (SIGNAL_FUNC) sig_window_activity);
	signal_remove("window refnum changed", (SIGNAL_FUNC) sig_window_refnum_changed);
	signal_remove("window item new", (SIGNAL_FUNC) sig_window_created);
	signal_remove("window item remove", (SIGNAL_FUNC) sig_window_destroyed);
	signal_remove("window item changed", (SIGNAL_FUNC) sig_window_changed);
	signal_remove("nicklist changed", (SIGNAL_FUNC) sig_nicklist_changed);
	signal_remove("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_changed);
	signal_remove("setup changed", (SIGNAL_FUNC) sig_setup_changed);
	
	/* Unregister commands */
	command_unbind("panel", (SIGNAL_FUNC) cmd_panel);
	
	/* Remove reservations from all mainwindows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		destroy_ctx(mw);
		if (mw->statusbar_columns_left)
			mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, 0);
		if (mw->statusbar_columns_right)
			mainwindow_set_statusbar_columns(mw, 0, -mw->statusbar_columns_right);
	}
	
	/* Destroy context hash table */
	g_hash_table_destroy(mw_to_ctx);
	mw_to_ctx = NULL;
}