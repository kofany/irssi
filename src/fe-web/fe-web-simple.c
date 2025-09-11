/*
 * fe-web-simple.c : Minimal web frontend module for irssi
 * Based on sb_splits.c example
 */

#include <irssi/src/common.h>
#include <irssi/src/core/modules.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/levels.h>
#include <irssi/src/core/network.h>
#include <irssi/src/core/servers.h>
#include <irssi/src/core/channels.h>
#include <irssi/src/fe-common/core/printtext.h>

#include <glib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define MODULE_NAME "fe_web"
#define DEFAULT_PORT 9001
#define BACKLOG 10

/* Global state */
static int server_fd = -1;
static GIOChannel *server_channel = NULL;
static guint server_tag = 0;
static GSList *clients = NULL;

typedef struct {
	int fd;
	GIOChannel *channel;
	guint tag;
	char *buffer;
	gsize buffer_size;
	gsize buffer_pos;
} WebClient;

/* Forward declarations */
static void cleanup_server(void);
static void cleanup_client(WebClient *client);
static gboolean server_accept_callback(GIOChannel *source, GIOCondition condition, gpointer data);
static gboolean client_read_callback(GIOChannel *source, GIOCondition condition, gpointer data);

static void cleanup_client(WebClient *client)
{
	if (client == NULL) return;
	
	if (client->tag > 0) {
		g_source_remove(client->tag);
	}
	
	if (client->channel) {
		g_io_channel_unref(client->channel);
	}
	
	if (client->fd >= 0) {
		close(client->fd);
	}
	
	g_free(client->buffer);
	clients = g_slist_remove(clients, client);
	g_free(client);
}

static void cleanup_server(void)
{
	/* Clean up all clients */
	while (clients) {
		cleanup_client((WebClient*)clients->data);
	}
	
	/* Clean up server */
	if (server_tag > 0) {
		g_source_remove(server_tag);
		server_tag = 0;
	}
	
	if (server_channel) {
		g_io_channel_unref(server_channel);
		server_channel = NULL;
	}
	
	if (server_fd >= 0) {
		close(server_fd);
		server_fd = -1;
	}
}

static gboolean client_read_callback(GIOChannel *source, GIOCondition condition, gpointer data)
{
	WebClient *client = (WebClient*)data;
	gchar buffer[1024];
	gsize bytes_read;
	GError *error = NULL;
	GIOStatus status;
	
	if (condition & (G_IO_HUP | G_IO_ERR)) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "fe-web: Client disconnected");
		cleanup_client(client);
		return FALSE;
	}
	
	status = g_io_channel_read_chars(source, buffer, sizeof(buffer)-1, &bytes_read, &error);
	
	if (status == G_IO_STATUS_ERROR) {
		if (error) {
			printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
				"fe-web: Client read error: %s", error->message);
			g_error_free(error);
		}
		cleanup_client(client);
		return FALSE;
	}
	
	if (status == G_IO_STATUS_EOF) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "fe-web: Client EOF");
		cleanup_client(client);
		return FALSE;
	}
	
	if (bytes_read > 0) {
		const char *response;
		buffer[bytes_read] = '\0';
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Received %d bytes from client: %s", (int)bytes_read, buffer);
		
		/* Simple HTTP response for now */
		response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nfe-web module running\n";
		g_io_channel_write_chars(source, response, -1, NULL, NULL);
		g_io_channel_flush(source, NULL);
	}
	
	return TRUE;
}

static gboolean server_accept_callback(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd;
	
	if (condition & (G_IO_HUP | G_IO_ERR)) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "fe-web: Server socket error");
		cleanup_server();
		return FALSE;
	}
	
	client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd < 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Failed to accept client: %s", strerror(errno));
		return TRUE;
	}
	
	/* Set non-blocking */
	{
		int flags = fcntl(client_fd, F_GETFL, 0);
		fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
	}
	
	/* Create client structure */
	{
		WebClient *client = g_new0(WebClient, 1);
		client->fd = client_fd;
		client->channel = g_io_channel_unix_new(client_fd);
		g_io_channel_set_encoding(client->channel, NULL, NULL);
		g_io_channel_set_buffered(client->channel, FALSE);
		
		client->tag = g_io_add_watch(client->channel, 
			G_IO_IN | G_IO_HUP | G_IO_ERR,
			client_read_callback, client);
		
		clients = g_slist_prepend(clients, client);
		
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Client connected from %s", inet_ntoa(client_addr.sin_addr));
	}
	
	return TRUE;
}

static gboolean start_server(int port)
{
	struct sockaddr_in server_addr;
	int opt = 1;
	
	/* Create socket */
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Failed to create socket: %s", strerror(errno));
		return FALSE;
	}
	
	/* Set socket options */
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	
	/* Set non-blocking */
	{
		int flags = fcntl(server_fd, F_GETFL, 0);
		fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
	}
	
	/* Bind */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);
	
	if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Failed to bind to port %d: %s", port, strerror(errno));
		close(server_fd);
		server_fd = -1;
		return FALSE;
	}
	
	/* Listen */
	if (listen(server_fd, BACKLOG) < 0) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Failed to listen: %s", strerror(errno));
		close(server_fd);
		server_fd = -1;
		return FALSE;
	}
	
	/* Create GIO channel for server socket */
	server_channel = g_io_channel_unix_new(server_fd);
	g_io_channel_set_encoding(server_channel, NULL, NULL);
	g_io_channel_set_buffered(server_channel, FALSE);
	
	server_tag = g_io_add_watch(server_channel, 
		G_IO_IN | G_IO_HUP | G_IO_ERR,
		server_accept_callback, NULL);
	
	return TRUE;
}

static void test_signal_handler(void)
{
	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "fe-web: Signal received!");
}

static void signal_message_public(SERVER_REC *server, const char *msg, const char *nick, 
				  const char *address, const char *target)
{
	if (clients) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Public message from %s in %s: %s", nick, target, msg);
		/* Here we would normally broadcast to WebSocket clients */
	}
}

static void signal_message_private(SERVER_REC *server, const char *msg, const char *nick, 
				   const char *address)
{
	if (clients) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Private message from %s: %s", nick, msg);
		/* Here we would normally broadcast to WebSocket clients */
	}
}

static void signal_channel_joined(CHANNEL_REC *channel)
{
	if (clients) {
		printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
			"fe-web: Joined channel %s", channel->name);
		/* Here we would normally broadcast to WebSocket clients */
	}
}

void fe_web_init(void)
{
	int port;
	gboolean enabled;
	
	/* Register module first */
	module_register("fe_web", "fe_web");
	
	/* Register settings */
	settings_add_bool("lookandfeel", "web_frontend_enabled", TRUE);
	settings_add_int("lookandfeel", "web_frontend_port", DEFAULT_PORT);
	
	/* Connect test signal */
	signal_add("window changed", (SIGNAL_FUNC) test_signal_handler);
	
	/* Connect IRC signals */
	signal_add("message public", (SIGNAL_FUNC) signal_message_public);
	signal_add("message private", (SIGNAL_FUNC) signal_message_private);
	signal_add("channel joined", (SIGNAL_FUNC) signal_channel_joined);

	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
	         "fe-web: Web frontend module loaded successfully!");
	
	/* Start server if enabled */
	enabled = settings_get_bool("web_frontend_enabled");
	if (enabled) {
		port = settings_get_int("web_frontend_port");
		if (start_server(port)) {
			printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
				"fe-web: HTTP server started on port %d", port);
		} else {
			printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
				"fe-web: Failed to start HTTP server on port %d", port);
		}
	}
}

void fe_web_deinit(void)
{
	/* Stop server and cleanup */
	cleanup_server();
	
	/* Remove signal handlers */
	signal_remove("window changed", (SIGNAL_FUNC) test_signal_handler);
	signal_remove("message public", (SIGNAL_FUNC) signal_message_public);
	signal_remove("message private", (SIGNAL_FUNC) signal_message_private);
	signal_remove("channel joined", (SIGNAL_FUNC) signal_channel_joined);
	
	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP,
	         "fe-web: Module unloaded.");
}

void fe_web_abicheck(int *version)
{
	*version = IRSSI_ABI_VERSION;
}
