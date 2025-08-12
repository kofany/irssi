#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/commands.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/term.h>

/* Settings */
static int sp_left_width;
static int sp_right_width;
static int sp_enable_left;
static int sp_enable_right;

/* Panel context per main window */
typedef struct {
	/* Selection and scroll state */
	int left_selected_index;
	int left_scroll_offset;
	int right_selected_index;
	int right_scroll_offset;
	
	/* Cached content for dirty checking */
	char **left_content_cache;
	char **right_content_cache;
	int left_content_lines;
	int right_content_lines;
} SP_MAINWIN_CTX;

static GHashTable *mw_to_ctx;

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
		ctx->left_content_cache = NULL;
		ctx->right_content_cache = NULL;
		ctx->left_content_lines = 0;
		ctx->right_content_lines = 0;
		g_hash_table_insert(mw_to_ctx, mw, ctx);
	}
	return ctx;
}

static void destroy_ctx(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = g_hash_table_lookup(mw_to_ctx, mw);
	if (!ctx) return;
	
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
	/* Terminal resize is handled by mainwindow resize, no action needed */
}

static void sig_mainwindow_resized(MAIN_WINDOW_REC *mw)
{
	/* Panel windows are already moved by mainwindow_resize_windows() */
	/* This is where we would trigger panel redraw in the future */
}

static void sig_mainwindow_moved(MAIN_WINDOW_REC *mw)
{
	/* Panel windows are already moved by mainwindow_resize_windows() */
	/* This is where we would trigger panel redraw in the future */
}

static void sig_window_changed(WINDOW_REC *old, WINDOW_REC *new)
{
	/* Window changed - need to update panel content */
	/* This is where we would update channel list and nicklist */
}

static void sig_window_created(WINDOW_REC *window)
{
	/* New window created - need to update channel list */
	/* This is where we would refresh left panel */
}

static void sig_window_destroyed(WINDOW_REC *window)
{
	/* Window destroyed - need to update channel list */
	/* This is where we would refresh left panel */
}

static void sig_window_activity(WINDOW_REC *window, int old_level)
{
	/* Window activity changed - need to update activity indicators */
	/* This is where we would update activity markers in left panel */
}

static void sig_window_refnum_changed(WINDOW_REC *window, gpointer old_refnum)
{
	/* Window refnum changed - need to update channel list */
	/* This is where we would refresh left panel */
}

static void sig_nicklist_changed(CHANNEL_REC *channel, NICK_REC *nick, const char *old_nick)
{
	/* Nicklist changed - need to update right panel */
	/* Only update if this is the active channel */
}

static void sig_nick_mode_changed(CHANNEL_REC *channel, NICK_REC *nick, const char *setby, const char *mode, const char *type)
{
	/* Nick mode changed - need to update right panel */
	/* Only update if this is the active channel */
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
	
	/* Nicklist signals */
	signal_add("nicklist changed", (SIGNAL_FUNC) sig_nicklist_changed);
	signal_add("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_changed);
	
	/* Settings signal */
	signal_add("setup changed", (SIGNAL_FUNC) sig_setup_changed);
	
	/* Register commands */
	command_bind("panel", NULL, (SIGNAL_FUNC) cmd_panel);
	
	/* Apply to existing mainwindows */
	apply_reservations_all();
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