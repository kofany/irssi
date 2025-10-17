# Irssi FE-Web Architecture

Internal architecture documentation for the FE-Web module.

## Overview

FE-Web is a frontend module for Irssi that exposes IRC functionality through a WebSocket API. It runs as a server within Irssi and allows web-based clients to connect and interact with IRC.

## Module Structure

```
fe-web/
├── src/
│   ├── fe-web.c              # Main module, initialization, commands
│   ├── fe-web.h              # Public API and type definitions
│   ├── module.h              # Module registration
│   ├── fe-web-server.c       # WebSocket server, connection handling
│   ├── fe-web-client.c       # Client connection management
│   ├── fe-web-signals.c      # IRC signal handlers
│   ├── fe-web-netserver.c    # Network/Server management
│   ├── fe-web-utils.c        # Message serialization utilities
│   ├── fe-web-json.c         # JSON parsing/building
│   ├── fe-web-websocket.c    # WebSocket protocol (RFC 6455)
│   ├── fe-web-ssl.c          # SSL/TLS certificate generation
│   └── fe-web-crypto.c       # AES-256-GCM encryption
├── docs/
│   ├── PROTOCOL.md           # WebSocket protocol specification
│   ├── ARCHITECTURE.md       # This file
│   └── CLIENT-SPEC.md        # Client implementation guide
└── meson.build               # Build configuration
```

## Component Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Irssi Core                            │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐       │
│  │Servers  │  │Channels │  │Queries  │  │Windows  │       │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘       │
│       │            │             │            │             │
│       └────────────┴─────────────┴────────────┘             │
│                         │                                    │
│                    Signal System                             │
└─────────────────────────┼────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    FE-Web Module                             │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              fe-web-signals.c                        │   │
│  │   IRC Event Handlers (message, join, part, etc.)    │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│                       ▼                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              fe-web-utils.c                          │   │
│  │   Message Creation & JSON Serialization              │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│                       ▼                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              fe-web-crypto.c                         │   │
│  │   AES-256-GCM Encryption/Decryption                  │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│                       ▼                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              fe-web-websocket.c                      │   │
│  │   WebSocket Frame Encoding/Decoding                  │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│                       ▼                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              fe-web-ssl.c                            │   │
│  │   SSL/TLS Transport Layer                            │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│                       ▼                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              fe-web-server.c                         │   │
│  │   TCP Server & Connection Management                 │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              fe-web-client.c                         │   │
│  │   Client State & Command Processing                  │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              fe-web-netserver.c                      │   │
│  │   Network/Server Configuration Management            │   │
│  └──────────────────────────────────────────────────────┘   │
└───────────────────────┼──────────────────────────────────────┘
                        │
                        ▼
                  ┌──────────┐
                  │ Web      │
                  │ Client   │
                  └──────────┘
```

## Data Flow

### Outbound (IRC → Client)

```
IRC Event
    ↓
Irssi Signal System
    ↓
fe-web-signals.c (sig_message_public, sig_channel_joined, etc.)
    ↓
fe_web_message_new() - Create WEB_MESSAGE_REC
    ↓
fe_web_message_to_json() - Serialize to JSON
    ↓
fe_web_crypto_encrypt() - AES-256-GCM encryption
    ↓
fe_web_websocket_create_frame() - Create WebSocket frame
    ↓
fe_web_ssl_send() - SSL/TLS write
    ↓
TCP Socket → Web Client
```

### Inbound (Client → IRC)

```
Web Client → TCP Socket
    ↓
fe_web_ssl_recv() - SSL/TLS read
    ↓
fe_web_websocket_parse_frame() - Parse WebSocket frame
    ↓
fe_web_crypto_decrypt() - AES-256-GCM decryption
    ↓
fe_web_json_get_string() - Parse JSON
    ↓
fe_web_client_handle_message() - Route by type
    ↓
fe_web_client_execute_command() - Execute IRC command
    ↓
signal_emit("send command") - Send to Irssi
    ↓
Irssi Command System
    ↓
IRC Server
```

## Key Components

### 1. Main Module (fe-web.c)

**Responsibilities:**
- Module registration and initialization
- Settings management
- Commands (`/FE_WEB STATUS`, etc.)
- Subsystem orchestration

**Key Functions:**
- `fe_web_init()` - Initialize module
- `fe_web_deinit()` - Cleanup module
- `cmd_fe_web_status()` - Status command handler

### 2. Server (fe-web-server.c)

**Responsibilities:**
- TCP server socket management
- WebSocket handshake processing
- Connection accept/close
- Input/output buffering
- Client authentication

**Key Functions:**
- `fe_web_server_init()` - Start listening server
- `sig_listen()` - Accept new connections
- `fe_web_handle_handshake()` - Process WebSocket upgrade
- `client_input()` - Handle incoming data

### 3. Client (fe-web-client.c)

**Responsibilities:**
- Per-client state management
- Message routing
- Command execution
- Server synchronization

**Key Data Structures:**
```c
typedef struct {
    int fd;
    char *id;
    IRC_SERVER_REC *server;      // Assigned server
    GSList *synced_channels;     // Channel list
    GHashTable *pending_requests; // Request tracking
    unsigned int authenticated:1;
    unsigned int handshake_done:1;
    unsigned int wants_all_servers:1;
} WEB_CLIENT_REC;
```

**Key Functions:**
- `fe_web_client_create()` - Initialize client
- `fe_web_client_handle_message()` - Process incoming message
- `fe_web_client_sync_server()` - Sync to IRC server
- `fe_web_client_execute_command()` - Execute IRC command

### 4. Signal Handlers (fe-web-signals.c)

**Responsibilities:**
- Listen to Irssi signals
- Convert IRC events to WebSocket messages
- Track WHOIS requests
- Manage activity indicators
- Handle nicklist updates

**Registered Signals:**
```c
// Messages
signal_add("message public", sig_message_public);
signal_add("message private", sig_message_private);
signal_add("message own_public", sig_message_own_public);
signal_add("message own_private", sig_message_own_private);

// Channel events
signal_add("message join", sig_channel_joined);
signal_add("message part", sig_channel_parted);
signal_add("message kick", sig_channel_kicked);
signal_add("message quit", sig_user_quit);
signal_add("message topic", sig_channel_topic);

// Nicklist
signal_add("nicklist new", sig_nicklist_new);
signal_add("nicklist remove", sig_nicklist_remove);
signal_add("nicklist changed", sig_nicklist_changed);

// Server status
signal_add("server connected", sig_server_connected);
signal_add("server disconnected", sig_server_disconnected);

// WHOIS events (multiple numeric handlers)
signal_add("event 311", event_whois);
signal_add("event 312", event_whois_server);
// ... etc

// Activity
signal_add("window hilight", sig_window_hilight);
signal_add("window activity", sig_window_activity);
```

### 5. Network/Server Management (fe-web-netserver.c)

**Responsibilities:**
- List networks and servers
- Add/modify networks
- Add/modify servers
- Remove networks/servers
- Validate configurations

**Key Functions:**
- `fe_web_handle_network_list()` - List all networks
- `fe_web_handle_network_add()` - Add/modify network
- `fe_web_handle_server_add()` - Add/modify server
- `fe_web_build_network_json()` - Serialize network config
- `fe_web_build_server_json()` - Serialize server config

### 6. Utilities (fe-web-utils.c)

**Responsibilities:**
- Message creation/destruction
- JSON serialization
- Message ID generation
- Message sending
- Type conversion

**Key Functions:**
- `fe_web_message_new()` - Create message
- `fe_web_message_free()` - Destroy message
- `fe_web_message_to_json()` - Serialize to JSON
- `fe_web_send_message()` - Send to client
- `fe_web_send_to_server_clients()` - Broadcast to server clients
- `fe_web_generate_message_id()` - Generate unique ID

### 7. JSON (fe-web-json.c)

**Responsibilities:**
- JSON parsing (minimal, manual)
- JSON building
- String escaping
- Type extraction

**Key Functions:**
- `fe_web_json_get_string()` - Extract string value
- `fe_web_json_get_int()` - Extract integer value
- `fe_web_json_has_key()` - Check key existence
- `fe_web_escape_json()` - Escape JSON string

### 8. WebSocket (fe-web-websocket.c)

**Responsibilities:**
- RFC 6455 WebSocket protocol
- Frame parsing/encoding
- Masking/unmasking
- Accept key computation

**Key Functions:**
- `fe_web_websocket_compute_accept()` - Compute Sec-WebSocket-Accept
- `fe_web_websocket_parse_frame()` - Parse WebSocket frame
- `fe_web_websocket_unmask()` - Unmask client data
- `fe_web_websocket_create_frame()` - Create server frame

### 9. SSL/TLS (fe-web-ssl.c)

**Responsibilities:**
- SSL context initialization
- Certificate generation (self-signed)
- TLS handshake
- Encrypted I/O

**Key Functions:**
- `fe_web_ssl_init()` - Initialize SSL subsystem
- `generate_rsa_key()` - Generate 2048-bit RSA key
- `generate_x509_cert()` - Generate self-signed certificate
- `fe_web_ssl_channel_new()` - Wrap socket in SSL
- `fe_web_ssl_read()`/`fe_web_ssl_write()` - SSL I/O

### 10. Crypto (fe-web-crypto.c)

**Responsibilities:**
- Application-level encryption (AES-256-GCM)
- Key derivation (PBKDF2)
- Encrypt/decrypt operations

**Key Functions:**
- `fe_web_crypto_init()` - Derive key from password
- `fe_web_crypto_encrypt()` - Encrypt message
- `fe_web_crypto_decrypt()` - Decrypt message
- `fe_web_crypto_derive_key()` - PBKDF2 key derivation

**Encryption Details:**
- Algorithm: AES-256-GCM
- Key Size: 256 bits
- IV Size: 12 bytes (random per message)
- Tag Size: 16 bytes (authentication)
- KDF: PBKDF2-HMAC-SHA256, 100,000 iterations

## Security Architecture

### Multi-Layer Security

```
┌──────────────────────────────────────────┐
│         Application Layer                │
│   AES-256-GCM Encryption                 │
│   (fe-web-crypto.c)                      │
└──────────────────┬───────────────────────┘
                   │
┌──────────────────▼───────────────────────┐
│         Transport Layer                  │
│   TLS 1.2+ (fe-web-ssl.c)                │
│   Self-signed certificate                │
└──────────────────┬───────────────────────┘
                   │
┌──────────────────▼───────────────────────┐
│         Network Layer                    │
│   TCP Socket                             │
└──────────────────────────────────────────┘
```

### Authentication Flow

```
Client                           Server
  |                               |
  |--- GET /?password=secret ---->|
  |                               |
  |                          [Verify Password]
  |                               |
  |<-- 101 Switching Protocols ---|
  |                               |
  |<-- auth_ok (encrypted) -------|
  |                               |
  [Authenticated Session]
```

### Encryption Flow

```
Plaintext JSON
    ↓
AES-256-GCM Encrypt
    ↓
[IV (12) | Ciphertext (N) | Tag (16)]
    ↓
Base64 or Binary (in WebSocket frame)
    ↓
TLS Encrypt
    ↓
Network
```

## Message Lifecycle

### Creating and Sending a Message

```c
// 1. Create message
WEB_MESSAGE_REC *msg = fe_web_message_new(WEB_MSG_MESSAGE);
msg->id = fe_web_generate_message_id();
msg->server_tag = g_strdup(server->tag);
msg->target = g_strdup("#irssi");
msg->nick = g_strdup("alice");
msg->text = g_strdup("Hello!");
msg->timestamp = time(NULL);

// 2. Send to client(s)
fe_web_send_to_server_clients(server, msg);
    ↓
// 3. Serialize to JSON
char *json = fe_web_message_to_json(msg);
    ↓
// 4. Encrypt
guchar *encrypted = fe_web_crypto_encrypt(json, strlen(json), &enc_len);
    ↓
// 5. Create WebSocket frame
guchar *frame = fe_web_websocket_create_frame(0x02, encrypted, enc_len, &frame_len);
    ↓
// 6. Send via SSL
fe_web_ssl_write(client->ssl_channel, frame, frame_len);
    ↓
// 7. Cleanup
fe_web_message_free(msg);
g_free(json);
g_free(encrypted);
g_free(frame);
```

### Receiving and Processing a Message

```c
// 1. Read from SSL socket
bytes = fe_web_ssl_read(client->ssl_channel, buffer, sizeof(buffer));
    ↓
// 2. Append to input buffer
g_byte_array_append(client->input_buffer, buffer, bytes);
    ↓
// 3. Parse WebSocket frame
fe_web_websocket_parse_frame(data, len, &fin, &opcode, &masked, 
                              &payload_len, mask_key, &payload);
    ↓
// 4. Unmask payload
fe_web_websocket_unmask(payload, payload_len, mask_key);
    ↓
// 5. Decrypt
char *json = fe_web_crypto_decrypt(payload, payload_len);
    ↓
// 6. Parse JSON and route
fe_web_client_handle_message(client, json);
    ↓
// 7. Execute command or sync
if (type == "command") {
    fe_web_client_execute_command(client, command);
        ↓
    signal_emit("send command", 3, command, server, NULL);
}
```

## Client State Management

### State Synchronization

When a client connects and syncs to a server:

```c
void fe_web_client_sync_server(client, "Freenode") {
    // 1. Assign server
    client->server = server;
    
    // 2. Dump complete state
    fe_web_dump_state(client);
        ↓
    // Send all joined channels
    for each channel in server->channels:
        send channel_join message
        send topic message
        send nicklist message
    
    // Send all queries
    for each query in server->queries:
        send query_opened message
    
    // Send activity levels
    for each window with activity:
        send activity_update message
}
```

### Activity Tracking

```c
// When message arrives in channel
sig_message_public() {
    // 1. Send message to clients
    send_message_to_clients();
    
    // 2. Check if window is active
    if (!window_is_active(window)) {
        // 3. Calculate activity level
        level = calculate_activity_level();
        
        // 4. Send activity update
        send_activity_update(server, channel, level);
    }
}

// When client marks channel as read
handle_mark_read() {
    // 1. Clear activity in Irssi
    window_activity(window, 0, NULL);
    
    // 2. Broadcast activity update to all clients
    send_activity_update(server, channel, 0);
}
```

## WHOIS Tracking

WHOIS responses arrive as multiple IRC numerics. FE-Web collects them:

```c
// Global hash table: "server:nick" -> WHOIS_REC
static GHashTable *active_whois;

// When numeric 311 arrives (WHOIS user)
event_whois() {
    rec = whois_get_or_create(server, nick);
    rec->user = parse_user(data);
    rec->host = parse_host(data);
}

// When numeric 312 arrives (WHOIS server)
event_whois_server() {
    rec = whois_get_or_create(server, nick);
    rec->server = parse_server(data);
}

// When numeric 318 arrives (end of WHOIS)
event_whois_end() {
    rec = get_whois_rec(server, nick);
    msg = build_whois_message(rec);
    fe_web_send_to_server_clients(server, msg);
    remove_whois_rec(server, nick);
}
```

## Performance Considerations

### Message Batching

- Nicklist updates are batched during NAMES response
- Activity updates are coalesced per window
- State dump sends multiple messages in sequence

### Memory Management

- All strings are allocated with `g_strdup()` and freed with `g_free()`
- Message structures use `WEB_MESSAGE_REC` with reference counting
- Client buffers use GLib data structures (`GString`, `GByteArray`)

### Buffer Management

```c
// Input buffer (client → server)
GByteArray *input_buffer;  // Accumulates partial WebSocket frames

// Output buffer (server → client)  
GString *output_buffer;     // Queues pending sends

// Flushing
fe_web_flush_output(client) {
    if (output_buffer->len > 0) {
        bytes_sent = fe_web_ssl_write(client->ssl_channel, 
                                       output_buffer->str, 
                                       output_buffer->len);
        g_string_erase(output_buffer, 0, bytes_sent);
    }
}
```

## Threading Model

**FE-Web is single-threaded** and runs in Irssi's main event loop:

- All I/O is non-blocking
- GLib `g_source_*` functions manage event sources
- SSL I/O uses non-blocking sockets
- No mutexes or locks required

## Error Handling

### Connection Errors

```c
// SSL read error
if (bytes < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return; // Would block, try later
    }
    // Fatal error
    fe_web_close_client(client);
}
```

### Protocol Errors

```c
// Invalid WebSocket frame
if (opcode == 0x08) {  // Close frame
    fe_web_close_client(client);
}

// Invalid JSON
if (json == NULL) {
    // Ignore message, keep connection open
    return;
}
```

### Graceful Degradation

- Unknown message types are silently ignored
- Missing fields use defaults
- Malformed JSON doesn't crash the module

## Extension Points

### Adding New Message Types

1. Add enum to `WEB_MESSAGE_TYPE` in `fe-web.h`
2. Add case to `fe_web_type_to_string()` in `fe-web-utils.c`
3. Add handler in `fe_web_client_handle_message()` in `fe-web-client.c`
4. Add signal handler in `fe-web-signals.c` (if IRC event)

### Adding New Commands

```c
// In fe-web.c
static void cmd_fe_web_new_command(const char *data, IRC_SERVER_REC *server) {
    // Implementation
}

void fe_web_init(void) {
    // ...
    command_bind("fe_web new_command", NULL, (SIGNAL_FUNC) cmd_fe_web_new_command);
}
```

## Testing

### Manual Testing

```bash
# Start Irssi with module
irssi
/LOAD fe-web
/SET fe_web_password test123
/SET fe_web_port 9001
/FE_WEB START

# Connect with wscat
wscat -c "wss://localhost:9001/?password=test123" --no-check

# Send test message
{"type":"ping"}
```

### Debug Logging

```c
// Enable in code
printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, 
          "fe-web: debug: %s", debug_info);
```

## Build System

### Meson Configuration

```meson
# Dependencies
irssi_dep = dependency('irssi-1', required: true)
glib_dep = dependency('glib-2.0', version: '>=2.32', required: true)
openssl_dep = dependency('openssl', version: '>=3.0', required: true)

# Build shared module
shared_module('fe_web',
  sources: fe_web_sources,
  dependencies: [irssi_dep, glib_dep, openssl_dep],
  name_prefix: 'lib',
  name_suffix: 'so',
  install: true,
  install_dir: moduledir
)
```

## License

GPL-2.0 - Same as Irssi
