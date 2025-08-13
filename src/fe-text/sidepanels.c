#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/commands.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/core/nicklist.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/gui-windows.h>
#include <irssi/src/fe-text/term.h>

static int sp_left_width;
static int sp_right_width;
static int sp_enabled;

/* focus state and per-mainwindow context */
typedef enum { SP_FOCUS_NONE = 0, SP_FOCUS_LEFT = 1, SP_FOCUS_RIGHT = 2 } SP_FOCUS;
static SP_FOCUS sp_focus = SP_FOCUS_NONE;

typedef struct SP_MAINWIN_CTX {
    int left_scroll;
    int left_selected_index; /* index in filtered windows list */
    int right_scroll;
} SP_MAINWIN_CTX;

static GHashTable *sp_ctx_ht; /* MAIN_WINDOW_REC* -> SP_MAINWIN_CTX* */

static SP_MAINWIN_CTX *sp_get_ctx(MAIN_WINDOW_REC *mw)
{
    SP_MAINWIN_CTX *ctx = g_hash_table_lookup(sp_ctx_ht, mw);
    if (ctx == NULL) {
        ctx = g_new0(SP_MAINWIN_CTX, 1);
        ctx->left_scroll = 0;
        ctx->left_selected_index = -1; /* -1 means follow active */
        ctx->right_scroll = 0;
        g_hash_table_insert(sp_ctx_ht, mw, ctx);
    }
    return ctx;
}

static void sp_free_ctx(gpointer key, gpointer value, gpointer user_data)
{
    g_free(value);
}

static void sp_read_settings(void)
{
    sp_enabled = settings_get_bool("sidepanels_enabled");
    sp_left_width = settings_get_int("sidepanel_left_width");
    sp_right_width = settings_get_int("sidepanel_right_width");
    if (sp_left_width < 0) sp_left_width = 0;
    if (sp_right_width < 0) sp_right_width = 0;
}

/* simple auto-hide policy for narrow screens */
static void sp_adjust_for_narrow_screen(int *left, int *right)
{
    int center = term_width - *left - *right;
    if (center < 28 && *right > 0) {
        *right = 0;
        center = term_width - *left - *right;
    }
    if (center < 20 && *left > 0) {
        *left = 0;
    }
}

static void sp_apply_to_window(MAIN_WINDOW_REC *mw)
{
    if (!sp_enabled) {
        /* clear columns */
        if (mw->statusbar_columns_left != 0 || mw->statusbar_columns_right != 0) {
            /* negative deltas to remove all */
            mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, -mw->statusbar_columns_right);
        }
        return;
    }
    int left = sp_left_width;
    int right = sp_right_width;

    sp_adjust_for_narrow_screen(&left, &right);

    /* set absolute widths: compute deltas */
    int dleft = left - mw->statusbar_columns_left;
    int dright = right - mw->statusbar_columns_right;
    if (dleft != 0 || dright != 0)
        mainwindow_set_statusbar_columns(mw, dleft, dright);
}

static void sp_apply_all(void)
{
    GSList *tmp;
    for (tmp = mainwindows; tmp != NULL; tmp = tmp->next)
        sp_apply_to_window(tmp->data);
}

/* build filtered list of windows belonging to mw */
static GSList *sp_windows_of(MAIN_WINDOW_REC *mw)
{
    GSList *out = NULL;
    for (GSList *t = windows; t != NULL; t = t->next) {
        WINDOW_REC *w = t->data;
        if (WINDOW_MAIN(w) == mw)
            out = g_slist_append(out, w);
    }
    return out;
}

/* helper: print name truncated with ellipsis if needed */
static void sp_addstr_ellipsis(TERM_WINDOW *tw, const char *str, int maxw)
{
    if (maxw <= 0) return;
    int len = 0;
    while (str[len] && len < maxw) len++;
    if (str[len] == '\0') {
        for (int i = 0; i < len; i++) term_addch(tw, str[i]);
        return;
    }
    /* need ellipsis */
    if (maxw >= 1) {
        int keep = maxw - 1;
        for (int i = 0; i < keep && str[i]; i++) term_addch(tw, str[i]);
        term_addch(tw, '\xE2'); /* '…' in UTF-8: E2 80 A6; fallback: '.' */
        term_addch(tw, '\x80');
        term_addch(tw, '\xA6');
    }
}

static void sp_draw_left_channels(MAIN_WINDOW_REC *mw)
{
    if (mw->left_panel_win == NULL) return;
    term_window_clear(mw->left_panel_win);

    SP_MAINWIN_CTX *ctx = sp_get_ctx(mw);

    GSList *list = sp_windows_of(mw);
    int count = g_slist_length(list);

    /* determine selection index: follow active if not set */
    int sel = ctx->left_selected_index;
    if (sel < 0) {
        sel = 0;
        int idx = 0;
        for (GSList *t = list; t != NULL; t = t->next, idx++) {
            if (t->data == mw->active) { sel = idx; break; }
        }
    } else if (sel >= count) {
        sel = count - 1;
    }

    /* ensure scroll fits selection */
    if (sel < ctx->left_scroll) ctx->left_scroll = sel;
    int visible = mw->left_panel_win->height;
    if (sel >= ctx->left_scroll + visible) ctx->left_scroll = sel - visible + 1;
    if (ctx->left_scroll < 0) ctx->left_scroll = 0;

    int y = 0;
    int idx = 0;
    for (GSList *t = list; t != NULL && y < mw->left_panel_win->height; t = t->next, idx++) {
        if (idx < ctx->left_scroll) continue;
        WINDOW_REC *w = t->data;
        const char *name = window_get_active_name(w);
        if (name == NULL) name = "(unnamed)";

        /* activity indicator */
        char indicator = ' ';
        if (w->data_level >= DATA_LEVEL_HILIGHT) indicator = '!';
        else if (w->data_level >= DATA_LEVEL_MSG) indicator = '*';

        /* selection/focus/active highlighting */
        int maxw = mw->left_panel_win->width;
        int x = 0;
        term_move(mw->left_panel_win, 0, y);
        term_addch(mw->left_panel_win, indicator);
        x++;
        if (idx == sel && sp_focus == SP_FOCUS_LEFT) {
            term_set_color(mw->left_panel_win, ATTR_UNDERLINE);
        } else if (w == mw->active) {
            term_set_color(mw->left_panel_win, ATTR_REVERSE);
        }
        sp_addstr_ellipsis(mw->left_panel_win, name, maxw - x);
        term_set_color(mw->left_panel_win, ATTR_RESET);
        y++;
    }

    /* separator */
    for (y = 0; y < mw->left_panel_win->height; y++) {
        term_move(mw->left_panel_win, mw->left_panel_win->width - 1, y);
        term_addch(mw->left_panel_win, '|');
    }
    term_refresh(mw->left_panel_win);

    g_slist_free(list);
}

static void sp_draw_right_nicklist(MAIN_WINDOW_REC *mw)
{
    if (mw->right_panel_win == NULL) return;
    term_window_clear(mw->right_panel_win);

    SP_MAINWIN_CTX *ctx = sp_get_ctx(mw);

    /* aktywny item musi być kanałem, inaczej pusta lista */
    if (mw->active != NULL && IS_CHANNEL(mw->active->active)) {
        CHANNEL_REC *chan = CHANNEL(mw->active->active);
        GSList *nicks = nicklist_getnicks(chan);
        int total = g_slist_length(nicks);
        int maxh = mw->right_panel_win->height;
        if (ctx->right_scroll < 0) ctx->right_scroll = 0;
        if (ctx->right_scroll > total - 1) ctx->right_scroll = total > 0 ? total - 1 : 0;

        int y = 0;
        int idx = 0;
        for (GSList *t = nicks; t != NULL && y < maxh; t = t->next, idx++) {
            if (idx < ctx->right_scroll) continue;
            NICK_REC *nick = t->data;
            const char *name = nick->nick;
            const char *prefix = nick->prefixes; /* e.g. @ + */
            term_move(mw->right_panel_win, 1, y);
            int maxw = mw->right_panel_win->width - 1;
            int used = 0;
            if (prefix != NULL && prefix[0] != '\0') {
                /* print prefixes truncated if needed */
                int i;
                for (i = 0; prefix[i] != '\0' && used < maxw; i++, used++)
                    term_addch(mw->right_panel_win, prefix[i]);
            }
            sp_addstr_ellipsis(mw->right_panel_win, name, maxw - used);
            y++;
        }
        /* lista z nicklist_getnicks nie jest do zwalniania tutaj */
    }
    /* separator pionowy po lewej */
    for (int y = 0; y < mw->right_panel_win->height; y++) {
        term_move(mw->right_panel_win, 0, y);
        term_addch(mw->right_panel_win, '|');
    }
    term_refresh(mw->right_panel_win);
}

static void sp_redraw_window(MAIN_WINDOW_REC *mw)
{
    sp_draw_left_channels(mw);
    sp_draw_right_nicklist(mw);
}

static void sp_sig_mainwindow_resized(MAIN_WINDOW_REC *mw)
{
    if (mw == NULL) return;
    sp_apply_to_window(mw);
    sp_redraw_window(mw);
}

static void sp_sig_mainwindow_moved(MAIN_WINDOW_REC *mw)
{
    sp_sig_mainwindow_resized(mw);
}

static void sp_sig_window_changed(void)
{
    if (active_mainwin != NULL)
        sp_redraw_window(active_mainwin);
}

/* odśwież kanały/nicki na odpowiednich sygnałach */
static void sp_sig_window_created(WINDOW_REC *w)
{
    if (w != NULL && WINDOW_MAIN(w) != NULL)
        sp_redraw_window(WINDOW_MAIN(w));
}
static void sp_sig_window_destroyed(WINDOW_REC *w)
{
    if (w != NULL && WINDOW_MAIN(w) != NULL)
        sp_redraw_window(WINDOW_MAIN(w));
}
static void sp_sig_window_refnum_changed(WINDOW_REC *w)
{
    if (w != NULL && WINDOW_MAIN(w) != NULL)
        sp_redraw_window(WINDOW_MAIN(w));
}
static void sp_sig_window_activity(WINDOW_REC *w, int old_level)
{
    if (w != NULL && WINDOW_MAIN(w) != NULL)
        sp_redraw_window(WINDOW_MAIN(w));
}

/* zmiany na kanale/nickach */
static void sp_sig_nicklist_changed(CHANNEL_REC *chan)
{
    if (active_mainwin != NULL) sp_redraw_window(active_mainwin);
}

/* navigation helpers for left/right panel */
static void sp_left_move(MAIN_WINDOW_REC *mw, int delta)
{
    SP_MAINWIN_CTX *ctx = sp_get_ctx(mw);
    GSList *list = sp_windows_of(mw);
    int count = g_slist_length(list);
    if (count == 0) { g_slist_free(list); return; }

    int sel = ctx->left_selected_index;
    if (sel < 0) {
        /* follow active - set to active index first */
        int idx = 0; sel = 0;
        for (GSList *t = list; t != NULL; t = t->next, idx++) {
            if (t->data == mw->active) { sel = idx; break; }
        }
    }
    sel += delta;
    if (sel < 0) sel = 0;
    if (sel >= count) sel = count - 1;
    ctx->left_selected_index = sel;
    g_slist_free(list);
}

static WINDOW_REC *sp_left_get_selected(MAIN_WINDOW_REC *mw)
{
    SP_MAINWIN_CTX *ctx = sp_get_ctx(mw);
    GSList *list = sp_windows_of(mw);
    int count = g_slist_length(list);
    if (count == 0) { g_slist_free(list); return NULL; }

    int sel = ctx->left_selected_index;
    if (sel < 0) {
        g_slist_free(list);
        return mw->active;
    }
    if (sel >= count) sel = count - 1;

    WINDOW_REC *w = g_slist_nth_data(list, sel);
    g_slist_free(list);
    return w;
}

static void sp_right_scroll(MAIN_WINDOW_REC *mw, int delta)
{
    SP_MAINWIN_CTX *ctx = sp_get_ctx(mw);
    ctx->right_scroll += delta;
    if (ctx->right_scroll < 0) ctx->right_scroll = 0;
}

static void cmd_panel(const char *data)
{
    char *cmd, *arg;
    void *free_arg;

    if (!cmd_get_params(data, &free_arg, 2, &cmd, &arg))
        return;

    if (g_ascii_strcasecmp(cmd, "left") == 0) {
        int n = atoi(arg);
        settings_set_int("sidepanel_left_width", n);
        sp_read_settings();
        sp_apply_all();
    } else if (g_ascii_strcasecmp(cmd, "right") == 0) {
        int n = atoi(arg);
        settings_set_int("sidepanel_right_width", n);
        sp_read_settings();
        sp_apply_all();
    } else if (g_ascii_strcasecmp(cmd, "on") == 0) {
        settings_set_bool("sidepanels_enabled", TRUE);
        sp_read_settings();
        sp_apply_all();
    } else if (g_ascii_strcasecmp(cmd, "off") == 0) {
        settings_set_bool("sidepanels_enabled", FALSE);
        sp_read_settings();
        sp_apply_all();
    } else if (g_ascii_strcasecmp(cmd, "focus") == 0) {
        if (g_ascii_strcasecmp(arg, "left") == 0) sp_focus = SP_FOCUS_LEFT;
        else if (g_ascii_strcasecmp(arg, "right") == 0) sp_focus = SP_FOCUS_RIGHT;
        else sp_focus = SP_FOCUS_NONE;
        if (active_mainwin) sp_redraw_window(active_mainwin);
    } else if (g_ascii_strcasecmp(cmd, "up") == 0) {
        if (active_mainwin) { sp_left_move(active_mainwin, -1); sp_redraw_window(active_mainwin); }
    } else if (g_ascii_strcasecmp(cmd, "down") == 0) {
        if (active_mainwin) { sp_left_move(active_mainwin, +1); sp_redraw_window(active_mainwin); }
    } else if (g_ascii_strcasecmp(cmd, "enter") == 0) {
        if (active_mainwin) {
            WINDOW_REC *w = sp_left_get_selected(active_mainwin);
            if (w != NULL) window_set_active(w);
        }
    } else if (g_ascii_strcasecmp(cmd, "rup") == 0) {
        if (active_mainwin) { sp_right_scroll(active_mainwin, -1); sp_redraw_window(active_mainwin); }
    } else if (g_ascii_strcasecmp(cmd, "rdown") == 0) {
        if (active_mainwin) { sp_right_scroll(active_mainwin, +1); sp_redraw_window(active_mainwin); }
    } else if (g_ascii_strcasecmp(cmd, "rpageup") == 0) {
        if (active_mainwin && active_mainwin->right_panel_win) { sp_right_scroll(active_mainwin, -active_mainwin->right_panel_win->height); sp_redraw_window(active_mainwin); }
    } else if (g_ascii_strcasecmp(cmd, "rpagedown") == 0) {
        if (active_mainwin && active_mainwin->right_panel_win) { sp_right_scroll(active_mainwin, +active_mainwin->right_panel_win->height); sp_redraw_window(active_mainwin); }
    }

    cmd_params_free(free_arg);
}

void sidepanels_init(void)
{
    settings_add_bool("lookandfeel", "sidepanels_enabled", FALSE);
    settings_add_int("lookandfeel", "sidepanel_left_width", 14);
    settings_add_int("lookandfeel", "sidepanel_right_width", 16);

    if (sp_ctx_ht == NULL) sp_ctx_ht = g_hash_table_new(NULL, NULL);

    sp_read_settings();
    sp_apply_all();

    signal_add("mainwindow resized", (SIGNAL_FUNC) sp_sig_mainwindow_resized);
    signal_add("mainwindow moved", (SIGNAL_FUNC) sp_sig_mainwindow_resized);
    signal_add("window changed", (SIGNAL_FUNC) sp_sig_window_changed);
    signal_add("setup changed", (SIGNAL_FUNC) sp_read_settings);

    signal_add("window created", (SIGNAL_FUNC) sp_sig_window_created);
    signal_add("window destroyed", (SIGNAL_FUNC) sp_sig_window_destroyed);
    signal_add("window refnum changed", (SIGNAL_FUNC) sp_sig_window_refnum_changed);
    signal_add("window activity", (SIGNAL_FUNC) sp_sig_window_activity);

    signal_add("nicklist changed", (SIGNAL_FUNC) sp_sig_nicklist_changed);

    command_bind("panel", NULL, (SIGNAL_FUNC) cmd_panel);
}

void sidepanels_deinit(void)
{
    command_unbind("panel", (SIGNAL_FUNC) cmd_panel);

    signal_remove("mainwindow resized", (SIGNAL_FUNC) sp_sig_mainwindow_resized);
    signal_remove("mainwindow moved", (SIGNAL_FUNC) sp_sig_mainwindow_resized);
    signal_remove("window changed", (SIGNAL_FUNC) sp_sig_window_changed);
    signal_remove("setup changed", (SIGNAL_FUNC) sp_read_settings);

    signal_remove("window created", (SIGNAL_FUNC) sp_sig_window_created);
    signal_remove("window destroyed", (SIGNAL_FUNC) sp_sig_window_destroyed);
    signal_remove("window refnum changed", (SIGNAL_FUNC) sp_sig_window_refnum_changed);
    signal_remove("window activity", (SIGNAL_FUNC) sp_sig_window_activity);

    signal_remove("nicklist changed", (SIGNAL_FUNC) sp_sig_nicklist_changed);

    /* remove columns and destroy panel windows */
    GSList *tmp;
    for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
        MAIN_WINDOW_REC *mw = tmp->data;
        if (mw->left_panel_win != NULL) {
            term_window_destroy(mw->left_panel_win);
            mw->left_panel_win = NULL;
        }
        if (mw->right_panel_win != NULL) {
            term_window_destroy(mw->right_panel_win);
            mw->right_panel_win = NULL;
        }
        if (mw->statusbar_columns_left != 0 || mw->statusbar_columns_right != 0)
            mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, -mw->statusbar_columns_right);
    }

    if (sp_ctx_ht != NULL) {
        g_hash_table_foreach(sp_ctx_ht, sp_free_ctx, NULL);
        g_hash_table_destroy(sp_ctx_ht);
        sp_ctx_ht = NULL;
    }
}