#include <src/core/module.h>
#include <src/core/signals.h>
#include <src/core/settings.h>
#include <src/core/modules.h>
#include <src/fe-common/core/fe-windows.h>
#include <src/fe-text/statusbar-item.h>
#include <src/fe-text/statusbar.h>
#include <src/fe-text/gui-windows.h>

#include <glib.h>
#include <string.h>

#undef MODULE_NAME
#define MODULE_NAME "awl/core"

/* Basic settings (subset of adv_windowlist.pl) */
static int setting_hide_data;       /* 0..3 */
static int setting_hide_empty;      /* 0/1 (hide visible windows without items) */
static char *setting_sort;          /* "refnum" | "-data_level" | "-last_line" */
static int setting_prefer_name;     /* boolean */
static char *setting_separator;     /* separator between items */

static char *awl_line; /* cached rendered line */

static void awl_settings_read(void)
{
    g_free(setting_sort); setting_sort = NULL;
    g_free(setting_separator); setting_separator = NULL;

    setting_hide_data = settings_get_int("awl_hide_data");
    setting_hide_empty = settings_get_int("awl_hide_empty");
    setting_prefer_name = settings_get_bool("awl_prefer_name");

    /* Sorting: default to refnum */
    const char *sortv = settings_get_str("awl_sort");
    setting_sort = g_strdup(sortv && *sortv ? sortv : "refnum");

    const char *sep = settings_get_str("awl_separator");
    setting_separator = g_strdup(sep && *sep ? sep : " ");
}

static const char *get_window_name(const WINDOW_REC *win)
{
    if (setting_prefer_name && win->name && *win->name) return win->name;
    return window_get_active_name((WINDOW_REC *) win);
}

static int window_sort_cmp_refnum(gconstpointer a, gconstpointer b)
{
    const WINDOW_REC *w1 = a, *w2 = b;
    return window_refnum_cmp((WINDOW_REC *) w1, (WINDOW_REC *) w2);
}

static int window_sort_cmp_data_level(gconstpointer a, gconstpointer b)
{
    const WINDOW_REC *w1 = a, *w2 = b;
    if (w1->data_level != w2->data_level) return w2->data_level - w1->data_level; /* desc */
    return window_refnum_cmp((WINDOW_REC *) w1, (WINDOW_REC *) w2);
}

static int window_sort_cmp_last_line(gconstpointer a, gconstpointer b)
{
    const WINDOW_REC *w1 = a, *w2 = b;
    if (w1->last_line != w2->last_line) return (w2->last_line > w1->last_line) ? 1 : -1; /* desc */
    return window_refnum_cmp((WINDOW_REC *) w1, (WINDOW_REC *) w2);
}

static gboolean should_hide_window(const WINDOW_REC *win)
{
    /* Hide by data level */
    if (setting_hide_data > 0 && win->data_level < setting_hide_data) return TRUE;

    /* Hide visible windows without items */
    if (setting_hide_empty > 0) {
        gboolean visible = FALSE;
        if (win->gui_data) {
            /* is_window_visible requires GUI_WINDOW_REC to be initialized */
            visible = is_window_visible((WINDOW_REC *) win);
        }
        if (visible && win->items == NULL) return TRUE;
    }

    return FALSE;
}

static void free_cached_line(void)
{
    g_free(awl_line); awl_line = NULL;
}

static void render_awl_line(void)
{
    free_cached_line();

    /* Gather windows */
    GSList *wins = windows_get_sorted(); /* ascending refnum */

    /* Sort if requested */
    if (setting_sort && g_strcmp0(setting_sort, "refnum") != 0) {
        GList *list = NULL;
        for (GSList *t = wins; t; t = t->next) list = g_list_prepend(list, t->data);
        list = g_list_reverse(list);
        if (g_strcmp0(setting_sort, "-data_level") == 0) {
            list = g_list_sort(list, window_sort_cmp_data_level);
        } else if (g_strcmp0(setting_sort, "-last_line") == 0) {
            list = g_list_sort(list, window_sort_cmp_last_line);
        } else {
            list = g_list_sort(list, window_sort_cmp_refnum);
        }
        /* Convert back to GSList to reuse simple iteration */
        GSList *nw = NULL;
        for (GList *lt = list; lt; lt = lt->next) nw = g_slist_prepend(nw, lt->data);
        nw = g_slist_reverse(nw);
        g_list_free(list);
        g_slist_free(wins);
        wins = nw;
    }

    GString *out = g_string_new(NULL);

    for (GSList *t = wins; t; t = t->next) {
        WINDOW_REC *win = t->data;
        if (win == NULL) continue;
        if (should_hide_window(win)) continue;

        const char *name = get_window_name(win);
        if (out->len > 0 && setting_separator) g_string_append(out, setting_separator);

        /* Highlight active window with brackets; add simple activity markers */
        gboolean is_active = (active_win && active_win->refnum == win->refnum);
        const char *marker = "";
        switch (win->data_level) {
            case DATA_LEVEL_HILIGHT: marker = "!"; break;
            case DATA_LEVEL_MSG: marker = "+"; break;
            case DATA_LEVEL_TEXT: marker = "."; break;
            default: marker = ""; break;
        }

        if (is_active) {
            g_string_append_printf(out, "[%d:%s%s]", win->refnum, marker, name ? name : "");
        } else {
            g_string_append_printf(out, "%d:%s%s", win->refnum, marker, name ? name : "");
        }
    }

    g_slist_free(wins);

    /* Wrap into a statusbar-safe string: use {sb $*} via default handler's value "$0" */
    awl_line = out->str; /* take ownership */
    g_string_free(out, FALSE);
}

static void awl_redraw(void)
{
    render_awl_line();
    statusbar_items_redraw("awl");
}

static void sig_setup_changed(void)
{
    awl_settings_read();
    awl_redraw();
}

static void sig_window_event(void)
{
    awl_redraw();
}

static void item_awl(SBAR_ITEM_REC *item, int get_size_only)
{
    if (awl_line == NULL) render_awl_line();

    /* similar to item_act: calculate size then draw */
    statusbar_item_default_handler(item, TRUE, NULL, awl_line ? awl_line : "", FALSE);
    statusbar_item_default_handler(item, get_size_only ? TRUE : FALSE, NULL, awl_line ? awl_line : "", FALSE);

    /* If not size-only and sizes changed, let framework handle redraw */
}

void awl_core_init(void)
{
    module_register("awl", "core");

    /* Register settings */
    settings_add_int("awl", "awl_hide_data", 0);
    settings_add_int("awl", "awl_hide_empty", 0);
    settings_add_bool("awl", "awl_prefer_name", FALSE);
    settings_add_str("awl", "awl_sort", "refnum");
    settings_add_str("awl", "awl_separator", " ");

    awl_settings_read();

    /* Register statusbar item; value of "$0" lets us feed our data string directly */
    statusbar_item_register("awl", "$0", item_awl);

    /* Signals to keep it up to date */
    signal_add("setup changed", (SIGNAL_FUNC) sig_setup_changed);
    signal_add("window changed", (SIGNAL_FUNC) sig_window_event);
    signal_add("window item changed", (SIGNAL_FUNC) sig_window_event);
    signal_add("window created", (SIGNAL_FUNC) sig_window_event);
    signal_add("window destroyed", (SIGNAL_FUNC) sig_window_event);
    signal_add("window name changed", (SIGNAL_FUNC) sig_window_event);
    signal_add("window refnum changed", (SIGNAL_FUNC) sig_window_event);
    signal_add("window hilight", (SIGNAL_FUNC) sig_window_event);

    /* Initial render */
    awl_redraw();
}

void awl_core_deinit(void)
{
    /* Unregister signals */
    signal_remove("setup changed", (SIGNAL_FUNC) sig_setup_changed);
    signal_remove("window changed", (SIGNAL_FUNC) sig_window_event);
    signal_remove("window item changed", (SIGNAL_FUNC) sig_window_event);
    signal_remove("window created", (SIGNAL_FUNC) sig_window_event);
    signal_remove("window destroyed", (SIGNAL_FUNC) sig_window_event);
    signal_remove("window name changed", (SIGNAL_FUNC) sig_window_event);
    signal_remove("window refnum changed", (SIGNAL_FUNC) sig_window_event);
    signal_remove("window hilight", (SIGNAL_FUNC) sig_window_event);

    statusbar_item_unregister("awl");

    free_cached_line();
}

void awl_core_abicheck(int *version)
{
    *version = IRSSI_ABI_VERSION;
}