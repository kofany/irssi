#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/sidepanels.h>
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

/* Settings */
static int sp_left_width;
static int sp_right_width;
static int sp_enable_left;
static int sp_enable_right;
static int sp_auto_hide_right;
static int sp_enable_mouse;

/* Context management */
static GHashTable *mw_to_ctx;

/* Forward declarations */
static void sidepanels_draw_left_panel(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx);
static void sidepanels_draw_right_panel(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx);

static void read_settings(void)
{
	sp_left_width = settings_get_int("sidepanel_left_width");
	sp_right_width = settings_get_int("sidepanel_right_width");
	sp_enable_left = settings_get_bool("sidepanel_left");
	sp_enable_right = settings_get_bool("sidepanel_right");
	sp_auto_hide_right = settings_get_bool("sidepanel_right_auto_hide");
	sp_enable_mouse = settings_get_bool("sidepanel_mouse");
	
	/* Apply new settings to all main windows */
	sidepanels_apply_settings_all();
}

SP_MAINWIN_CTX *sidepanels_get_context(MAIN_WINDOW_REC *mw, gboolean create)
{
	SP_MAINWIN_CTX *ctx = g_hash_table_lookup(mw_to_ctx, mw);
	if (!ctx && create) {
		ctx = g_new0(SP_MAINWIN_CTX, 1);
		g_hash_table_insert(mw_to_ctx, mw, ctx);
	}
	return ctx;
}

void sidepanels_destroy_context(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = g_hash_table_lookup(mw_to_ctx, mw);
	if (!ctx) return;
	
	/* Free cached content */
	if (ctx->left_content_cache) {
		for (int i = 0; i < ctx->left_content_lines; i++) {
			g_free(ctx->left_content_cache[i]);
		}
		g_free(ctx->left_content_cache);
	}
	if (ctx->right_content_cache) {
		for (int i = 0; i < ctx->right_content_lines; i++) {
			g_free(ctx->right_content_cache[i]);
		}
		g_free(ctx->right_content_cache);
	}
	
	g_hash_table_remove(mw_to_ctx, mw);
	g_free(ctx);
}

void sidepanels_apply_settings_all(void)
{
	GSList *tmp;
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		
		/* Reset previous reservations if any by setting negative, then apply new */
		if (mw->statusbar_columns_left)
			mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, 0);
		if (mw->statusbar_columns_right)
			mainwindow_set_statusbar_columns(mw, 0, -mw->statusbar_columns_right);
			
		/* Apply new reservations */
		if (sp_enable_left && sp_left_width > 0)
			mainwindow_set_statusbar_columns(mw, sp_left_width, 0);
		if (sp_enable_right && sp_right_width > 0)
			mainwindow_set_statusbar_columns(mw, 0, sp_right_width);
	}
}

static void sig_mainwindow_created(MAIN_WINDOW_REC *mw)
{
	/* Apply column reservations to new main window */
	if (sp_enable_left && sp_left_width > 0)
		mainwindow_set_statusbar_columns(mw, sp_left_width, 0);
	if (sp_enable_right && sp_right_width > 0)
		mainwindow_set_statusbar_columns(mw, 0, sp_right_width);
}

static void sig_mainwindow_destroyed(MAIN_WINDOW_REC *mw)
{
	sidepanels_destroy_context(mw);
}

static void sig_mainwindow_resized(MAIN_WINDOW_REC *mw)
{
	sidepanels_redraw_mainwin(mw);
}

static void sig_window_changed(WINDOW_REC *w)
{
	(void)w;
	sidepanels_redraw_all();
}

static void sig_channel_list_changed(CHANNEL_REC *ch)
{
	(void)ch; 
	sidepanels_redraw_all();
}

static void sig_query_list_changed(QUERY_REC *q)
{
	(void)q; 
	sidepanels_redraw_all();
}

static void sig_irssi_init_finished(void)
{
	sidepanels_apply_settings_all();
	sidepanels_redraw_all();
}

void sidepanels_redraw_all(void)
{
	GSList *tmp;
	for (tmp = mainwindows; tmp; tmp = tmp->next) {
		sidepanels_redraw_mainwin(tmp->data);
	}
}

void sidepanels_redraw_mainwin(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = sidepanels_get_context(mw, TRUE);
	if (!ctx) return;
	
	/* Update cached geometry */
	if (mw->left_panel_win) {
		ctx->left_x = mw->first_column;
		ctx->left_y = mw->first_line + mw->statusbar_lines_top;
		ctx->left_w = mw->statusbar_columns_left;
		ctx->left_h = mw->height - mw->statusbar_lines;
	} else {
		ctx->left_h = 0;
	}
	
	if (mw->right_panel_win) {
		ctx->right_x = mw->last_column - mw->statusbar_columns_right + 1;
		ctx->right_y = mw->first_line + mw->statusbar_lines_top;
		ctx->right_w = mw->statusbar_columns_right;
		ctx->right_h = mw->height - mw->statusbar_lines;
	} else {
		ctx->right_h = 0;
	}
	
	/* Render left and right panels */
	sidepanels_draw_left_panel(mw, ctx);
	sidepanels_draw_right_panel(mw, ctx);
}

gboolean sidepanels_try_parse_mouse_key(unichar key)
{
	/* TODO: Implement mouse parsing */
	/* This will be implemented in task 18 */
	return FALSE;
}

void sidepanels_init(void)
{
	settings_add_bool("lookandfeel", "sidepanel_left", TRUE);
	settings_add_bool("lookandfeel", "sidepanel_right", TRUE);
	settings_add_int("lookandfeel", "sidepanel_left_width", 18);
	settings_add_int("lookandfeel", "sidepanel_right_width", 18);
	settings_add_bool("lookandfeel", "sidepanel_right_auto_hide", TRUE);
	settings_add_bool("lookandfeel", "sidepanel_mouse", FALSE);
	
	read_settings();
	
	mw_to_ctx = g_hash_table_new(g_direct_hash, g_direct_equal);
	
	/* Apply to existing main windows */
	sidepanels_apply_settings_all();
	sidepanels_redraw_all();
	
	/* Connect signals */
	signal_add("irssi init finished", (SIGNAL_FUNC) sig_irssi_init_finished);
	signal_add("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
	signal_add("mainwindow destroyed", (SIGNAL_FUNC) sig_mainwindow_destroyed);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
	signal_add("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized);
	signal_add("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_add("channel created", (SIGNAL_FUNC) sig_channel_list_changed);
	signal_add("channel destroyed", (SIGNAL_FUNC) sig_channel_list_changed);
	signal_add("query created", (SIGNAL_FUNC) sig_query_list_changed);
	signal_add("query destroyed", (SIGNAL_FUNC) sig_query_list_changed);
}

void sidepanels_deinit(void)
{
	GSList *tmp;
	
	/* Remove signal handlers */
	signal_remove("irssi init finished", (SIGNAL_FUNC) sig_irssi_init_finished);
	signal_remove("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
	signal_remove("mainwindow destroyed", (SIGNAL_FUNC) sig_mainwindow_destroyed);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
	signal_remove("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized);
	signal_remove("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_remove("channel created", (SIGNAL_FUNC) sig_channel_list_changed);
	signal_remove("channel destroyed", (SIGNAL_FUNC) sig_channel_list_changed);
	signal_remove("query created", (SIGNAL_FUNC) sig_query_list_changed);
	signal_remove("query destroyed", (SIGNAL_FUNC) sig_query_list_changed);
	
	/* Remove column reservations from all main windows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		sidepanels_destroy_context(mw);
		if (mw->statusbar_columns_left)
			mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, 0);
		if (mw->statusbar_columns_right)
			mainwindow_set_statusbar_columns(mw, 0, -mw->statusbar_columns_right);
	}
	
	if (mw_to_ctx) {
		g_hash_table_destroy(mw_to_ctx);
		mw_to_ctx = NULL;
	}
}

/* Helper functions for drawing */
static void clear_panel_window(TERM_WINDOW *tw, int height)
{
	int i;
	if (!tw) return;
	for (i = 0; i < height; i++) {
		term_window_clrtoeol(tw, i);
	}
}

static void draw_panel_border_vertical(TERM_WINDOW *tw, int width, int height, int left)
{
	int i;
	int x;
	if (!tw) return;
	x = left ? width - 1 : 0;
	for (i = 0; i < height; i++) {
		term_move(tw, x, i);
		term_addch(tw, '|');
	}
}

static void draw_themed_text(TERM_WINDOW *tw, int x, int y, WINDOW_REC *wctx, int format_id, const char *text)
{
	TEXT_DEST_REC dest;
	THEME_REC *theme;
	char *out;
	char *plain;

	if (!tw || !text) return;

	format_create_dest(&dest, NULL, NULL, 0, wctx);
	theme = window_get_theme(wctx);
	out = format_get_text_theme(theme, MODULE_NAME, &dest, format_id, text);
	plain = strip_codes(out);

	term_move(tw, x, y);
	term_addstr(tw, plain ? plain : out);

	g_free(plain);
	g_free(out);
}

static int compute_activity_level(WI_ITEM_REC *item)
{
	return item ? item->data_level : 0;
}

static void sidepanels_draw_left_panel(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx)
{
	TERM_WINDOW *tw;
	int row, skip, height, index;
	GSList *st;

	if (!ctx || !mw->left_panel_win) return;

	tw = mw->left_panel_win;
	clear_panel_window(tw, ctx->left_h);

	row = 0;
	skip = ctx->left_scroll_offset;
	height = ctx->left_h;
	index = 0;

	if (servers == NULL) {
		/* Fallback: list existing windows by name */
		GSList *w;
		for (w = windows; w && row < height; w = w->next) {
			WINDOW_REC *win = w->data;
			const char *name = window_get_active_name(win);
			if (index++ >= skip && row < height) {
				int format = (index-1 == ctx->left_selected_index) ?
					TXT_SIDEPANEL_ITEM_SELECTED : TXT_SIDEPANEL_ITEM;
				draw_themed_text(tw, 0, row, mw->active, format, name ? name : "window");
				row++;
			}
		}
		draw_panel_border_vertical(tw, ctx->left_w, ctx->left_h, 1);
		term_refresh(tw);
		return;
	}

	/* Draw servers, channels and queries */
	for (st = servers; st && row < height; st = st->next) {
		SERVER_REC *srv = st->data;
		const char *net = srv->connrec && srv->connrec->chatnet ?
			srv->connrec->chatnet : (srv->connrec ? srv->connrec->address : "server");

		if (index++ >= skip && row < height) {
			int format = (index-1 == ctx->left_selected_index) ?
				TXT_SIDEPANEL_ITEM_SELECTED : TXT_SIDEPANEL_HEADER;
			draw_themed_text(tw, 0, row, mw->active, format, net ? net : "server");
			row++;
		}

		/* Draw channels */
		{
			GSList *ct;
			for (ct = srv->channels; ct && row < height; ct = ct->next) {
				CHANNEL_REC *ch = ct->data;
				int activity, format;
				if (!ch || !ch->name) continue;

				activity = compute_activity_level((WI_ITEM_REC*)ch);
				if (index++ >= skip && row < height) {
					if (index-1 == ctx->left_selected_index) {
						format = TXT_SIDEPANEL_ITEM_SELECTED;
					} else if (activity > 0) {
						format = TXT_SIDEPANEL_ITEM_ACTIVITY;
					} else {
						format = TXT_SIDEPANEL_ITEM;
					}
					draw_themed_text(tw, 0, row, mw->active, format, ch->name);
					row++;
				}
			}
		}

		/* Draw queries */
		{
			GSList *qt;
			for (qt = srv->queries; qt && row < height; qt = qt->next) {
				QUERY_REC *q = qt->data;
				int activity, format;
				if (!q || !q->name) continue;

				activity = compute_activity_level((WI_ITEM_REC*)q);
				if (index++ >= skip && row < height) {
					if (index-1 == ctx->left_selected_index) {
						format = TXT_SIDEPANEL_ITEM_SELECTED;
					} else if (activity > 0) {
						format = TXT_SIDEPANEL_ITEM_ACTIVITY;
					} else {
						format = TXT_SIDEPANEL_ITEM;
					}
					draw_themed_text(tw, 0, row, mw->active, format, q->name);
					row++;
				}
			}
		}
	}

	draw_panel_border_vertical(tw, ctx->left_w, ctx->left_h, 1);
	term_refresh(tw);
}

static void sidepanels_draw_right_panel(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx)
{
	TERM_WINDOW *tw;
	WINDOW_REC *aw;
	int height, skip, index, row;
	GSList *nicks, *nt;
	CHANNEL_REC *ch;

	if (!ctx || !mw->right_panel_win) return;

	tw = mw->right_panel_win;
	clear_panel_window(tw, ctx->right_h);

	aw = mw->active;
	height = ctx->right_h;
	skip = ctx->right_scroll_offset;
	index = 0;
	row = 0;

	if (!aw || !aw->active || !IS_CHANNEL(aw->active)) {
		draw_panel_border_vertical(tw, ctx->right_w, ctx->right_h, 0);
		term_refresh(tw);
		return;
	}

	ch = CHANNEL(aw->active);
	nicks = nicklist_getnicks(ch);

	for (nt = nicks; nt && row < height; nt = nt->next) {
		NICK_REC *nick = nt->data;
		int format;
		if (!nick || !nick->nick) continue;

		if (index++ < skip) continue;

		if (index-1 == ctx->right_selected_index) {
			format = TXT_SIDEPANEL_ITEM_SELECTED;
		} else if (nick->op) {
			format = TXT_SIDEPANEL_NICK_OP;
		} else if (nick->voice) {
			format = TXT_SIDEPANEL_NICK_VOICE;
		} else {
			format = TXT_SIDEPANEL_NICK_NORMAL;
		}

		draw_themed_text(tw, 1, row, mw->active, format, nick->nick);
		row++;
	}

	draw_panel_border_vertical(tw, ctx->right_w, ctx->right_h, 0);
	term_refresh(tw);
}
