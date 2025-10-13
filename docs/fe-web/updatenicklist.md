# Nicklist Update System (Delta Updates)

**Date:** 2025-10-13
**Purpose:** Optimize network traffic by sending delta updates instead of full nicklists

## Overview

Instead of sending the entire nicklist (which can be 13KB+ for large channels) after every JOIN/PART/KICK/QUIT/MODE event, we now send small delta updates (~150 bytes) that tell the client what changed.

**Traffic Reduction:**
- Full nicklist for #polska (358 users): ~13KB
- Delta update: ~150 bytes
- **Reduction: 99%**

---

## Message Type: `nicklist_update`

### Format

```json
{
  "id": "1760270400-0123",
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "kofany",
  "task": "add",
  "timestamp": 1760270400
}
```

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Message ID (timestamp-counter) |
| `type` | string | Always `"nicklist_update"` |
| `server` | string | Server tag |
| `channel` | string | Channel name |
| `nick` | string | Nickname affected by the change |
| `task` | string | Operation to perform (see below) |
| `timestamp` | number | Unix timestamp |

### Task Values

| Task | Description | When Sent | Action |
|------|-------------|-----------|--------|
| `"add"` | Add user to nicklist | JOIN | Add nick with no modes (normal user) |
| `"remove"` | Remove user from nicklist | PART, KICK, QUIT | Remove nick from nicklist |
| `"change"` | Rename user in nicklist | NICK | Rename user from old nick to new nick |
| `"+o"` | Give operator status | MODE +o | Add @ prefix to user |
| `"-o"` | Remove operator status | MODE -o | Remove @ prefix from user |
| `"+v"` | Give voice status | MODE +v | Add + prefix to user |
| `"-v"` | Remove voice status | MODE -v | Remove + prefix from user |
| `"+h"` | Give halfop status | MODE +h | Add % prefix to user |
| `"-h"` | Remove halfop status | MODE -h | Remove % prefix from user |

---

## Event Flow

### JOIN Event

When a user joins a channel:

**Server → Client:**
```json
{
  "type": "channel_join",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "alice",
  "timestamp": 1760270400
}
```

**Immediately followed by:**
```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "alice",
  "task": "add",
  "timestamp": 1760270400
}
```

**Client Action:**
1. Receive `channel_join` message
2. Display "alice has joined #polska"
3. Receive `nicklist_update` with `task: "add"`
4. Add "alice" to the nicklist with no prefix

---

### PART Event

When a user parts a channel:

**Server → Client:**
```json
{
  "type": "channel_part",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "bob",
  "text": "Leaving",
  "timestamp": 1760270400
}
```

**Immediately followed by:**
```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "bob",
  "task": "remove",
  "timestamp": 1760270400
}
```

**Client Action:**
1. Receive `channel_part` message
2. Display "bob has left #polska (Leaving)"
3. Receive `nicklist_update` with `task: "remove"`
4. Remove "bob" from the nicklist

---

### KICK Event

When a user is kicked from a channel:

**Server → Client:**
```json
{
  "type": "channel_kick",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "charlie",
  "text": "Kicked",
  "extra": {
    "kicker": "admin"
  },
  "timestamp": 1760270400
}
```

**Immediately followed by:**
```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "charlie",
  "task": "remove",
  "timestamp": 1760270400
}
```

**Client Action:**
1. Receive `channel_kick` message
2. Display "charlie was kicked by admin (Kicked)"
3. Receive `nicklist_update` with `task: "remove"`
4. Remove "charlie" from the nicklist

---

### QUIT Event

When a user quits IRC (affects ALL channels):

**Server → Client:**
```json
{
  "type": "user_quit",
  "server": "IRCal",
  "nick": "david",
  "text": "Quit: Connection reset",
  "timestamp": 1760270400
}
```

**Immediately followed by multiple updates (one per channel):**
```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "david",
  "task": "remove",
  "timestamp": 1760270400
}
```

```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#ircnet",
  "nick": "david",
  "task": "remove",
  "timestamp": 1760270400
}
```

**Client Action:**
1. Receive `user_quit` message
2. Display "david has quit (Connection reset)" in all channels
3. Receive `nicklist_update` for EACH channel the user was in
4. Remove "david" from ALL affected channel nicklists

---

### NICK Event

When a user changes their nickname (affects ALL channels):

**Server → Client:**
```json
{
  "type": "nick_change",
  "server": "IRCal",
  "nick": "alice",
  "text": "alice_away",
  "timestamp": 1760270400
}
```

**Immediately followed by multiple updates (one per channel):**
```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "alice",
  "task": "change",
  "extra": {
    "new_nick": "alice_away"
  },
  "timestamp": 1760270400
}
```

```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#ircnet",
  "nick": "alice",
  "task": "change",
  "extra": {
    "new_nick": "alice_away"
  },
  "timestamp": 1760270400
}
```

**Client Action:**
1. Receive `nick_change` message
2. Display "alice is now known as alice_away" in all channels
3. Receive `nicklist_update` for EACH channel the user is in
4. Rename "alice" to "alice_away" in ALL affected channel nicklists

**Note:** The `extra.new_nick` field contains the new nickname. The `nick` field contains the old nickname.

---

### MODE Event (User Modes)

When a user's channel modes change:

**Server → Client:**
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "admin",
  "extra": {
    "mode": "+o",
    "params": ["kofany"]
  },
  "timestamp": 1760270400
}
```

**Immediately followed by:**
```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "kofany",
  "task": "+o",
  "timestamp": 1760270400
}
```

**Client Action:**
1. Receive `channel_mode` message
2. Display "admin sets mode +o kofany"
3. Receive `nicklist_update` with `task: "+o"`
4. Add @ prefix to "kofany" in the nicklist

#### Multiple Mode Changes

**Example:** `/MODE #polska +ov-v alice bob charlie`

**Server → Client:**
```json
{
  "type": "channel_mode",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "admin",
  "extra": {
    "mode": "+ov-v",
    "params": ["alice", "bob", "charlie"]
  },
  "timestamp": 1760270400
}
```

**Immediately followed by THREE delta updates:**
```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "alice",
  "task": "+o",
  "timestamp": 1760270400
}
```

```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "bob",
  "task": "+v",
  "timestamp": 1760270400
}
```

```json
{
  "type": "nicklist_update",
  "server": "IRCal",
  "channel": "#polska",
  "nick": "charlie",
  "task": "-v",
  "timestamp": 1760270400
}
```

**Client Action:**
1. Receive `channel_mode` message
2. Display "admin sets mode +ov-v alice bob charlie"
3. Receive three `nicklist_update` messages
4. Add @ to alice, add + to bob, remove + from charlie

---

## NAMES Command

The NAMES command allows clients to request a fresh nicklist refresh on demand.

### Request Format

**Client → Server:**
```json
{
  "type": "names",
  "server": "IRCal",
  "channel": "#polska"
}
```

### Response Format

**Server → Client:**

The server executes `/NAMES #polska` in IRC and immediately sends the full nicklist:

```json
{
  "type": "nicklist",
  "server": "IRCal",
  "channel": "#polska",
  "text": "[{\"nick\":\"kofany\",\"prefix\":\"@\"},{\"nick\":\"alice\",\"prefix\":\"@\"},{\"nick\":\"bob\",\"prefix\":\"+\"},{\"nick\":\"charlie\",\"prefix\":\"\"}]",
  "timestamp": 1760270400
}
```

### Behavior

1. Client sends `names` request with server and channel
2. Server executes physical IRC `NAMES` command to refresh irssi's internal state
3. Server immediately sends the current nicklist from irssi's memory
4. irssi tracks nicklist state automatically, so we trust its internal state

### Use Cases

- Client reconnects and wants to refresh nicklist
- Client suspects nicklist is out of sync
- Client implements "Refresh Names" button in UI
- Client wants to update stale nicklist without rejoining channel

---

## Client Implementation Guide

### Handling `nicklist_update`

```javascript
function handleNicklistUpdate(msg) {
  const channel = findChannel(msg.server, msg.channel);
  if (!channel) {
    console.error('Channel not found:', msg.channel);
    return;
  }

  switch (msg.task) {
    case 'add':
      // Add user with no modes
      channel.addUser({ nick: msg.nick, prefix: '' });
      break;

    case 'remove':
      // Remove user from nicklist
      channel.removeUser(msg.nick);
      break;

    case 'change':
      // Nick change - rename user
      if (msg.extra && msg.extra.new_nick) {
        channel.renameUser(msg.nick, msg.extra.new_nick);
      } else {
        console.error('Nick change missing new_nick in extra data');
      }
      break;

    case '+o':
      // Add operator status
      channel.setUserMode(msg.nick, 'op', true);
      break;

    case '-o':
      // Remove operator status
      channel.setUserMode(msg.nick, 'op', false);
      break;

    case '+v':
      // Add voice status
      channel.setUserMode(msg.nick, 'voice', true);
      break;

    case '-v':
      // Remove voice status
      channel.setUserMode(msg.nick, 'voice', false);
      break;

    case '+h':
      // Add halfop status
      channel.setUserMode(msg.nick, 'halfop', true);
      break;

    case '-h':
      // Remove halfop status
      channel.setUserMode(msg.nick, 'halfop', false);
      break;

    default:
      console.warn('Unknown task:', msg.task);
  }

  // Re-sort nicklist (op → voice → normal)
  channel.sortUsers();
}
```

### Requesting NAMES

```javascript
function refreshNicklist(server, channel) {
  const message = {
    type: 'names',
    server: server,
    channel: channel
  };

  ws.send(JSON.stringify(message));
}
```

### Expected Response

After sending `names` request, expect a `nicklist` message (NOT `nicklist_update`) with the full list:

```javascript
case 'nicklist':
  const channel = findChannel(msg.server, msg.channel);
  if (!channel) {
    console.error('Channel not found:', msg.channel);
    return;
  }

  // Parse JSON array from text field
  const users = JSON.parse(msg.text);

  // Clear old nicklist
  channel.clearUsers();

  // Add all users
  users.forEach(user => {
    channel.addUser({
      nick: user.nick,
      prefix: user.prefix,
      mode: prefixToMode(user.prefix)
    });
  });

  // Sort nicklist
  channel.sortUsers();
  break;
```

---

## Prefix Mapping

The prefix field in nicklist indicates user status:

| Prefix | Mode | Description |
|--------|------|-------------|
| `@` | op | Operator (channel admin) |
| `%` | halfop | Half-operator (partial admin) |
| `+` | voice | Voice (can speak in moderated channel) |
| `` | normal | Normal user (no special status) |

**Note:** Multiple modes can stack (e.g., `@%` for op+halfop), but this is rare.

---

## Debugging

### Check if Delta Updates Are Being Sent

**In irssi:**
```
/fe_web status
```

Watch for messages like:
```
fe-web: Sending nicklist_update: {"type":"nicklist_update","server":"IRCal","channel":"#polska","nick":"alice","task":"add"}
```

### Verify NAMES Command

**In irssi:**
1. Watch for NAMES request:
```
fe-web: [uuid] Received NAMES request: channel=#polska server=IRCal
fe-web: [uuid] Executing: /NAMES #polska
```

2. Verify nicklist is sent:
```
fe-web: [uuid] Sent nicklist for #polska
```

### Common Issues

**❌ No nicklist_update after JOIN:**
- Check if `fe_web_send_nicklist_update()` is called in `sig_message_join`

**❌ MODE changes send full nicklist instead of delta:**
- Check if `sig_message_irc_mode` sends delta updates
- Verify `sig_nick_mode_changed` is NOT sending full nicklist

**❌ NAMES returns empty nicklist:**
- Verify channel exists in irssi
- Check if user is actually on the channel
- Confirm irssi's internal state is correct

---

## Migration Notes

### For The Lounge Client

**Old Behavior:**
- Received full `nicklist` message after every JOIN/PART/KICK/QUIT/MODE

**New Behavior:**
- Receives `nicklist_update` with `task` field
- Full `nicklist` only sent during:
  - Initial sync (`sync_server`)
  - State dump (when YOU join a channel)
  - Manual NAMES request

**Required Changes:**
1. Add handler for `nicklist_update` message type
2. Parse `task` field and apply delta changes
3. Implement NAMES request functionality
4. Update existing `nicklist` handler to support both initial sync and NAMES responses

---

## Performance Impact

### Before (Full Nicklist)

**Example: #polska with 358 users**
- Every JOIN: ~13KB
- Every PART: ~13KB
- Every KICK: ~13KB
- Every QUIT: ~13KB × number of channels
- Every MODE: ~13KB

**Total for 100 events:** ~1.3 MB

### After (Delta Updates)

**Example: #polska with 358 users**
- Every JOIN: ~150 bytes
- Every PART: ~150 bytes
- Every KICK: ~150 bytes
- Every QUIT: ~150 bytes × number of channels
- Every MODE: ~150 bytes per mode change

**Total for 100 events:** ~15 KB

**🎯 Result: 99% traffic reduction**

---

## Summary

- **`nicklist_update`**: Lightweight delta updates for JOIN/PART/KICK/QUIT/MODE events
- **`task` field**: Tells client what operation to perform (`add`, `remove`, `+o`, `-o`, `+v`, `-v`, `+h`, `-h`)
- **`names` command**: Allows clients to request full nicklist refresh on demand
- **Traffic reduction**: 99% for large channels
- **Backwards compatible**: Full `nicklist` still sent during initial sync

**Implementation Status:** ✅ Complete
