#ifndef IRSSI_FE_TEXT_SIDEPANELS_H
#define IRSSI_FE_TEXT_SIDEPANELS_H

#include <glib.h>
#include <irssi/src/common.h>

/* Main sidepanels API - implemented in sidepanels-core.c */
void sidepanels_init(void);
void sidepanels_deinit(void);

/* Settings accessors */
int get_sp_left_width(void);
int get_sp_right_width(void);
int get_sp_enable_left(void);
int get_sp_enable_right(void);
int get_sp_auto_hide_right(void);
int get_sp_enable_mouse(void);
int get_sp_debug(void);
int get_mouse_scroll_chat(void);

/* Feed one key (gunichar) from sig_gui_key_pressed; returns TRUE if consumed by mouse parser. */
gboolean sidepanels_try_parse_mouse_key(gunichar key);

/* Include all subsystem headers for convenience */
#include "sidepanels-render.h"
#include "sidepanels-activity.h"
#include "sidepanels-signals.h"
#include "sidepanels-layout.h"

#endif