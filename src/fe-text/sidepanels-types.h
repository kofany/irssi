#ifndef IRSSI_FE_TEXT_SIDEPANELS_TYPES_H
#define IRSSI_FE_TEXT_SIDEPANELS_TYPES_H

#include <glib.h>
#include <irssi/src/common.h>
#include <irssi/src/fe-text/term.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/core/servers.h>

/* SP_MAINWIN_CTX structure definition - shared across all modules */
typedef struct {
	TERM_WINDOW *left_tw;
	TERM_WINDOW *right_tw;
	int left_w;
	int right_w;
	/* selection and scroll state */
	int left_selected_index;
	int left_scroll_offset;
	int right_selected_index;
	int right_scroll_offset;
	/* cached geometry for hit-test and drawing */
	int left_x;
	int left_y;
	int left_h;
	int right_x;
	int right_y;
	int right_h;
	/* ordered nick pointers matching rendered order */
	GSList *right_order;
} SP_MAINWIN_CTX;

/* Window Priority State - Simpler approach */
typedef struct {
	WINDOW_REC *window;
	int current_priority; /* 0=none, 1=events, 2=highlight, 3=activity, 4=nick/query */
} window_priority_state;

/* Window sorting structure for activity-based ordering */
typedef struct {
	WINDOW_REC *win;
	int sort_group; /* 0=Notices, 1=server, 2=channel, 3=query, 4=named_orphan, 5=unnamed_orphan */
	char *sort_key; /* For alphabetical sorting within group */
	SERVER_REC *server; /* Server for grouping */
} WINDOW_SORT_REC;

#endif
