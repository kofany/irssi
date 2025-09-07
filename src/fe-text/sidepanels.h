#ifndef IRSSI_FE_TEXT_SIDEPANELS_H
#define IRSSI_FE_TEXT_SIDEPANELS_H

#include <glib.h>
#include <irssi/src/common.h>

void sidepanels_init(void);
void sidepanels_deinit(void);

/* Feed one key (unichar) from sig_gui_key_pressed; returns TRUE if consumed by mouse parser. */
gboolean sidepanels_try_parse_mouse_key(unichar key);

/* Mouse gesture functionality - these are internal to sidepanels.c but exposed for testing */
/* Mouse gesture types for configuration */
typedef enum {
	GESTURE_NONE = 0,
	GESTURE_LEFT_SHORT,
	GESTURE_LEFT_LONG,
	GESTURE_RIGHT_SHORT,
	GESTURE_RIGHT_LONG
} MouseGestureType;

#endif