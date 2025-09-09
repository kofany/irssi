/*
 fe-web-utils.c : Utility functions for web frontend

    Copyright (C) 2025 irssip project
*/

#include "module.h"
#include "fe-web.h"
#include <irssip/src/core/levels.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>

/* WebSocket magic string for handshake */
#define WEBSOCKET_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

WEB_MESSAGE_REC *fe_web_message_new(WEB_MESSAGE_TYPE type)
{
	WEB_MESSAGE_REC *msg;
	
	msg = g_new0(WEB_MESSAGE_REC, 1);
	msg->type = type;
	msg->timestamp = time(NULL);
	msg->extra_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	
	return msg;
}

void fe_web_message_free(WEB_MESSAGE_REC *msg)
{
	if (msg == NULL) return;
	
	g_free(msg->server_tag);
	g_free(msg->target);
	g_free(msg->nick);
	g_free(msg->text);
	
	if (msg->extra_data) {
		g_hash_table_destroy(msg->extra_data);
	}
	
	g_free(msg);
}

char *fe_web_escape_json_string(const char *str)
{
	GString *escaped;
	const char *p;
	
	if (str == NULL) return g_strdup("null");
	
	escaped = g_string_new("\"");
	
	for (p = str; *p != '\0'; p++) {
		switch (*p) {
			case '"':
				g_string_append(escaped, "\\\"");
				break;
			case '\\':
				g_string_append(escaped, "\\\\");
				break;
			case '\b':
				g_string_append(escaped, "\\b");
				break;
			case '\f':
				g_string_append(escaped, "\\f");
				break;
			case '\n':
				g_string_append(escaped, "\\n");
				break;
			case '\r':
				g_string_append(escaped, "\\r");
				break;
			case '\t':
				g_string_append(escaped, "\\t");
				break;
			default:
				if (*p < 32) {
					g_string_append_printf(escaped, "\\u%04x", (unsigned char)*p);
				} else {
					g_string_append_c(escaped, *p);
				}
				break;
		}
	}
	
	g_string_append_c(escaped, '"');
	return g_string_free(escaped, FALSE);
}

static void hash_table_to_json(gpointer key, gpointer value, gpointer user_data)
{
	GString *json = (GString *)user_data;
	char *escaped_key = fe_web_escape_json_string((char *)key);
	char *escaped_value = fe_web_escape_json_string((char *)value);
	
	if (json->len > 1) { /* More than just opening brace */
		g_string_append_c(json, ',');
	}
	
	g_string_append_printf(json, "%s:%s", escaped_key, escaped_value);
	
	g_free(escaped_key);
	g_free(escaped_value);
}

char *fe_web_message_to_json(WEB_MESSAGE_REC *msg)
{
	GString *json;
	char *escaped_server, *escaped_target, *escaped_nick, *escaped_text;
	char *extra_json;
	
	if (msg == NULL) return g_strdup("{}");
	
	json = g_string_new("{");
	
	/* Basic fields */
	g_string_append_printf(json, "\"type\":%d,", msg->type);
	g_string_append_printf(json, "\"timestamp\":%ld,", msg->timestamp);
	g_string_append_printf(json, "\"level\":%d", msg->level);
	
	/* Optional string fields */
	if (msg->server_tag) {
		escaped_server = fe_web_escape_json_string(msg->server_tag);
		g_string_append_printf(json, ",\"server\":%s", escaped_server);
		g_free(escaped_server);
	}
	
	if (msg->target) {
		escaped_target = fe_web_escape_json_string(msg->target);
		g_string_append_printf(json, ",\"channel\":%s", escaped_target);
		g_free(escaped_target);
	}
	
	if (msg->nick) {
		escaped_nick = fe_web_escape_json_string(msg->nick);
		g_string_append_printf(json, ",\"nick\":%s", escaped_nick);
		g_free(escaped_nick);
	}
	
	if (msg->text) {
		escaped_text = fe_web_escape_json_string(msg->text);
		g_string_append_printf(json, ",\"text\":%s", escaped_text);
		g_free(escaped_text);
	}
	
	/* Extra data */
	if (msg->extra_data && g_hash_table_size(msg->extra_data) > 0) {
		GString *extra = g_string_new("{");
		g_hash_table_foreach(msg->extra_data, hash_table_to_json, extra);
		g_string_append_c(extra, '}');
		extra_json = g_string_free(extra, FALSE);
		g_string_append_printf(json, ",\"extra\":%s", extra_json);
		g_free(extra_json);
	}
	
	g_string_append_c(json, '}');
	return g_string_free(json, FALSE);
}

WEB_MESSAGE_REC *fe_web_message_from_json(const char *json)
{
	/* Simple JSON parser for commands from client */
	/* This is a basic implementation - in production you'd use a proper JSON library */
	WEB_MESSAGE_REC *msg;
	
	if (json == NULL) return NULL;
	
	msg = fe_web_message_new(WEB_MSG_COMMAND);
	
	/* Parse basic command structure: {"type":"command","server":"tag","channel":"#chan","command":"/join #test"} */
	/* This is a simplified parser - real implementation would be more robust */
	
	return msg;
}

char *fe_web_websocket_create_response(const char *key)
{
	char *concat_key;
	unsigned char hash[SHA_DIGEST_LENGTH];
	char *base64_hash;
	char *response;
	
	if (key == NULL) return NULL;
	
	/* Concatenate key with magic string */
	concat_key = g_strdup_printf("%s%s", key, WEBSOCKET_MAGIC_STRING);
	
	/* Calculate SHA1 hash */
	SHA1((unsigned char *)concat_key, strlen(concat_key), hash);
	
	/* Base64 encode */
	base64_hash = g_base64_encode(hash, SHA_DIGEST_LENGTH);
	
	/* Create response */
	response = g_strdup_printf(
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n"
		"\r\n",
		base64_hash
	);
	
	g_free(concat_key);
	g_free(base64_hash);
	
	return response;
}

/* WebSocket frame encoding/decoding */
static void fe_web_websocket_encode_frame(const char *payload, gsize payload_len, 
                                         char **frame, gsize *frame_len)
{
	gsize header_len = 2;
	char *buffer;
	
	if (payload_len > 65535) {
		header_len += 8; /* 64-bit length */
	} else if (payload_len > 125) {
		header_len += 2; /* 16-bit length */
	}
	
	*frame_len = header_len + payload_len;
	buffer = g_malloc(*frame_len);
	
	/* First byte: FIN=1, opcode=1 (text frame) */
	buffer[0] = 0x81;
	
	/* Payload length */
	if (payload_len > 65535) {
		buffer[1] = 127;
		/* 64-bit length (big endian) */
		for (int i = 0; i < 8; i++) {
			buffer[2 + i] = (payload_len >> (8 * (7 - i))) & 0xFF;
		}
	} else if (payload_len > 125) {
		buffer[1] = 126;
		/* 16-bit length (big endian) */
		buffer[2] = (payload_len >> 8) & 0xFF;
		buffer[3] = payload_len & 0xFF;
	} else {
		buffer[1] = payload_len & 0x7F;
	}
	
	/* Copy payload */
	memcpy(buffer + header_len, payload, payload_len);
	
	*frame = buffer;
}

void fe_web_client_send_message(WEB_CLIENT_REC *client, WEB_MESSAGE_REC *msg)
{
	char *json;
	char *frame;
	gsize frame_len;
	
	if (client == NULL || msg == NULL || client->fd < 0) return;
	
	json = fe_web_message_to_json(msg);
	fe_web_websocket_encode_frame(json, strlen(json), &frame, &frame_len);
	
	send(client->fd, frame, frame_len, 0);
	
	g_free(json);
	g_free(frame);
}

void fe_web_broadcast_message(WEB_MESSAGE_REC *msg)
{
	GSList *tmp;
	
	for (tmp = web_clients; tmp != NULL; tmp = tmp->next) {
		WEB_CLIENT_REC *client = tmp->data;
		if (client->handshake_done) {
			fe_web_client_send_message(client, msg);
		}
	}
}
