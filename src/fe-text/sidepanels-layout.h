#ifndef IRSSI_FE_TEXT_SIDEPANELS_LAYOUT_H
#define IRSSI_FE_TEXT_SIDEPANELS_LAYOUT_H

#include <glib.h>
#include <irssi/src/common.h>
#include <irssi/src/fe-text/term.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include "sidepanels-types.h"

/* Panel positioning and geometry */
void position_tw(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx);
void apply_reservations_all(void);
void setup_ctx_for(MAIN_WINDOW_REC *mw);
void apply_and_redraw(void);

/* Selection management */
void update_left_selection_to_active(void);

/* Window management */
void renumber_windows_by_position(void);

/* Main window border drawing */
void draw_main_window_borders(MAIN_WINDOW_REC *mw);

/* Signal handlers are now in sidepanels-core.c */

/* Initialization */
void sidepanels_layout_init(void);
void sidepanels_layout_deinit(void);

#endif
