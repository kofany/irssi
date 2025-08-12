#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/commands.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/gui-windows.h>
#include <irssi/src/fe-text/term.h>

static int sp_left_width;
static int sp_right_width;
static int sp_enabled;

static void sp_read_settings(void)
{
    sp_enabled = settings_get_bool("sidepanels_enabled");
    sp_left_width = settings_get_int("sidepanel_left_width");
    sp_right_width = settings_get_int("sidepanel_right_width");
    if (sp_left_width < 0) sp_left_width = 0;
    if (sp_right_width < 0) sp_right_width = 0;
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

static void sp_draw_left_channels(MAIN_WINDOW_REC *mw)
{
    if (mw->left_panel_win == NULL) return;
    term_window_clear(mw->left_panel_win);

    GSList *tmp;
    int y = 0;
    for (tmp = windows; tmp != NULL && y < mw->left_panel_win->height; tmp = tmp->next) {
        WINDOW_REC *w = tmp->data;
        /* tylko okna należące do tego mainwindow */
        if (WINDOW_MAIN(w) != mw) continue;
        const char *name = window_get_active_name(w);
        if (name == NULL) name = "(unnamed)";

        if (w == mw->active) {
            term_set_color(mw->left_panel_win, ATTR_REVERSE);
        }
        term_move(mw->left_panel_win, 0, y);
        /* przytnij do szerokości */
        int maxw = mw->left_panel_win->width;
        int i;
        for (i = 0; name[i] != '\0' && i < maxw; i++)
            term_addch(mw->left_panel_win, name[i]);
        if (w == mw->active) {
            term_set_color(mw->left_panel_win, ATTR_RESET);
        }
        y++;
    }
    /* pionowy separator */
    for (y = 0; y < mw->left_panel_win->height; y++) {
        term_move(mw->left_panel_win, mw->left_panel_win->width - 1, y);
        term_addch(mw->left_panel_win, '|');
    }
    term_refresh(mw->left_panel_win);
}

static void sp_draw_right_nicklist(MAIN_WINDOW_REC *mw)
{
    /* Placeholder: tylko separator pionowy, nicklista w kolejnych krokach */
    if (mw->right_panel_win == NULL) return;
    int y;
    term_window_clear(mw->right_panel_win);
    for (y = 0; y < mw->right_panel_win->height; y++) {
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
    }

    cmd_params_free(free_arg);
}

void sidepanels_init(void)
{
    settings_add_bool("lookandfeel", "sidepanels_enabled", FALSE);
    settings_add_int("lookandfeel", "sidepanel_left_width", 14);
    settings_add_int("lookandfeel", "sidepanel_right_width", 16);

    sp_read_settings();
    sp_apply_all();

    signal_add("mainwindow resized", (SIGNAL_FUNC) sp_sig_mainwindow_resized);
    signal_add("mainwindow moved", (SIGNAL_FUNC) sp_sig_mainwindow_resized);
    signal_add("window changed", (SIGNAL_FUNC) sp_sig_window_changed);
    signal_add("setup changed", (SIGNAL_FUNC) sp_read_settings);

    command_bind("panel", NULL, (SIGNAL_FUNC) cmd_panel);
}

void sidepanels_deinit(void)
{
    command_unbind("panel", (SIGNAL_FUNC) cmd_panel);

    signal_remove("mainwindow resized", (SIGNAL_FUNC) sp_sig_mainwindow_resized);
    signal_remove("mainwindow moved", (SIGNAL_FUNC) sp_sig_mainwindow_resized);
    signal_remove("window changed", (SIGNAL_FUNC) sp_sig_window_changed);
    signal_remove("setup changed", (SIGNAL_FUNC) sp_read_settings);

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
}