#ifndef IRSSI_FE_TEXT_SPLIT_PANELS_H
#define IRSSI_FE_TEXT_SPLIT_PANELS_H

#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-common/core/fe-windows.h>

/* Panel types */
typedef enum {
	SPLIT_PANEL_TYPE_CHANNELS,    /* Channel/query list */
	SPLIT_PANEL_TYPE_NICKLIST,    /* Nicklist for current channel */
	SPLIT_PANEL_TYPE_USER         /* User-defined panel content */
} SplitPanelType;

/* Panel positions */
typedef enum {
	SPLIT_PANEL_POS_LEFT,
	SPLIT_PANEL_POS_RIGHT
} SplitPanelPosition;

/* Panel display modes */
typedef enum {
	SPLIT_PANEL_MODE_HIDDEN,      /* Panel is hidden */
	SPLIT_PANEL_MODE_AUTO,        /* Show/hide based on context */
	SPLIT_PANEL_MODE_ALWAYS       /* Always visible */
} SplitPanelMode;

/* Split panel record structure */
typedef struct _SPLIT_PANEL_REC {
	SplitPanelType type;
	SplitPanelPosition position;
	SplitPanelMode mode;
	
	/* Dimensions */
	int width;              /* Panel width in columns */
	int min_width;          /* Minimum allowed width */
	int max_width;          /* Maximum allowed width */
	
	/* Associated main window (the split window) */
	MAIN_WINDOW_REC *main_window;
	WINDOW_REC *window;     /* The Irssi window in this split */
	
	/* Panel content data */
	GSList *items;          /* List of panel items */
	int scroll_offset;      /* Vertical scroll position */
	int selected_item;      /* Currently selected item index */
	
	/* Display state */
	unsigned int visible:1;
	unsigned int dirty:1;
	unsigned int size_dirty:1;
} SPLIT_PANEL_REC;

/* Panel item structure */
typedef struct _SPLIT_PANEL_ITEM_REC {
	char *text;             /* Display text */
	char *data;             /* Associated data */
	int color;              /* Text color */
	int attr;               /* Text attributes */
	unsigned int selected:1;
} SPLIT_PANEL_ITEM_REC;

/* Global split panel system state */
extern GSList *split_panels_left;      /* Left-side panels */
extern GSList *split_panels_right;     /* Right-side panels */

/* Split panel system functions */
void split_panels_init(void);
void split_panels_deinit(void);

/* Split panel management */
SPLIT_PANEL_REC *split_panel_create(SplitPanelType type, SplitPanelPosition pos, int width);
void split_panel_destroy(SPLIT_PANEL_REC *panel);
void split_panel_set_width(SPLIT_PANEL_REC *panel, int width);
void split_panel_set_mode(SPLIT_PANEL_REC *panel, SplitPanelMode mode);
void split_panel_show(SPLIT_PANEL_REC *panel);
void split_panel_hide(SPLIT_PANEL_REC *panel);
void split_panel_toggle(SPLIT_PANEL_REC *panel);

/* Split panel content management */
void split_panel_clear_items(SPLIT_PANEL_REC *panel);
SPLIT_PANEL_ITEM_REC *split_panel_add_item(SPLIT_PANEL_REC *panel, const char *text, const char *data);
void split_panel_remove_item(SPLIT_PANEL_REC *panel, SPLIT_PANEL_ITEM_REC *item);
void split_panel_update_item(SPLIT_PANEL_ITEM_REC *item, const char *text);
void split_panel_set_item_color(SPLIT_PANEL_ITEM_REC *item, int color, int attr);

/* Split panel display functions */
void split_panel_redraw(SPLIT_PANEL_REC *panel);
void split_panel_redraw_all(void);
void split_panel_scroll(SPLIT_PANEL_REC *panel, int lines);

/* Split panel lookup functions */
SPLIT_PANEL_REC *split_panel_find_by_type(SplitPanelType type);
SPLIT_PANEL_REC *split_panel_find_by_position(SplitPanelPosition pos);
SPLIT_PANEL_REC *split_panel_find_by_window(WINDOW_REC *window);

/* Split panel settings */
int split_panel_get_setting_width(SplitPanelType type);
SplitPanelMode split_panel_get_setting_mode(SplitPanelType type);
void split_panel_settings_init(void);

/* Default panel widths */
#define SPLIT_PANEL_DEFAULT_CHANNELS_WIDTH    20
#define SPLIT_PANEL_DEFAULT_NICKLIST_WIDTH    15
#define SPLIT_PANEL_MIN_WIDTH                 8
#define SPLIT_PANEL_MAX_WIDTH                 80

#endif
