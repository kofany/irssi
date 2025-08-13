#ifndef IRSSI_FE_TEXT_SIDEPANELS_H
#define IRSSI_FE_TEXT_SIDEPANELS_H

#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/term.h>

/* Panel context per main window */
typedef struct {
	/* Selection and scroll state */
	int left_selected_index;
	int left_scroll_offset;
	int right_selected_index;
	int right_scroll_offset;
	
	/* Cached content for dirty checking */
	char **left_content_cache;
	char **right_content_cache;
	int left_content_lines;
	int right_content_lines;
	
	/* Cached geometry for hit-testing and drawing */
	int left_x, left_y, left_w, left_h;
	int right_x, right_y, right_w, right_h;
} SP_MAINWIN_CTX;

/* Core API Functions */
void sidepanels_init(void);
void sidepanels_deinit(void);
gboolean sidepanels_try_parse_mouse_key(unichar key);

/* Internal functions */
SP_MAINWIN_CTX *sidepanels_get_context(MAIN_WINDOW_REC *mw, gboolean create);
void sidepanels_destroy_context(MAIN_WINDOW_REC *mw);
void sidepanels_apply_settings_all(void);
void sidepanels_redraw_all(void);
void sidepanels_redraw_mainwin(MAIN_WINDOW_REC *mw);

#endif
