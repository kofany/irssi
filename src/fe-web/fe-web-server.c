/*
 fe-web-server.c : WebSocket server for web frontend

    Copyright (C) 2025 irssip project
*/

#include "module.h"
#include "fe-web.h"
#include <irssip/src/core/network.h>
#include <irssip/src/fe-common/core/printtext.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

static GIOChannel *web_server_channel = NULL;
static guint web_server_tag = 0;

/* HTTP response for serving static files */
static const char *http_response_header = 
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/html; charset=utf-8\r\n"
	"Content-Length: %d\r\n"
	"Connection: close\r\n"
	"\r\n";

/* WebSocket handshake response */
static const char *websocket_response = 
	"HTTP/1.1 101 Switching Protocols\r\n"
	"Upgrade: websocket\r\n"
	"Connection: Upgrade\r\n"
	"Sec-WebSocket-Accept: %s\r\n"
	"\r\n";

static gboolean web_server_accept(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd;
	WEB_CLIENT_REC *client;
	
	if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
		         "Web server socket error");
		return FALSE;
	}
	
	client_fd = accept(web_server_fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd < 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
		         "Failed to accept web client: %s", strerror(errno));
		return TRUE;
	}
	
	/* Check client limit */
	if (g_slist_length(web_clients) >= settings_get_int("web_frontend_max_clients")) {
		close(client_fd);
		return TRUE;
	}
	
	/* Set non-blocking */
	fcntl(client_fd, F_SETFL, O_NONBLOCK);
	
	/* Create client record */
	client = fe_web_client_new(client_fd);
	web_clients = g_slist_append(web_clients, client);
	
	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
	         "Web client connected from %s (total: %d)",
	         inet_ntoa(client_addr.sin_addr), g_slist_length(web_clients));
	
	return TRUE;
}

gboolean fe_web_server_start(int port)
{
	struct sockaddr_in server_addr;
	const char *bind_addr;
	int opt = 1;
	
	if (web_server_running) {
		return TRUE;
	}
	
	/* Create socket */
	web_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (web_server_fd < 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
		         "Failed to create web server socket: %s", strerror(errno));
		return FALSE;
	}
	
	/* Set socket options */
	setsockopt(web_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	
	/* Bind socket */
	bind_addr = settings_get_str("web_frontend_bind");
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(bind_addr);
	
	if (bind(web_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
		         "Failed to bind web server to %s:%d: %s", 
		         bind_addr, port, strerror(errno));
		close(web_server_fd);
		web_server_fd = -1;
		return FALSE;
	}
	
	/* Listen for connections */
	if (listen(web_server_fd, 10) < 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
		         "Failed to listen on web server socket: %s", strerror(errno));
		close(web_server_fd);
		web_server_fd = -1;
		return FALSE;
	}
	
	/* Set up GLib event handling */
	web_server_channel = g_io_channel_unix_new(web_server_fd);
	web_server_tag = g_io_add_watch(web_server_channel, G_IO_IN | G_IO_ERR | G_IO_HUP,
	                               web_server_accept, NULL);
	
	web_server_running = TRUE;
	return TRUE;
}

void fe_web_server_stop(void)
{
	GSList *tmp;
	
	if (!web_server_running) {
		return;
	}
	
	/* Disconnect all clients */
	tmp = web_clients;
	while (tmp != NULL) {
		WEB_CLIENT_REC *client = tmp->data;
		tmp = tmp->next;
		fe_web_client_free(client);
	}
	g_slist_free(web_clients);
	web_clients = NULL;
	
	/* Close server socket */
	if (web_server_tag > 0) {
		g_source_remove(web_server_tag);
		web_server_tag = 0;
	}
	
	if (web_server_channel) {
		g_io_channel_unref(web_server_channel);
		web_server_channel = NULL;
	}
	
	if (web_server_fd >= 0) {
		close(web_server_fd);
		web_server_fd = -1;
	}
	
	web_server_running = FALSE;
}

void fe_web_server_init(void)
{
	web_clients = NULL;
	web_server_fd = -1;
	web_server_running = FALSE;
}

void fe_web_server_deinit(void)
{
	fe_web_server_stop();
}
