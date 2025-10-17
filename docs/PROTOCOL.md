# Irssi FE-Web Protocol Specification v1.0.0

Complete WebSocket protocol documentation for the Irssi FE-Web module.

## Table of Contents

1. [Connection and Authentication](#connection-and-authentication)
2. [Message Format](#message-format)
3. [Client → Server Messages](#client--server-messages)
4. [Server → Client Messages](#server--client-messages)
5. [Message Type Reference](#message-type-reference)
6. [Security Model](#security-model)
7. [Error Handling](#error-handling)

---

## Connection and Authentication

### Connection URL

```
wss://host:port/?password=your-password
```

**Required Parameters:**
- `password`: Authentication password (configured via `/SET fe_web_password`)

### Connection Flow

```
Client                                    Server
  |                                         |
  |------ TCP Connect -------------------->|
  |<----- TCP Accept ----------------------|
  |                                         |
  |------ TLS Handshake ------------------>|
  |<----- TLS Handshake -------------------|
  |                                         |
  |------ HTTP Upgrade Request ----------->|
  |       GET /?password=secret HTTP/1.1   |
  |       Upgrade: websocket               |
  |       Sec-WebSocket-Key: ...           |
  |                                         |
  |<----- HTTP 101 Switching Protocols ----|
  |       Sec-WebSocket-Accept: ...        |
  |                                         |
  |<----- auth_ok (encrypted) -------------|
  |                                         |
  |------ sync_server -------------------->|
  |<----- state_dump (encrypted) ----------|
  |                                         |
  [Normal WebSocket Communication]
```

### Authentication Response

**Type:** `auth_ok` (1)

```json
{
  "type": "auth_ok",
  "timestamp": 1729180000
}
```

**Success:** Client receives `auth_ok` message
**Failure:** Connection closed immediately

---

## Message Format

All messages after authentication are **JSON-encoded** and transmitted through **WebSocket binary frames**.

### Encryption

All messages (except the initial HTTP handshake) are encrypted using **AES-256-GCM**:

```
WebSocket Frame → Decrypt → JSON → Parse
```

### Base Message Structure

```json
{
  "id": "message-id",
  "type": "message_type",
  "timestamp": 1729180000,
  "server": "server-tag",
  "target": "#channel-or-nick",
  "nick": "nickname",
  "text": "message content",
  "level": 1,
  "is_own": false
}
```

**Common Fields:**
- `id` (string, optional): Unique message identifier (timestamp-counter format: `1729180000-0001`)
- `type` (string, required): Message type name
- `timestamp` (integer, optional): Unix timestamp
- `server` (string, optional): Server tag (e.g., "Freenode", "EFnet")
- `target` (string, optional): Channel name or nickname
- `nick` (string, optional): Sender nickname
- `text` (string, optional): Message content
- `level` (integer, optional): Irssi message level (bitmask)
- `is_own` (boolean, optional): Message sent by current user

---

## Client → Server Messages

### 1. sync_server

Synchronize client to a specific IRC server.

```json
{
  "type": "sync_server",
  "server": "Freenode"
}
```

**Special value:** `"server": "*"` - Sync all servers

**Response:** Server sends `state_dump` message

---

### 2. command

Execute an IRC command.

```json
{
  "type": "command",
  "command": "/msg NickServ identify password",
  "server": "Freenode"
}
```

**Fields:**
- `command` (string, required): Command to execute (with or without leading `/`)
- `server` (string, optional): Server tag for context

**Examples:**
```json
{"type": "command", "command": "/join #irssi"}
{"type": "command", "command": "/msg #irssi Hello!"}
{"type": "command", "command": "/whois alice"}
{"type": "command", "command": "/connect irc.libera.chat"}
```

---

### 3. ping

Keep-alive ping.

```json
{
  "type": "ping",
  "id": "ping-12345"
}
```

**Response:** `pong` message with matching `response_to` field

---

### 4. names

Request nicklist for a channel.

```json
{
  "type": "names",
  "channel": "#irssi",
  "server": "Freenode"
}
```

**Response:** `nicklist` message

---

### 5. close_query

Close a private query window.

```json
{
  "type": "close_query",
  "nick": "alice",
  "server": "Freenode"
}
```

---

### 6. mark_read

Mark channel/query as read (clear activity).

```json
{
  "type": "mark_read",
  "target": "#irssi",
  "server": "Freenode"
}
```

**Effect:** Clears activity indicators and unread markers

---

### 7. network_list

Request list of all configured networks.

```json
{
  "type": "network_list",
  "id": "req-001"
}
```

**Response:** `network_list_response`

---

### 8. server_list

Request list of all configured servers.

```json
{
  "type": "server_list",
  "id": "req-002"
}
```

**Response:** `server_list_response`

---

### 9. network_add

Add or modify an IRC network.

```json
{
  "type": "network_add",
  "id": "req-003",
  "name": "Freenode",
  "nick": "mynick",
  "username": "myuser",
  "realname": "My Real Name",
  "autosendcmd": "/msg NickServ identify password"
}
```

**Fields:**
- `name` (string, required): Network name
- `nick` (string, optional): Default nickname
- `username` (string, optional): Username/ident
- `realname` (string, optional): Real name
- `autosendcmd` (string, optional): Commands to run on connect

**Response:** `command_result`

---

### 10. network_remove

Remove an IRC network.

```json
{
  "type": "network_remove",
  "id": "req-004",
  "name": "OldNetwork"
}
```

**Response:** `command_result`

---

### 11. server_add

Add or modify a server.

```json
{
  "type": "server_add",
  "id": "req-005",
  "address": "irc.libera.chat",
  "port": 6697,
  "chatnet": "Libera",
  "use_tls": true,
  "autoconnect": true
}
```

**Fields:**
- `address` (string, required): Server hostname/IP
- `port` (integer, optional): Port number (default: 6667)
- `chatnet` (string, optional): Associated network name
- `password` (string, optional): Server password
- `use_tls` (boolean, optional): Use SSL/TLS
- `autoconnect` (boolean, optional): Auto-connect on startup

**Response:** `command_result`

---

### 12. server_remove

Remove a server.

```json
{
  "type": "server_remove",
  "id": "req-006",
  "address": "old.server.net",
  "port": 6667,
  "chatnet": "OldNet"
}
```

**Response:** `command_result`

---

## Server → Client Messages

### 1. auth_ok (1)

Authentication successful.

```json
{
  "type": "auth_ok",
  "timestamp": 1729180000
}
```

---

### 2. message (2)

IRC message (PRIVMSG, NOTICE, ACTION).

```json
{
  "type": "message",
  "id": "1729180000-0042",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "nick": "alice",
  "text": "Hello everyone!",
  "level": 1,
  "is_own": false
}
```

**Level values (bitmask):**
- `1` - MSGLEVEL_CRAP
- `2` - MSGLEVEL_MSGS
- `4` - MSGLEVEL_PUBLIC
- `8` - MSGLEVEL_NOTICES
- `16` - MSGLEVEL_SNOTES
- `32` - MSGLEVEL_CTCPS
- `64` - MSGLEVEL_ACTIONS
- `128` - MSGLEVEL_JOINS
- `256` - MSGLEVEL_PARTS
- `512` - MSGLEVEL_QUITS
- `1024` - MSGLEVEL_KICKS
- `2048` - MSGLEVEL_MODES
- `4096` - MSGLEVEL_TOPICS
- `8192` - MSGLEVEL_WALLOPS
- `16384` - MSGLEVEL_INVITES
- `32768` - MSGLEVEL_NICKS
- `65536` - MSGLEVEL_DCC
- `131072` - MSGLEVEL_DCCMSGS
- `262144` - MSGLEVEL_CLIENTNOTICE
- `524288` - MSGLEVEL_CLIENTCRAP
- `1048576` - MSGLEVEL_CLIENTERROR
- `2097152` - MSGLEVEL_HILIGHT

---

### 3. server_status (3)

Server connection status change.

```json
{
  "type": "server_status",
  "id": "1729180000-0001",
  "timestamp": 1729180000,
  "server": "Freenode",
  "text": "connected",
  "extra": {
    "address": "irc.freenode.net",
    "port": "6697"
  }
}
```

**Status values:**
- `"connected"` - Successfully connected
- `"disconnected"` - Disconnected from server

---

### 4. channel_join (4)

User joined a channel.

```json
{
  "type": "channel_join",
  "id": "1729180000-0010",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "nick": "bob",
  "text": "bob@host.example.com"
}
```

**Fields:**
- `nick`: Nickname that joined
- `text`: Full hostmask (user@host)

---

### 5. channel_part (5)

User left a channel.

```json
{
  "type": "channel_part",
  "id": "1729180000-0011",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "nick": "bob",
  "text": "Goodbye!"
}
```

**Fields:**
- `text`: Part reason/message

---

### 6. channel_kick (6)

User was kicked from a channel.

```json
{
  "type": "channel_kick",
  "id": "1729180000-0012",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "nick": "spammer",
  "text": "Kicked by alice: Stop spamming"
}
```

---

### 7. user_quit (7)

User quit IRC.

```json
{
  "type": "user_quit",
  "id": "1729180000-0013",
  "timestamp": 1729180000,
  "server": "Freenode",
  "nick": "bob",
  "text": "Quit: Connection reset"
}
```

---

### 8. topic (8)

Channel topic.

```json
{
  "type": "topic",
  "id": "1729180000-0014",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "nick": "alice",
  "text": "Welcome to #irssi | Latest: 1.4.5"
}
```

**Fields:**
- `nick`: Who set the topic (empty if from server)
- `text`: Topic text

---

### 9. channel_mode (9)

Channel mode change.

```json
{
  "type": "channel_mode",
  "id": "1729180000-0015",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "nick": "alice",
  "text": "+o bob"
}
```

---

### 10. nicklist (10)

Complete nicklist for a channel.

```json
{
  "type": "nicklist",
  "id": "1729180000-0016",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "text": "[{\"nick\":\"alice\",\"prefix\":\"@\"},{\"nick\":\"bob\",\"prefix\":\"+\"}]"
}
```

**Text field format:** JSON array of objects
```json
[
  {"nick": "alice", "prefix": "@"},
  {"nick": "bob", "prefix": "+"},
  {"nick": "charlie", "prefix": ""}
]
```

**Prefix values:**
- `@` - Op
- `%` - Half-op
- `+` - Voice
- `""` - Regular user

---

### 11. nicklist_update (11)

Incremental nicklist update.

```json
{
  "type": "nicklist_update",
  "id": "1729180000-0017",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "nick": "bob",
  "text": "+o"
}
```

**Update types (text field):**
- `"add"` - Nick joined
- `"remove"` - Nick left
- `"+o"` - Op granted
- `"-o"` - Op removed
- `"+v"` - Voice granted
- `"-v"` - Voice removed
- `"+h"` - Half-op granted
- `"-h"` - Half-op removed

---

### 12. nick_change (12)

Nickname change.

```json
{
  "type": "nick_change",
  "id": "1729180000-0018",
  "timestamp": 1729180000,
  "server": "Freenode",
  "nick": "alice",
  "text": "alice_away"
}
```

**Fields:**
- `nick`: Old nickname
- `text`: New nickname

---

### 13. user_mode (13)

User mode change.

```json
{
  "type": "user_mode",
  "id": "1729180000-0019",
  "timestamp": 1729180000,
  "server": "Freenode",
  "text": "+i"
}
```

---

### 14. away (14)

Away status change.

```json
{
  "type": "away",
  "id": "1729180000-0020",
  "timestamp": 1729180000,
  "server": "Freenode",
  "text": "Away: Lunch break"
}
```

**Empty text:** User is back (not away)

---

### 15. whois (15)

WHOIS response.

```json
{
  "type": "whois",
  "id": "1729180000-0021",
  "timestamp": 1729180000,
  "server": "Freenode",
  "response_to": "req-whois-alice",
  "extra": {
    "nick": "alice",
    "user": "alice",
    "host": "example.com",
    "realname": "Alice Smith",
    "server": "irc.freenode.net",
    "server_info": "Freenode Server",
    "idle": "120",
    "signon": "1729170000",
    "channels": "#irssi #linux",
    "account": "alice_account",
    "secure": "1",
    "oper": "0"
  }
}
```

---

### 16. state_dump (17)

Complete IRC state snapshot.

```json
{
  "type": "state_dump",
  "id": "1729180000-0030",
  "timestamp": 1729180000,
  "text": "state_dump_complete"
}
```

**Followed by:**
- Multiple `channel_join` messages
- Multiple `topic` messages
- Multiple `nicklist` messages
- Multiple `activity_update` messages

---

### 18. error (18)

Error message.

```json
{
  "type": "error",
  "id": "1729180000-0040",
  "timestamp": 1729180000,
  "text": "Server not found"
}
```

---

### 19. pong (19)

Ping response.

```json
{
  "type": "pong",
  "id": "1729180000-0041",
  "timestamp": 1729180000,
  "response_to": "ping-12345"
}
```

---

### 20. query_opened (20)

Private query opened.

```json
{
  "type": "query_opened",
  "id": "1729180000-0042",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "alice"
}
```

---

### 21. query_closed (21)

Private query closed.

```json
{
  "type": "query_closed",
  "id": "1729180000-0043",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "alice"
}
```

---

### 22. activity_update (22)

Window activity level changed.

```json
{
  "type": "activity_update",
  "id": "1729180000-0044",
  "timestamp": 1729180000,
  "server": "Freenode",
  "target": "#irssi",
  "level": 4
}
```

**Activity levels:**
- `0` - No activity
- `1` - Text activity
- `2` - Message/notice
- `3` - Highlight

---

### 24. network_list_response (25)

Network list response.

```json
{
  "type": "network_list_response",
  "id": "1729180000-0050",
  "response_to": "req-001",
  "text": "[{\"name\":\"Freenode\",\"nick\":\"mybot\",\"username\":\"bot\",\"realname\":\"My Bot\"}]"
}
```

**Text field format:** JSON array of network objects

---

### 26. server_list_response (27)

Server list response.

```json
{
  "type": "server_list_response",
  "id": "1729180000-0051",
  "response_to": "req-002",
  "text": "[{\"address\":\"irc.libera.chat\",\"port\":6697,\"chatnet\":\"Libera\",\"use_tls\":true}]"
}
```

**Text field format:** JSON array of server objects

---

### 32. command_result (32)

Command execution result.

```json
{
  "type": "command_result",
  "id": "1729180000-0052",
  "response_to": "req-003",
  "text": "{\"success\":true,\"message\":\"Network added successfully\"}"
}
```

**Text field format:** JSON object
```json
{
  "success": true,
  "message": "Operation successful",
  "error_code": null
}
```

---

## Message Type Reference

| Type | ID | Direction | Description |
|------|----|-----------| ------------|
| `auth_ok` | 1 | S→C | Authentication successful |
| `message` | 2 | S→C | IRC message (PRIVMSG/NOTICE) |
| `server_status` | 3 | S→C | Server connected/disconnected |
| `channel_join` | 4 | S→C | User joined channel |
| `channel_part` | 5 | S→C | User left channel |
| `channel_kick` | 6 | S→C | User kicked from channel |
| `user_quit` | 7 | S→C | User quit IRC |
| `topic` | 8 | S→C | Channel topic |
| `channel_mode` | 9 | S→C | Channel mode change |
| `nicklist` | 10 | S→C | Complete nicklist |
| `nicklist_update` | 11 | S→C | Nicklist delta update |
| `nick_change` | 12 | S→C | Nickname changed |
| `user_mode` | 13 | S→C | User mode changed |
| `away` | 14 | S→C | Away status changed |
| `whois` | 15 | S→C | WHOIS response |
| `channel_list` | 16 | S→C | Channel list (unused) |
| `state_dump` | 17 | S→C | Complete state snapshot |
| `error` | 18 | S→C | Error message |
| `pong` | 19 | S→C | Ping response |
| `query_opened` | 20 | S→C | Query window opened |
| `query_closed` | 21 | S→C | Query window closed |
| `activity_update` | 22 | S→C | Activity level changed |
| `mark_read` | 23 | C→S | Mark as read |
| `network_list` | 24 | C→S | Request network list |
| `network_list_response` | 25 | S→C | Network list response |
| `server_list` | 26 | C→S | Request server list |
| `server_list_response` | 27 | S→C | Server list response |
| `network_add` | 28 | C→S | Add/modify network |
| `network_remove` | 29 | C→S | Remove network |
| `server_add` | 30 | C→S | Add/modify server |
| `server_remove` | 31 | C→S | Remove server |
| `command_result` | 32 | S→C | Command result |

---

## Security Model

### Transport Layer

**Protocol:** WebSocket Secure (WSS)
**TLS Version:** 1.2 minimum
**Certificate:** Auto-generated self-signed (2048-bit RSA)

### Application Layer

**Encryption:** AES-256-GCM
**Key Derivation:** PBKDF2
- Iterations: 10,000
- Hash: SHA-256
- Salt: Fixed (`"irssi-fe-web-v1"`)

**Encryption Flow:**
```
Password → PBKDF2 → 256-bit Key
Message JSON → AES-256-GCM Encrypt → WebSocket Frame
```

### Authentication

**Method:** Password in URL query parameter
**Validation:** On WebSocket handshake
**Failure:** Immediate connection termination

---

## Error Handling

### Connection Errors

**Invalid Password:**
- Connection closed during handshake
- No error message sent

**Network Errors:**
- Client should implement reconnection logic
- Exponential backoff recommended

### Protocol Errors

**Unknown Message Type:**
- Message silently ignored
- No error response sent

**Malformed JSON:**
- Message silently ignored
- Connection remains open

**Missing Required Fields:**
- Message silently ignored
- Operation not performed

### Server Errors

Server sends `error` message with description:

```json
{
  "type": "error",
  "text": "Server not found"
}
```

**Command Result Errors:**

```json
{
  "type": "command_result",
  "text": "{\"success\":false,\"message\":\"Network name required\",\"error_code\":\"MISSING_NAME\"}"
}
```

---

## Best Practices

### Connection Management

1. **Always use WSS** - Never use unencrypted WS
2. **Validate TLS certificate** - Accept self-signed for localhost
3. **Implement reconnection** - Exponential backoff on disconnect
4. **Send periodic pings** - Every 30-60 seconds

### Message Handling

1. **Track request IDs** - Match responses to requests
2. **Handle out-of-order messages** - Use timestamps for ordering
3. **Implement message queue** - Handle bursts gracefully
4. **Parse JSON safely** - Handle malformed data

### State Management

1. **Request state_dump on connect** - Get initial state
2. **Apply deltas incrementally** - Process updates as they arrive
3. **Cache nicklists locally** - Apply updates efficiently
4. **Track activity per channel** - Update UI indicators

### Performance

1. **Batch mark_read requests** - Don't send on every click
2. **Throttle UI updates** - Coalesce rapid nicklist changes
3. **Lazy-load history** - Request only when needed
4. **Compress long messages** - Consider pagination

---

## Version History

- **v1.0.0** (2025) - Initial release
  - WebSocket protocol with TLS
  - AES-256-GCM encryption
  - Full IRC event coverage
  - Network/server management

---

## License

GPL-2.0 - Same as Irssi

## Contact

- GitHub: https://github.com/kofany/irssi-fe-web
- Irssi: https://irssi.org/
