# fe-web WebSocket Client Specification

## Version 1.0 (2025-01-25)

This document provides a complete specification for implementing a WebSocket client that connects to the irssi fe-web module.

---

## Table of Contents

1. [Connection and Handshake](#connection-and-handshake)
2. [WebSocket Protocol](#websocket-protocol)
3. [Message Format (JSON)](#message-format-json)
4. [Client → Server Messages](#client--server-messages)
5. [Server → Client Messages](#server--client-messages)
6. [Connection Lifecycle](#connection-lifecycle)
7. [Authentication](#authentication)
8. [Complete Implementation Example](#complete-implementation-example)

---

## Connection and Handshake

### 1. TCP Connection

Connect to the irssi server:

```
Host: 127.0.0.1 (default, configurable via fe_web_bind)
Port: 9001 (default, configurable via fe_web_port)
```

### 2. WebSocket Handshake

Send HTTP upgrade request:

```http
GET / HTTP/1.1
Host: 127.0.0.1:9001
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13
```

**Important**:
- `Sec-WebSocket-Key` must be a random 16-byte value, base64-encoded
- Generate new key for each connection

### 3. Server Response

Server responds with:

```http
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=

```

The `Sec-WebSocket-Accept` value is computed as:
```
BASE64(SHA1(Sec-WebSocket-Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
```

### 4. Post-Handshake

After successful handshake:
1. Server immediately sends `auth_ok` message (see below)
2. Client is now authenticated (no password required in current implementation)
3. Connection is ready for bidirectional communication

---

## WebSocket Protocol

### Frame Format

All messages are sent as **WebSocket TEXT frames** (opcode 0x1) containing JSON.

#### Client → Server Frames
- **MUST** be masked (RFC 6455 requirement)
- Use random 4-byte masking key per frame
- Payload is JSON text

#### Server → Client Frames
- **MUST NOT** be masked (RFC 6455 requirement)
- Payload is JSON text

### Supported Opcodes

| Opcode | Name  | Direction | Description |
|--------|-------|-----------|-------------|
| 0x1    | TEXT  | Both      | JSON message |
| 0x8    | CLOSE | Both      | Connection close |
| 0x9    | PING  | Both      | Keepalive ping |
| 0xA    | PONG  | Both      | Keepalive pong |

### Keepalive

- Server automatically responds to PING frames with PONG
- Client should send PING every 30-60 seconds to keep connection alive
- Client should respond to server PING with PONG

---

## Message Format (JSON)

All messages are JSON objects with a `type` field.

### Common Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | Yes | Message type identifier |
| `id` | string | No | Unique message ID (for tracking requests) |
| `timestamp` | number | No | Unix timestamp (server messages only) |
| `server` | string | No | IRC server tag |
| `channel` | string | No | Channel name (with #) |
| `nick` | string | No | Nickname |
| `text` | string | No | Message text/content |

---

## Client → Server Messages

### 1. sync_server - Synchronize with IRC Network(s)

Synchronize client with one or all IRC networks.

**Sync with all networks** (recommended for multi-network UI):
```json
{
  "type": "sync_server",
  "server": "*"
}
```

**Sync with specific network**:
```json
{
  "type": "sync_server",
  "server": "libera"
}
```

**Response**: Server sends complete state dump (see State Dump section).

**Fields**:
- `server` (string, required): IRC network tag or `"*"` for all networks

**Best Practices**:
- Use `"*"` at startup to receive all networks/channels
- Include `server` field in subsequent `command` messages
- Re-sync only if connection was lost or state is stale
- Avoid repeated `sync_server` for switching contexts

---

### 2. command - Execute IRC Command

Execute an IRC command on a specific server/network.

```json
{
  "type": "command",
  "command": "/join #irssi",
  "server": "libera"
}
```

**Examples**:
```json
{"type": "command", "command": "/join #channel", "server": "libera"}
{"type": "command", "command": "/msg nick Hello!", "server": "ircnet"}
{"type": "command", "command": "/whois alice alice", "server": "libera"}
{"type": "command", "command": "/mode #channel +b *!*@spam.com", "server": "efnet"}
{"type": "command", "command": "/op nick", "server": "libera"}
{"type": "command", "command": "/kick #channel spammer Get out", "server": "libera"}
```

**Fields**:
- `command` (string, required): IRC command starting with `/`
- `server` (string, optional): IRC network/server tag for this command
  - If provided, command executes on specified server
  - If omitted, uses last server from `sync_server`
  - **Recommended**: Always include for multi-network setups

**Why include server field?**
- Allows commands on different networks without re-syncing
- Better for UI with multiple networks visible simultaneously
- Avoids excessive `sync_server` messages

**Response**: Depends on command:
- Messages result in `message` events
- WHOIS results in `whois` response
- MODE results in `channel_mode` events
- Errors result in `error` messages

---

### 3. ping - Keepalive

Application-level ping (separate from WebSocket PING).

```json
{
  "id": "ping-123",
  "type": "ping"
}
```

**Response**:
```json
{
  "id": "1234567890-0001",
  "type": "pong",
  "response_to": "ping-123",
  "timestamp": 1706198400
}
```

**Fields**:
- `id` (string, optional): Request identifier for matching responses

---

### 4. close_query - Close Query Window

Close a query (private message) window with a specific user.

```json
{
  "type": "close_query",
  "server": "libera",
  "nick": "alice"
}
```

**Fields**:
- `server` (string, required): IRC network/server tag
- `nick` (string, required): Nickname of the user to close query with

**Response**: Server emits `query_closed` event to all connected clients.

**Use Case**: When user closes a private message window in the UI, send this command to synchronize the close action with irssi and other connected clients.

---

## Server → Client Messages

### Message Types Overview

| Type | Description |
|------|-------------|
| `auth_ok` | Authentication successful |
| `message` | IRC message (public/private) |
| `server_status` | Server connection status |
| `channel_join` | User joined channel |
| `channel_part` | User left channel |
| `channel_kick` | User kicked from channel |
| `user_quit` | User quit IRC |
| `topic` | Channel topic |
| `channel_mode` | Channel mode change |
| `nicklist` | Complete channel nicklist |
| `nick_change` | Nick change |
| `user_mode` | User mode change |
| `away` | Away status change |
| `whois` | WHOIS response |
| `channel_list` | Channel list (ban/except/invite) |
| `state_dump` | Initial state dump |
| `query_opened` | Query (PM) window opened |
| `query_closed` | Query (PM) window closed |
| `error` | Error message |
| `pong` | Pong response |

---

### 1. auth_ok - Authentication Success

Sent immediately after WebSocket handshake.

```json
{
  "id": "1706198400-0001",
  "type": "auth_ok",
  "timestamp": 1706198400
}
```

---

### 2. message - IRC Message

Public or private IRC message.

```json
{
  "id": "1706198400-0002",
  "type": "message",
  "server": "libera",
  "channel": "#irssi",
  "nick": "alice",
  "text": "Hello everyone!",
  "timestamp": 1706198400,
  "level": 1,
  "is_own": false
}
```

**Private message**:
```json
{
  "id": "1706198400-0003",
  "type": "message",
  "server": "libera",
  "channel": "alice",
  "nick": "alice",
  "text": "Private message",
  "timestamp": 1706198400,
  "level": 8,
  "is_own": false
}
```

**Own message**:
```json
{
  "id": "1706198400-0004",
  "type": "message",
  "server": "libera",
  "channel": "#irssi",
  "nick": "mynick",
  "text": "My message",
  "timestamp": 1706198400,
  "level": 1,
  "is_own": true
}
```

**Fields**:
- `server` (string): IRC server tag
- `channel` (string): Channel name or nick (for private messages)
- `nick` (string): Sender nickname
- `text` (string): Message text
- `level` (number): Message level (1=public, 8=private)
- `is_own` (boolean): True if message is from client's own nick

---

### 3. server_status - Server Connection Status

Server connected or disconnected.

```json
{
  "id": "1706198400-0005",
  "type": "server_status",
  "server": "libera",
  "text": "connected",
  "timestamp": 1706198400
}
```

**Values for `text`**:
- `"connected"` - Server connection established
- `"disconnected"` - Server connection lost

---

### 4. channel_join - User Joined Channel

```json
{
  "id": "1706198400-0006",
  "type": "channel_join",
  "server": "libera",
  "channel": "#irssi",
  "nick": "bob",
  "timestamp": 1706198400
}
```

---

### 5. channel_part - User Left Channel

```json
{
  "id": "1706198400-0007",
  "type": "channel_part",
  "server": "libera",
  "channel": "#irssi",
  "nick": "bob",
  "text": "Goodbye!",
  "timestamp": 1706198400
}
```

**Fields**:
- `text` (string, optional): Part message/reason

---

### 6. channel_kick - User Kicked from Channel

```json
{
  "id": "1706198400-0008",
  "type": "channel_kick",
  "server": "libera",
  "channel": "#irssi",
  "nick": "spammer",
  "text": "Spam",
  "timestamp": 1706198400,
  "extra": {
    "kicker": "alice"
  }
}
```

**Fields**:
- `text` (string, optional): Kick reason
- `extra.kicker` (string): Who performed the kick

---

### 7. user_quit - User Quit IRC

**Important**: This affects ALL channels the user was in.

```json
{
  "id": "1706198400-0009",
  "type": "user_quit",
  "server": "libera",
  "nick": "bob",
  "text": "Connection reset",
  "timestamp": 1706198400
}
```

**Fields**:
- `text` (string, optional): Quit message

---

### 8. topic - Channel Topic

```json
{
  "id": "1706198400-0010",
  "type": "topic",
  "server": "libera",
  "channel": "#irssi",
  "nick": "alice",
  "text": "Welcome to #irssi | https://irssi.org",
  "timestamp": 1706198400
}
```

**Fields**:
- `nick` (string, optional): Who set the topic (empty if from state dump)
- `text` (string): Topic text

---

### 9. channel_mode - Channel Mode Change

```json
{
  "id": "1706198400-0011",
  "type": "channel_mode",
  "server": "libera",
  "channel": "#irssi",
  "nick": "alice",
  "text": "+o bob",
  "timestamp": 1706198400
}
```

**Examples**:
- `"+o bob"` - Give op to bob
- `"-o bob"` - Remove op from bob
- `"+v alice"` - Give voice to alice
- `"+h charlie"` - Give halfop to charlie
- `"+b *!*@spam.com"` - Set ban
- `"+m"` - Set moderated mode

**Fields**:
- `nick` (string): Who performed mode change
- `text` (string): Mode change string

---

### 10. nicklist - Complete Channel Nicklist

Sent during state dump or on manual request.

```json
{
  "id": "1706198400-0012",
  "type": "nicklist",
  "server": "libera",
  "channel": "#irssi",
  "text": "[{\"nick\":\"alice\",\"prefix\":\"@\"},{\"nick\":\"bob\",\"prefix\":\"\"},{\"nick\":\"charlie\",\"prefix\":\"+\"}]",
  "timestamp": 1706198400
}
```

**`text` field contains JSON array**:
```json
[
  {"nick": "alice", "prefix": "@"},
  {"nick": "bob", "prefix": ""},
  {"nick": "charlie", "prefix": "+"}
]
```

**Prefix meanings**:
- `@` - Channel operator (op)
- `%` - Half-operator (halfop)
- `+` - Voice
- `""` - No privileges

---

### 11. nick_change - Nick Change

```json
{
  "id": "1706198400-0013",
  "type": "nick_change",
  "server": "libera",
  "nick": "alice",
  "text": "alice2",
  "timestamp": 1706198400
}
```

**Fields**:
- `nick` (string): Old nickname
- `text` (string): New nickname

---

### 12. user_mode - User Mode Change

```json
{
  "id": "1706198400-0014",
  "type": "user_mode",
  "server": "libera",
  "nick": "mynick",
  "text": "+i",
  "timestamp": 1706198400
}
```

---

### 13. away - Away Status Change

```json
{
  "id": "1706198400-0015",
  "type": "away",
  "server": "libera",
  "nick": "alice",
  "text": "Gone for lunch",
  "timestamp": 1706198400
}
```

---

### 14. whois - WHOIS Response

```json
{
  "id": "1706198400-0016",
  "type": "whois",
  "response_to": "whois-request-123",
  "server": "libera",
  "nick": "alice",
  "timestamp": 1706198400,
  "extra": {
    "user": "~alice",
    "host": "user.example.com",
    "realname": "Alice Smith",
    "server": "irc.libera.chat",
    "server_info": "Libera Chat Server",
    "channels": "@#irssi +#help",
    "idle": "300",
    "signon": "1706198100",
    "account": "alice_acc",
    "secure": "true",
    "special": [
      "is a Cloaked Connection (Spoof)",
      "is using modes +ix"
    ]
  }
}
```

**Fields in `extra`**:
- `user` (string): Username/ident
- `host` (string): Hostname
- `realname` (string): Real name/GECOS field
- `server` (string): IRC server name
- `server_info` (string): IRC server description
- `channels` (string): Space-separated channel list with prefixes
- `idle` (string): Idle time in seconds
- `signon` (string): Signon time (Unix timestamp as string)
- `account` (string): Account name (if identified)
- `secure` (string): "true" if using SSL/TLS
- `special` (array of strings): Non-standard WHOIS lines (e.g., "is a Cloaked Connection")

---

### 15. channel_list - Channel List Response

Response to MODE queries for ban/exception/invite lists.

```json
{
  "id": "1706198400-0017",
  "type": "channel_list",
  "response_to": "mode-request-123",
  "server": "libera",
  "channel": "#irssi",
  "timestamp": 1706198400,
  "extra": {
    "list_type": "b",
    "entries": "*!*@spam.com *!*@troll.com"
  }
}
```

**Fields in `extra`**:
- `list_type` (string): "b" (ban), "e" (except), "I" (invite)
- `entries` (string): Space-separated list of masks

---

### 16. state_dump - Initial State Dump

Sent after `sync_server` command. This is a marker message followed by multiple other messages.

```json
{
  "id": "1706198400-0018",
  "type": "state_dump",
  "server": "libera",
  "timestamp": 1706198400
}
```

**After state_dump, expect**:
1. `channel_join` for each channel you're in
2. `topic` for each channel (if topic is set)
3. `nicklist` for each channel

**Example sequence**:
```json
{"type": "state_dump", "server": "libera"}
{"type": "channel_join", "server": "libera", "channel": "#irssi", "nick": "mynick"}
{"type": "topic", "server": "libera", "channel": "#irssi", "text": "Welcome!"}
{"type": "nicklist", "server": "libera", "channel": "#irssi", "text": "[...]"}
{"type": "channel_join", "server": "libera", "channel": "#help", "nick": "mynick"}
{"type": "nicklist", "server": "libera", "channel": "#help", "text": "[...]"}
```

---

### 17. error - Error Message

```json
{
  "id": "1706198400-0019",
  "type": "error",
  "text": "Server not found",
  "timestamp": 1706198400
}
```

**Common errors**:
- `"Not connected to any server"` - Client not synced to a server
- `"Server not found"` - Invalid server tag in sync_server

---

### 18. pong - Pong Response

Response to client `ping` message.

```json
{
  "id": "1706198400-0020",
  "type": "pong",
  "response_to": "ping-123",
  "timestamp": 1706198400
}
```

---

### 19. query_opened - Query Window Opened

Sent when a query (private message) window is opened, either by `/query` command or when receiving a PM from someone.

```json
{
  "id": "1706198400-0021",
  "type": "query_opened",
  "server": "libera",
  "nick": "alice",
  "timestamp": 1706198400
}
```

**Fields**:
- `server` (string): IRC server tag
- `nick` (string): Nickname of the user the query is with

**When sent**:
- User executes `/query nick` command
- Incoming private message from user without existing query window

**Client behavior**:
- Create query window in UI if not exists
- Switch focus to query window (optional, based on UI preferences)
- No action needed if query already exists (idempotent)

---

### 20. query_closed - Query Window Closed

Sent when a query (private message) window is closed.

```json
{
  "id": "1706198400-0022",
  "type": "query_closed",
  "server": "libera",
  "nick": "alice",
  "timestamp": 1706198400
}
```

**Fields**:
- `server` (string): IRC server tag
- `nick` (string): Nickname of the user the query was with

**When sent**:
- User closes query window in irssi (`/wc`, `/window close`)
- Client sends `close_query` command (see Client → Server Messages)

**Client behavior**:
- Remove query window from UI
- Clean up any associated state/history
- No error if query doesn't exist (idempotent)

**Synchronization**: Query open/close state is synchronized across all connected clients. When one client closes a query, all clients receive `query_closed` and should update their UI accordingly.

---

## Connection Lifecycle

### Complete Flow

```
1. TCP Connect → 127.0.0.1:9001

2. WebSocket Handshake
   Client → Server: HTTP Upgrade request
   Server → Client: 101 Switching Protocols

3. Authentication
   Server → Client: {"type": "auth_ok"}

4. Sync to Server
   Client → Server: {"type": "sync_server", "server": "libera"}
   Server → Client: {"type": "state_dump"}
   Server → Client: Multiple channel_join, topic, nicklist messages

5. Normal Operation
   - Client sends commands
   - Server sends events
   - Bidirectional communication

6. Connection Close
   Either side: WebSocket CLOSE frame (0x8)
```

### Error Handling

**Network Errors**:
- TCP connection drops → Reconnect
- WebSocket close → Reconnect
- No data for >90s → Send PING or reconnect

**Application Errors**:
- `error` message → Display to user
- Invalid command → Server ignores (no response)

---

## Authentication

**Current Implementation**: No password required.

`auth_ok` is sent immediately after handshake.

**Future Implementation** (from settings):
```json
// Client sends after handshake:
{
  "type": "auth",
  "password": "mypassword"
}

// Server responds:
{
  "type": "auth_ok"
}
// or
{
  "type": "error",
  "text": "Authentication failed"
}
```

Currently, `fe_web_password` setting is registered but not enforced.

---

## Complete Implementation Example

### JavaScript (Browser/Node.js)

```javascript
class IrssiWebClient {
  constructor(host = '127.0.0.1', port = 9001) {
    this.host = host;
    this.port = port;
    this.ws = null;
    this.connected = false;
    this.authenticated = false;
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.ws = new WebSocket(`ws://${this.host}:${this.port}`);

      this.ws.onopen = () => {
        console.log('WebSocket connected');
        this.connected = true;
      };

      this.ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        this.handleMessage(msg);

        if (msg.type === 'auth_ok') {
          this.authenticated = true;
          resolve();
        }
      };

      this.ws.onerror = (error) => {
        console.error('WebSocket error:', error);
        reject(error);
      };

      this.ws.onclose = () => {
        console.log('WebSocket closed');
        this.connected = false;
        this.authenticated = false;
      };
    });
  }

  handleMessage(msg) {
    console.log('Received:', msg);

    switch(msg.type) {
      case 'auth_ok':
        console.log('Authenticated!');
        break;

      case 'message':
        console.log(`[${msg.server}/${msg.channel}] <${msg.nick}> ${msg.text}`);
        break;

      case 'channel_join':
        console.log(`${msg.nick} joined ${msg.channel}`);
        break;

      case 'channel_part':
        console.log(`${msg.nick} left ${msg.channel}: ${msg.text || ''}`);
        break;

      case 'topic':
        console.log(`Topic for ${msg.channel}: ${msg.text}`);
        break;

      case 'nicklist':
        const nicks = JSON.parse(msg.text);
        console.log(`Users in ${msg.channel}:`, nicks);
        break;

      case 'error':
        console.error('Error:', msg.text);
        break;
    }
  }

  syncServer(serverTag) {
    this.send({
      type: 'sync_server',
      server: serverTag
    });
  }

  sendCommand(command) {
    this.send({
      type: 'command',
      command: command
    });
  }

  send(data) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(data));
    }
  }

  ping() {
    this.send({
      id: `ping-${Date.now()}`,
      type: 'ping'
    });
  }

  disconnect() {
    if (this.ws) {
      this.ws.close();
    }
  }
}

// Usage example:
async function main() {
  const client = new IrssiWebClient();

  try {
    await client.connect();
    console.log('Connected and authenticated!');

    // Sync to server
    client.syncServer('libera');

    // Wait for state dump
    await new Promise(resolve => setTimeout(resolve, 1000));

    // Send a message
    client.sendCommand('/msg #irssi Hello from WebSocket!');

    // Keepalive ping every 30 seconds
    setInterval(() => client.ping(), 30000);

  } catch (error) {
    console.error('Connection failed:', error);
  }
}

main();
```

---

### Python (websocket-client library)

```python
import json
import websocket
import threading
import time

class IrssiWebClient:
    def __init__(self, host='127.0.0.1', port=9001):
        self.host = host
        self.port = port
        self.ws = None
        self.connected = False
        self.authenticated = False

    def connect(self):
        url = f"ws://{self.host}:{self.port}"
        self.ws = websocket.WebSocketApp(
            url,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close
        )

        # Run in thread
        wst = threading.Thread(target=self.ws.run_forever)
        wst.daemon = True
        wst.start()

        # Wait for authentication
        for _ in range(50):
            if self.authenticated:
                return True
            time.sleep(0.1)
        return False

    def on_open(self, ws):
        print("WebSocket connected")
        self.connected = True

    def on_message(self, ws, message):
        msg = json.loads(message)
        print(f"Received: {msg}")

        if msg['type'] == 'auth_ok':
            self.authenticated = True
            print("Authenticated!")

        elif msg['type'] == 'message':
            print(f"[{msg['server']}/{msg['channel']}] <{msg['nick']}> {msg['text']}")

        elif msg['type'] == 'channel_join':
            print(f"{msg['nick']} joined {msg['channel']}")

        elif msg['type'] == 'error':
            print(f"Error: {msg['text']}")

    def on_error(self, ws, error):
        print(f"Error: {error}")

    def on_close(self, ws, close_status_code, close_msg):
        print("WebSocket closed")
        self.connected = False
        self.authenticated = False

    def send(self, data):
        if self.ws and self.connected:
            self.ws.send(json.dumps(data))

    def sync_server(self, server_tag):
        self.send({
            'type': 'sync_server',
            'server': server_tag
        })

    def send_command(self, command):
        self.send({
            'type': 'command',
            'command': command
        })

    def ping(self):
        self.send({
            'id': f'ping-{int(time.time())}',
            'type': 'ping'
        })

# Usage:
client = IrssiWebClient()
if client.connect():
    client.sync_server('libera')
    time.sleep(1)
    client.send_command('/msg #irssi Hello from Python!')

    # Keep alive
    while True:
        client.ping()
        time.sleep(30)
```

---

## Testing with websocat

Quick testing without writing code:

```bash
# Install websocat
brew install websocat  # macOS
# or
cargo install websocat

# Connect
websocat ws://127.0.0.1:9001

# You'll see:
# {"id":"1706198400-0001","type":"auth_ok","timestamp":1706198400}

# Send sync_server:
{"type":"sync_server","server":"libera"}

# Send command:
{"type":"command","command":"/msg #irssi Test message"}

# Send ping:
{"type":"ping","id":"test-1"}
```

---

## Configuration

irssi settings (via `/set` command):

```
/set fe_web_enabled ON
/set fe_web_port 9001
/set fe_web_bind 127.0.0.1
/set fe_web_password ""
```

Check status:
```
/fe_web status
```

---

## Implementation Checklist

When implementing a client, ensure:

- ✅ WebSocket handshake with random Sec-WebSocket-Key
- ✅ Client frames are masked (RFC 6455 requirement)
- ✅ Handle TEXT frames (opcode 0x1)
- ✅ JSON parsing for all message types
- ✅ Send sync_server before issuing commands
- ✅ Handle state dump sequence (channel_join + topic + nicklist)
- ✅ Respond to WebSocket PING frames with PONG
- ✅ Send application-level ping every 30-60s
- ✅ Reconnect on connection loss
- ✅ Display all message types appropriately

---

## Message Type Reference Table

| Type | Fields | Description |
|------|--------|-------------|
| `auth_ok` | - | Authentication success |
| `message` | server, channel, nick, text, level, is_own | IRC message |
| `server_status` | server, text | Connection status |
| `channel_join` | server, channel, nick | User joined |
| `channel_part` | server, channel, nick, text? | User left |
| `channel_kick` | server, channel, nick, text?, extra.kicker | User kicked |
| `user_quit` | server, nick, text? | User quit |
| `topic` | server, channel, nick?, text | Topic change |
| `channel_mode` | server, channel, nick, text | Mode change |
| `nicklist` | server, channel, text (JSON array) | Nicklist |
| `nick_change` | server, nick, text | Nick change |
| `user_mode` | server, nick, text | User mode |
| `away` | server, nick, text | Away status |
| `whois` | server, nick, response_to?, extra (user, host, realname, server, server_info, channels, idle, signon, account, secure, special) | WHOIS data |
| `channel_list` | server, channel, response_to?, extra | Ban/except/invite list |
| `state_dump` | server | State dump marker |
| `query_opened` | server, nick | Query window opened |
| `query_closed` | server, nick | Query window closed |
| `error` | text | Error message |
| `pong` | response_to? | Pong response |

---

## Version History

- **1.0** (2025-01-25): Initial specification
  - Complete WebSocket RFC 6455 implementation
  - JSON-based message protocol
  - All IRC events supported
  - State dump mechanism

---

## Support

For issues or questions:
- GitHub: https://github.com/kofany/irssi
- IRC: #irssi on Libera.Chat

---

**End of Specification**
