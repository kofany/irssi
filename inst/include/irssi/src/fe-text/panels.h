#ifndef IRSSI_FE_TEXT_PANELS_H
#define IRSSI_FE_TEXT_PANELS_H

#include <irssi/src/fe-text/term.h>
#include <irssi/src/fe-common/core/fe-windows.h>

/* Panel types */
typedef enum {
	PANEL_TYPE_CHANNELS,    /* Channel/query list */
	PANEL_TYPE_NICKLIST,    /* Nicklist for current channel */
	PANEL_TYPE_USER         /* User-defined panel content */
} PanelType;

/* Panel positions */
typedef enum {
	PANEL_POS_LEFT,
	PANEL_POS_RIGHT
} PanelPosition;

/* Panel display modes */
typedef enum {
	PANEL_MODE_HIDDEN,      /* Panel is hidden */
	PANEL_MODE_AUTO,        /* Show/hide based on context */
	PANEL_MODE_ALWAYS       /* Always visible */
} PanelMode;

/* mainwindows.h must be included before this header */

/* Panel record structure */
typedef struct _PANEL_REC {
	PanelType type;
	PanelPosition position;
	PanelMode mode;
	
	/* Dimensions and position */
	int width;              /* Panel width in columns */
	int min_width;          /* Minimum allowed width */
	int max_width;          /* Maximum allowed width */
	int visible_width;      /* Current visible width (0 if hidden) */
	
	/* Terminal window for panel content */
	TERM_WINDOW *term_win;

	/* Parent main window */
	MAIN_WINDOW_REC *main_window;
	
	/* Panel content data */
	GSList *items;          /* List of panel items */
	int scroll_offset;      /* Vertical scroll position */
	int selected_item;      /* Currently selected item index */
	
	/* Display flags */
	unsigned int dirty:1;           /* Panel needs redraw */
	unsigned int size_dirty:1;      /* Panel needs resize */
	unsigned int visible:1;         /* Panel is currently visible */
	unsigned int has_focus:1;       /* Panel has keyboard focus */
} PANEL_REC;

/* Panel item structure (for channel list, nicklist, etc.) */
typedef struct _PANEL_ITEM_REC {
	char *text;             /* Display text */
	char *data;             /* Associated data (channel name, nick, etc.) */
	void *user_data;        /* Additional context data */
	
	/* Display attributes */
	int color;              /* Text color */
	int attr;               /* Text attributes (bold, etc.) */
	unsigned int selected:1;        /* Item is selected */
	unsigned int highlighted:1;     /* Item is highlighted */
} PANEL_ITEM_REC;

/* Global panel system state */
extern GSList *panels_left;      /* Left-side panels */
extern GSList *panels_right;     /* Right-side panels */

/* Panel system functions */
void panels_init(void);
void panels_deinit(void);

/* Panel management */
PANEL_REC *panel_create(MAIN_WINDOW_REC *mainwin, PanelType type, PanelPosition pos);
void panel_destroy(PANEL_REC *panel);
void panel_set_width(PANEL_REC *panel, int width);
void panel_set_mode(PANEL_REC *panel, PanelMode mode);
void panel_show(PANEL_REC *panel);
void panel_hide(PANEL_REC *panel);
void panel_toggle(PANEL_REC *panel);

/* Panel content management */
void panel_clear_items(PANEL_REC *panel);
PANEL_ITEM_REC *panel_add_item(PANEL_REC *panel, const char *text, const char *data);
void panel_remove_item(PANEL_REC *panel, PANEL_ITEM_REC *item);
void panel_update_item(PANEL_ITEM_REC *item, const char *text);
void panel_set_item_color(PANEL_ITEM_REC *item, int color, int attr);

/* Panel display functions */
void panel_redraw(PANEL_REC *panel);
void panel_redraw_all(void);
void panel_resize(PANEL_REC *panel, int width, int height);
void panel_scroll(PANEL_REC *panel, int lines);
void panel_update_term_window(PANEL_REC *panel);

/* Panel navigation */
void panel_select_item(PANEL_REC *panel, int index);
void panel_select_next(PANEL_REC *panel);
void panel_select_prev(PANEL_REC *panel);
void panel_scroll_up(PANEL_REC *panel);
void panel_scroll_down(PANEL_REC *panel);
void panel_page_up(PANEL_REC *panel);
void panel_page_down(PANEL_REC *panel);
PANEL_ITEM_REC *panel_get_selected_item(PANEL_REC *panel);

/* Panel queries */
PANEL_REC *panel_find_by_type(MAIN_WINDOW_REC *mainwin, PanelType type);
PANEL_REC *panel_find_by_position(MAIN_WINDOW_REC *mainwin, PanelPosition pos);
int panel_get_total_width(MAIN_WINDOW_REC *mainwin, PanelPosition pos);
gboolean panel_has_visible_panels(MAIN_WINDOW_REC *mainwin);

/* Panel content update functions */
void panel_update_channels(PANEL_REC *panel);
void panel_update_nicklist(PANEL_REC *panel, CHANNEL_REC *channel);

/* Panel coordinate calculations */
void panel_calculate_layout(MAIN_WINDOW_REC *mainwin, 
                           int *chat_x, int *chat_y, 
                           int *chat_width, int *chat_height);

/* Panel settings */
int panel_get_setting_width(PanelType type);
PanelMode panel_get_setting_mode(PanelType type);
void panel_settings_init(void);

/* Mouse support */
void panel_handle_mouse_click(int x, int y, int button);

/* Panel system functions */
void panels_init(void);
void panels_deinit(void);

/* Default panel widths */
#define PANEL_DEFAULT_CHANNELS_WIDTH    20
#define PANEL_DEFAULT_NICKLIST_WIDTH    15
#define PANEL_MIN_WIDTH                 8
#define PANEL_MAX_WIDTH                 80

/* Panel drawing characters */
#define PANEL_SEPARATOR_CHAR    '|'  /* ASCII vertical bar instead of Unicode */
#define PANEL_SELECTED_CHAR     '>'
#define PANEL_UNSELECTED_CHAR   ' '

#endif