# fe-web - Architecture Documentation

**WebSocket-based Web Frontend for irssi**

Version: 2.0 (Clean Rewrite)
Status: Design Phase
Date: 2025-01-11

---

## Table of Contents

1. [Overview](#overview)
2. [Design Principles](#design-principles)
3. [Architecture Pattern: irssiproxy-Inspired](#architecture-pattern)
4. [Module Structure](#module-structure)
5. [Signal Flow](#signal-flow)
6. [Client Management](#client-management)
7. [State Synchronization](#state-synchronization)
8. [Protocol Design](#protocol-design)
9. [Comparison with Old Implementation](#comparison-with-old-implementation)

---

## Overview

**fe-web** is a WebSocket-based relay module for irssi that enables web-based IRC clients to connect to a running irssi instance. Unlike the previous implementation which suffered from message duplication and architectural issues, this rewrite follows the proven patterns from **irssiproxy**.

### Key Features

- ✅ **WebSocket server** with JSON protocol
- ✅ **Per-client state tracking** (no blind broadcast)
- ✅ **Initial state dump** on connection
- ✅ **Proper signal handling** (no `print text` hack)
- ✅ **Own message support** (using `message_own_*` signals)
- ✅ **Multi-server support** with per-client server assignment
- ✅ **REST API** for state queries (optional)

---

## Design Principles

### 1. Learn from irssiproxy

The irssiproxy module (`src/irc/proxy/`) has been relaying IRC for years successfully. Key lessons:

- **Use `server incoming` + `server event`** signals (not `print text`)
- **Track client state** (which server/channels each client is synced to)
- **Dump initial state** when client connects
- **Per-server broadcast** (not global broadcast)
- **Handle own messages** via `message_own_public/private/action`

### 2. JSON over Binary

Unlike WeeChat relay protocol (complex binary format with hdata), we use simple JSON:

- ✅ Easy to implement
- ✅ Easy to debug
- ✅ Native web support
- ✅ Human-readable
- ✅ No complex binary parsing

### 3. Direct irssi Structure Mapping

No translation layer needed:

```
IRC_SERVER_REC    → JSON server object
IRC_CHANNEL_REC   → JSON channel object
NICK_REC          → JSON nick object
WINDOW_REC        → JSON window object
```

### 4. Stateful Clients

Each client tracks:

- Which server(s) they're synced to
- Which channels they're interested in
- Authentication state
- Connection metadata

---

## Architecture Pattern

### Inspired by irssiproxy

```
┌─────────────────────────────────────────────────────────────┐
│                         irssi Core                           │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐              │
│  │ Server 1 │    │ Server 2 │    │ Server N │              │
│  │ (libera) │    │ (oftc)   │    │  (...)   │              │
│  └────┬─────┘    └────┬─────┘    └────┬─────┘              │
│       │               │               │                      │
│       └───────────────┴───────────────┘                      │
│                       │                                      │
│                  IRC Signals                                 │
│                       │                                      │
│       ┌───────────────┴───────────────┐                     │
│       │         fe-web Module         │                     │
│       │  ┌─────────────────────────┐  │                     │
│       │  │   Signal Handlers       │  │                     │
│       │  │  - server incoming      │  │                     │
│       │  │  - server event         │  │                     │
│       │  │  - message_own_*        │  │                     │
│       │  │  - server connected     │  │                     │
│       │  │  - server disconnected  │  │                     │
│       │  └──────────┬──────────────┘  │                     │
│       │             │                  │                     │
│       │  ┌──────────▼──────────────┐  │                     │
│       │  │   Client Manager        │  │                     │
│       │  │  - Track connections    │  │                     │
│       │  │  - Per-client state     │  │                     │
│       │  │  - Server assignment    │  │                     │
│       │  └──────────┬──────────────┘  │                     │
│       │             │                  │                     │
│       │  ┌──────────▼──────────────┐  │                     │
│       │  │  WebSocket Server       │  │                     │
│       │  │  (port 9001)            │  │                     │
│       │  └──────────┬──────────────┘  │                     │
│       └─────────────┼──────────────────┘                     │
└─────────────────────┼────────────────────────────────────────┘
                      │
           WebSocket (JSON messages)
                      │
        ┌─────────────┴─────────────┐
        │                           │
   ┌────▼────┐                 ┌────▼────┐
   │ Client 1│                 │ Client 2│
   │ (Web UI)│                 │ (Mobile)│
   │         │                 │         │
   │ server: │                 │ server: │
   │ libera  │                 │ oftc    │
   └─────────┘                 └─────────┘
```

### Key Differences from Old Implementation

| Aspect | Old (Broken) | New (irssiproxy-inspired) |
|--------|--------------|---------------------------|
| **Main signal** | `print text` ❌ | `server incoming` + `server event` ✅ |
| **Own messages** | Missing ❌ | `message_own_*` signals ✅ |
| **Broadcast** | Blind to all clients ❌ | Per-server targeted ✅ |
| **State sync** | None ❌ | Initial dump on connect ✅ |
| **Client tracking** | Minimal ❌ | Full state tracking ✅ |
| **Duplication** | Yes ❌ | No ✅ |

---

## Module Structure

### File Organization

```
src/fe-web/
├── fe-web.c              # Module initialization, settings
├── fe-web.h              # Structure definitions, API
├── fe-web-server.c       # WebSocket TCP server
├── fe-web-client.c       # Client connection handling
├── fe-web-signals.c      # irssi signal handlers (core logic)
├── fe-web-dump.c         # Initial state dump (like irssiproxy)
├── fe-web-utils.c        # JSON serialization, helpers
├── fe-web-api.c          # REST API (optional)
└── meson.build           # Build configuration
```

### Core Structures

```c
/* Client connection record */
typedef struct _WEB_CLIENT_REC {
    int fd;
    char *id;                    /* UUID */
    time_t connected_at;
    gboolean authenticated;

    /* WebSocket state */
    gboolean handshake_done;
    char *websocket_key;

    /* irssi context - KEY DIFFERENCE FROM OLD IMPL */
    IRC_SERVER_REC *server;      /* Assigned server (or NULL for all) */
    GSList *synced_channels;     /* List of channel names */
    gboolean wants_all_servers;  /* Sync all servers? */

    /* Output buffer */
    NET_SENDBUF_REC *handle;
    GString *output_buffer;

} WEB_CLIENT_REC;

/* WebSocket message types */
typedef enum {
    WEB_MSG_CHAT = 1,
    WEB_MSG_COMMAND,
    WEB_MSG_SERVER_STATUS,
    WEB_MSG_CHANNEL_JOIN,
    WEB_MSG_CHANNEL_PART,
    WEB_MSG_NICK_LIST,
    WEB_MSG_TOPIC,
    WEB_MSG_STATE_DUMP,
    WEB_MSG_ERROR
} WEB_MESSAGE_TYPE;

/* JSON message structure */
typedef struct _WEB_MESSAGE_REC {
    char *id;                    /* Unique message ID (UUID) */
    WEB_MESSAGE_TYPE type;
    char *server_tag;
    char *target;                /* channel or nick */
    char *nick;
    char *text;
    int level;                   /* MSGLEVEL_* */
    time_t timestamp;
    gboolean is_own;             /* Is this our own message? */
    GHashTable *extra_data;      /* Additional metadata */
} WEB_MESSAGE_REC;
```

---

## Signal Flow

### Signal Handlers (from irssiproxy pattern)

```c
// fe-web-signals.c

void fe_web_signals_init(void) {
    /* Main IRC traffic - like irssiproxy */
    signal_add("server incoming", (SIGNAL_FUNC) sig_incoming);
    signal_add("server event", (SIGNAL_FUNC) sig_server_event);

    /* Own messages - CRITICAL for seeing your own messages */
    signal_add("message own_public", (SIGNAL_FUNC) sig_message_own_public);
    signal_add("message own_private", (SIGNAL_FUNC) sig_message_own_private);
    signal_add("message irc own_action", (SIGNAL_FUNC) sig_message_own_action);

    /* Server lifecycle */
    signal_add("event connected", (SIGNAL_FUNC) event_connected);
    signal_add("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);

    /* Channel events */
    signal_add("channel joined", (SIGNAL_FUNC) sig_channel_joined);
    signal_add("channel destroyed", (SIGNAL_FUNC) sig_channel_destroyed);

    /* Nick changes */
    signal_add("event nick", (SIGNAL_FUNC) event_nick);
}
```

### Example: Message Flow for Public Message

```
IRC Server: ":user!~u@host PRIVMSG #test :Hello!"
        │
        ▼
    [server incoming]  ← Captures raw IRC line
        │
        ▼
    [server event]     ← Parsed event with nick/address
        │
        ▼
    sig_server_event() in fe-web-signals.c
        │
        ├─ Parse event type (PRIVMSG)
        ├─ Create WEB_MESSAGE_REC
        ├─ Set: server_tag, target, nick, text
        └─ Call: fe_web_send_to_server_clients(server, msg)
                │
                ▼
            For each client in web_clients:
                if (client->server == server || client->wants_all_servers)
                    → Send JSON to client
```

### Example: Own Message Flow

```
User types: "/msg #test Hello from web!"
        │
        ▼
    Frontend sends: {"type":"command", "command":"/msg #test Hello"}
        │
        ▼
    fe_web_client_handle_command()
        │
        ├─ Parse command
        └─ Call: irc_send_cmd(server, "/msg #test Hello")
                │
                ▼
            irssi sends to IRC server
                │
                ▼
            [message own_public] signal emitted  ← KEY!
                │
                ▼
            sig_message_own_public() in fe-web-signals.c
                │
                ├─ Create WEB_MESSAGE_REC
                ├─ Set: is_own = TRUE
                └─ Call: fe_web_send_to_server_clients(server, msg)
                        │
                        ▼
                    All clients (including sender) see the message!
```

---

## Client Management

### Connection Lifecycle

```
1. TCP accept
   ↓
2. WebSocket handshake
   ↓
3. Authentication (optional)
   ↓
4. Server selection/sync request
   ↓
5. Initial state dump (like irssiproxy dump_data)
   ↓
6. Real-time message streaming
```

### Per-Client Server Assignment

```c
// Client commands to sync with servers:

// Sync with specific server
{
    "type": "sync_server",
    "server": "libera"
}

// Sync with all servers
{
    "type": "sync_server",
    "server": "*"
}

// Implementation:
void fe_web_client_sync_server(WEB_CLIENT_REC *client, const char *server_tag) {
    if (g_strcmp0(server_tag, "*") == 0) {
        client->wants_all_servers = TRUE;
        client->server = NULL;
        // Dump state for ALL servers
    } else {
        IRC_SERVER_REC *server = IRC_SERVER(server_find_tag(server_tag));
        if (server) {
            client->server = server;
            client->wants_all_servers = FALSE;
            // Dump state for THIS server
            fe_web_dump_state(client);
        }
    }
}
```

### Targeted Broadcast (Not Blind!)

```c
// GOOD: Only send to clients synced with this server
void fe_web_send_to_server_clients(IRC_SERVER_REC *server,
                                    WEB_MESSAGE_REC *msg) {
    GSList *tmp;
    char *json = fe_web_message_to_json(msg);

    for (tmp = web_clients; tmp != NULL; tmp = tmp->next) {
        WEB_CLIENT_REC *client = tmp->data;

        // Send ONLY if client is synced to this server
        if (client->authenticated &&
            (client->server == server || client->wants_all_servers)) {
            fe_web_send_json(client, json);
        }
    }

    g_free(json);
}
```

---

## State Synchronization

### Initial State Dump (from irssiproxy pattern)

When a client connects and selects a server, dump complete state:

```c
void fe_web_dump_state(WEB_CLIENT_REC *client) {
    /* 1. Server info */
    fe_web_send_server_info(client);

    /* 2. User mode */
    if (client->server->usermode)
        fe_web_send_user_mode(client);

    /* 3. Away status */
    if (client->server->usermode_away)
        fe_web_send_away_status(client);

    /* 4. ALL channels with nicklists and topics */
    for (GSList *tmp = client->server->channels; tmp; tmp = tmp->next) {
        IRC_CHANNEL_REC *channel = tmp->data;
        fe_web_dump_channel(client, channel);
    }
}

void fe_web_dump_channel(WEB_CLIENT_REC *client, IRC_CHANNEL_REC *channel) {
    /* Send JOIN */
    fe_web_send_channel_join(client, channel);

    /* Send TOPIC */
    if (channel->topic)
        fe_web_send_topic(client, channel);

    /* Send complete NICKLIST */
    GSList *nicks = nicklist_getnicks(CHANNEL(channel));
    for (GSList *tmp = nicks; tmp; tmp = tmp->next) {
        NICK_REC *nick = tmp->data;
        fe_web_send_nick(client, channel, nick, "join");
    }
    g_slist_free(nicks);
}
```

---

## Protocol Design

See [PROTOCOL.md](./PROTOCOL.md) for complete specification.

### Message Format

```json
{
    "id": "uuid-or-timestamp",
    "type": "message",
    "server": "libera",
    "channel": "#test",
    "nick": "user",
    "text": "Hello!",
    "timestamp": 1737825000,
    "level": 4,
    "is_own": false
}
```

### Client Commands

```json
{
    "type": "sync_server",
    "server": "libera"
}

{
    "type": "command",
    "server": "libera",
    "command": "/msg #test Hello"
}
```

---

## Comparison with Old Implementation

### Old Implementation Problems

1. **Wrong Signal**: Used `print text` (renderowanie UI)
2. **No Own Messages**: Missing `message_own_*` handlers
3. **Blind Broadcast**: Every client got ALL messages
4. **No State Dump**: Client had to figure out state
5. **Duplication**: Messages appeared 2x (status + channel window)
6. **Timestamp Hacks**: Deduplication via time comparison

### New Implementation Solutions

1. **✅ Correct Signals**: `server incoming` + `server event` + `message_own_*`
2. **✅ Own Messages**: Fully supported
3. **✅ Targeted Delivery**: Per-server client assignment
4. **✅ State Dump**: Complete initial sync (like irssiproxy)
5. **✅ No Duplication**: Proper signal handling
6. **✅ Message IDs**: UUID-based, no hacks

---

## Implementation Timeline

See [IMPLEMENTATION.md](./IMPLEMENTATION.md) for detailed phases.

**Estimated total: 10-14 days**

### Phase 1: Signal Refactoring (2-3 days)
### Phase 2: Client State Tracking (1-2 days)
### Phase 3: State Dump (2 days)
### Phase 4: JSON Protocol (1 day)
### Phase 5: Frontend (3-4 days)

---

## References

- `src/irc/proxy/` - irssiproxy implementation (inspiration)
- `docs/proxy.txt` - irssiproxy documentation
- `docs/signals.txt` - Complete signal reference
- WeeChat Relay Protocol (for comparison/lessons learned)

---

**Status**: ✅ Architecture Approved
**Next Step**: Implement Phase 1 (Signal Refactoring)
**Author**: kofany + Claude
**Date**: 2025-01-11
