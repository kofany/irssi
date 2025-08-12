#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
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

static void sp_redraw_window(MAIN_WINDOW_REC *mw)
{
    /* Placeholder: draw simple borders in panel windows */
    int y;
    if (mw->left_panel_win != NULL) {
        for (y = 0; y < mw->left_panel_win->height; y++) {
            term_move(mw->left_panel_win, mw->left_panel_win->width - 1, y);
            term_addch(mw->left_panel_win, '|');
        }
        term_refresh(mw->left_panel_win);
    }
    if (mw->right_panel_win != NULL) {
        for (y = 0; y < mw->right_panel_win->height; y++) {
            term_move(mw->right_panel_win, 0, y);
            term_addch(mw->right_panel_win, '|');
        }
        term_refresh(mw->right_panel_win);
    }
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
}

void sidepanels_deinit(void)
{
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