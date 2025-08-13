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
#include <stdarg.h>

/* Forward declarations for static functions used before definition (C89) */
static void apply_reservations_all(void);
static void apply_and_redraw(void);
static void enable_mouse_tracking(void);
static void disable_mouse_tracking(void);

/* Settings */
static int sp_left_width;
static int sp_right_width;
static int sp_enable_left;
static int sp_enable_right;
static int sp_auto_hide_right;
static int sp_enable_mouse;
static int sp_debug;

static int esc_pending;
static int reemit_guard;

static FILE *sp_log;
static void sp_log_open(void) { if (!sp_log) sp_log = fopen("/tmp/irssi_sidepanels.log", "a"); }
static void sp_logf(const char *fmt, ...)
{
	va_list ap;
	if (!sp_debug) return;
	sp_log_open();
	if (!sp_log) return;
	va_start(ap, fmt);
	vfprintf(sp_log, fmt, ap);
	fprintf(sp_log, "\n");
	va_end(ap);
	fflush(sp_log);
}

static void read_settings(void)
{
	int old_mouse = sp_enable_mouse;
	sp_left_width = settings_get_int("sidepanel_left_width");
	sp_right_width = settings_get_int("sidepanel_right_width");
	sp_enable_left = settings_get_bool("sidepanel_left");
	sp_enable_right = settings_get_bool("sidepanel_right");
	sp_auto_hide_right = settings_get_bool("sidepanel_right_auto_hide");
	sp_enable_mouse = TRUE; /* always on natively */
	sp_debug = settings_get_bool("sidepanel_debug");
	apply_reservations_all();
	apply_and_redraw();
	if (!old_mouse) enable_mouse_tracking();
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
	/* cached geometry for hit-test and drawing */
	int left_x; int left_y; int left_h;
	int right_x; int right_y; int right_h;
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
	int y;
	int h;
	int x;
	int w;
	gboolean show_right;
	WINDOW_REC *aw;
	y = mw->first_line + mw->statusbar_lines_top;
	h = mw->height - mw->statusbar_lines;
	if (sp_enable_left && ctx->left_w > 0) {
		x = mw->first_column;
		w = ctx->left_w;
		if (ctx->left_tw) term_window_move(ctx->left_tw, x, y, w, h);
		else ctx->left_tw = term_window_create(x, y, w, h);
		ctx->left_x = x; ctx->left_y = y; ctx->left_h = h;
	}
	else if (ctx->left_tw) { term_window_destroy(ctx->left_tw); ctx->left_tw = NULL; ctx->left_h = 0; }
	/* Auto hide right if enabled and active is not channel */
	show_right = sp_enable_right && ctx->right_w > 0;
	if (sp_auto_hide_right) {
		aw = mw->active;
		if (!(aw && aw->active && IS_CHANNEL(aw->active))) show_right = FALSE;
	}
	if (show_right) {
		w = ctx->right_w;
		x = mw->last_column - w + 1;
		if (ctx->right_tw) term_window_move(ctx->right_tw, x, y, w, h);
		else ctx->right_tw = term_window_create(x, y, w, h);
		ctx->right_x = x; ctx->right_y = y; ctx->right_h = h;
	}
	else if (ctx->right_tw) { term_window_destroy(ctx->right_tw); ctx->right_tw = NULL; ctx->right_h = 0; }
}

static void draw_border_vertical(TERM_WINDOW *tw, int width, int height, int left)
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

static void clear_window_full(TERM_WINDOW *tw, int width, int height)
{
	int y;
	int x;
	if (!tw) return;
	for (y = 0; y < height; y++) {
		term_move(tw, 0, y);
		for (x = 0; x < width; x++) term_addch(tw, ' ');
	}
}

static void draw_str_themed(TERM_WINDOW *tw, int x, int y, WINDOW_REC *wctx, int format_id, const char *text)
{
	TEXT_DEST_REC dest;
	THEME_REC *theme;
	char *out;
	char *plain;
	format_create_dest(&dest, NULL, NULL, 0, wctx);
	theme = window_get_theme(wctx);
	out = format_get_text_theme(theme, MODULE_NAME, &dest, format_id, text);
	plain = strip_codes(out);
	term_move(tw, x, y);
	term_addstr(tw, plain ? plain : out);
	g_free(plain);
	g_free(out);
}

static int compute_activity_for_channel(CHANNEL_REC *ch)
{
	WI_ITEM_REC *item = (WI_ITEM_REC*)ch;
	return item ? item->data_level : 0;
}

static int compute_activity_for_query(QUERY_REC *q)
{
	WI_ITEM_REC *item = (WI_ITEM_REC*)q;
	return item ? item->data_level : 0;
}

static void draw_left_contents(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx)
{
	TERM_WINDOW *tw;
	int row;
	int skip;
	int height;
	int index;
	GSList *st;
	if (!ctx) return;
	tw = ctx->left_tw;
	if (!tw) return;
	clear_window_full(tw, ctx->left_w, ctx->left_h);
	row = 0;
	skip = ctx->left_scroll_offset;
	height = ctx->left_h;
	index = 0;
	if (servers == NULL) {
		/* Fallback: list existing windows by name */
		GSList *w;
		for (w = windows; w && row < height; w = w->next) {
			WINDOW_REC *win = w->data;
			const char *nm = window_get_active_name(win);
			if (index++ >= skip && row < height) {
				term_move(tw, 0, row);
				term_addch(tw, (index-1 == ctx->left_selected_index) ? '>' : ' ');
				draw_str_themed(tw, 1, row, mw->active, TXT_SIDEPANEL_ITEM, nm ? nm : "window");
				row++;
			}
		}
		draw_border_vertical(tw, ctx->left_w, ctx->left_h, 1);
		irssi_set_dirty();
		return;
	}
	for (st = servers; st && row < height; st = st->next) {
		SERVER_REC *srv = st->data;
		const char *net = srv->connrec && srv->connrec->chatnet ? srv->connrec->chatnet : (srv->connrec ? srv->connrec->address : "server");
		if (index++ >= skip && row < height) {
			term_move(tw, 0, row);
			term_addch(tw, (index-1 == ctx->left_selected_index) ? '>' : ' ');
			draw_str_themed(tw, 1, row, mw->active, TXT_SIDEPANEL_HEADER, net ? net : "server");
			row++;
		}
		{
			GSList *ct;
			for (ct = srv->channels; ct && row < height; ct = ct->next) {
				CHANNEL_REC *ch = ct->data; int activity; int format;
				if (!ch || !ch->name) continue;
				activity = compute_activity_for_channel(ch);
				if (index++ >= skip && row < height) {
					term_move(tw, 0, row);
					term_addch(tw, (index-1 == ctx->left_selected_index) ? '>' : ' ');
					format = (index-1 == ctx->left_selected_index) ? TXT_SIDEPANEL_ITEM_SELECTED : (activity ? TXT_SIDEPANEL_ITEM_ACTIVITY : TXT_SIDEPANEL_ITEM);
					draw_str_themed(tw, 1, row, mw->active, format, ch->name);
					row++;
				}
			}
		}
		{
			GSList *qt;
			for (qt = srv->queries; qt && row < height; qt = qt->next) {
				QUERY_REC *q = qt->data; int activity; int format;
				if (!q || !q->name) continue;
				activity = compute_activity_for_query(q);
				if (index++ >= skip && row < height) {
					term_move(tw, 0, row);
					term_addch(tw, (index-1 == ctx->left_selected_index) ? '>' : ' ');
					format = (index-1 == ctx->left_selected_index) ? TXT_SIDEPANEL_ITEM_SELECTED : (activity ? TXT_SIDEPANEL_ITEM_ACTIVITY : TXT_SIDEPANEL_ITEM);
					draw_str_themed(tw, 1, row, mw->active, format, q->name);
					row++;
				}
			}
		}
	}
	draw_border_vertical(tw, ctx->left_w, ctx->left_h, 1);
	irssi_set_dirty();
}

static void draw_right_contents(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx)
{
	TERM_WINDOW *tw;
	WINDOW_REC *aw;
	int height;
	int skip;
	int index;
	int row;
	GSList *nt;
	if (!ctx) return;
	tw = ctx->right_tw;
	if (!tw) return;
	clear_window_full(tw, ctx->right_w, ctx->right_h);
	aw = mw->active;
	height = ctx->right_h;
	skip = ctx->right_scroll_offset;
	index = 0; row = 0;
	if (!aw || !aw->active) { draw_border_vertical(tw, ctx->right_w, ctx->right_h, 0); irssi_set_dirty(); return; }
	if (!IS_CHANNEL(aw->active)) { draw_border_vertical(tw, ctx->right_w, ctx->right_h, 0); irssi_set_dirty(); return; }
	{
		CHANNEL_REC *ch = CHANNEL(aw->active);
		GSList *nicks = nicklist_getnicks(ch);
		for (nt = nicks; nt && row < height; nt = nt->next) {
			NICK_REC *nick = nt->data; int format;
			if (!nick || !nick->nick) continue;
			if (index++ < skip) continue;
			term_move(tw, 1, row);
			term_addch(tw, (index-1 == ctx->right_selected_index) ? '>' : ' ');
			format = TXT_SIDEPANEL_NICK_NORMAL;
			if (nick->op) format = TXT_SIDEPANEL_NICK_OP; else if (nick->voice) format = TXT_SIDEPANEL_NICK_VOICE;
			draw_str_themed(tw, 2, row, mw->active, format, nick->nick);
			row++;
		}
	}
	draw_border_vertical(tw, ctx->right_w, ctx->right_h, 0);
	irssi_set_dirty();
}

static void redraw_one(MAIN_WINDOW_REC *mw)
{
	SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
	if (!ctx) return;
	sp_logf("redraw_one mainwin=%p L[%d,%d w=%d h=%d] R[%d,%d w=%d h=%d]", (void*)mw, ctx->left_x, ctx->left_y, ctx->left_w, ctx->left_h, ctx->right_x, ctx->right_y, ctx->right_w, ctx->right_h);
	position_tw(mw, ctx);
	draw_left_contents(mw, ctx);
	draw_right_contents(mw, ctx);
	irssi_set_dirty();
	term_refresh(NULL);
}

static void redraw_all(void)
{
	GSList *t;
	for (t = mainwindows; t; t = t->next) redraw_one(t->data);
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
	SP_MAINWIN_CTX *ctx;
	ctx = get_ctx(mw, TRUE);
	ctx->left_w = (sp_enable_left ? sp_left_width : 0);
	ctx->right_w = (sp_enable_right ? sp_right_width : 0);
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
	GSList *mt;
	for (mt = mainwindows; mt; mt = mt->next) {
		MAIN_WINDOW_REC *mw = mt->data;
		SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
		if (!ctx) continue;
		if (ctx->left_tw) {
			int px = ctx->left_x, py = ctx->left_y, pw = ctx->left_w, ph = ctx->left_h;
			if (x >= px && x < px+pw && y >= py && y < py+ph) {
				int row = y - py;
				int target_index = ctx->left_scroll_offset + row;
				int idx = 0;
				GSList *st;
				for (st = servers; st; st = st->next) {
					SERVER_REC *srv = st->data;
					if (idx++ == target_index) {
						WINDOW_REC *w = window_find_level(srv, MSGLEVEL_ALL);
						ctx->left_selected_index = target_index;
						if (w) { window_set_active(w); }
						redraw_one(mw);
						irssi_set_dirty();
						return TRUE;
					}
					{
						GSList *ct;
						for (ct = srv->channels; ct; ct = ct->next) {
							CHANNEL_REC *ch = ct->data;
							if (idx++ == target_index) {
								WINDOW_REC *w = window_item_window((WI_ITEM_REC*)ch);
								ctx->left_selected_index = target_index;
								if (!w) w = window_find_item(ch->server, ch->name);
								if (w) { window_set_active(w); }
								redraw_one(mw);
								irssi_set_dirty();
								return TRUE;
							}
						}
					}
					{
						GSList *qt;
						for (qt = srv->queries; qt; qt = qt->next) {
							QUERY_REC *q = qt->data;
							if (idx++ == target_index) {
								WINDOW_REC *w = window_item_window((WI_ITEM_REC*)q);
								ctx->left_selected_index = target_index;
								if (!w) w = window_find_item(q->server, q->name);
								if (w) { window_set_active(w); }
								redraw_one(mw);
								irssi_set_dirty();
								return TRUE;
							}
						}
					}
				}
			}
		}
		if (ctx->right_tw) {
			int px = ctx->right_x, py = ctx->right_y, pw = ctx->right_w, ph = ctx->right_h;
			if (x >= px && x < px+pw && y >= py && y < py+ph) {
				int row = y - py;
				WINDOW_REC *aw = mw->active;
				if (aw && IS_CHANNEL(aw->active)) {
					CHANNEL_REC *ch = CHANNEL(aw->active);
					GSList *nicks = nicklist_getnicks(ch);
					int target_index = ctx->right_scroll_offset + row;
					int idx = 0;
					GSList *nt;
					for (nt = nicks; nt; nt = nt->next) {
						if (idx++ == target_index) {
							NICK_REC *nick = nt->data;
							ctx->right_selected_index = target_index;
							if (nick && nick->nick)
								signal_emit("command query", 3, nick->nick, ch->server, ch);
							redraw_one(mw);
							irssi_set_dirty();
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
	char *s;
	char *sc1;
	char *sc2;
	char *end;
	char last;
	int braw;
	int x;
	int y;
	gboolean press;
	GSList *mt;
	if (!sp_enable_mouse) return FALSE;
	if (mouse_state == 0) {
		if (key == 0x1b) { mouse_state = 1; mouse_len = 0; esc_pending = 1; return TRUE; }
		return FALSE;
	} else if (mouse_state == 1) {
		if (key == '[') { mouse_state = 2; return TRUE; }
		/* Not SGR - re-emit ESC and let this key pass */
		mouse_state = 0; mouse_len = 0;
		if (esc_pending && !reemit_guard) { reemit_guard = 1; signal_emit("gui key pressed", 1, GINT_TO_POINTER(0x1b)); reemit_guard = 0; esc_pending = 0; }
		return FALSE;
	} else if (mouse_state >= 2) {
		if (mouse_len < (int)sizeof(mouse_buf)-1) mouse_buf[mouse_len++] = (char)key;
		mouse_buf[mouse_len] = '\0';
		s = mouse_buf;
		if (*s != '<') { /* not SGR mouse - cancel */ mouse_state = 0; mouse_len = 0; esc_pending = 0; return TRUE; }
		sc1 = strchr(s, ';'); if (!sc1) return TRUE;
		sc2 = strchr(sc1+1, ';'); if (!sc2) return TRUE;
		end = sc2+1; if (*end == '\0') return TRUE;
		last = end[(int)strlen(end)-1];
		if (last != 'M' && last != 'm') return TRUE;
		braw = atoi(s+1);
		x = atoi(sc1+1);
		y = atoi(sc2+1);
		x -= 1; y -= 1;
		press = (last == 'M');
		mouse_state = 0; mouse_len = 0; esc_pending = 0;
		if ((braw & 64) && press) {
			int dir;
			int delta;
			int px, py, pw, ph;
			dir = braw - 64;
			delta = (dir == 0 ? -3 : 3);
			for (mt = mainwindows; mt; mt = mt->next) {
				MAIN_WINDOW_REC *mw = mt->data;
				SP_MAINWIN_CTX *ctx = get_ctx(mw, FALSE);
				if (!ctx) continue;
				if (ctx->left_tw) {
					px = ctx->left_x; py = ctx->left_y; pw = ctx->left_w; ph = ctx->left_h;
					if (x >= px && x < px+pw && y >= py && y < py+ph) {
						ctx->left_scroll_offset = MAX(0, ctx->left_scroll_offset + delta);
						redraw_one(mw);
						irssi_set_dirty();
						return TRUE;
					}
				}
				if (ctx->right_tw) {
					px = ctx->right_x; py = ctx->right_y; pw = ctx->right_w; ph = ctx->right_h;
					if (x >= px && x < px+pw && y >= py && y < py+ph) {
						ctx->right_scroll_offset = MAX(0, ctx->right_scroll_offset + delta);
						redraw_one(mw);
						irssi_set_dirty();
						return TRUE;
					}
				}
			}
			return TRUE;
		}
		{
			int button;
			button = (braw & 3) + 1;
			if (press && button == 1) {
				/* consume click always */
				(void)handle_click_at(x, y, button);
				return TRUE;
			}
		}
		return TRUE;
	}
	return FALSE;
}

static void enable_mouse_tracking(void)
{
	fputs("\x1b[?1000h", stdout);
	fputs("\x1b[?1006h", stdout);
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

static void sig_irssi_init_finished(void)
{
	sp_logf("signal: irssi init finished -> redraw_all");
	apply_reservations_all();
	apply_and_redraw();
}

void sidepanels_init(void)
{
	settings_add_bool("lookandfeel", "sidepanel_left", TRUE);
	settings_add_bool("lookandfeel", "sidepanel_right", TRUE);
	settings_add_int("lookandfeel", "sidepanel_left_width", 18);
	settings_add_int("lookandfeel", "sidepanel_right_width", 18);
	settings_add_bool("lookandfeel", "sidepanel_right_auto_hide", TRUE);
	settings_add_bool("lookandfeel", "sidepanel_mouse", FALSE);
	settings_add_bool("lookandfeel", "sidepanel_debug", FALSE);
	sp_enable_mouse = TRUE; /* force native */
	read_settings();
	mw_to_ctx = g_hash_table_new(g_direct_hash, g_direct_equal);
	/* Apply to existing */
	apply_reservations_all();
	apply_and_redraw();
	enable_mouse_tracking();
	esc_pending = 0; reemit_guard = 0;
	signal_add("irssi init finished", (SIGNAL_FUNC) sig_irssi_init_finished);
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
	GSList *tmp;
	signal_remove("irssi init finished", (SIGNAL_FUNC) sig_irssi_init_finished);
	signal_remove("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
	signal_remove("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized);
	signal_remove("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_remove("channel created", (SIGNAL_FUNC) sig_channel_list_changed);
	signal_remove("channel destroyed", (SIGNAL_FUNC) sig_channel_list_changed);
	signal_remove("query created", (SIGNAL_FUNC) sig_query_list_changed);
	signal_remove("query destroyed", (SIGNAL_FUNC) sig_query_list_changed);
	/* Remove reservations */
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
	if (sp_log) { fclose(sp_log); sp_log = NULL; }
}