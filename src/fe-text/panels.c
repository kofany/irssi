/*
 panels.c : Side panel system for Irssi

    Copyright (C) 2024 Generated with Claude Code

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "module.h"
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/core/servers.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/core/queries.h>
#include <irssi/src/core/nicklist.h>
#include <irssi/src/fe-common/core/fe-windows.h>
#include <irssi/src/fe-text/term.h>
#include <irssi/src/fe-text/mainwindows.h>
#include <irssi/src/fe-text/panels.h>

/* TERM_WINDOW structure definition for field access */
struct _TERM_WINDOW {
	void *term;  /* TERM_REC *term */
	int x, y;
	int width, height;
};

/* Global panel system variables */
GSList *panels_left = NULL;
GSList *panels_right = NULL;

/* Forward declarations */
static void panel_item_destroy(PANEL_ITEM_REC *item);
static void panel_draw_border(PANEL_REC *panel);
static void panel_draw_content(PANEL_REC *panel);
static void panel_update_term_window(PANEL_REC *panel);

/* External references */
extern GSList *mainwindows;
extern GSList *servers;

/* Panel item management */
static PANEL_ITEM_REC *panel_item_create(const char *text, const char *data)
{
	PANEL_ITEM_REC *item;
	
	g_return_val_if_fail(text != NULL, NULL);
	
	item = g_new0(PANEL_ITEM_REC, 1);
	item->text = g_strdup(text);
	item->data = data ? g_strdup(data) : NULL;
	item->color = 0;
	item->attr = 0;
	item->selected = FALSE;
	item->highlighted = FALSE;
	item->user_data = NULL;
	
	return item;
}

static void panel_item_destroy(PANEL_ITEM_REC *item)
{
	if (item == NULL) return;
	
	g_free(item->text);
	g_free(item->data);
	g_free(item);
}

/* Panel creation and destruction */
PANEL_REC *panel_create(MAIN_WINDOW_REC *mainwin, PanelType type, PanelPosition pos)
{
	PANEL_REC *panel;
	int default_width;
	
	g_return_val_if_fail(mainwin != NULL, NULL);
	
	panel = g_new0(PANEL_REC, 1);
	panel->type = type;
	panel->position = pos;
	panel->mode = panel_get_setting_mode(type);
	panel->main_window = mainwin;
	
	/* Set default dimensions */
	default_width = panel_get_setting_width(type);
	panel->width = default_width;
	panel->min_width = PANEL_MIN_WIDTH;
	panel->max_width = PANEL_MAX_WIDTH;
	panel->visible_width = 0;
	
	/* Initialize content */
	panel->items = NULL;
	panel->scroll_offset = 0;
	panel->selected_item = 0;
	
	/* Initialize display flags */
	panel->dirty = TRUE;
	panel->size_dirty = TRUE;
	panel->visible = FALSE;
	panel->has_focus = FALSE;
	panel->term_win = NULL;
	
	/* Add to appropriate panel list */
	if (pos == PANEL_POS_LEFT) {
		mainwin->panels_left = g_slist_append(mainwin->panels_left, panel);
	} else {
		mainwin->panels_right = g_slist_append(mainwin->panels_right, panel);
	}
	
	/* Update main window panel columns */
	mainwindow_update_panels(mainwin);
	
	return panel;
}

void panel_destroy(PANEL_REC *panel)
{
	MAIN_WINDOW_REC *mainwin;
	
	g_return_if_fail(panel != NULL);
	
	mainwin = panel->main_window;
	
	/* Destroy terminal window */
	if (panel->term_win) {
		term_window_destroy(panel->term_win);
		panel->term_win = NULL;
	}
	
	/* Clear all items */
	panel_clear_items(panel);
	
	/* Remove from panel list */
	if (panel->position == PANEL_POS_LEFT) {
		mainwin->panels_left = g_slist_remove(mainwin->panels_left, panel);
	} else {
		mainwin->panels_right = g_slist_remove(mainwin->panels_right, panel);
	}
	
	g_free(panel);
	
	/* Update main window layout */
	if (mainwin) {
		mainwindow_update_panels(mainwin);
	}
}

/* Panel visibility management */
void panel_show(PANEL_REC *panel)
{
	g_return_if_fail(panel != NULL);
	
	if (panel->visible) return;
	
	panel->visible = TRUE;
	panel->visible_width = panel->width;
	panel->dirty = TRUE;
	panel->size_dirty = TRUE;
	
	panel_update_term_window(panel);
	mainwindow_update_panels(panel->main_window);
}

void panel_hide(PANEL_REC *panel)
{
	g_return_if_fail(panel != NULL);
	
	if (!panel->visible) return;
	
	panel->visible = FALSE;
	panel->visible_width = 0;
	
	if (panel->term_win) {
		term_window_destroy(panel->term_win);
		panel->term_win = NULL;
	}
	
	mainwindow_update_panels(panel->main_window);
}

void panel_toggle(PANEL_REC *panel)
{
	g_return_if_fail(panel != NULL);
	
	if (panel->visible) {
		panel_hide(panel);
	} else {
		panel_show(panel);
	}
}

/* Panel content management */
void panel_clear_items(PANEL_REC *panel)
{
	GSList *tmp;
	
	g_return_if_fail(panel != NULL);
	
	for (tmp = panel->items; tmp != NULL; tmp = tmp->next) {
		panel_item_destroy(tmp->data);
	}
	
	g_slist_free(panel->items);
	panel->items = NULL;
	panel->selected_item = 0;
	panel->dirty = TRUE;
}

PANEL_ITEM_REC *panel_add_item(PANEL_REC *panel, const char *text, const char *data)
{
	PANEL_ITEM_REC *item;
	
	g_return_val_if_fail(panel != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);
	
	item = panel_item_create(text, data);
	panel->items = g_slist_append(panel->items, item);
	panel->dirty = TRUE;
	
	return item;
}

void panel_remove_item(PANEL_REC *panel, PANEL_ITEM_REC *item)
{
	g_return_if_fail(panel != NULL);
	g_return_if_fail(item != NULL);
	
	panel->items = g_slist_remove(panel->items, item);
	panel_item_destroy(item);
	panel->dirty = TRUE;
}

void panel_update_item(PANEL_ITEM_REC *item, const char *text)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(text != NULL);
	
	g_free(item->text);
	item->text = g_strdup(text);
}

void panel_set_item_color(PANEL_ITEM_REC *item, int color, int attr)
{
	g_return_if_fail(item != NULL);
	
	item->color = color;
	item->attr = attr;
}

/* Panel navigation */
void panel_select_item(PANEL_REC *panel, int index)
{
	int item_count;
	
	g_return_if_fail(panel != NULL);
	
	item_count = g_slist_length(panel->items);
	if (item_count == 0) return;
	
	if (index < 0) index = 0;
	if (index >= item_count) index = item_count - 1;
	
	if (panel->selected_item != index) {
		panel->selected_item = index;
		panel->dirty = TRUE;
	}
}

void panel_select_next(PANEL_REC *panel)
{
	g_return_if_fail(panel != NULL);
	
	panel_select_item(panel, panel->selected_item + 1);
}

void panel_select_prev(PANEL_REC *panel)
{
	g_return_if_fail(panel != NULL);
	
	panel_select_item(panel, panel->selected_item - 1);
}

PANEL_ITEM_REC *panel_get_selected_item(PANEL_REC *panel)
{
	g_return_val_if_fail(panel != NULL, NULL);
	
	return g_slist_nth_data(panel->items, panel->selected_item);
}

/* Panel display functions */
static void panel_update_term_window(PANEL_REC *panel)
{
	MAIN_WINDOW_REC *mainwin;
	int x, y, width, height;
	
	g_return_if_fail(panel != NULL);
	g_return_if_fail(panel->visible);
	
	mainwin = panel->main_window;
	
	/* Calculate panel position and size */
	height = mainwin->height - mainwin->statusbar_lines;
	width = panel->visible_width;
	y = mainwin->first_line + mainwin->statusbar_lines_top;
	
	if (panel->position == PANEL_POS_LEFT) {
		x = mainwin->first_column + mainwin->statusbar_columns_left;
	} else {
		x = mainwin->last_column - mainwin->statusbar_columns_right - width + 1;
	}
	
	/* Create or update terminal window */
	if (panel->term_win) {
		term_window_move(panel->term_win, x, y, width, height);
	} else {
		panel->term_win = term_window_create(x, y, width, height);
	}
}

static void panel_draw_border(PANEL_REC *panel)
{
	int y, height;
	
	g_return_if_fail(panel != NULL);
	g_return_if_fail(panel->term_win != NULL);
	
	height = panel->term_win->height;
	
	/* Draw right border for left panels, left border for right panels */
	if (panel->position == PANEL_POS_LEFT) {
		/* Draw separator on the right edge */
		for (y = 0; y < height; y++) {
			term_move(panel->term_win, panel->visible_width - 1, y);
			term_addch(panel->term_win, PANEL_SEPARATOR_CHAR);
		}
	} else {
		/* Draw separator on the left edge */
		for (y = 0; y < height; y++) {
			term_move(panel->term_win, 0, y);
			term_addch(panel->term_win, PANEL_SEPARATOR_CHAR);
		}
	}
}

static void panel_draw_content(PANEL_REC *panel)
{
	GSList *tmp;
	PANEL_ITEM_REC *item;
	int y, x_offset, content_width, visible_items, start_item;
	int item_index = 0;
	
	g_return_if_fail(panel != NULL);
	g_return_if_fail(panel->term_win != NULL);
	
	visible_items = panel->term_win->height;
	content_width = panel->visible_width - (panel->position == PANEL_POS_RIGHT ? 2 : 1);
	x_offset = panel->position == PANEL_POS_RIGHT ? 1 : 0;
	
	/* Calculate starting item for scrolling */
	start_item = panel->scroll_offset;
	if (panel->selected_item >= start_item + visible_items) {
		start_item = panel->selected_item - visible_items + 1;
	}
	if (panel->selected_item < start_item) {
		start_item = panel->selected_item;
	}
	panel->scroll_offset = start_item;
	
	/* Clear content area */
	for (y = 0; y < visible_items; y++) {
		term_move(panel->term_win, x_offset, y);
		for (int x = 0; x < content_width; x++) {
			term_addch(panel->term_win, ' ');
		}
	}
	
	/* Draw items */
	y = 0;
	for (tmp = g_slist_nth(panel->items, start_item); 
	     tmp != NULL && y < visible_items; 
	     tmp = tmp->next, y++) {
		
		item = tmp->data;
		item_index = start_item + y;
		
		term_move(panel->term_win, x_offset, y);
		
		/* Draw selection indicator */
		if (item_index == panel->selected_item) {
			term_addch(panel->term_win, PANEL_SELECTED_CHAR);
		} else {
			term_addch(panel->term_win, PANEL_UNSELECTED_CHAR);
		}
		
		/* Set text attributes */
		if (item->attr) {
			term_set_color(panel->term_win, item->color);
		}
		
		/* Draw item text, truncating if necessary */
		if (item->text) {
			int text_width = content_width - 1; /* Account for selection indicator */
			int text_len = strlen(item->text);
			
			if (text_len > text_width) {
				/* Truncate text */
				for (int i = 0; i < text_width - 3; i++) {
					term_addch(panel->term_win, item->text[i]);
				}
				term_addstr(panel->term_win, "...");
			} else {
				term_addstr(panel->term_win, item->text);
			}
		}
		
		/* Reset attributes */
		if (item->attr) {
			term_set_color(panel->term_win, 0);
		}
	}
}

void panel_redraw(PANEL_REC *panel)
{
	g_return_if_fail(panel != NULL);
	
	if (!panel->visible || !panel->term_win) return;
	
	if (panel->size_dirty) {
		panel_update_term_window(panel);
		panel->size_dirty = FALSE;
	}
	
	if (panel->dirty) {
		panel_draw_content(panel);
		panel_draw_border(panel);
		panel->dirty = FALSE;
	}
}

void panel_redraw_all(void)
{
	GSList *tmp;
	GSList *panel_tmp;
	MAIN_WINDOW_REC *mainwin;
	PANEL_REC *panel;
	
	for (tmp = mainwindows; tmp != NULL; tmp = tmp->next) {
		mainwin = tmp->data;
		
		/* Redraw left panels */
		for (panel_tmp = mainwin->panels_left; panel_tmp != NULL; panel_tmp = panel_tmp->next) {
			panel = panel_tmp->data;
			panel_redraw(panel);
		}
		
		/* Redraw right panels */
		for (panel_tmp = mainwin->panels_right; panel_tmp != NULL; panel_tmp = panel_tmp->next) {
			panel = panel_tmp->data;
			panel_redraw(panel);
		}
	}
}

/* Panel queries */
PANEL_REC *panel_find_by_type(MAIN_WINDOW_REC *mainwin, PanelType type)
{
	GSList *tmp;
	PANEL_REC *panel;
	
	g_return_val_if_fail(mainwin != NULL, NULL);
	
	/* Search left panels */
	for (tmp = mainwin->panels_left; tmp != NULL; tmp = tmp->next) {
		panel = tmp->data;
		if (panel->type == type) return panel;
	}
	
	/* Search right panels */
	for (tmp = mainwin->panels_right; tmp != NULL; tmp = tmp->next) {
		panel = tmp->data;
		if (panel->type == type) return panel;
	}
	
	return NULL;
}

PANEL_REC *panel_find_by_position(MAIN_WINDOW_REC *mainwin, PanelPosition pos)
{
	GSList *panels;
	
	g_return_val_if_fail(mainwin != NULL, NULL);
	
	panels = (pos == PANEL_POS_LEFT) ? mainwin->panels_left : mainwin->panels_right;
	
	return panels ? panels->data : NULL;
}

int panel_get_total_width(MAIN_WINDOW_REC *mainwin, PanelPosition pos)
{
	GSList *panels, *tmp;
	PANEL_REC *panel;
	int total_width = 0;
	
	g_return_val_if_fail(mainwin != NULL, 0);
	
	panels = (pos == PANEL_POS_LEFT) ? mainwin->panels_left : mainwin->panels_right;
	
	for (tmp = panels; tmp != NULL; tmp = tmp->next) {
		panel = tmp->data;
		if (panel->visible) {
			total_width += panel->visible_width;
		}
	}
	
	return total_width;
}

gboolean panel_has_visible_panels(MAIN_WINDOW_REC *mainwin)
{
	g_return_val_if_fail(mainwin != NULL, FALSE);
	
	return (panel_get_total_width(mainwin, PANEL_POS_LEFT) > 0) ||
	       (panel_get_total_width(mainwin, PANEL_POS_RIGHT) > 0);
}

/* Panel layout calculation */
void panel_calculate_layout(MAIN_WINDOW_REC *mainwin, 
                           int *chat_x, int *chat_y, 
                           int *chat_width, int *chat_height)
{
	g_return_if_fail(mainwin != NULL);
	
	/* Calculate chat area coordinates accounting for panels */
	if (chat_x) *chat_x = MAIN_WINDOW_CHAT_LEFT(mainwin);
	if (chat_y) *chat_y = mainwin->first_line + mainwin->statusbar_lines_top;
	if (chat_width) *chat_width = MAIN_WINDOW_CHAT_WIDTH(mainwin);
	if (chat_height) *chat_height = mainwin->height - mainwin->statusbar_lines;
}

/* Panel settings */
int panel_get_setting_width(PanelType type)
{
	switch (type) {
	case PANEL_TYPE_CHANNELS:
		return settings_get_int("panel_channels_width");
	case PANEL_TYPE_NICKLIST:
		return settings_get_int("panel_nicklist_width");
	default:
		return PANEL_DEFAULT_CHANNELS_WIDTH;
	}
}

PanelMode panel_get_setting_mode(PanelType type)
{
	const char *mode_str;
	
	switch (type) {
	case PANEL_TYPE_CHANNELS:
		mode_str = settings_get_str("panel_channels_mode");
		break;
	case PANEL_TYPE_NICKLIST:
		mode_str = settings_get_str("panel_nicklist_mode");
		break;
	default:
		return PANEL_MODE_AUTO;
	}
	
	if (g_ascii_strcasecmp(mode_str, "hidden") == 0) {
		return PANEL_MODE_HIDDEN;
	} else if (g_ascii_strcasecmp(mode_str, "always") == 0) {
		return PANEL_MODE_ALWAYS;
	} else {
		return PANEL_MODE_AUTO;
	}
}

void panel_settings_init(void)
{
	settings_add_int("lookandfeel", "panel_channels_width", PANEL_DEFAULT_CHANNELS_WIDTH);
	settings_add_int("lookandfeel", "panel_nicklist_width", PANEL_DEFAULT_NICKLIST_WIDTH);
	settings_add_str("lookandfeel", "panel_channels_mode", "auto");
	settings_add_str("lookandfeel", "panel_nicklist_mode", "auto");
	settings_add_bool("lookandfeel", "panel_channels_auto_create", TRUE);
	settings_add_bool("lookandfeel", "panel_nicklist_auto_create", TRUE);
}

/* Panel system initialization */
void panels_init(void)
{
	panel_settings_init();
}

void panels_deinit(void)
{
	/* Clean up any remaining panels */
	while (panels_left) {
		panel_destroy(panels_left->data);
	}
	while (panels_right) {
		panel_destroy(panels_right->data);
	}
}

/* Panel content update functions */
void panel_update_channels(PANEL_REC *panel)
{
	GSList *tmp;
	GSList *channels;
	GSList *ch_tmp;
	GSList *queries;
	GSList *q_tmp;
	SERVER_REC *server;
	CHANNEL_REC *channel;
	QUERY_REC *query;
	char *text;
	int activity_level;
	PANEL_ITEM_REC *item;
	
	g_return_if_fail(panel != NULL);
	g_return_if_fail(panel->type == PANEL_TYPE_CHANNELS);
	
	/* Clear existing items */
	panel_clear_items(panel);
	
	/* Add channels and queries from all servers */
	for (tmp = servers; tmp != NULL; tmp = tmp->next) {
		server = tmp->data;
		
		/* Add channels */
		channels = server->channels;
		for (ch_tmp = channels; ch_tmp != NULL; ch_tmp = ch_tmp->next) {
			channel = ch_tmp->data;
			
			/* Format channel name with activity indicator */
			activity_level = 0; /* TODO: Get actual activity level */
			if (activity_level > 0) {
				text = g_strdup_printf("%s *", channel->name);
			} else {
				text = g_strdup(channel->name);
			}
			
			item = panel_add_item(panel, text, channel->name);
			item->user_data = channel;
			
			g_free(text);
		}
		
		/* Add queries */
		queries = server->queries;
		for (q_tmp = queries; q_tmp != NULL; q_tmp = q_tmp->next) {
			query = q_tmp->data;
			
			activity_level = 0; /* TODO: Get actual activity level */
			if (activity_level > 0) {
				text = g_strdup_printf("%s *", query->name);
			} else {
				text = g_strdup(query->name);
			}
			
			item = panel_add_item(panel, text, query->name);
			item->user_data = query;
			
			g_free(text);
		}
	}
}

void panel_update_nicklist(PANEL_REC *panel, CHANNEL_REC *channel)
{
	GSList *nicks, *tmp;
	NICK_REC *nick;
	char *text;
	PANEL_ITEM_REC *item;
	
	g_return_if_fail(panel != NULL);
	g_return_if_fail(panel->type == PANEL_TYPE_NICKLIST);
	
	/* Clear existing items */
	panel_clear_items(panel);
	
	if (channel == NULL) return;
	
	/* Get and sort nicklist */
	nicks = nicklist_getnicks(channel);
	
	/* Add nicks to panel */
	for (tmp = nicks; tmp != NULL; tmp = tmp->next) {
		nick = tmp->data;
		
		/* Format nick with prefix/status */
		if (nick->prefixes[0] != '\0') {
			text = g_strdup_printf("%c%s", nick->prefixes[0], nick->nick);
		} else {
			text = g_strdup(nick->nick);
		}
		
		item = panel_add_item(panel, text, nick->nick);
		item->user_data = nick;
		
		/* Set color based on nick status */
		if (nick->op) {
			panel_set_item_color(item, 14, 0); /* Yellow for ops */
		} else if (nick->voice) {
			panel_set_item_color(item, 11, 0); /* Cyan for voice */
		}
		
		g_free(text);
	}
	
	g_slist_free(nicks);
}