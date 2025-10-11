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

**Note:** If `channel` is provided and `command` doesn't start with `/`, it's treated as a message to that channel/nick.

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

User left a channel:

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

### 5. Channel Topic

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

### 6. Nick List

Nicklist update (join/part/mode change):

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
- `"part"` - Nick removed from channel
- `"mode"` - Nick mode changed

---

### 7. Nick Change

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

### 8. User Mode

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

### 9. Away Status

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

### 10. State Dump

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

### 11. Error

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
| `channel_part` | User left channel | `server`, `channel`, `nick` |
| `topic` | Topic changed | `server`, `channel`, `topic` |
| `nicklist` | Nicklist update | `server`, `channel`, `nick`, `action` |
| `nick_change` | Nick changed | `server`, `old_nick`, `new_nick` |
| `user_mode` | User mode changed | `server`, `mode` |
| `away` | Away status | `server`, `away` |
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
- [ ] Channel modes detailed info
- [ ] WHOIS responses
- [ ] Server notices formatting
- [ ] Compression (zlib for WebSocket frames)

---

**Status**: ✅ Protocol Draft Complete
**Next**: Implement in fe-web module
**Author**: kofany + Claude
**Date**: 2025-01-11
