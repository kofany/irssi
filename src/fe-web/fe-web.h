#ifndef __FE_WEB_H
#define __FE_WEB_H

#include <src/common.h>
#include <src/core/core.h>
#include <src/core/servers.h>
#include <time.h>
#include <glib.h>

/* Forward declarations */
typedef struct _SERVER_REC SERVER_REC;
typedef struct _WINDOW_REC WINDOW_REC;

/* WebSocket server configuration */
#define FE_WEB_DEFAULT_PORT 9001
#define FE_WEB_MAX_CLIENTS 50
#define FE_WEB_BUFFER_SIZE 8192

/* Client connection structure */
typedef struct _WEB_CLIENT_REC {
	int fd;
	char *id;
	char *user_agent;
	time_t connected;
	gboolean authenticated;
	
	/* WebSocket state */
	gboolean handshake_done;
	char *websocket_key;
	
	/* Irssi context */
	SERVER_REC *active_server;
	WINDOW_REC *active_window;
	
	/* Output buffer */
	GString *output_buffer;
	gboolean buffer_full;
} WEB_CLIENT_REC;

/* WebSocket message types */
typedef enum {
	WEB_MSG_CHAT = 1,
	WEB_MSG_COMMAND,
	WEB_MSG_WINDOW_CHANGE,
	WEB_MSG_SERVER_STATUS,
	WEB_MSG_NICK_LIST,
	WEB_MSG_TOPIC_CHANGE,
	WEB_MSG_HIGHLIGHT,
	WEB_MSG_NOTIFICATION,
	WEB_MSG_ERROR
} WEB_MESSAGE_TYPE;

/* JSON message structure */
typedef struct _WEB_MESSAGE_REC {
	WEB_MESSAGE_TYPE type;
	char *server_tag;
	char *target;
	char *nick;
	char *text;
	int level;
	time_t timestamp;
	GHashTable *extra_data;
} WEB_MESSAGE_REC;

/* Function declarations */

/* fe-web-server.c */
void fe_web_server_init(void);
void fe_web_server_deinit(void);
gboolean fe_web_server_start(int port);
void fe_web_server_stop(void);
void fe_web_client_send_message(WEB_CLIENT_REC *client, WEB_MESSAGE_REC *msg);
void fe_web_broadcast_message(WEB_MESSAGE_REC *msg);

/* fe-web-signals.c */
void fe_web_signals_init(void);
void fe_web_signals_deinit(void);

/* fe-web-api.c */
void fe_web_api_init(void);
void fe_web_api_deinit(void);
char *fe_web_api_serialize_window(WINDOW_REC *window);
char *fe_web_api_serialize_server(SERVER_REC *server);
char *fe_web_api_serialize_channel(CHANNEL_REC *channel);

/* fe-web-utils.c */
WEB_MESSAGE_REC *fe_web_message_new(WEB_MESSAGE_TYPE type);
void fe_web_message_free(WEB_MESSAGE_REC *msg);
char *fe_web_message_to_json(WEB_MESSAGE_REC *msg);
WEB_MESSAGE_REC *fe_web_message_from_json(const char *json);
char *fe_web_escape_json_string(const char *str);
char *fe_web_websocket_create_response(const char *key);

/* fe-web-client.c */
WEB_CLIENT_REC *fe_web_client_new(int fd);
void fe_web_client_free(WEB_CLIENT_REC *client);
void fe_web_client_handle_message(WEB_CLIENT_REC *client, const char *data);
gboolean fe_web_client_websocket_handshake(WEB_CLIENT_REC *client, const char *request);

/* Global variables */
extern GSList *web_clients;
extern int web_server_fd;
extern gboolean web_server_running;

#endif
