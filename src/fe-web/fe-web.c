/*
 fe-web.c : Web frontend for irssi

    Copyright (C) 2025 irssi project

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#include "module.h"
#include "fe-web.h"
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/core/modules.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/fe-common/core/themes.h>

/* Global variables */
#define MODULE_NAME "fe-web"

GSList *web_clients = NULL;
int web_server_fd = -1;
gboolean web_server_running = FALSE;

static void sig_exit(void)
{
	fe_web_server_stop();
}

static void sig_settings_changed(void)
{
	int port;
	gboolean enabled;
	
	enabled = settings_get_bool("web_frontend_enabled");
	port = settings_get_int("web_frontend_port");
	
	if (enabled && !web_server_running) {
		if (!fe_web_server_start(port)) {
			printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
			         "Failed to start web frontend on port %d", port);
		} else {
			printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
			         "Web frontend started on port %d", port);
		}
	} else if (!enabled && web_server_running) {
		fe_web_server_stop();
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
		         "Web frontend stopped");
	}
}

void fe_web_init(void)
{
	/* Register settings */
	settings_add_bool("lookandfeel", "web_frontend_enabled", FALSE);
	settings_add_int("lookandfeel", "web_frontend_port", FE_WEB_DEFAULT_PORT);
	settings_add_str("lookandfeel", "web_frontend_bind", "127.0.0.1");
	settings_add_int("lookandfeel", "web_frontend_max_clients", FE_WEB_MAX_CLIENTS);
	settings_add_bool("lookandfeel", "web_frontend_auth_required", TRUE);
	settings_add_str("lookandfeel", "web_frontend_static_path", "/opt/irssi/share/irssi/web");
	
	/* Initialize components */
	fe_web_server_init();
	fe_web_signals_init();
	fe_web_api_init();
	
	/* Connect signals */
	signal_add("gui exit", (SIGNAL_FUNC) sig_exit);
	signal_add("setup changed", (SIGNAL_FUNC) sig_settings_changed);
	
	/* Auto-start if enabled */
	sig_settings_changed();
	
	module_register("web", "fe");
	
	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
	         "Web frontend module loaded. Use /SET web_frontend_enabled ON to start.");
}

void fe_web_deinit(void)
{
	signal_remove("gui exit", (SIGNAL_FUNC) sig_exit);
	signal_remove("setup changed", (SIGNAL_FUNC) sig_settings_changed);
	
	fe_web_server_stop();
	
	fe_web_api_deinit();
	fe_web_signals_deinit();
	fe_web_server_deinit();
	
	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
	         "Web frontend module unloaded.");
}

void fe_web_abicheck(int *version)
{
	*version = IRSSI_ABI_VERSION;
}
