/*
 * sidebar.c: right-side columns for window list and nicklist
 */
#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/servers.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/core/nicklist.h>
#include <irssi/src/irc/core/irc-channels.h>

#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/fe-text/term.h>
#include <irssi/src/fe-text/mainwindows.h>

/* Configuration */
static int sidebar_enabled;
static int winlist_width;
static int nicklist_width;

/* Global window list sidebar (root-level) */
static TERM_WINDOW *winlist_term;

/* Per-mainwindow nicklist sidebar */
typedef struct {
    MAIN_WINDOW_REC *mainwin;
    TERM_WINDOW *term;
} NICK_SIDEBAR;

static GSList *nick_sidebars; /* list of NICK_SIDEBAR* */

static NICK_SIDEBAR *nick_sidebar_find(MAIN_WINDOW_REC *mw)
{
    GSList *tmp;
    for (tmp = nick_sidebars; tmp != NULL; tmp = tmp->next) {
        NICK_SIDEBAR *ns = tmp->data;
        if (ns->mainwin == mw) return ns;
    }
    return NULL;
}

static void nick_sidebar_destroy(NICK_SIDEBAR *ns)
{
    if (ns == NULL) return;
    if (ns->term) term_window_destroy(ns->term);
    nick_sidebars = g_slist_remove(nick_sidebars, ns);
    g_free(ns);
}

static gint nick_alpha_cmp(gconstpointer ap, gconstpointer bp)
{
    const NICK_REC *a = ap;
    const NICK_REC *b = bp;
    if (a == NULL || b == NULL) return 0;
    return g_ascii_strcasecmp(a->nick ? a->nick : "", b->nick ? b->nick : "");
}

static void sidebar_redraw_winlist(void)
{
    int y;
    GSList *swins;
    if (!sidebar_enabled || winlist_term == NULL) return;

    term_window_clear(winlist_term);

    y = 0;
    swins = windows_get_sorted();
    for (GSList *t = swins; t != NULL; t = t->next) {
        WINDOW_REC *w = t->data;
        const char *name = window_get_active_name(w);
        char line[256];
        g_snprintf(line, sizeof(line), "%2d %s%s",
                   w->refnum,
                   w->data_level >= DATA_LEVEL_HILIGHT ? "*" : " ",
                   name ? name : "");
        term_move(winlist_term, 0, y++);
        term_addstr(winlist_term, line);
        if (y >= term_height) break;
    }
    g_slist_free(swins);
}

static void sidebar_redraw_nicklist(MAIN_WINDOW_REC *mw)
{
    NICK_SIDEBAR *ns;
    WINDOW_REC *w;
    WI_ITEM_REC *item;
    CHANNEL_REC *chan;
    GSList *nicks;
    int y;

    if (!sidebar_enabled || mw == NULL) return;
    ns = nick_sidebar_find(mw);
    if (ns == NULL || ns->term == NULL) return;

    term_window_clear(ns->term);

    w = mw->active;
    if (w == NULL || w->active == NULL) return;
    item = w->active;

    /* Only show nicklist for channel items */
    chan = CHANNEL(item);
    if (!chan) return;

    nicks = nicklist_getnicks(chan);
    nicks = g_slist_sort(nicks, nick_alpha_cmp);

    y = 0;
    for (GSList *t = nicks; t != NULL; t = t->next) {
        NICK_REC *nick = t->data;
        char prefix = nick->op ? '@' : (nick->halfop ? '%' : (nick->voice ? '+' : ' '));
        char line[256];
        g_snprintf(line, sizeof(line), "%c %s", prefix, nick->nick);
        term_move(ns->term, 0, y++);
        term_addstr(ns->term, line);
        if (y >= mw->height) break;
    }
    g_slist_free(nicks);
    /* nicklist_getnicks returns internal list; do not free elements */
}

static void sidebar_resize_layout(void)
{
    GSList *tmp;
    if (!sidebar_enabled) return;

    /* Reserve global right columns for window list */
    mainwindows_reserve_columns(0, winlist_width);

    /* Create/resize root winlist TERM_WINDOW */
    if (winlist_term == NULL)
        winlist_term = term_window_create(term_width - winlist_width, 0, winlist_width, term_height);
    else
        term_window_move(winlist_term, term_width - winlist_width, 0, winlist_width, term_height);

    /* For each mainwindow, reserve per-window right columns for nicklist and create TERM_WINDOW */
    for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
        MAIN_WINDOW_REC *mw;
        NICK_SIDEBAR *ns;
        int x, y, h, w;
        mw = tmp->data;
        ns = nick_sidebar_find(mw);
        if (ns == NULL) {
            ns = g_new0(NICK_SIDEBAR, 1);
            ns->mainwin = mw;
            nick_sidebars = g_slist_append(nick_sidebars, ns);
        }
        mainwindow_set_statusbar_columns(mw, 0, nicklist_width);
        /* Place term window at the right edge of the mainwindow's area */
        x = mw->last_column - mw->statusbar_columns_right + 1;
        y = mw->first_line + mw->statusbar_lines_top;
        h = mw->height - mw->statusbar_lines;
        w = nicklist_width;
        if (ns->term == NULL)
            ns->term = term_window_create(x, y, w, h);
        else
            term_window_move(ns->term, x, y, w, h);
    }
}

static void sidebar_release_layout(void)
{
    /* Release per-mainwindow columns and destroy per-window terms */
    GSList *tmp;
    for (tmp = nick_sidebars; tmp != NULL; tmp = tmp->next) {
        NICK_SIDEBAR *ns = tmp->data;
        if (ns->mainwin != NULL)
            mainwindow_set_statusbar_columns(ns->mainwin, 0, -nicklist_width);
        if (ns->term) term_window_destroy(ns->term);
        ns->term = NULL;
    }

    /* Release global right columns */
    mainwindows_reserve_columns(0, -winlist_width);

    if (winlist_term) {
        term_window_destroy(winlist_term);
        winlist_term = NULL;
    }
}

/* Signals */
static void sig_terminal_resized_sidebar(void)
{
    GSList *tmp;
    sidebar_resize_layout();
    sidebar_redraw_winlist();
    /* Redraw all nicklists */
    for (tmp = mainwindows; tmp != NULL; tmp = tmp->next)
        sidebar_redraw_nicklist(tmp->data);
}

static void sig_mainwindow_resized_sidebar(MAIN_WINDOW_REC *mw)
{
    sidebar_resize_layout();
    sidebar_redraw_nicklist(mw);
}

static void sig_window_changed_sidebar(void)
{
    sidebar_redraw_winlist();
    if (active_mainwin)
        sidebar_redraw_nicklist(active_mainwin);
}

static void sig_window_activity_sidebar(WINDOW_REC *w, gpointer old)
{
    (void)old;
    if (!sidebar_enabled) return;
    sidebar_redraw_winlist();
}

static void sig_nicklist_new(CHANNEL_REC *c, NICK_REC *n)
{
    (void)c; (void)n;
    if (active_mainwin) sidebar_redraw_nicklist(active_mainwin);
}

static void sig_nicklist_remove(CHANNEL_REC *c, NICK_REC *n)
{
    (void)c; (void)n;
    if (active_mainwin) sidebar_redraw_nicklist(active_mainwin);
}

static void sig_nicklist_changed(CHANNEL_REC *c, NICK_REC *n, const char *old)
{
    (void)c; (void)n; (void)old;
    if (active_mainwin) sidebar_redraw_nicklist(active_mainwin);
}

static void sig_nick_simple2(CHANNEL_REC *c, NICK_REC *n)
{
    (void)c; (void)n;
    if (active_mainwin) sidebar_redraw_nicklist(active_mainwin);
}

static void sig_nick_mode_changed(IRC_CHANNEL_REC *chan, NICK_REC *nick, const char *setby, const char *mode, const char *type)
{
    (void)chan; (void)nick; (void)setby; (void)mode; (void)type;
    if (active_mainwin) sidebar_redraw_nicklist(active_mainwin);
}

/* Public init/deinit */
void sidebar_init(void)
{
    settings_add_bool("lookandfeel", "sidebar_enabled", TRUE);
    settings_add_int("lookandfeel", "sidebar_winlist_width", 14);
    settings_add_int("lookandfeel", "sidebar_nicklist_width", 16);

    sidebar_enabled = settings_get_bool("sidebar_enabled");
    winlist_width = settings_get_int("sidebar_winlist_width");
    nicklist_width = settings_get_int("sidebar_nicklist_width");

    if (!sidebar_enabled)
        return;

    winlist_term = NULL;
    nick_sidebars = NULL;

    sidebar_resize_layout();
    sidebar_redraw_winlist();

    /* Signals */
    signal_add("terminal resized", (SIGNAL_FUNC) sig_terminal_resized_sidebar);
    signal_add("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized_sidebar);
    signal_add("mainwindow moved", (SIGNAL_FUNC) sig_mainwindow_resized_sidebar);
    signal_add("window changed", (SIGNAL_FUNC) sig_window_changed_sidebar);
    signal_add("window activity", (SIGNAL_FUNC) sig_window_activity_sidebar);
    signal_add("window hilight", (SIGNAL_FUNC) sig_window_activity_sidebar);

    signal_add("nicklist new", (SIGNAL_FUNC) sig_nicklist_new);
    signal_add("nicklist remove", (SIGNAL_FUNC) sig_nicklist_remove);
    signal_add("nicklist changed", (SIGNAL_FUNC) sig_nicklist_changed);
    signal_add("nicklist host changed", (SIGNAL_FUNC) sig_nick_simple2);
    signal_add("nicklist account changed", (SIGNAL_FUNC) sig_nick_simple2);
    signal_add("nicklist gone changed", (SIGNAL_FUNC) sig_nick_simple2);
    signal_add("nicklist serverop changed", (SIGNAL_FUNC) sig_nick_simple2);
    signal_add("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_changed);
}

void sidebar_deinit(void)
{
    if (!sidebar_enabled)
        return;

    signal_remove("terminal resized", (SIGNAL_FUNC) sig_terminal_resized_sidebar);
    signal_remove("mainwindow resized", (SIGNAL_FUNC) sig_mainwindow_resized_sidebar);
    signal_remove("mainwindow moved", (SIGNAL_FUNC) sig_mainwindow_resized_sidebar);
    signal_remove("window changed", (SIGNAL_FUNC) sig_window_changed_sidebar);
    signal_remove("window activity", (SIGNAL_FUNC) sig_window_activity_sidebar);
    signal_remove("window hilight", (SIGNAL_FUNC) sig_window_activity_sidebar);

    signal_remove("nicklist new", (SIGNAL_FUNC) sig_nicklist_new);
    signal_remove("nicklist remove", (SIGNAL_FUNC) sig_nicklist_remove);
    signal_remove("nicklist changed", (SIGNAL_FUNC) sig_nicklist_changed);
    signal_remove("nicklist host changed", (SIGNAL_FUNC) sig_nick_simple2);
    signal_remove("nicklist account changed", (SIGNAL_FUNC) sig_nick_simple2);
    signal_remove("nicklist gone changed", (SIGNAL_FUNC) sig_nick_simple2);
    signal_remove("nicklist serverop changed", (SIGNAL_FUNC) sig_nick_simple2);
    signal_remove("nick mode changed", (SIGNAL_FUNC) sig_nick_mode_changed);

    /* Destroy sidebar resources */
    sidebar_release_layout();

    while (nick_sidebars != NULL) {
        nick_sidebar_destroy(nick_sidebars->data);
    }
}