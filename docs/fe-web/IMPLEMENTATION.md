# fe-web Implementation Plan

**Step-by-Step Implementation Guide**

Version: 2.0
Status: Implementation Plan
Date: 2025-01-11

---

## Table of Contents

1. [Overview](#overview)
2. [Phase 1: Signal Refactoring](#phase-1-signal-refactoring)
3. [Phase 2: Client State Tracking](#phase-2-client-state-tracking)
4. [Phase 3: State Dump](#phase-3-state-dump)
5. [Phase 4: JSON Protocol](#phase-4-json-protocol)
6. [Phase 5: Frontend](#phase-5-frontend)
7. [Testing Strategy](#testing-strategy)
8. [Milestones](#milestones)

---

## Overview

### Timeline Estimate

| Phase | Duration | Description |
|-------|----------|-------------|
| Phase 1 | 2-3 days | Signal refactoring (core) |
| Phase 2 | 1-2 days | Client state tracking |
| Phase 3 | 2 days | Initial state dump |
| Phase 4 | 1 day | JSON protocol finalization |
| Phase 5 | 3-4 days | Frontend implementation |
| **Total** | **10-14 days** | Complete working implementation |

### Prerequisites

- ✅ Clean branch from upstream/master (`fe-web-dev`)
- ✅ Architecture documented
- ✅ Protocol specified
- ✅ irssiproxy code reviewed

---

## Phase 1: Signal Refactoring

**Goal**: Replace `print text` signal with proper IRC signals (irssiproxy pattern)

**Duration**: 2-3 days

### Step 1.1: Create Module Skeleton

**Files to create:**

```bash
src/fe-web/
├── fe-web.h              # Header with structures
├── fe-web.c              # Module init/deinit
├── fe-web-server.c       # TCP WebSocket server
├── fe-web-client.c       # Client handling
├── fe-web-signals.c      # Signal handlers (main logic)
├── fe-web-utils.c        # JSON utilities
└── meson.build           # Build configuration
```

**Tasks:**

1. Create `fe-web.h` with basic structures:
```c
typedef struct _WEB_CLIENT_REC {
    int fd;
    char *id;
    gboolean authenticated;
    gboolean handshake_done;
    IRC_SERVER_REC *server;
    NET_SENDBUF_REC *handle;
} WEB_CLIENT_REC;

typedef struct _WEB_MESSAGE_REC {
    char *id;
    int type;
    char *server_tag;
    char *target;
    char *nick;
    char *text;
    int level;
    time_t timestamp;
    gboolean is_own;
    GHashTable *extra_data;
} WEB_MESSAGE_REC;
```

2. Create `fe-web.c` with module registration:
```c
void fe_web_init(void) {
    settings_add_bool("lookandfeel", "fe_web_enabled", FALSE);
    settings_add_int("lookandfeel", "fe_web_port", 9001);
    settings_add_str("lookandfeel", "fe_web_bind", "127.0.0.1");
    settings_add_str("lookandfeel", "fe_web_password", "");

    fe_web_server_init();
    fe_web_signals_init();

    module_register("web", "fe");
}
```

3. Create `fe-web-server.c` with TCP server (copy pattern from irssiproxy):
   - `fe_web_server_start(int port)`
   - `fe_web_server_stop()`
   - Accept connections
   - Create `WEB_CLIENT_REC` for each connection

### Step 1.2: Implement Signal Handlers

**File**: `src/fe-web/fe-web-signals.c`

**Pattern from irssiproxy** (`src/irc/proxy/listen.c`):

```c
static GString *next_line = NULL;

static void sig_incoming(IRC_SERVER_REC *server, const char *line) {
    if (!IS_IRC_SERVER(server)) return;
    if (g_slist_length(web_clients) == 0) return;

    // Store the raw IRC line
    g_string_printf(next_line, "%s", line);
}

static void sig_server_event(IRC_SERVER_REC *server,
                              const char *line,
                              const char *nick,
                              const char *address) {
    WEB_MESSAGE_REC *msg;
    char *event, *args;

    if (!IS_IRC_SERVER(server)) return;

    // Parse event type (PRIVMSG, JOIN, PART, etc.)
    event = g_strconcat("event ", line, NULL);
    args = strchr(event+6, ' ');
    if (args != NULL) *args++ = '\0';

    // Create message based on event type
    if (g_strcmp0(event, "event privmsg") == 0) {
        msg = fe_web_parse_privmsg(server, args, nick, address);
        fe_web_send_to_server_clients(server, msg);
        fe_web_message_free(msg);
    }
    // ... handle other events

    g_free(event);
}

static void sig_message_own_public(IRC_SERVER_REC *server,
                                    const char *msg,
                                    const char *target) {
    WEB_MESSAGE_REC *web_msg;

    if (!IS_IRC_SERVER(server)) return;

    web_msg = fe_web_message_new(WEB_MSG_CHAT);
    web_msg->server_tag = g_strdup(server->tag);
    web_msg->target = g_strdup(target);
    web_msg->nick = g_strdup(server->nick);
    web_msg->text = g_strdup(msg);
    web_msg->level = MSGLEVEL_PUBLIC;
    web_msg->timestamp = time(NULL);
    web_msg->is_own = TRUE;  // ← KEY!

    fe_web_send_to_server_clients(server, web_msg);
    fe_web_message_free(web_msg);
}

void fe_web_signals_init(void) {
    next_line = g_string_new(NULL);

    signal_add("server incoming", (SIGNAL_FUNC) sig_incoming);
    signal_add("server event", (SIGNAL_FUNC) sig_server_event);
    signal_add("message own_public", (SIGNAL_FUNC) sig_message_own_public);
    signal_add("message own_private", (SIGNAL_FUNC) sig_message_own_private);
    signal_add("message irc own_action", (SIGNAL_FUNC) sig_message_own_action);
    signal_add("event connected", (SIGNAL_FUNC) event_connected);
    signal_add("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
}
```

**Key signals to implement:**

- ✅ `server incoming` - Capture raw IRC line
- ✅ `server event` - Parse and route IRC events
- ✅ `message own_public` - **YOUR OWN** public messages
- ✅ `message own_private` - **YOUR OWN** private messages
- ✅ `message irc own_action` - **YOUR OWN** actions
- ✅ `event connected` - Server connected
- ✅ `server disconnected` - Server disconnected

### Step 1.3: Targeted Broadcast

**File**: `src/fe-web/fe-web-utils.c`

```c
void fe_web_send_to_server_clients(IRC_SERVER_REC *server,
                                    WEB_MESSAGE_REC *msg) {
    GSList *tmp;
    char *json;

    json = fe_web_message_to_json(msg);

    for (tmp = web_clients; tmp != NULL; tmp = tmp->next) {
        WEB_CLIENT_REC *client = tmp->data;

        // Send ONLY to clients synced with this server
        if (client->authenticated &&
            (client->server == server || client->wants_all_servers)) {
            fe_web_send_json(client, json);
        }
    }

    g_free(json);
}
```

### Phase 1 Deliverables

- [ ] Module loads successfully
- [ ] WebSocket server starts on port 9001
- [ ] Accepts connections
- [ ] Signals properly connected
- [ ] Own messages captured
- [ ] No duplication

### Testing Phase 1

```bash
# Build and install
meson setup Build --prefix=/opt/erssi -Dwith-perl=yes -Dwith-otr=yes -Ddisable-utf8proc=no
ninja -C Build

# Test (note: build only, user will test!)
# User will run: /opt/erssi/bin/irssi
# User will run: /LOAD fe_web
# User will run: /SET fe_web_enabled ON

# In another terminal: connect with websocat
websocat ws://localhost:9001

# In irssi: connect to IRC
/CONNECT irc.libera.chat
/JOIN #test

# Type a message - should see in websocat
```

---

## Phase 2: Client State Tracking

**Goal**: Implement per-client server assignment and state tracking

**Duration**: 1-2 days

### Step 2.1: Extend WEB_CLIENT_REC

```c
typedef struct _WEB_CLIENT_REC {
    int fd;
    char *id;
    time_t connected_at;
    gboolean authenticated;
    gboolean handshake_done;

    // Server assignment
    IRC_SERVER_REC *server;      // NULL = not synced
    gboolean wants_all_servers;  // TRUE = sync all

    // Channel sync (optional - for future)
    GSList *synced_channels;     // List of channel names

    NET_SENDBUF_REC *handle;
    GString *output_buffer;
} WEB_CLIENT_REC;
```

### Step 2.2: Client Commands

**File**: `src/fe-web/fe-web-client.c`

```c
void fe_web_client_handle_command(WEB_CLIENT_REC *client, const char *json) {
    GHashTable *parsed = fe_web_parse_json(json);
    const char *type = g_hash_table_lookup(parsed, "type");

    if (g_strcmp0(type, "sync_server") == 0) {
        const char *server_tag = g_hash_table_lookup(parsed, "server");
        fe_web_client_sync_server(client, server_tag);
    }
    else if (g_strcmp0(type, "command") == 0) {
        const char *cmd = g_hash_table_lookup(parsed, "command");
        fe_web_client_execute_command(client, cmd);
    }

    g_hash_table_destroy(parsed);
}

void fe_web_client_sync_server(WEB_CLIENT_REC *client, const char *server_tag) {
    if (g_strcmp0(server_tag, "*") == 0) {
        // Sync all servers
        client->wants_all_servers = TRUE;
        client->server = NULL;

        // TODO: Dump state for all servers
    } else {
        // Sync specific server
        IRC_SERVER_REC *server = IRC_SERVER(server_find_tag(server_tag));
        if (server) {
            client->server = server;
            client->wants_all_servers = FALSE;

            // Dump state for this server
            fe_web_dump_state(client);
        } else {
            fe_web_send_error(client, "ERR_SERVER_NOT_FOUND",
                             "Server not found");
        }
    }
}
```

### Phase 2 Deliverables

- [ ] Client can select server with `sync_server` command
- [ ] Client only receives messages from assigned server
- [ ] Multiple clients can connect to different servers
- [ ] Client can sync all servers with `"*"`

---

## Phase 3: State Dump

**Goal**: Send complete state snapshot when client connects (irssiproxy `dump_data` pattern)

**Duration**: 2 days

### Step 3.1: Dump Server State

**File**: `src/fe-web/fe-web-dump.c` (new file!)

**Pattern from irssiproxy** (`src/irc/proxy/dump.c`):

```c
void fe_web_dump_state(WEB_CLIENT_REC *client) {
    if (client->server == NULL) {
        // No server selected - send server list
        fe_web_send_server_list(client);
        return;
    }

    // 1. Server info
    fe_web_send_server_info(client);

    // 2. User mode
    if (client->server->usermode)
        fe_web_send_user_mode(client);

    // 3. Away status
    if (client->server->usermode_away)
        fe_web_send_away_status(client);

    // 4. All channels
    GSList *tmp;
    for (tmp = client->server->channels; tmp != NULL; tmp = tmp->next) {
        IRC_CHANNEL_REC *channel = tmp->data;
        fe_web_dump_channel(client, channel);
    }

    // 5. End of dump marker
    fe_web_send_dump_complete(client);
}

void fe_web_dump_channel(WEB_CLIENT_REC *client, IRC_CHANNEL_REC *channel) {
    WEB_MESSAGE_REC *msg;

    // Channel joined
    msg = fe_web_message_new(WEB_MSG_CHANNEL_JOIN);
    msg->server_tag = g_strdup(channel->server->tag);
    msg->target = g_strdup(channel->name);
    msg->nick = g_strdup(client->server->nick);
    fe_web_send_message(client, msg);
    fe_web_message_free(msg);

    // Topic
    if (channel->topic) {
        msg = fe_web_message_new(WEB_MSG_TOPIC);
        msg->server_tag = g_strdup(channel->server->tag);
        msg->target = g_strdup(channel->name);
        msg->text = g_strdup(channel->topic);
        // Add topic_by and topic_time in extra_data
        fe_web_send_message(client, msg);
        fe_web_message_free(msg);
    }

    // Complete nicklist
    GSList *nicks = nicklist_getnicks(CHANNEL(channel));
    for (GSList *tmp = nicks; tmp != NULL; tmp = tmp->next) {
        NICK_REC *nick = tmp->data;

        msg = fe_web_message_new(WEB_MSG_NICK_LIST);
        msg->server_tag = g_strdup(channel->server->tag);
        msg->target = g_strdup(channel->name);
        msg->nick = g_strdup(nick->nick);
        msg->text = g_strdup("join");

        // Add nick modes
        if (nick->op)
            g_hash_table_insert(msg->extra_data, g_strdup("op"), g_strdup("1"));
        if (nick->voice)
            g_hash_table_insert(msg->extra_data, g_strdup("voice"), g_strdup("1"));

        fe_web_send_message(client, msg);
        fe_web_message_free(msg);
    }
    g_slist_free(nicks);
}
```

### Phase 3 Deliverables

- [ ] Client receives complete state on `sync_server`
- [ ] All channels with topics
- [ ] Complete nicklists for all channels
- [ ] User mode and away status
- [ ] Server info

---

## Phase 4: JSON Protocol

**Goal**: Implement complete JSON protocol from PROTOCOL.md

**Duration**: 1 day

### Step 4.1: JSON Serialization

**File**: `src/fe-web/fe-web-utils.c`

```c
char *fe_web_message_to_json(WEB_MESSAGE_REC *msg) {
    GString *json = g_string_new("{");

    // id
    if (msg->id)
        g_string_append_printf(json, "\"id\":\"%s\",", msg->id);

    // type
    g_string_append_printf(json, "\"type\":\"%s\",",
                          fe_web_type_to_string(msg->type));

    // server
    if (msg->server_tag)
        g_string_append_printf(json, "\"server\":\"%s\",", msg->server_tag);

    // channel
    if (msg->target)
        g_string_append_printf(json, "\"channel\":\"%s\",",
                              fe_web_escape_json(msg->target));

    // nick
    if (msg->nick)
        g_string_append_printf(json, "\"nick\":\"%s\",",
                              fe_web_escape_json(msg->nick));

    // text
    if (msg->text)
        g_string_append_printf(json, "\"text\":\"%s\",",
                              fe_web_escape_json(msg->text));

    // timestamp
    g_string_append_printf(json, "\"timestamp\":%ld,", msg->timestamp);

    // level
    g_string_append_printf(json, "\"level\":%d,", msg->level);

    // is_own
    g_string_append_printf(json, "\"is_own\":%s",
                          msg->is_own ? "true" : "false");

    g_string_append_c(json, '}');

    return g_string_free(json, FALSE);
}
```

### Step 4.2: Message IDs

Generate unique IDs:

```c
char *fe_web_generate_message_id(void) {
    static int counter = 0;
    time_t now = time(NULL);

    // Format: timestamp-counter
    return g_strdup_printf("%ld-%04d", now, counter++);
}
```

### Phase 4 Deliverables

- [ ] All message types serialized to JSON
- [ ] Unique message IDs
- [ ] Proper JSON escaping
- [ ] Protocol matches PROTOCOL.md spec

---

## Phase 5: Frontend

**Goal**: Simple web UI to test and use the protocol

**Duration**: 3-4 days

### Step 5.1: Basic Next.js App

```bash
cd web_proto
npx create-next-app@latest irssi-web --typescript --tailwind --app
cd irssi-web
```

### Step 5.2: WebSocket Client

```typescript
// lib/websocket.ts
export class IrssiWebSocket {
    private ws: WebSocket | null = null;
    private messageHandlers: Map<string, Function> = new Map();

    connect(url: string) {
        this.ws = new WebSocket(url);

        this.ws.onopen = () => {
            console.log('Connected to irssi');
        };

        this.ws.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            const handler = this.messageHandlers.get(msg.type);
            if (handler) handler(msg);
        };

        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
    }

    send(message: object) {
        if (this.ws?.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
        }
    }

    on(type: string, handler: Function) {
        this.messageHandlers.set(type, handler);
    }

    syncServer(server: string) {
        this.send({ type: 'sync_server', server });
    }

    sendCommand(server: string, command: string) {
        this.send({ type: 'command', server, command });
    }
}
```

### Step 5.3: Basic UI Components

- Server selector
- Channel list
- Message display
- Input box
- Nicklist (sidebar)

### Phase 5 Deliverables

- [ ] Working web client
- [ ] Can connect to irssi
- [ ] Can select server
- [ ] Can view channels and messages
- [ ] Can send messages
- [ ] Can see own messages

---

## Testing Strategy

### Unit Testing

For each phase, test:

- [ ] Module loads without errors
- [ ] Signals are properly connected
- [ ] Client connections accepted
- [ ] JSON serialization works
- [ ] State dump complete

### Integration Testing

```bash
# Terminal 1: irssi (user will run this)
/opt/erssi/bin/irssi
/LOAD fe_web
/SET fe_web_enabled ON
/CONNECT irc.libera.chat
/JOIN #test

# Terminal 2: websocat (raw WebSocket client)
websocat ws://localhost:9001

# Send:
{"type":"sync_server","server":"libera"}

# Should receive state dump!

# Terminal 3: Next.js frontend
cd web_proto/irssi-web && npm run dev
# Open http://localhost:3000
```

### Manual Testing Checklist

- [ ] Can connect to irssi via WebSocket
- [ ] Can sync to specific server
- [ ] Receives complete state dump
- [ ] Sees incoming IRC messages
- [ ] Can send messages
- [ ] Sees own messages
- [ ] Multiple clients work simultaneously
- [ ] No message duplication
- [ ] Disconnect/reconnect works

---

## Milestones

### Milestone 1: Basic Communication (Day 3)

- WebSocket server running
- Can accept connections
- Basic signal handling works
- Can send JSON messages to client

### Milestone 2: State Sync (Day 7)

- Client can select server
- Receives complete state dump
- Targeted broadcast working
- Own messages visible

### Milestone 3: Complete Protocol (Day 10)

- All message types implemented
- JSON protocol complete
- Error handling
- Multiple clients tested

### Milestone 4: Working Frontend (Day 14)

- Web UI functional
- Can chat via web interface
- All features working
- Ready for production use

---

## Next Steps

After complete implementation:

1. **Documentation** - User guide, API docs
2. **Performance Testing** - Many clients, high message rate
3. **Security** - TLS, authentication, rate limiting
4. **Advanced Features**:
   - History/scrollback
   - File transfer
   - Notifications
   - Mobile apps (React Native)

---

## Git Workflow

### Branching Strategy

```
upstream/master (stock irssi)
    │
    └─ fe-web-dev (our development branch)
         │
         ├─ fe-web-phase1 (Phase 1 work)
         ├─ fe-web-phase2 (Phase 2 work)
         └─ ...
```

### Commits

- Use descriptive commit messages
- Reference phase/step (e.g., "Phase 1.2: Add signal handlers")
- Commit after each working step

### Example Commit Messages

```
Phase 1.1: Create module skeleton

- Add fe-web.h with basic structures
- Add fe-web.c with module registration
- Add fe-web-server.c with TCP server
- Add meson.build
```

---

**Status**: ✅ Implementation Plan Complete
**Next**: Start Phase 1 - Signal Refactoring
**Author**: kofany + Claude
**Date**: 2025-01-11
