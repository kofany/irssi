#ifndef IRSSI_FE_TEXT_SIDEPANELS_RENDER_H
#define IRSSI_FE_TEXT_SIDEPANELS_RENDER_H

#include <glib.h>
#include <irssi/src/common.h>
#include <irssi/src/fe-text/term.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include "sidepanels-types.h"

/* Redraw batching system */
extern gboolean redraw_pending;
extern int redraw_timer_tag;
extern int redraw_batch_timeout;
extern gboolean batch_mode_active;

/* Core rendering functions */
void clear_window_full(TERM_WINDOW *tw, int width, int height);
void draw_border_vertical(TERM_WINDOW *tw, int width, int height, int right_border);

/* Theme-based drawing functions */
void draw_str_themed(TERM_WINDOW *tw, int x, int y, WINDOW_REC *wctx, int format_id, const char *text);
void draw_str_themed_2params(TERM_WINDOW *tw, int x, int y, WINDOW_REC *wctx, int format_id, 
                            const char *param1, const char *param2);

/* Nick formatting */
char *truncate_nick_for_sidepanel(const char *nick, int max_width);

/* Panel content drawing */
void draw_left_contents(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx);
void draw_right_contents(MAIN_WINDOW_REC *mw, SP_MAINWIN_CTX *ctx);

/* Redraw functions */
void redraw_one(MAIN_WINDOW_REC *mw);
void redraw_all(void);
void redraw_right_panels_only(const char *event_name);
void redraw_left_panels_only(const char *event_name);
void redraw_both_panels_only(const char *event_name);

/* Batching system */
void schedule_batched_redraw(const char *event_name);

/* Initialization */
void sidepanels_render_init(void);
void sidepanels_render_deinit(void);

#endif
