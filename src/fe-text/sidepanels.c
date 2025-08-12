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

static void read_settings(void)
{
	sp_left_width = settings_get_int("sidepanel_left_width");
	sp_right_width = settings_get_int("sidepanel_right_width");
	sp_enable_left = settings_get_bool("sidepanel_left");
	sp_enable_right = settings_get_bool("sidepanel_right");
}

static void apply_reservations_all(void)
{
	GSList *tmp;
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		/* reset previous reservations if any by setting negative, then apply new */
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
	if (sp_enable_left && sp_left_width > 0)
		mainwindow_set_statusbar_columns(mw, sp_left_width, 0);
	if (sp_enable_right && sp_right_width > 0)
		mainwindow_set_statusbar_columns(mw, 0, sp_right_width);
}

typedef struct {
	TERM_WINDOW *left_tw;
	TERM_WINDOW *right_tw;
	int left_w;
	int right_w;
	/* selection and scroll state */
	int left_selected_index;
	int left_scroll_offset;
	int right_selected_index;
	int right_scroll_offset;
} SP_MAINWIN_CTX;

static GHashTable *mw_to_ctx;

static SP_MAINWIN_CTX *get_ctx(MAIN_WINDOW_REC *mw, gboolean create)
{
	SP_MAINWIN_CTX *ctx = g_hash_table_lookup(mw_to_ctx, mw);
	if (!ctx && create) {
		ctx = g_new0(SP_MAINWIN_CTX, 1);
		g_hash_table_insert(mw_to_ctx, mw, ctx);
	}
	return ctx;
}

static void destroy_ctx(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = g_hash_table_lookup(mw_to_ctx, mw);
	if (!ctx) return;
	if (ctx->left_tw) { term_window_destroy(ctx->left_tw); ctx->left_tw = NULL; }
	if (ctx->right_tw) { term_window_destroy(ctx->right_tw); ctx->right_tw = NULL; }
	g_hash_table_remove(mw_to_ctx, mw);
	g_free(ctx);
}

static void position_tw(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx)
{
	int y = mw->first_line + mw->statusbar_lines_top;
	int h = mw->height - mw->statusbar_lines;
	if (sp_enable_left && ctx->left_w > 0) {
		int x = mw->first_column;
		int w = ctx->left_w;
		if (ctx->left_tw) term_window_move(ctx->left_tw, x, y, w, h);
		else ctx->left_tw = term_window_create(x, y, w, h);
	}
	else if (ctx->left_tw) { term_window_destroy(ctx->left_tw); ctx->left_tw = NULL; }
	if (sp_enable_right && ctx->right_w > 0) {
		int w = ctx->right_w;
		int x = mw->last_column - w + 1;
		if (ctx->right_tw) term_window_move(ctx->right_tw, x, y, w, h);
		else ctx->right_tw = term_window_create(x, y, w, h);
	}
	else if (ctx->right_tw) { term_window_destroy(ctx->right_tw); ctx->right_tw = NULL; }
}

static void draw_border_vertical(TERM_WINDOW *tw, int left)
{
	if (!tw) return;
	int h = tw->height;
	int x = left ? tw->width - 1 : 0;
	for (int i = 0; i < h; i++) {
		term_move(tw, x, i);
		term_addch(tw, '|');
	}
}

static void clear_window(TERM_WINDOW *tw)
{
	if (!tw) return;
	for (int i = 0; i < tw->height; i++) {
		term_window_clrtoeol(tw, i);
	}
}

static void draw_str_themed(TERM_WINDOW *tw, int x, int y, WINDOW_REC *wctx, int format_id, const char *text)
{
	/* Resolve format via theme and render raw to terminal (attr handling simplified) */
	TEXT_DEST_REC dest; format_create_dest(&dest, NULL, NULL, 0, wctx);
	THEME_REC *theme = window_get_theme(wctx);
	char *out = format_get_text_theme(theme, MODULE_NAME, &dest, format_id, text);
	term_move(tw, x, y);
	/* naive: ignore embedded GUI flags here, printed literally; could parse later */
	term_addstr(tw, out);
	g_free(out);
}

static int compute_activity_for_channel(CHANNEL_REC *ch)
{
	/* Placeholder: use window activity_level if available from mainwindow-activity.
	   Fallback 0 = none. */
	(void)ch; return 0;
}

static void draw_left_contents(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx)
{
	TERM_WINDOW *tw = ctx->left_tw;
	if (!tw) return;
	clear_window(tw);
	int row = 0;
	int skip = ctx->left_scroll_offset;
	int height = tw->height;
	int index = 0;
	/* Show servers, channels, queries with scroll + selection marker */
	for (GSList *st = servers; st && row < height; st = st->next) {
		SERVER_REC *srv = st->data;
		const char *net = srv->connrec && srv->connrec->chatnet ? srv->connrec->chatnet : (srv->connrec ? srv->connrec->address : "server");
		if (index++ >= skip && row < height) {
			term_move(tw, 0, row);
			term_addch(tw, (index-1 == ctx->left_selected_index) ? '>' : ' ');
			draw_str_themed(tw, 1, row, mw->active, TXT_SIDEPANEL_HEADER, net ? net : "server");
			row++;
		}
		for (GSList *ct = srv->channels; ct && row < height; ct = ct->next) {
			CHANNEL_REC *ch = ct->data; if (!ch || !ch->name) continue;
			int activity = compute_activity_for_channel(ch);
			if (index++ >= skip && row < height) {
				term_move(tw, 0, row);
				term_addch(tw, (index-1 == ctx->left_selected_index) ? '>' : ' ');
				int format = (index-1 == ctx->left_selected_index) ? TXT_SIDEPANEL_ITEM_SELECTED : (activity ? TXT_SIDEPANEL_ITEM_ACTIVITY : TXT_SIDEPANEL_ITEM);
				draw_str_themed(tw, 1, row, mw->active, format, ch->name);
				row++;
			}
		}
		for (GSList *qt = srv->queries; qt && row < height; qt = qt->next) {
			QUERY_REC *q = qt->data; if (!q || !q->name) continue;
			if (index++ >= skip && row < height) {
				term_move(tw, 0, row);
				term_addch(tw, (index-1 == ctx->left_selected_index) ? '>' : ' ');
				int format = (index-1 == ctx->left_selected_index) ? TXT_SIDEPANEL_ITEM_SELECTED : TXT_SIDEPANEL_ITEM;
				draw_str_themed(tw, 1, row, mw->active, format, q->name);
				row++;
			}
		}
	}
	draw_border_vertical(tw, TRUE);
}

static void draw_right_contents(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx)
{
	TERM_WINDOW *tw = ctx->right_tw;
	if (!tw) return;
	clear_window(tw);
	WINDOW_REC *aw = mw->active;
	int height = tw->height;
	int skip = ctx->right_scroll_offset;
	int index = 0, row = 0;
	if (!aw || !aw->active) { draw_border_vertical(tw, FALSE); return; }
	if (!IS_CHANNEL(aw->active)) { draw_border_vertical(tw, FALSE); return; }
	CHANNEL_REC *ch = CHANNEL(aw->active);
	GSList *nicks = nicklist_getnicks(ch);
	for (GSList *nt = nicks; nt && row < height; nt = nt->next) {
		NICK_REC *nick = nt->data;
		if (!nick || !nick->nick) continue;
		if (index++ < skip) continue;
		term_move(tw, 1, row);
		term_addch(tw, (index-1 == ctx->right_selected_index) ? '>' : ' ');
		int format = TXT_SIDEPANEL_NICK_NORMAL;
		if (nick->op) format = TXT_SIDEPANEL_NICK_OP; else if (nick->voice) format = TXT_SIDEPANEL_NICK_VOICE;
		draw_str_themed(tw, 2, row, mw->active, format, nick->nick);
		row++;
	}
	draw_border_vertical(tw, FALSE);
}

static void redraw_one(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
	if (!ctx) return;
	position_tw(mw, ctx);
	draw_left_contents(mw, ctx);
	draw_right_contents(mw, ctx);
}

static void redraw_all(void)
{
	for (GSList *t = mainwindows; t; t = t->next) redraw_one(t->data);
}

static void sig_mainwindow_resized(MAIN_WINDOW_REC *mw)
{
	redraw_one(mw);
}

static void sig_window_changed(WINDOW_REC *w)
{
	(void)w;
	redraw_all();
}

static void sig_channel_list_changed(CHANNEL_REC *ch)
{
	(void)ch; redraw_all();
}

static void sig_query_list_changed(QUERY_REC *q)
{
	(void)q; redraw_all();
}

static void setup_ctx_for(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, TRUE);
	ctx->left_w = (sp_enable_left ? sp_left_width : 0);
	ctx->right_w = (sp_enable_right ? sp_right_width : 0);
	/* keep selection/scroll */
	position_tw(mw, ctx);
}

static void apply_and_redraw(void)
{
	GSList *tmp;
	for (tmp = mainwindows; tmp; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		setup_ctx_for(mw);
	}
	redraw_all();
}

/* Simple mouse parser state for SGR (1006) mode: ESC [ < btn ; x ; y M/m */
static gboolean mouse_tracking_enabled = FALSE;
static int mouse_state = 0; /* 0 idle, >0 reading sequence */
static char mouse_buf[64];
static int mouse_len = 0;

static gboolean handle_click_at(int x, int y, int button)
{
	/* Iterate mainwindows and hit-test left/right term windows */
	for (GSList *mt = mainwindows; mt; mt = mt->next) {
		MAIN_WINDOW_REC *mw = mt->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (!ctx) continue;
		if (ctx->left_tw) {
			int px = ctx->left_tw->x, py = ctx->left_tw->y, pw = ctx->left_tw->width, ph = ctx->left_tw->height;
			if (x >= px && x < px+pw && y >= py && y < py+ph) {
				/* Map row to item using scroll offset */
				int row = y - py;
				int target_index = ctx->left_scroll_offset + row;
				int idx = 0;
				for (GSList *st = servers; st; st = st->next) {
					SERVER_REC *srv = st->data;
					if (idx++ == target_index) { /* select and activate server's window */
						ctx->left_selected_index = target_index;
						WINDOW_REC *w = window_find_level(srv, MSGLEVEL_ALL);
						if (w) { window_set_active(w); }
						redraw_one(mw);
						return TRUE;
					}
					for (GSList *ct = srv->channels; ct; ct = ct->next) {
						CHANNEL_REC *ch = ct->data;
						if (idx++ == target_index) {
							ctx->left_selected_index = target_index;
							WINDOW_REC *w = window_item_window((WI_ITEM_REC*)ch);
							if (!w) w = window_find_item(ch->server, ch->name);
							if (w) { window_set_active(w); }
							redraw_one(mw);
							return TRUE;
						}
					}
					for (GSList *qt = srv->queries; qt; qt = qt->next) {
						QUERY_REC *q = qt->data;
						if (idx++ == target_index) {
							ctx->left_selected_index = target_index;
							WINDOW_REC *w = window_item_window((WI_ITEM_REC*)q);
							if (!w) w = window_find_item(q->server, q->name);
							if (w) { window_set_active(w); }
							redraw_one(mw);
							return TRUE;
						}
					}
				}
			}
		}
		if (ctx->right_tw) {
			int px = ctx->right_tw->x, py = ctx->right_tw->y, pw = ctx->right_tw->width, ph = ctx->right_tw->height;
			if (x >= px && x < px+pw && y >= py && y < py+ph) {
				int row = y - py;
				WINDOW_REC *aw = mw->active;
				if (aw && IS_CHANNEL(aw->active)) {
					CHANNEL_REC *ch = CHANNEL(aw->active);
					GSList *nicks = nicklist_getnicks(ch);
					int target_index = ctx->right_scroll_offset + row;
					int idx = 0;
					for (GSList *nt = nicks; nt; nt = nt->next) {
						if (idx++ == target_index) {
							ctx->right_selected_index = target_index;
							NICK_REC *nick = nt->data;
							if (nick && nick->nick)
								signal_emit("command query", 3, nick->nick, ch->server, ch);
							redraw_one(mw);
							return TRUE;
						}
					}
				}
			}
		}
	}
	return FALSE;
}

gboolean sidepanels_try_parse_mouse_key(unichar key)
{
	/* Enable basic parser for ESC [ < ... */
	if (!mouse_tracking_enabled) return FALSE;
	if (mouse_state == 0) {
		if (key == 0x1b) { mouse_state = 1; mouse_len = 0; return TRUE; }
		return FALSE;
	} else if (mouse_state == 1) {
		if (key == '[') { mouse_state = 2; return TRUE; }
		mouse_state = 0; return FALSE;
	} else if (mouse_state >= 2) {
		if (mouse_len < (int)sizeof(mouse_buf)-1) mouse_buf[mouse_len++] = (char)key;
		mouse_buf[mouse_len] = '\0';
		/* Try to match pattern: "<b;x;yM" or "<b;x;ym" */
		char *s = mouse_buf;
		if (*s != '<') return TRUE; /* keep buffering */
		char *sc1 = strchr(s, ';'); if (!sc1) return TRUE;
		char *sc2 = strchr(sc1+1, ';'); if (!sc2) return TRUE;
		char *end = sc2+1; if (*end == '\0') return TRUE;
		char last = end[strlen(end)-1];
		if (last != 'M' && last != 'm') return TRUE;
		int braw = atoi(s+1);
		int x = atoi(sc1+1);
		int y = atoi(sc2+1);
		/* Convert 1-based to 0-based */
		x -= 1; y -= 1;
		gboolean press = (last == 'M');
		/* Reset state */
		mouse_state = 0; mouse_len = 0;
		/* Wheel? (SGR: 64=up, 65=down, 66=left, 67=right) */
		if ((braw & 64) && press) {
			int dir = braw - 64; /* 0 up, 1 down */
			/* Scroll panel under cursor by 3 lines */
			for (GSList *mt = mainwindows; mt; mt = mt->next) {
				MAIN_WINDOW_REC *mw = mt->data;
				SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
				if (!ctx) continue;
				if (ctx->left_tw) {
					int px = ctx->left_tw->x, py = ctx->left_tw->y, pw = ctx->left_tw->width, ph = ctx->left_tw->height;
					if (x >= px && x < px+pw && y >= py && y < py+ph) {
						int delta = (dir == 0 ? -3 : 3);
						ctx->left_scroll_offset = MAX(0, ctx->left_scroll_offset + delta);
						redraw_one(mw);
						return TRUE;
					}
				}
				if (ctx->right_tw) {
					int px = ctx->right_tw->x, py = ctx->right_tw->y, pw = ctx->right_tw->width, ph = ctx->right_tw->height;
					if (x >= px && x < px+pw && y >= py && y < py+ph) {
						int delta = (dir == 0 ? -3 : 3);
						ctx->right_scroll_offset = MAX(0, ctx->right_scroll_offset + delta);
						redraw_one(mw);
						return TRUE;
					}
				}
			}
			return TRUE;
		}
		/* Left button press: activate selection */
		int button = (braw & 3) + 1;
		if (press && button == 1) {
			return handle_click_at(x, y, button);
		}
		return TRUE; /* consumed */
	}
	return FALSE;
}

static void enable_mouse_tracking(void)
{
	/* SGR extended mode */
	fputs("\x1b[?1000h", stdout); /* basic */
	fputs("\x1b[?1006h", stdout); /* SGR */
	fflush(stdout);
	mouse_tracking_enabled = TRUE;
}

static void disable_mouse_tracking(void)
{
	fputs("\x1b[?1006l", stdout);
	fputs("\x1b[?1000l", stdout);
	fflush(stdout);
	mouse_tracking_enabled = FALSE;
}

void sidepanels_init(void)
{
	settings_add_bool("lookandfeel", "sidepanel_left", TRUE);
	settings_add_bool("lookandfeel", "sidepanel_right", TRUE);
	settings_add_int("lookandfeel", "sidepanel_left_width", 18);
	settings_add_int("lookandfeel", "sidepanel_right_width", 18);
	read_settings();
	mw_to_ctx = g_hash_table_new(g_direct_hash, g_direct_equal);
	/* Apply to existing */
	apply_reservations_all();
	apply_and_redraw();
	enable_mouse_tracking();
	signal_add("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
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
	signal_remove("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
	signal_remove("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized);
	signal_remove("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_remove("channel created", (SIGNAL_FUNC) sig_channel_list_changed);
	signal_remove("channel destroyed", (SIGNAL_FUNC) sig_channel_list_changed);
	signal_remove("query created", (SIGNAL_FUNC) sig_query_list_changed);
	signal_remove("query destroyed", (SIGNAL_FUNC) sig_query_list_changed);
	/* Remove reservations */
	GSList *tmp;
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		destroy_ctx(mw);
		if (mw->statusbar_columns_left)
			mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, 0);
		if (mw->statusbar_columns_right)
			mainwindow_set_statusbar_columns(mw, 0, -mw->statusbar_columns_right);
	}
	if (mw_to_ctx) { g_hash_table_destroy(mw_to_ctx); mw_to_ctx = NULL; }
	disable_mouse_tracking();
}