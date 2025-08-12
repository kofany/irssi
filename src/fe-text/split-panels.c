#include "module.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/commands.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/gui-windows.h>
#include <irssi/src/fe-text/gui-printtext.h>
#include "split-panels.h"

/* Global split panel lists */
GSList *split_panels_left = NULL;
GSList *split_panels_right = NULL;

/* Forward declarations */
static void split_panel_create_window(SPLIT_PANEL_REC *panel);
static void split_panel_destroy_window(SPLIT_PANEL_REC *panel);
static void split_panel_update_content(SPLIT_PANEL_REC *panel);

/* Create a new split panel */
SPLIT_PANEL_REC *split_panel_create(SplitPanelType type, SplitPanelPosition pos, int width)
{
	SPLIT_PANEL_REC *panel;
	GSList **panel_list;
	
	/* Validate parameters */
	if (width < SPLIT_PANEL_MIN_WIDTH) width = SPLIT_PANEL_MIN_WIDTH;
	if (width > SPLIT_PANEL_MAX_WIDTH) width = SPLIT_PANEL_MAX_WIDTH;
	
	/* Check if panel of this type already exists */
	panel = split_panel_find_by_type(type);
	if (panel != NULL) {
		/* Panel already exists, just update position/width */
		split_panel_set_width(panel, width);
		return panel;
	}
	
	/* Create new panel */
	panel = g_new0(SPLIT_PANEL_REC, 1);
	panel->type = type;
	panel->position = pos;
	panel->mode = SPLIT_PANEL_MODE_ALWAYS;
	panel->width = width;
	panel->min_width = SPLIT_PANEL_MIN_WIDTH;
	panel->max_width = SPLIT_PANEL_MAX_WIDTH;
	panel->visible = FALSE;
	panel->dirty = TRUE;
	panel->size_dirty = TRUE;
	panel->scroll_offset = 0;
	panel->selected_item = -1;
	
	/* Add to appropriate list */
	panel_list = (pos == SPLIT_PANEL_POS_LEFT) ? &split_panels_left : &split_panels_right;
	*panel_list = g_slist_append(*panel_list, panel);
	
	/* Create the split window */
	split_panel_create_window(panel);
	
	return panel;
}

/* Destroy a split panel */
void split_panel_destroy(SPLIT_PANEL_REC *panel)
{
	GSList **panel_list;
	
	if (panel == NULL) return;
	
	/* Remove from list */
	panel_list = (panel->position == SPLIT_PANEL_POS_LEFT) ? &split_panels_left : &split_panels_right;
	*panel_list = g_slist_remove(*panel_list, panel);
	
	/* Destroy the split window */
	split_panel_destroy_window(panel);
	
	/* Free panel items */
	split_panel_clear_items(panel);
	
	/* Free panel structure */
	g_free(panel);
}

/* Create the split window for a panel */
static void split_panel_create_window(SPLIT_PANEL_REC *panel)
{
	MAIN_WINDOW_REC *parent;
	int right_split;
	
	/* Get the main window to split */
	parent = WINDOW_GUI(active_win)->parent;
	if (parent == NULL) return;
	
	/* Determine split direction */
	right_split = (panel->position == SPLIT_PANEL_POS_RIGHT) ? 1 : 0;
	
	/* Create the split window using native Irssi function */
	panel->main_window = mainwindow_create(right_split);
	if (panel->main_window == NULL) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, "Failed to create split window for panel");
		return;
	}
	
	/* Set the window size - mainwindow_set_size takes (window, height, resize_lower) */
	/* For now, we'll use the default height and let Irssi handle width automatically */
	
	/* Create a window in the split */
	panel->window = window_create(NULL, FALSE);
	if (panel->window != NULL) {
		/* Set window name based on panel type */
		switch (panel->type) {
		case SPLIT_PANEL_TYPE_CHANNELS:
			window_set_name(panel->window, "Channels");
			break;
		case SPLIT_PANEL_TYPE_NICKLIST:
			window_set_name(panel->window, "Nicklist");
			break;
		case SPLIT_PANEL_TYPE_USER:
			window_set_name(panel->window, "Panel");
			break;
		}

		/* Move window to the split */
		window_set_active(panel->window);
		gui_window_reparent(panel->window, panel->main_window);
	}
	
	panel->visible = TRUE;
	panel->dirty = TRUE;
	
	/* Update panel content */
	split_panel_update_content(panel);
}

/* Destroy the split window for a panel */
static void split_panel_destroy_window(SPLIT_PANEL_REC *panel)
{
	if (panel->window != NULL) {
		window_destroy(panel->window);
		panel->window = NULL;
	}
	
	if (panel->main_window != NULL) {
		mainwindow_destroy(panel->main_window);
		panel->main_window = NULL;
	}
	
	panel->visible = FALSE;
}

/* Update panel content based on type */
static void split_panel_update_content(SPLIT_PANEL_REC *panel)
{
	if (panel == NULL || panel->window == NULL) return;
	
	/* Display content based on panel type */
	switch (panel->type) {
	case SPLIT_PANEL_TYPE_CHANNELS:
		/* TODO: Display channel list */
		printtext_window(panel->window, MSGLEVEL_CLIENTCRAP, "Channels Panel");
		break;

	case SPLIT_PANEL_TYPE_NICKLIST:
		/* TODO: Display nicklist */
		printtext_window(panel->window, MSGLEVEL_CLIENTCRAP, "Nicklist Panel");
		break;

	case SPLIT_PANEL_TYPE_USER:
		/* TODO: Display user content */
		printtext_window(panel->window, MSGLEVEL_CLIENTCRAP, "User Panel");
		break;
	}
	
	panel->dirty = FALSE;
}

/* Set panel width */
void split_panel_set_width(SPLIT_PANEL_REC *panel, int width)
{
	if (panel == NULL) return;
	
	/* Validate width */
	if (width < panel->min_width) width = panel->min_width;
	if (width > panel->max_width) width = panel->max_width;
	
	if (panel->width != width) {
		panel->width = width;
		panel->size_dirty = TRUE;
		
		/* TODO: Resize the split window if it exists */
		/* mainwindow_set_size() only handles height, width is handled automatically by split system */
	}
}

/* Show a panel */
void split_panel_show(SPLIT_PANEL_REC *panel)
{
	if (panel == NULL) return;
	
	if (!panel->visible) {
		split_panel_create_window(panel);
	}
}

/* Hide a panel */
void split_panel_hide(SPLIT_PANEL_REC *panel)
{
	if (panel == NULL) return;
	
	if (panel->visible) {
		split_panel_destroy_window(panel);
	}
}

/* Toggle panel visibility */
void split_panel_toggle(SPLIT_PANEL_REC *panel)
{
	if (panel == NULL) return;
	
	if (panel->visible) {
		split_panel_hide(panel);
	} else {
		split_panel_show(panel);
	}
}

/* Find panel by type */
SPLIT_PANEL_REC *split_panel_find_by_type(SplitPanelType type)
{
	GSList *tmp;
	
	/* Search left panels */
	for (tmp = split_panels_left; tmp != NULL; tmp = tmp->next) {
		SPLIT_PANEL_REC *panel = tmp->data;
		if (panel->type == type) return panel;
	}
	
	/* Search right panels */
	for (tmp = split_panels_right; tmp != NULL; tmp = tmp->next) {
		SPLIT_PANEL_REC *panel = tmp->data;
		if (panel->type == type) return panel;
	}
	
	return NULL;
}

/* Clear all panel items */
void split_panel_clear_items(SPLIT_PANEL_REC *panel)
{
	GSList *tmp;
	
	if (panel == NULL) return;
	
	for (tmp = panel->items; tmp != NULL; tmp = tmp->next) {
		SPLIT_PANEL_ITEM_REC *item = tmp->data;
		g_free(item->text);
		g_free(item->data);
		g_free(item);
	}
	
	g_slist_free(panel->items);
	panel->items = NULL;
	panel->dirty = TRUE;
}

/* Redraw a panel */
void split_panel_redraw(SPLIT_PANEL_REC *panel)
{
	if (panel == NULL || !panel->visible) return;
	
	split_panel_update_content(panel);
}

/* Redraw all panels */
void split_panel_redraw_all(void)
{
	GSList *tmp;
	
	for (tmp = split_panels_left; tmp != NULL; tmp = tmp->next) {
		split_panel_redraw(tmp->data);
	}
	
	for (tmp = split_panels_right; tmp != NULL; tmp = tmp->next) {
		split_panel_redraw(tmp->data);
	}
}

/* Command: /panel */
static void cmd_panel(const char *data, SERVER_REC *server, void *item)
{
	GHashTable *optlist;
	char *type_str, *position_str, *width_str;
	void *free_arg;
	SplitPanelType type;
	SplitPanelPosition position = SPLIT_PANEL_POS_LEFT;
	int width = 0;
	SPLIT_PANEL_REC *panel;

	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_OPTIONS | PARAM_FLAG_GETREST,
			    "panel", &optlist, &type_str, &position_str, &width_str))
		return;

	if (*type_str == '\0') {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "Usage: /PANEL <type> [left|right] [width]");
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "Types: channels, nicklist");
		cmd_params_free(free_arg);
		return;
	}

	/* Parse panel type */
	if (g_ascii_strcasecmp(type_str, "channels") == 0) {
		type = SPLIT_PANEL_TYPE_CHANNELS;
		width = width > 0 ? width : SPLIT_PANEL_DEFAULT_CHANNELS_WIDTH;
	} else if (g_ascii_strcasecmp(type_str, "nicklist") == 0) {
		type = SPLIT_PANEL_TYPE_NICKLIST;
		width = width > 0 ? width : SPLIT_PANEL_DEFAULT_NICKLIST_WIDTH;
	} else {
		printtext(NULL, NULL, MSGLEVEL_CRAP, "Unknown panel type: %s", type_str);
		cmd_params_free(free_arg);
		return;
	}

	/* Parse position */
	if (*position_str != '\0') {
		if (g_ascii_strcasecmp(position_str, "left") == 0) {
			position = SPLIT_PANEL_POS_LEFT;
		} else if (g_ascii_strcasecmp(position_str, "right") == 0) {
			position = SPLIT_PANEL_POS_RIGHT;
		} else {
			printtext(NULL, NULL, MSGLEVEL_CRAP, "Invalid position: %s (use 'left' or 'right')", position_str);
			cmd_params_free(free_arg);
			return;
		}
	}

	/* Parse width */
	if (*width_str != '\0') {
		width = atoi(width_str);
		if (width <= 0) {
			printtext(NULL, NULL, MSGLEVEL_CRAP, "Invalid width: %s", width_str);
			cmd_params_free(free_arg);
			return;
		}
	}

	/* Check for hide option */
	if (g_hash_table_lookup(optlist, "hide") != NULL) {
		panel = split_panel_find_by_type(type);
		if (panel != NULL) {
			split_panel_hide(panel);
			printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "Panel hidden: %s", type_str);
		} else {
			printtext(NULL, NULL, MSGLEVEL_CRAP, "Panel not found: %s", type_str);
		}
		cmd_params_free(free_arg);
		return;
	}

	/* Create or show panel */
	panel = split_panel_create(type, position, width);
	if (panel != NULL) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "Panel created: %s (%s, width %d)",
			  type_str, position == SPLIT_PANEL_POS_LEFT ? "left" : "right", width);
	} else {
		printtext(NULL, NULL, MSGLEVEL_CRAP, "Failed to create panel: %s", type_str);
	}

	cmd_params_free(free_arg);
}

/* Initialize split panels system */
void split_panels_init(void)
{
	split_panels_left = NULL;
	split_panels_right = NULL;

	/* Register commands */
	command_bind("panel", NULL, (SIGNAL_FUNC) cmd_panel);
	command_set_options("panel", "hide");
}

/* Deinitialize split panels system */
void split_panels_deinit(void)
{
	/* Unregister commands */
	command_unbind("panel", (SIGNAL_FUNC) cmd_panel);

	/* Destroy all panels */
	while (split_panels_left != NULL) {
		split_panel_destroy(split_panels_left->data);
	}

	while (split_panels_right != NULL) {
		split_panel_destroy(split_panels_right->data);
	}
}
