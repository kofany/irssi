#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/term.h>

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
	if (sp_enable_left && sp_left_width > 0)
		mainwindow_set_statusbar_columns(mw, sp_left_width, 0);
	if (sp_enable_right && sp_right_width > 0)
		mainwindow_set_statusbar_columns(mw, 0, sp_right_width);
}

static void sig_setup_changed(void)
{
	read_settings();
	apply_reservations_all();
}

void sidepanels_init(void)
{
	settings_add_int("lookandfeel", "sidepanel_left_width", 14);
	settings_add_int("lookandfeel", "sidepanel_right_width", 16);
	settings_add_bool("lookandfeel", "sidepanel_left", TRUE);
	settings_add_bool("lookandfeel", "sidepanel_right", TRUE);

	read_settings();
	
	signal_add("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
	signal_add("setup changed", (SIGNAL_FUNC) sig_setup_changed);
	
	/* Apply to existing mainwindows */
	apply_reservations_all();
}

void sidepanels_deinit(void)
{
	GSList *tmp;
	
	signal_remove("mainwindow created", (SIGNAL_FUNC) sig_mainwindow_created);
	signal_remove("setup changed", (SIGNAL_FUNC) sig_setup_changed);
	
	/* Remove reservations from all mainwindows */
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		MAIN_WINDOW_REC *mw = tmp->data;
		if (mw->statusbar_columns_left)
			mainwindow_set_statusbar_columns(mw, -mw->statusbar_columns_left, 0);
		if (mw->statusbar_columns_right)
			mainwindow_set_statusbar_columns(mw, 0, -mw->statusbar_columns_right);
	}
}