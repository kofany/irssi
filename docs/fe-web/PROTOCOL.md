# fe-web Protocol Specification

**JSON-based WebSocket Protocol for irssi Web Frontend**

Version: 2.0
Status: Draft
Date: 2025-01-11

---

## Table of Contents

1. [Overview](#overview)
2. [Connection Flow](#connection-flow)
3. [Message Format](#message-format)
4. [Client → Server Messages](#client--server-messages)
5. [Server → Client Messages](#server--client-messages)
6. [Message Types Reference](#message-types-reference)
7. [Error Handling](#error-handling)
8. [Examples](#examples)

---

## Overview

The fe-web protocol is a simple, JSON-based WebSocket protocol designed for irssi web frontends. It prioritizes:

- **Simplicity** - Easy to implement in any language
- **Debuggability** - Human-readable JSON
- **Efficiency** - Minimal overhead
- **Extensibility** - Easy to add new message types

### Why Not WeeChat Relay Protocol?

- ❌ WeeChat uses complex binary format (hdata, pointers, compression)
- ❌ Requires 6000+ lines of C code to implement
- ❌ Never 100% compatible (architectural differences)
- ✅ JSON is simple, debuggable, and universal

---

## Connection Flow

```
Client                          Server (irssi fe-web)
  │
  ├─ TCP Connect (port 9001)
  ├─────────────────────────────▶
  │
  │◀─ TCP Accept
  │
  ├─ WebSocket Handshake (HTTP)
  ├─────────────────────────────▶
  │                               Parse Sec-WebSocket-Key
  │◀─────────────────────────────┤
  │  WebSocket Accept (101)       Generate Accept hash
  │
  ├─ {"type":"auth", "password":"..."}  (optional)
  ├─────────────────────────────▶
  │                               Verify password
  │◀─────────────────────────────┤
  │  {"type":"auth_ok"}
  │
  ├─ {"type":"sync_server", "server":"libera"}
  ├─────────────────────────────▶
  │                               Assign client to server
  │◀─────────────────────────────┤
  │  State dump (server info,     Call fe_web_dump_state()
  │   channels, nicks, topics)
  │
  ├─ Real-time messages
  │◀────────────────────────────▶
  │  Bi-directional streaming
```

---

## Message Format

### Base Structure

All messages (both directions) are JSON objects:

```json
{
    "id": "string",           // UUID or timestamp (optional)
    "type": "string",         // Message type (required)
    // ... type-specific fields
}
```

### Field Types

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique message identifier (UUID or `timestamp-counter`) |
| `type` | string | Message type (see below) |
| `server` | string | Server tag (e.g., "libera", "oftc") |
| `channel` | string | Channel name (e.g., "#test") or nick for private messages |
| `nick` | string | Nickname |
| `text` | string | Message text or content |
| `timestamp` | integer | Unix timestamp (seconds since epoch) |
| `level` | integer | irssi MSGLEVEL_* constant |
| `is_own` | boolean | True if this is user's own message |

---

## Client → Server Messages

### 1. Authentication (Optional)

```json
{
    "type": "auth",
    "password": "secret123"
}
```

**Response:**
```json
{
    "type": "auth_ok"
}
```

Or error:
```json
{
    "type": "error",
    "message": "Authentication failed"
}
```

---

### 2. Sync Server

Select which server(s) to synchronize with:

```json
{
    "type": "sync_server",
    "server": "libera"
}
```

**Special values:**
- `"*"` - Sync all servers
- `"libera"` - Sync specific server by tag

**Response:** Initial state dump (multiple messages)

---

### 3. Send Command

Execute an IRC command or send a message:

```json
{
    "type": "command",
    "server": "libera",
    "command": "/join #test"
}
```

Or send a message:

```json
{
    "type": "command",
    "server": "libera",
    "channel": "#test",
    "command": "Hello world!"
}
```

**With request tracking (for commands that return data):**

```json
{
    "id": "req-12345",
    "type": "command",
    "server": "libera",
    "command": "/whois alice alice"
}
```

**Note:**
- If `channel` is provided and `command` doesn't start with `/`, it's treated as a message to that channel/nick
- If `id` is provided, responses to this command will include `response_to: "req-12345"` field
- This is useful for commands like `/whois`, `/who`, `/list`, `/mode #channel +b` etc.

**Response:**
- Simple commands: No direct response, IRC signals generate appropriate messages
- Query commands with `id`: Response messages will include `response_to` field

---

### 4. Ping

Keep-alive ping:

```json
{
    "type": "ping",
    "timestamp": 1737825000
}
```

**Response:**
```json
{
    "type": "pong",
    "timestamp": 1737825000
}
```

---

## Server → Client Messages

### 1. Chat Message

Public channel message or private message:

```json
{
    "id": "550e8400-e29b-41d4-a716-446655440000",
    "type": "message",
    "server": "libera",
    "channel": "#test",
    "nick": "alice",
    "text": "Hello everyone!",
    "timestamp": 1737825000,
    "level": 4,
    "is_own": false
}
```

**Fields:**
- `level` - irssi MSGLEVEL_* (1=CRAP, 2=MSGS, 4=PUBLIC, 8=NOTICES, ...)
- `is_own` - `true` if this is your own message

---

### 2. Server Status

Server connected/disconnected:

```json
{
    "type": "server_status",
    "server": "libera",
    "status": "connected",
    "address": "irc.libera.chat",
    "port": 6697,
    "nick": "mynick",
    "timestamp": 1737825000
}
```

**Status values:**
- `"connected"`
- `"disconnected"`
- `"connecting"`
- `"error"`

---

### 3. Channel Join

User joined a channel:

```json
{
    "type": "channel_join",
    "server": "libera",
    "channel": "#test",
    "nick": "alice",
    "address": "~alice@host.com",
    "timestamp": 1737825000
}
```

---

### 4. Channel Part

User left a channel voluntarily (PART):

```json
{
    "type": "channel_part",
    "server": "libera",
    "channel": "#test",
    "nick": "alice",
    "reason": "Leaving",
    "timestamp": 1737825000
}
```

---

### 5. Channel Kick

User was kicked from a channel:

```json
{
    "type": "channel_kick",
    "server": "libera",
    "channel": "#test",
    "nick": "alice",
    "kicked_by": "bob",
    "reason": "Flood",
    "timestamp": 1737825000
}
```

**Fields:**
- `nick` - User who was kicked
- `kicked_by` - Operator who performed the kick
- `reason` - Kick reason (may be empty)

---

### 6. User Quit

User disconnected from server (affects ALL channels):

```json
{
    "type": "user_quit",
    "server": "libera",
    "nick": "alice",
    "reason": "Ping timeout",
    "timestamp": 1737825000
}
```

**Note:** Unlike PART/KICK (single channel), QUIT removes user from ALL channels on the server. Frontend should remove this nick from all channel nicklists.

---

### 7. Channel Topic

Channel topic update:

```json
{
    "type": "topic",
    "server": "libera",
    "channel": "#test",
    "topic": "Welcome to #test!",
    "set_by": "bob",
    "set_at": 1737820000,
    "timestamp": 1737825000
}
```

---

### 8. Channel Mode

Channel mode changed:

```json
{
    "type": "channel_mode",
    "server": "libera",
    "channel": "#test",
    "mode": "+m",
    "set_by": "alice",
    "timestamp": 1737825000
}
```

**Common channel modes:**
- `+m` - Moderated (only ops/voice can talk)
- `+t` - Only ops can change topic
- `+n` - No external messages
- `+i` - Invite-only
- `+s` - Secret channel
- `+k password` - Channel key (password required)
- `+l 50` - User limit

**Example with parameter:**
```json
{
    "type": "channel_mode",
    "server": "libera",
    "channel": "#test",
    "mode": "+k",
    "mode_param": "secret123",
    "set_by": "alice",
    "timestamp": 1737825000
}
```

---

### 9. Nick List

Nicklist update (join/part/kick/quit/mode change):

```json
{
    "type": "nicklist",
    "server": "libera",
    "channel": "#test",
    "nick": "alice",
    "action": "join",
    "modes": {
        "op": false,
        "voice": true,
        "halfop": false
    },
    "timestamp": 1737825000
}
```

**Actions:**
- `"join"` - Nick added to channel
- `"part"` - Nick removed from channel (voluntary PART)
- `"kick"` - Nick removed from channel (kicked by op)
- `"quit"` - Nick removed from channel (disconnected from server)
- `"mode"` - Nick mode changed

**Note:** For `user_quit`, you'll receive multiple `nicklist` messages (one per channel the user was on), all with `action: "quit"`.

---

### 10. Nick Change

Nick changed:

```json
{
    "type": "nick_change",
    "server": "libera",
    "old_nick": "alice",
    "new_nick": "alice_",
    "timestamp": 1737825000
}
```

---

### 11. User Mode

Your user mode changed:

```json
{
    "type": "user_mode",
    "server": "libera",
    "mode": "+i",
    "timestamp": 1737825000
}
```

---

### 12. Away Status

Away status changed:

```json
{
    "type": "away",
    "server": "libera",
    "away": true,
    "reason": "AFK",
    "timestamp": 1737825000
}
```

---

### 13. WHOIS Response

WHOIS information about a user:

```json
{
    "id": "550e8400-e29b-41d4-a716-446655440000",
    "type": "whois",
    "response_to": "req-12345",
    "server": "libera",
    "nick": "alice",
    "data": {
        "user": "~alice",
        "host": "user.example.com",
        "realname": "Alice Smith",
        "server_name": "irc.libera.chat",
        "server_info": "Libera Chat",
        "channels": ["@#test", "+#dev", "#support"],
        "idle": 300,
        "signon": 1737820000,
        "account": "alice_acc",
        "secure": true,
        "bot": false,
        "oper": false
    },
    "timestamp": 1737825000
}
```

**Fields:**
- `response_to` - ID of the original `/whois` command (if tracking enabled)
- `data.channels` - List of channels with prefixes (@=op, +=voice, %=halfop)
- `data.idle` - Idle time in seconds
- `data.signon` - Unix timestamp when user signed on
- `data.account` - NickServ account name (if authenticated)
- `data.secure` - Using secure connection (SSL/TLS)
- `data.bot` - User is a bot
- `data.oper` - User is IRC operator

---

### 14. Ban/Exception/Invite List

Channel ban/exception/invite list response:

```json
{
    "id": "550e8400-e29b-41d4-a716-446655440001",
    "type": "channel_list",
    "response_to": "req-12346",
    "server": "libera",
    "channel": "#test",
    "list_type": "ban",
    "entries": [
        {
            "mask": "*!*@spammer.com",
            "set_by": "alice",
            "set_at": 1737820000
        },
        {
            "mask": "badnick!*@*",
            "set_by": "bob",
            "set_at": 1737821000
        }
    ],
    "timestamp": 1737825000
}
```

**List types:**
- `"ban"` - Ban list (mode +b)
- `"exception"` - Ban exception list (mode +e)
- `"invite"` - Invite exception list (mode +I)
- `"quiet"` - Quiet list (mode +q, network-specific)

---

### 15. State Dump

Initial state after `sync_server` (multiple messages):

**Server info:**
```json
{
    "type": "state_dump",
    "dump_type": "server",
    "server": "libera",
    "connected": true,
    "address": "irc.libera.chat",
    "port": 6697,
    "nick": "mynick",
    "usermode": "+i",
    "away": false
}
```

**Channel info:**
```json
{
    "type": "state_dump",
    "dump_type": "channel",
    "server": "libera",
    "channel": "#test",
    "topic": "Welcome!",
    "users": 42,
    "modes": "+nt"
}
```

**Nicklist:**
```json
{
    "type": "state_dump",
    "dump_type": "nick",
    "server": "libera",
    "channel": "#test",
    "nick": "alice",
    "modes": {
        "op": false,
        "voice": true
    }
}
```

**End of dump:**
```json
{
    "type": "state_dump",
    "dump_type": "complete",
    "server": "libera"
}
```

---

### 16. Error

Error message:

```json
{
    "type": "error",
    "message": "Not connected to server",
    "code": "ERR_NOT_CONNECTED",
    "timestamp": 1737825000
}
```

**Error codes:**
- `ERR_NOT_CONNECTED` - Not connected to any server
- `ERR_AUTH_REQUIRED` - Authentication required
- `ERR_AUTH_FAILED` - Authentication failed
- `ERR_INVALID_COMMAND` - Invalid command
- `ERR_SERVER_NOT_FOUND` - Server not found

---

## Message Types Reference

### Client → Server

| Type | Description | Required Fields |
|------|-------------|-----------------|
| `auth` | Authenticate | `password` |
| `sync_server` | Select server to sync | `server` |
| `command` | Send IRC command/message | `server`, `command` |
| `ping` | Keep-alive ping | - |

### Server → Client

| Type | Description | Typical Fields |
|------|-------------|----------------|
| `auth_ok` | Authentication successful | - |
| `message` | Chat message | `server`, `channel`, `nick`, `text`, `is_own` |
| `server_status` | Server connection status | `server`, `status` |
| `channel_join` | User joined channel | `server`, `channel`, `nick` |
| `channel_part` | User left channel (voluntary) | `server`, `channel`, `nick`, `reason` |
| `channel_kick` | User kicked from channel | `server`, `channel`, `nick`, `kicked_by`, `reason` |
| `user_quit` | User disconnected (all channels) | `server`, `nick`, `reason` |
| `topic` | Topic changed | `server`, `channel`, `topic` |
| `channel_mode` | Channel mode changed | `server`, `channel`, `mode`, `set_by` |
| `nicklist` | Nicklist update | `server`, `channel`, `nick`, `action` |
| `nick_change` | Nick changed | `server`, `old_nick`, `new_nick` |
| `user_mode` | User mode changed | `server`, `mode` |
| `away` | Away status | `server`, `away` |
| `whois` | WHOIS response | `server`, `nick`, `data`, `response_to` |
| `channel_list` | Ban/exception/invite list | `server`, `channel`, `list_type`, `entries`, `response_to` |
| `state_dump` | Initial state dump | `dump_type`, ... |
| `error` | Error message | `message`, `code` |
| `pong` | Ping response | - |

---

## Error Handling

### Connection Errors

If WebSocket connection is lost, client should:

1. Wait 5 seconds
2. Reconnect
3. Re-authenticate (if needed)
4. Re-sync server

### Message Errors

Invalid JSON or unknown message types should be ignored by server (no response).

---

## Examples

### Complete Connection Flow

```
1. Client connects:
   → {"type":"auth", "password":"secret"}
   ← {"type":"auth_ok"}

2. Client syncs server:
   → {"type":"sync_server", "server":"libera"}
   ← {"type":"state_dump", "dump_type":"server", ...}
   ← {"type":"state_dump", "dump_type":"channel", "channel":"#test", ...}
   ← {"type":"state_dump", "dump_type":"nick", "channel":"#test", "nick":"alice", ...}
   ← {"type":"state_dump", "dump_type":"nick", "channel":"#test", "nick":"bob", ...}
   ← {"type":"state_dump", "dump_type":"complete"}

3. Real-time messages:
   ← {"type":"message", "channel":"#test", "nick":"alice", "text":"Hello!", ...}

4. Client sends message:
   → {"type":"command", "server":"libera", "channel":"#test", "command":"Hi alice!"}
   ← {"type":"message", "channel":"#test", "nick":"mynick", "text":"Hi alice!", "is_own":true, ...}

5. Someone joins:
   ← {"type":"channel_join", "channel":"#test", "nick":"charlie", ...}
   ← {"type":"nicklist", "channel":"#test", "nick":"charlie", "action":"join", ...}

6. Someone gets kicked:
   ← {"type":"channel_kick", "channel":"#test", "nick":"spammer", "kicked_by":"alice", "reason":"Flood"}
   ← {"type":"nicklist", "channel":"#test", "nick":"spammer", "action":"kick"}

7. Someone quits IRC:
   ← {"type":"user_quit", "nick":"bob", "reason":"Ping timeout"}
   ← {"type":"nicklist", "channel":"#test", "nick":"bob", "action":"quit"}
   ← {"type":"nicklist", "channel":"#dev", "nick":"bob", "action":"quit"}
   (one nicklist update per channel bob was on)

8. Channel mode changed:
   ← {"type":"channel_mode", "channel":"#test", "mode":"+m", "set_by":"alice"}

9. Channel mode with parameter:
   ← {"type":"channel_mode", "channel":"#test", "mode":"+k", "mode_param":"secret123", "set_by":"alice"}
```

### Sending Commands

**Join channel:**
```json
→ {"type":"command", "server":"libera", "command":"/join #newchannel"}
```

**Send private message:**
```json
→ {"type":"command", "server":"libera", "channel":"alice", "command":"Hey, how are you?"}
```

**Change nick:**
```json
→ {"type":"command", "server":"libera", "command":"/nick newnick"}
```

### Operator Commands

**Give OP:**
```json
→ {"type":"command", "server":"libera", "command":"/mode #test +o alice"}
```

**Take OP:**
```json
→ {"type":"command", "server":"libera", "command":"/mode #test -o bob"}
```

**Give Voice:**
```json
→ {"type":"command", "server":"libera", "command":"/mode #test +v charlie"}
```

**Ban user:**
```json
→ {"type":"command", "server":"libera", "command":"/mode #test +b *!*@spammer.com"}
```

**Kick user:**
```json
→ {"type":"command", "server":"libera", "command":"/kick #test spammer Flood"}
```

### Query Commands with Response Tracking

**WHOIS query:**
```json
→ {"id":"req-001", "type":"command", "server":"libera", "command":"/whois alice alice"}

← {"type":"whois", "response_to":"req-001", "server":"libera", "nick":"alice",
   "data":{"user":"~alice", "host":"user.example.com", "realname":"Alice",
   "channels":["@#test", "+#dev"], "idle":300, "account":"alice_acc", ...}}
```

**Get ban list:**
```json
→ {"id":"req-002", "type":"command", "server":"libera", "command":"/mode #test +b"}

← {"type":"channel_list", "response_to":"req-002", "server":"libera",
   "channel":"#test", "list_type":"ban",
   "entries":[{"mask":"*!*@spammer.com", "set_by":"alice", "set_at":1737820000}, ...]}
```

**Get exception list:**
```json
→ {"id":"req-003", "type":"command", "server":"libera", "command":"/mode #test +e"}

← {"type":"channel_list", "response_to":"req-003", "server":"libera",
   "channel":"#test", "list_type":"exception", "entries":[...]}
```

**Get invite exception list:**
```json
→ {"id":"req-004", "type":"command", "server":"libera", "command":"/mode #test +I"}

← {"type":"channel_list", "response_to":"req-004", "server":"libera",
   "channel":"#test", "list_type":"invite", "entries":[...]}
```

---

## Quick Reference: Full-Featured IRC Commands

### Basic Operations

| Action | Command | Example JSON |
|--------|---------|--------------|
| Join channel | `/join #channel` | `{"type":"command","server":"libera","command":"/join #test"}` |
| Part channel | `/part #channel` | `{"type":"command","server":"libera","command":"/part #test"}` |
| Send message | `text` | `{"type":"command","server":"libera","channel":"#test","command":"Hello!"}` |
| Private message | `/msg nick text` | `{"type":"command","server":"libera","command":"/msg alice Hello"}` |
| Change nick | `/nick newnick` | `{"type":"command","server":"libera","command":"/nick alice2"}` |
| Set topic | `/topic #ch text` | `{"type":"command","server":"libera","command":"/topic #test New topic"}` |

### Operator Commands

| Action | Command | Example JSON |
|--------|---------|--------------|
| Give OP | `/mode #ch +o nick` | `{"type":"command","server":"libera","command":"/mode #test +o alice"}` |
| Take OP | `/mode #ch -o nick` | `{"type":"command","server":"libera","command":"/mode #test -o bob"}` |
| Give Voice | `/mode #ch +v nick` | `{"type":"command","server":"libera","command":"/mode #test +v charlie"}` |
| Take Voice | `/mode #ch -v nick` | `{"type":"command","server":"libera","command":"/mode #test -v charlie"}` |
| Give Halfop | `/mode #ch +h nick` | `{"type":"command","server":"libera","command":"/mode #test +h dave"}` |
| Kick | `/kick #ch nick reason` | `{"type":"command","server":"libera","command":"/kick #test spammer Flood"}` |

### Channel Modes

| Action | Command | Example JSON |
|--------|---------|--------------|
| Moderated | `/mode #ch +m` | `{"type":"command","server":"libera","command":"/mode #test +m"}` |
| Remove moderated | `/mode #ch -m` | `{"type":"command","server":"libera","command":"/mode #test -m"}` |
| Invite only | `/mode #ch +i` | `{"type":"command","server":"libera","command":"/mode #test +i"}` |
| Set key | `/mode #ch +k pass` | `{"type":"command","server":"libera","command":"/mode #test +k secret"}` |
| Remove key | `/mode #ch -k pass` | `{"type":"command","server":"libera","command":"/mode #test -k secret"}` |
| Set limit | `/mode #ch +l N` | `{"type":"command","server":"libera","command":"/mode #test +l 50"}` |
| Topic protection | `/mode #ch +t` | `{"type":"command","server":"libera","command":"/mode #test +t"}` |

### Ban/Exception/Invite Management

| Action | Command | Example JSON |
|--------|---------|--------------|
| Ban user | `/mode #ch +b mask` | `{"type":"command","server":"libera","command":"/mode #test +b *!*@spam.com"}` |
| Unban user | `/mode #ch -b mask` | `{"type":"command","server":"libera","command":"/mode #test -b *!*@spam.com"}` |
| Add exception | `/mode #ch +e mask` | `{"type":"command","server":"libera","command":"/mode #test +e *!*@friend.com"}` |
| Add invite exception | `/mode #ch +I mask` | `{"type":"command","server":"libera","command":"/mode #test +I *!*@trusted.com"}` |
| Quiet user | `/mode #ch +q mask` | `{"type":"command","server":"libera","command":"/mode #test +q *!*@noisy.com"}` |
| **Get ban list** | `/mode #ch +b` | `{"id":"req-1","type":"command","server":"libera","command":"/mode #test +b"}` |
| **Get exception list** | `/mode #ch +e` | `{"id":"req-2","type":"command","server":"libera","command":"/mode #test +e"}` |
| **Get invite list** | `/mode #ch +I` | `{"id":"req-3","type":"command","server":"libera","command":"/mode #test +I"}` |

### Query Commands (with response tracking)

| Action | Command | Example JSON |
|--------|---------|--------------|
| WHOIS | `/whois nick nick` | `{"id":"req-001","type":"command","server":"libera","command":"/whois alice alice"}` |
| WHO | `/who #channel` | `{"id":"req-002","type":"command","server":"libera","command":"/who #test"}` |
| NAMES | `/names #channel` | `{"id":"req-003","type":"command","server":"libera","command":"/names #test"}` |
| LIST | `/list` | `{"id":"req-004","type":"command","server":"libera","command":"/list"}` |

**Note:** Commands marked with `id` will receive structured responses with `response_to` field.

---

## Protocol Version

Current version: **2.0**

Version negotiation (future):
```json
{
    "type": "hello",
    "protocol_version": "2.0",
    "client": "irssi-web-client/1.0"
}
```

---

## Implementation Notes

### Message IDs

Generate unique IDs using:
- UUID v4 (recommended)
- `timestamp-counter` format (e.g., "1737825000-001")

### Timestamps

All timestamps are Unix epoch (seconds), not milliseconds.

### Text Encoding

All text MUST be UTF-8 encoded.

### JSON Escaping

Follow standard JSON escaping rules for strings.

---

## Future Extensions

Possible future additions:

- [ ] File transfer support
- [ ] DCC support
- [ ] History retrieval (scrollback)
- [ ] Server notices formatting
- [ ] Compression (zlib for WebSocket frames)
- [ ] WHO command responses (`who` message type)
- [ ] LIST command responses (`channel_list` message type for `/list`)
- [ ] NAMES full list on demand
- [ ] Server statistics (STATS, LUSERS)

---

**Status**: ✅ Protocol Specification Complete (Full-Featured)
**Version**: 2.0
**Features**:
- ✅ Basic IRC operations (join/part/msg/topic)
- ✅ Operator commands (op/deop/voice/kick)
- ✅ Channel modes (all common modes + parameters)
- ✅ Ban/Exception/Invite list management
- ✅ WHOIS queries with full data
- ✅ Request/Response tracking
- ✅ Real-time events (join/part/kick/quit/mode)
- ✅ Own message support
- ✅ Multi-server support
- ✅ Initial state dump

**Next**: Implement in fe-web module (Phase 1: Signal Refactoring)
**Author**: kofany + Claude
**Date**: 2025-01-25
