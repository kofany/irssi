#ifndef IRSSI_FE_TEXT_SIDEPANELS_ACTIVITY_H
#define IRSSI_FE_TEXT_SIDEPANELS_ACTIVITY_H

#include <glib.h>
#include <irssi/src/common.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/fe-common/core/printtext.h>
#include "sidepanels-types.h"

/* Data levels for activity tracking - DATA_LEVEL_* constants are already defined in fe-windows.h */
#define DATA_LEVEL_EVENT 10

/* Global activity tracking */
extern GHashTable *window_priorities;

/* Activity management functions */
void handle_new_activity(WINDOW_REC *window, int data_level);

/* Window list management */
GSList *build_sorted_window_list(void);
void free_sorted_window_list(GSList *list);

/* Priority state management */
int get_window_current_priority(WINDOW_REC *win);
void reset_window_priority(WINDOW_REC *win);

/* Nick comparison for sorting */
int ci_nick_compare(gconstpointer a, gconstpointer b);

/* Signal handlers for activity tracking */
void sig_print_text(TEXT_DEST_REC *dest, const char *msg);

/* Initialization */
void sidepanels_activity_init(void);
void sidepanels_activity_deinit(void);

#endif
