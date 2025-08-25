/*
 fe-web-client.c : WebSocket client handling for web frontend

    Copyright (C) 2025 irssip project
*/

#include "module.h"
#include "fe-web.h"
#include <src/core/commands.h>
#include <src/core/signals.h>
#include <src/core/levels.h>
#include <src/fe-common/core/printtext.h>

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static GIOChannel *client_channels[FE_WEB_MAX_CLIENTS];
static guint client_tags[FE_WEB_MAX_CLIENTS];

static gboolean client_input_handler(GIOChannel *source, GIOCondition condition, gpointer data);

WEB_CLIENT_REC *fe_web_client_new(int fd)
{
	WEB_CLIENT_REC *client;
	GIOChannel *channel;
	guint tag;
	
	client = g_new0(WEB_CLIENT_REC, 1);
	client->fd = fd;
	client->id = g_strdup_printf("client_%d_%ld", fd, time(NULL));
	client->connected = time(NULL);
	client->handshake_done = FALSE;
	client->authenticated = FALSE;
	client->output_buffer = g_string_new("");
	client->buffer_full = FALSE;
	
	/* Set up GLib event handling for this client */
	channel = g_io_channel_unix_new(fd);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	
	tag = g_io_add_watch(channel, G_IO_IN | G_IO_ERR | G_IO_HUP,
	                    client_input_handler, client);
	
	/* Store channel and tag for cleanup */
	for (int i = 0; i < FE_WEB_MAX_CLIENTS; i++) {
		if (client_channels[i] == NULL) {
			client_channels[i] = channel;
			client_tags[i] = tag;
			break;
		}
	}
	
	return client;
}

void fe_web_client_free(WEB_CLIENT_REC *client)
{
	if (client == NULL) return;
	
	/* Remove from global list */
	web_clients = g_slist_remove(web_clients, client);
	
	/* Clean up GLib event handling */
	for (int i = 0; i < FE_WEB_MAX_CLIENTS; i++) {
		if (client_channels[i] && g_io_channel_unix_get_fd(client_channels[i]) == client->fd) {
			g_source_remove(client_tags[i]);
			g_io_channel_unref(client_channels[i]);
			client_channels[i] = NULL;
			client_tags[i] = 0;
			break;
		}
	}
	
	/* Close socket */
	if (client->fd >= 0) {
		close(client->fd);
	}
	
	/* Free memory */
	g_free(client->id);
	g_free(client->user_agent);
	g_free(client->websocket_key);
	
	if (client->output_buffer) {
		g_string_free(client->output_buffer, TRUE);
	}
	
	g_free(client);
	
	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
	         "Web client disconnected (total: %d)", g_slist_length(web_clients));
}

gboolean fe_web_client_websocket_handshake(WEB_CLIENT_REC *client, const char *request)
{
	char **lines;
	char *websocket_key = NULL;
	char *response;
	gboolean is_websocket = FALSE;
	int i;
	
	if (client == NULL || request == NULL) return FALSE;
	
	lines = g_strsplit(request, "\r\n", -1);
	
	/* Parse HTTP headers */
	for (i = 0; lines[i] != NULL; i++) {
		if (g_str_has_prefix(lines[i], "Upgrade:") && 
		    strstr(lines[i], "websocket")) {
			is_websocket = TRUE;
		} else if (g_str_has_prefix(lines[i], "Sec-WebSocket-Key:")) {
			websocket_key = g_strdup(lines[i] + 18);
			g_strstrip(websocket_key);
		} else if (g_str_has_prefix(lines[i], "User-Agent:")) {
			client->user_agent = g_strdup(lines[i] + 11);
			g_strstrip(client->user_agent);
		}
	}
	
	g_strfreev(lines);
	
	if (!is_websocket || !websocket_key) {
		g_free(websocket_key);
		return FALSE;
	}
	
	/* Create WebSocket response */
	response = fe_web_websocket_create_response(websocket_key);
	if (response) {
		send(client->fd, response, strlen(response), 0);
		client->handshake_done = TRUE;
		client->websocket_key = websocket_key;
		
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
		         "Web client completed WebSocket handshake: %s", 
		         client->user_agent ? client->user_agent : "unknown");
		
		g_free(response);
		return TRUE;
	}
	
	g_free(websocket_key);
	return FALSE;
}

static void fe_web_client_handle_command(WEB_CLIENT_REC *client, const char *command)
{
	char **parts;
	const char *cmd;
	
	if (command == NULL || !g_str_has_prefix(command, "/")) {
		return;
	}
	
	parts = g_strsplit(command + 1, " ", -1); /* Skip leading / */
	if (parts[0] == NULL) {
		g_strfreev(parts);
		return;
	}
	
	cmd = parts[0];
	
	/* Handle basic IRC commands */
	if (g_ascii_strcasecmp(cmd, "connect") == 0) {
		if (parts[1]) {
			signal_emit("command connect", 2, parts[1], "");
		}
	} else if (g_ascii_strcasecmp(cmd, "join") == 0) {
		if (parts[1]) {
			signal_emit("command join", 2, parts[1], "");
		}
	} else if (g_ascii_strcasecmp(cmd, "part") == 0) {
		if (parts[1]) {
			signal_emit("command part", 2, parts[1], parts[2] ? parts[2] : "");
		}
	} else if (g_ascii_strcasecmp(cmd, "msg") == 0) {
		if (parts[1] && parts[2]) {
			char *message = g_strjoinv(" ", parts + 2);
			signal_emit("command msg", 2, parts[1], message);
			g_free(message);
		}
	} else if (g_ascii_strcasecmp(cmd, "nick") == 0) {
		if (parts[1]) {
			signal_emit("command nick", 2, parts[1], "");
		}
	} else if (g_ascii_strcasecmp(cmd, "quit") == 0) {
		char *reason = parts[1] ? g_strjoinv(" ", parts + 1) : NULL;
		signal_emit("command quit", 2, reason ? reason : "", "");
		g_free(reason);
	} else {
		/* Forward unknown commands to irssi */
		char *full_command = g_strjoinv(" ", parts);
		signal_emit("send command", 1, full_command);
		g_free(full_command);
	}
	
	g_strfreev(parts);
}

void fe_web_client_handle_message(WEB_CLIENT_REC *client, const char *data)
{
	/* Simple JSON parsing for client messages */
	/* In production, you'd use a proper JSON library */
	
	if (client == NULL || data == NULL) return;
	
	/* Look for command in JSON: {"type":"command","command":"/join #test"} */
	if (strstr(data, "\"type\":\"command\"") && strstr(data, "\"command\":")) {
		char *command_start = strstr(data, "\"command\":\"");
		if (command_start) {
			char *command_end;
			command_start += 11; /* Skip "command":" */
			command_end = strchr(command_start, '"');
			if (command_end) {
				char *command = g_strndup(command_start, command_end - command_start);
				fe_web_client_handle_command(client, command);
				g_free(command);
			}
		}
	}
}

static char *fe_web_websocket_decode_frame(const char *frame, gsize frame_len, gsize *payload_len)
{
	const unsigned char *data = (const unsigned char *)frame;
	gsize header_len = 2;
	gsize len;
	gboolean masked;
	unsigned char mask[4];
	char *payload;
	gsize i;
	
	if (frame_len < 2) return NULL;
	
	/* Check if frame is masked */
	masked = (data[1] & 0x80) != 0;
	
	/* Get payload length */
	len = data[1] & 0x7F;
	
	if (len == 126) {
		if (frame_len < 4) return NULL;
		len = (data[2] << 8) | data[3];
		header_len = 4;
	} else if (len == 127) {
		if (frame_len < 10) return NULL;
		/* 64-bit length - simplified for now */
		len = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
		header_len = 10;
	}
	
	if (masked) {
		if (frame_len < header_len + 4) return NULL;
		memcpy(mask, data + header_len, 4);
		header_len += 4;
	}
	
	if (frame_len < header_len + len) return NULL;
	
	payload = g_malloc(len + 1);
	memcpy(payload, data + header_len, len);
	payload[len] = '\0';
	
	/* Unmask payload if needed */
	if (masked) {
		for (i = 0; i < len; i++) {
			payload[i] ^= mask[i % 4];
		}
	}
	
	*payload_len = len;
	return payload;
}

static gboolean client_input_handler(GIOChannel *source, GIOCondition condition, gpointer data)
{
	WEB_CLIENT_REC *client = (WEB_CLIENT_REC *)data;
	char buffer[FE_WEB_BUFFER_SIZE];
	gssize bytes_read;
	
	if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) {
		fe_web_client_free(client);
		return FALSE;
	}
	
	bytes_read = recv(client->fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read <= 0) {
		fe_web_client_free(client);
		return FALSE;
	}
	
	buffer[bytes_read] = '\0';
	
	if (!client->handshake_done) {
		/* Handle WebSocket handshake */
		if (!fe_web_client_websocket_handshake(client, buffer)) {
			fe_web_client_free(client);
			return FALSE;
		}
	} else {
		/* Handle WebSocket frame */
		gsize payload_len;
		char *payload = fe_web_websocket_decode_frame(buffer, bytes_read, &payload_len);
		
		if (payload) {
			fe_web_client_handle_message(client, payload);
			g_free(payload);
		}
	}
	
	return TRUE;
}
