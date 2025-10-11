/*
 fe-web-server.c : TCP/WebSocket server for fe-web

    Copyright (C) 2025

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "module.h"
#include "fe-web.h"

#include <irssi/src/core/network.h>
#include <irssi/src/core/net-sendbuffer.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/fe-common/core/printtext.h>

#include <string.h>
#include <errno.h>

static GIOChannel *listen_channel = NULL;
static int listen_port = -1;
static int listen_tag = -1;

/* Forward declarations */
static void sig_listen(void);
static void client_input(WEB_CLIENT_REC *client);

/* Find client by file descriptor */
static WEB_CLIENT_REC *fe_web_find_client_by_fd(int fd)
{
	GSList *tmp;

	for (tmp = web_clients; tmp != NULL; tmp = tmp->next) {
		WEB_CLIENT_REC *client = tmp->data;
		if (client->fd == fd) {
			return client;
		}
	}

	return NULL;
}

/* Close client connection */
static void fe_web_close_client(WEB_CLIENT_REC *client)
{
	if (client == NULL) {
		return;
	}

	/* Remove input handler */
	if (client->recv_tag != -1) {
		g_source_remove(client->recv_tag);
		client->recv_tag = -1;
	}

	/* Close socket */
	if (client->handle != NULL) {
		net_sendbuffer_destroy(client->handle, TRUE);
		client->handle = NULL;
	}

	/* Destroy client record */
	fe_web_client_destroy(client);
}

/* Handle WebSocket handshake (simplified version) */
static int fe_web_handle_handshake(WEB_CLIENT_REC *client, const char *data)
{
	char *key_line;
	char *key_start;
	char *key_end;
	GString *response;

	/* Look for Sec-WebSocket-Key header */
	key_line = strstr(data, "Sec-WebSocket-Key:");
	if (key_line == NULL) {
		return 0; /* Not complete handshake yet */
	}

	/* Extract key value */
	key_start = key_line + strlen("Sec-WebSocket-Key:");
	while (*key_start == ' ' || *key_start == '\t') {
		key_start++;
	}

	key_end = strchr(key_start, '\r');
	if (key_end == NULL) {
		key_end = strchr(key_start, '\n');
	}

	if (key_end == NULL) {
		return 0;
	}

	/* Store key */
	if (client->websocket_key != NULL) {
		g_free(client->websocket_key);
	}
	client->websocket_key = g_strndup(key_start, key_end - key_start);

	/* Build handshake response (simplified - no SHA1/base64 for now) */
	response = g_string_new("");
	g_string_append(response, "HTTP/1.1 101 Switching Protocols\r\n");
	g_string_append(response, "Upgrade: websocket\r\n");
	g_string_append(response, "Connection: Upgrade\r\n");
	g_string_append_printf(response, "Sec-WebSocket-Accept: %s\r\n",
	                      client->websocket_key);
	g_string_append(response, "\r\n");

	/* Send response */
	if (client->handle != NULL) {
		net_sendbuffer_send(client->handle, response->str, response->len);
	}

	g_string_free(response, TRUE);

	client->handshake_done = TRUE;
	return 1;
}

/* Handle WebSocket frame (simplified - text frames only) */
static void fe_web_handle_frame(WEB_CLIENT_REC *client, const char *data, int len)
{
	/* TODO: Implement proper WebSocket frame parsing */
	/* For now, assume data is JSON text */
	/* Frame format: FIN+opcode (1 byte), mask+length (1+ bytes), mask key (4 bytes), payload */

	/* Skip WebSocket framing for now */
	/* This will be implemented properly in Phase 2 */

	fe_web_client_handle_message(client, data);
}

/* Read data from client */
static void client_input(WEB_CLIENT_REC *client)
{
	char *str;
	int ret;

	if (client == NULL || client->handle == NULL) {
		return;
	}

	/* Use net_sendbuffer_receive_line for line-based protocol */
	ret = net_sendbuffer_receive_line(client->handle, &str, 1);

	if (ret == -1) {
		/* Connection closed or error */
		fe_web_close_client(client);
		return;
	}

	if (ret == 0) {
		/* No complete line yet */
		return;
	}

	/* Handle handshake first */
	if (!client->handshake_done) {
		if (fe_web_handle_handshake(client, str)) {
			/* Handshake complete - send auth_ok */
			WEB_MESSAGE_REC *msg;
			msg = fe_web_message_new(WEB_MSG_AUTH_OK);
			msg->id = fe_web_generate_message_id();
			fe_web_send_message(client, msg);
			fe_web_message_free(msg);

			client->authenticated = TRUE;
		}
		return;
	}

	/* Handle WebSocket frames */
	/* TODO: For now we assume line-based JSON, will implement proper WS framing later */
	fe_web_client_handle_message(client, str);
}

/* Accept new connection */
static void sig_listen(void)
{
	IPADDR ip;
	int port;
	GIOChannel *handle;
	char host[MAX_IP_LEN];
	char *addr;
	WEB_CLIENT_REC *client;
	NET_SENDBUF_REC *sendbuf;

	/* Accept connection */
	handle = net_accept(listen_channel, &ip, &port);
	if (handle == NULL) {
		return;
	}

	/* Get address string */
	net_ip2host(&ip, host);
	addr = g_strdup_printf("%s:%d", host, port);

	/* Create client record */
	client = fe_web_client_create(g_io_channel_unix_get_fd(handle), addr);

	/* Create send buffer */
	sendbuf = net_sendbuffer_create(handle, 0);
	client->handle = sendbuf;

	/* Add input handler */
	client->recv_tag = i_input_add(handle, I_INPUT_READ,
	                               (GInputFunction) client_input, client);

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: New connection from %s (id: %s)", addr, client->id);

	g_free(addr);
}

/* Initialize server */
void fe_web_server_init(void)
{
	IPADDR *bind_ip;
	const char *bind_addr;
	int port;

	/* Check if already running */
	if (listen_channel != NULL) {
		return;
	}

	/* Get settings */
	port = settings_get_int("fe_web_port");
	bind_addr = settings_get_str("fe_web_bind");

	/* Parse bind address */
	bind_ip = g_new0(IPADDR, 1);
	if (net_host2ip(bind_addr, bind_ip) != 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
		          "fe-web: Invalid bind address: %s", bind_addr);
		g_free(bind_ip);
		return;
	}

	/* Create listening socket */
	listen_channel = net_listen(bind_ip, &port);
	g_free(bind_ip);

	if (listen_channel == NULL) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR,
		          "fe-web: Failed to bind to %s:%d: %s",
		          bind_addr, port, strerror(errno));
		return;
	}

	listen_port = port;

	/* Add input handler */
	listen_tag = i_input_add(listen_channel, I_INPUT_READ,
	                         (GInputFunction) sig_listen, NULL);

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: WebSocket server listening on %s:%d",
	          bind_addr, port);
}

/* Deinitialize server */
void fe_web_server_deinit(void)
{
	GSList *tmp;
	GSList *next;

	/* Close all clients */
	for (tmp = web_clients; tmp != NULL; tmp = next) {
		WEB_CLIENT_REC *client = tmp->data;
		next = tmp->next;
		fe_web_close_client(client);
	}

	/* Close listening socket */
	if (listen_tag != -1) {
		g_source_remove(listen_tag);
		listen_tag = -1;
	}

	if (listen_channel != NULL) {
		net_disconnect(listen_channel);
		listen_channel = NULL;
	}

	listen_port = -1;

	printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
	          "fe-web: WebSocket server stopped");
}
