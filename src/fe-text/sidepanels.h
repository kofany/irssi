#ifndef IRSSI_FE_TEXT_SIDEPANELS_H
#define IRSSI_FE_TEXT_SIDEPANELS_H

#include <glib.h>
#include <irssi/src/common.h>

/* Forward declarations */
struct MAIN_WINDOW_REC;

void sidepanels_init(void);
void sidepanels_deinit(void);

/* Selection and scroll management functions */
void sidepanels_move_left_selection(struct MAIN_WINDOW_REC *mw, int delta);
void sidepanels_move_right_selection(struct MAIN_WINDOW_REC *mw, int delta);
void sidepanels_scroll_left(struct MAIN_WINDOW_REC *mw, int delta);
void sidepanels_scroll_right(struct MAIN_WINDOW_REC *mw, int delta);
int sidepanels_get_left_selection(struct MAIN_WINDOW_REC *mw);
int sidepanels_get_right_selection(struct MAIN_WINDOW_REC *mw);

#endif