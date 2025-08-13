#include <src/core/module.h>
#include <src/core/signals.h>
#include <src/core/settings.h>
#include <src/core/modules.h>
#include <src/core/commands.h>
#include <src/fe-common/core/fe-windows.h>
#include <src/fe-common/core/formats.h>
#include <src/fe-text/statusbar-item.h>
#include <src/fe-text/statusbar.h>
#include <src/fe-text/gui-windows.h>
#include <src/fe-text/term.h>

#include <glib.h>
#include <string.h>

#undef MODULE_NAME
#define MODULE_NAME "awl/core"

/* Settings */
static int setting_hide_data;
static int setting_hide_empty;
static char *setting_sort;
static int setting_prefer_name;
static char *setting_separator;

/* Panel settings */
static int setting_panel;              /* bool: enable panel mode */
static int setting_block;              /* column width (>0 chars) */
static int setting_height_adjust;      /* rows to leave empty at bottom */

/* Cache */
static char *awl_line;                 /* single-line mode cached text */

/* Panel cache */
typedef struct {
	int row;       /* 0-based within panel rows */
	int col_start; /* inclusive visual column start */
	int col_end;   /* inclusive visual column end */
	int win_refnum;
} HitZone;

static GPtrArray *panel_items;  /* GPtrArray of char* entries (rendered item texts) */
static GPtrArray *hit_zones;    /* GPtrArray of HitZone* for mouse hit testing */
static int panel_rows;          /* actual rendered rows */
static int panel_cols;          /* actual rendered columns */

static void free_cached_line(void)
{
	g_free(awl_line); awl_line = NULL;
}

static void clear_panel_cache(void)
{
	if (panel_items) {
		for (guint i = 0; i < panel_items->len; i++) g_free(g_ptr_array_index(panel_items, i));
		g_ptr_array_free(panel_items, TRUE);
		panel_items = NULL;
	}
	if (hit_zones) {
		for (guint i = 0; i < hit_zones->len; i++) g_free(g_ptr_array_index(hit_zones, i));
		g_ptr_array_free(hit_zones, TRUE);
		hit_zones = NULL;
	}
	panel_rows = panel_cols = 0;
}

static void awl_settings_read(void)
{
	g_free(setting_sort); setting_sort = NULL;
	g_free(setting_separator); setting_separator = NULL;

	setting_hide_data = settings_get_int("awl_hide_data");
	setting_hide_empty = settings_get_int("awl_hide_empty");
	setting_prefer_name = settings_get_bool("awl_prefer_name");
	const char *sortv = settings_get_str("awl_sort");
	setting_sort = g_strdup(sortv && *sortv ? sortv : "refnum");
	const char *sep = settings_get_str("awl_separator");
	setting_separator = g_strdup(sep && *sep ? sep : " ");

	setting_panel = settings_get_bool("awl_panel");
	setting_block = settings_get_int("awl_block");
	if (setting_block <= 0) setting_block = 15;
	setting_height_adjust = settings_get_int("awl_height_adjust");
	if (setting_height_adjust < 0) setting_height_adjust = 0;
}

static const char *get_window_name(const WINDOW_REC *win)
{
	if (setting_prefer_name && win->name && *win->name) return win->name;
	return window_get_active_name((WINDOW_REC *) win);
}

static int window_sort_cmp_refnum(gconstpointer a, gconstpointer b)
{
	const WINDOW_REC *w1 = a, *w2 = b;
	return window_refnum_cmp((WINDOW_REC *) w1, (WINDOW_REC *) w2);
}

static int window_sort_cmp_data_level(gconstpointer a, gconstpointer b)
{
	const WINDOW_REC *w1 = a, *w2 = b;
	if (w1->data_level != w2->data_level) return w2->data_level - w1->data_level; /* desc */
	return window_refnum_cmp((WINDOW_REC *) w1, (WINDOW_REC *) w2);
}

static int window_sort_cmp_last_line(gconstpointer a, gconstpointer b)
{
	const WINDOW_REC *w1 = a, *w2 = b;
	if (w1->last_line != w2->last_line) return (w2->last_line > w1->last_line) ? 1 : -1; /* desc */
	return window_refnum_cmp((WINDOW_REC *) w1, (WINDOW_REC *) w2);
}

static gboolean should_hide_window(const WINDOW_REC *win)
{
	if (setting_hide_data > 0 && win->data_level < setting_hide_data) return TRUE;
	if (setting_hide_empty > 0) {
		gboolean visible = FALSE;
		if (win->gui_data) visible = is_window_visible((WINDOW_REC *) win);
		if (visible && win->items == NULL) return TRUE;
	}
	return FALSE;
}

static GSList *collect_sorted_windows(void)
{
	GSList *wins = windows_get_sorted(); /* ascending refnum */
	if (setting_sort && g_strcmp0(setting_sort, "refnum") != 0) {
		GList *list = NULL;
		for (GSList *t = wins; t; t = t->next) list = g_list_prepend(list, t->data);
		list = g_list_reverse(list);
		if (g_strcmp0(setting_sort, "-data_level") == 0) list = g_list_sort(list, window_sort_cmp_data_level);
		else if (g_strcmp0(setting_sort, "-last_line") == 0) list = g_list_sort(list, window_sort_cmp_last_line);
		else list = g_list_sort(list, window_sort_cmp_refnum);
		GSList *nw = NULL;
		for (GList *lt = list; lt; lt = lt->next) nw = g_slist_prepend(nw, lt->data);
		nw = g_slist_reverse(nw);
		g_list_free(list);
		g_slist_free(wins);
		wins = nw;
	}
	return wins;
}

static void render_awl_line(void)
{
	free_cached_line();
	GSList *wins = collect_sorted_windows();
	GString *out = g_string_new(NULL);
	for (GSList *t = wins; t; t = t->next) {
		WINDOW_REC *win = t->data; if (!win) continue;
		if (should_hide_window(win)) continue;
		const char *name = get_window_name(win);
		if (out->len > 0 && setting_separator) g_string_append(out, setting_separator);
		gboolean is_active = (active_win && active_win->refnum == win->refnum);
		const char *marker = "";
		switch (win->data_level) { case DATA_LEVEL_HILIGHT: marker = "!"; break; case DATA_LEVEL_MSG: marker = "+"; break; case DATA_LEVEL_TEXT: marker = "."; break; default: marker = ""; }
		if (is_active) g_string_append_printf(out, "[%d:%s%s]", win->refnum, marker, name ? name : "");
		else g_string_append_printf(out, "%d:%s%s", win->refnum, marker, name ? name : "");
	}
	g_slist_free(wins);
	awl_line = out->str; g_string_free(out, FALSE);
}

static void render_panel(void)
{
	clear_panel_cache();
	panel_items = g_ptr_array_new();
	hit_zones = g_ptr_array_new();

	GSList *wins = collect_sorted_windows();
	GPtrArray *entries = g_ptr_array_new(); /* char* */
	GPtrArray *refnums = g_ptr_array_new(); /* gpointer(intptr) */
	for (GSList *t = wins; t; t = t->next) {
		WINDOW_REC *win = t->data; if (!win) continue;
		if (should_hide_window(win)) continue;
		const char *name = get_window_name(win);
		gboolean is_active = (active_win && active_win->refnum == win->refnum);
		const char *marker = "";
		switch (win->data_level) { case DATA_LEVEL_HILIGHT: marker = "!"; break; case DATA_LEVEL_MSG: marker = "+"; break; case DATA_LEVEL_TEXT: marker = "."; break; default: marker = ""; }
		char *txt = g_strdup_printf("%d:%s%s", win->refnum, marker, name ? name : "");
		/* pad/cut to block */
		int len = format_get_length(txt);
		if (len > setting_block) {
			/* naive cut */
			txt[setting_block] = '\0';
		} else if (len < setting_block) {
			GString *pad = g_string_new(txt);
			for (int i = 0; i < setting_block - len; i++) g_string_append_c(pad, ' ');
			g_free(txt); txt = pad->str; g_string_free(pad, FALSE);
		}
		if (is_active) {
			/* Mark active by surrounding with [] within block if space permits */
			if (setting_block >= 2) {
				if (txt[0] != '[') txt[0] = '[';
				if (txt[setting_block-1] != ']') txt[setting_block-1] = ']';
			}
		}
		g_ptr_array_add(entries, txt);
		g_ptr_array_add(refnums, GINT_TO_POINTER(win->refnum));
	}
	g_slist_free(wins);

	/* compute columns/rows */
	int avail_cols = term_width;
	int step = setting_block + 1; /* +1 for separator space */
	panel_cols = step > 0 ? (avail_cols + 1) / step : 1;
	if (panel_cols < 1) panel_cols = 1;
	int total = (int) entries->len;
	int avail_rows = term_height - setting_height_adjust; if (avail_rows < 1) avail_rows = 1;
	panel_rows = (total + panel_cols - 1) / panel_cols; if (panel_rows > avail_rows) panel_rows = avail_rows;

	/* Layout: fill rows first */
	int idx = 0;
	for (int r = 0; r < panel_rows; r++) {
		GString *row = g_string_new(NULL);
		for (int c = 0; c < panel_cols; c++) {
			int k = c * panel_rows + r;
			if (k >= total) break;
			char *cell = g_strdup(g_ptr_array_index(entries, k));
			int refnum = GPOINTER_TO_INT(g_ptr_array_index(refnums, k));
			int col_start = (int) row->len;
			g_string_append(row, cell);
			int col_end = (int) row->len - 1;
			HitZone *hz = g_new0(HitZone, 1);
			hz->row = r; hz->col_start = col_start; hz->col_end = col_end; hz->win_refnum = refnum;
			g_ptr_array_add(hit_zones, hz);
			g_free(cell);
			/* separator between columns */
			if (c + 1 < panel_cols && (k + panel_rows) < total) g_string_append_c(row, ' ');
			idx++;
		}
		g_ptr_array_add(panel_items, row->str);
		g_string_free(row, FALSE);
	}

	/* free temp arrays */
	for (guint i = 0; i < entries->len; i++) g_free(g_ptr_array_index(entries, i));
	g_ptr_array_free(entries, TRUE);
	g_ptr_array_free(refnums, TRUE);
}

static void awl_redraw(void)
{
	if (setting_panel) {
		render_panel();
		/* redraw each panel row item */
		for (int i = 0; i < panel_rows; i++) {
			char name[32]; g_snprintf(name, sizeof(name), "awl_row_%d", i);
			statusbar_items_redraw(name);
		}
	} else {
		render_awl_line();
		statusbar_items_redraw("awl");
	}
}

static void sig_setup_changed(void)
{
	awl_settings_read();
	awl_redraw();
}

static void sig_window_event(void)
{
	awl_redraw();
}

/* Single-line item */
static void item_awl(SBAR_ITEM_REC *item, int get_size_only)
{
	if (awl_line == NULL) render_awl_line();
	statusbar_item_default_handler(item, TRUE, NULL, awl_line ? awl_line : "", FALSE);
	statusbar_item_default_handler(item, get_size_only ? TRUE : FALSE, NULL, awl_line ? awl_line : "", FALSE);
}

/* Panel row items */
static void item_awl_row(SBAR_ITEM_REC *item, int get_size_only)
{
	int row_index = 0;
	/* Name is awl_row_%d; we do not get name here, so rely on config ordering; we store row index in config->priority */
	if (item && item->config) row_index = item->config->priority;
	if (!panel_items || row_index < 0 || row_index >= panel_rows) {
		statusbar_item_set_size(item, 0, 0);
		return;
	}
	const char *line = g_ptr_array_index(panel_items, row_index);
	statusbar_item_default_handler(item, TRUE, NULL, line, FALSE);
	statusbar_item_default_handler(item, get_size_only ? TRUE : FALSE, NULL, line, FALSE);
}

/* Mouse handling via ESC [ M ... sequences parsed by gui-readline -> gui key pressed events.
   We approximate detection by watching three consecutive keys with offsets. */

static int mouse_state = -1; /* -1: off, 0..2 collecting */
static int mouse_triplet[3];
static int last_triplet[3];
static int setting_mouse_scroll = 3;

static void awl_mouse_enable(void)
{
	/* We do not toggle terminal mouse here; Irssi core doesn't have API for it.
	   We just parse sequences emitted to gui key pressed. */
}

static void awl_mouse_disable(void)
{
	mouse_state = -1;
}

static void sig_gui_key_pressed(int key)
{
	/* This mirrors the Perl script minimal logic: collect three bytes after ESC [ M encoded as key+32 */
	if (mouse_state == -1) return;
	if (mouse_state == 0) { memcpy(last_triplet, mouse_triplet, sizeof(last_triplet)); }
	mouse_triplet[mouse_state] = key - 32;
	mouse_state++;
	if (mouse_state < 3) return;
	mouse_state = -1;
	int btn = mouse_triplet[0];
	int x = mouse_triplet[1] - 1; /* 0-based */
	int y = mouse_triplet[2] - 1; /* 0-based */
	int last_btn = last_triplet[0];
	int last_x = last_triplet[1] - 1;
	int last_y = last_triplet[2] - 1;
	/* Only act if same spot (click release) */
	if (!setting_panel) return;
	if ((btn == 64 || btn == 65) && btn == last_btn && x == last_x && y == last_y) {
		/* Wheel: 64 up, 65 down -> scrollback */
		if (setting_mouse_scroll > 0) {
			char cmd[64]; g_snprintf(cmd, sizeof(cmd), "/scrollback goto %s%d", btn == 64 ? "-" : "+", setting_mouse_scroll);
			command_runsub("", cmd, NULL, NULL);
		}
		return;
	}
	if (btn == 64 || btn == 65) return; /* ignore plain wheel */
	/* Map y to panel row (assuming bar at bottom; accurate mapping requires knowing placement).
	   We assume rows are contiguous at bottom lines: take last panel_rows lines. */
	int row0 = term_height - panel_rows; /* top of panel */
	if (y < row0 || y >= term_height) return;
	int row = y - row0;
	/* find zone containing x */
	for (guint i = 0; i < hit_zones->len; i++) {
		HitZone *hz = g_ptr_array_index(hit_zones, i);
		if (hz->row != row) continue;
		if (x >= hz->col_start && x <= hz->col_end) {
			char cmd[32]; g_snprintf(cmd, sizeof(cmd), "/window %d", hz->win_refnum);
			command_runsub("", cmd, NULL, NULL);
			break;
		}
	}
}

static void sig_gui_read_char(int key)
{
	/* entry path to bootstrap mouse triplet collection */
	/* Not used; we rely on external binding to set mouse_state to start collecting on ESC [ M */
	(void) key;
}

static void item_register_single(void)
{
	statusbar_item_register("awl", "$0", item_awl);
}

static void item_register_panel(void)
{
	/* Create N row items with distinct internal names. We'll encode row index via priority. */
	for (int i = 0; i < 50; i++) {
		char iname[32]; g_snprintf(iname, sizeof(iname), "awl_row_%d", i);
		statusbar_item_register(iname, "$0", item_awl_row);
	}
}

void awl_core_init(void)
{
	module_register("awl", "core");
	settings_add_int("awl", "awl_hide_data", 0);
	settings_add_int("awl", "awl_hide_empty", 0);
	settings_add_bool("awl", "awl_prefer_name", FALSE);
	settings_add_str("awl", "awl_sort", "refnum");
	settings_add_str("awl", "awl_separator", " ");
	settings_add_bool("awl", "awl_panel", TRUE);
	settings_add_int("awl", "awl_block", 15);
	settings_add_int("awl", "awl_height_adjust", 2);
	settings_add_int("awl", "mouse_scroll", 3);

	awl_settings_read();
	setting_mouse_scroll = settings_get_int("mouse_scroll");

	if (setting_panel) item_register_panel(); else item_register_single();

	signal_add("setup changed", (SIGNAL_FUNC) sig_setup_changed);
	signal_add("window changed", (SIGNAL_FUNC) sig_window_event);
	signal_add("window item changed", (SIGNAL_FUNC) sig_window_event);
	signal_add("window created", (SIGNAL_FUNC) sig_window_event);
	signal_add("window destroyed", (SIGNAL_FUNC) sig_window_event);
	signal_add("window name changed", (SIGNAL_FUNC) sig_window_event);
	signal_add("window refnum changed", (SIGNAL_FUNC) sig_window_event);
	signal_add("window hilight", (SIGNAL_FUNC) sig_window_event);
	/* mouse: hook gui key pressed to parse triplets */
	signal_add("gui key pressed", (SIGNAL_FUNC) sig_gui_key_pressed);
	awl_mouse_enable();

	awl_redraw();
}

void awl_core_deinit(void)
{
	signal_remove("setup changed", (SIGNAL_FUNC) sig_setup_changed);
	signal_remove("window changed", (SIGNAL_FUNC) sig_window_event);
	signal_remove("window item changed", (SIGNAL_FUNC) sig_window_event);
	signal_remove("window created", (SIGNAL_FUNC) sig_window_event);
	signal_remove("window destroyed", (SIGNAL_FUNC) sig_window_event);
	signal_remove("window name changed", (SIGNAL_FUNC) sig_window_event);
	signal_remove("window refnum changed", (SIGNAL_FUNC) sig_window_event);
	signal_remove("window hilight", (SIGNAL_FUNC) sig_window_event);
	signal_remove("gui key pressed", (SIGNAL_FUNC) sig_gui_key_pressed);

	if (setting_panel) {
		for (int i = 0; i < 50; i++) { char iname[32]; g_snprintf(iname, sizeof(iname), "awl_row_%d", i); statusbar_item_unregister(iname); }
		clear_panel_cache();
	} else {
		statusbar_item_unregister("awl");
		free_cached_line();
	}
}

void awl_core_abicheck(int *version)
{
	*version = IRSSI_ABI_VERSION;
}