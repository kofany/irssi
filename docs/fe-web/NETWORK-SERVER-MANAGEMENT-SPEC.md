# Network/Server Management WebSocket API Specification

## Overview

This document specifies the WebSocket message protocol for managing IRC networks and servers in irssi fe-web. It provides structured JSON-based communication between The Lounge (Node.js) and irssi, eliminating the need for plain text parsing.

## Architecture

```
┌─────────────┐                    ┌──────────────┐                 ┌─────────────┐
│             │   WebSocket (JSON) │              │   Direct API    │             │
│ The Lounge  │ ◄─────────────────►│  irssi       │ ◄──────────────►│   irssi     │
│  (Node.js)  │                    │   fe-web     │      calls      │    core     │
│             │                    │              │                 │             │
└─────────────┘                    └──────────────┘                 └─────────────┘
     Client                            Bridge                         IRC Engine
```

### Data Flow

1. **Query Networks**: Node.js → `network_list` → irssi fe-web → reads `chatnets` global → `network_list_response` → Node.js
2. **Query Servers**: Node.js → `server_list` → irssi fe-web → reads `setupservers` global → `server_list_response` → Node.js
3. **Add Network**: Node.js → `network_add` → irssi fe-web → calls `ircnet_create()` → `command_result` → Node.js
4. **Remove Network**: Node.js → `network_remove` → irssi fe-web → calls `chatnet_remove()` → `command_result` → Node.js
5. **Add Server**: Node.js → `server_add` → irssi fe-web → calls `server_setup_add()` → `command_result` → Node.js
6. **Remove Server**: Node.js → `server_remove` → irssi fe-web → calls `server_setup_remove()` → `command_result` → Node.js

## Message Types

### Enum Values (fe-web.h)

```c
typedef enum {
    // ... existing types ...
    WEB_MSG_NETWORK_LIST = 30,           // Request: list all networks
    WEB_MSG_NETWORK_LIST_RESPONSE = 31,  // Response: network list
    WEB_MSG_SERVER_LIST = 32,            // Request: list all servers
    WEB_MSG_SERVER_LIST_RESPONSE = 33,   // Response: server list
    WEB_MSG_NETWORK_ADD = 34,            // Request: add/modify network
    WEB_MSG_NETWORK_REMOVE = 35,         // Request: remove network
    WEB_MSG_SERVER_ADD = 36,             // Request: add/modify server
    WEB_MSG_SERVER_REMOVE = 37,          // Request: remove server
    WEB_MSG_COMMAND_RESULT = 38          // Response: operation result
} WEB_MESSAGE_TYPE;
```

## JSON Message Schemas

### 1. network_list (Request)

**Purpose**: Query all configured IRC networks.

**Request (Node.js → irssi):**
```json
{
  "type": "network_list",
  "id": "req-uuid-12345"
}
```

**Response (irssi → Node.js):**
```json
{
  "type": "network_list_response",
  "id": "msg-uuid-67890",
  "response_to": "req-uuid-12345",
  "timestamp": 1697385600,
  "networks": [
    {
      "name": "Libera.Chat",
      "chat_type": "IRC",
      "nick": "myuser",
      "alternate_nick": "myuser_",
      "username": "myuser",
      "realname": "My Real Name",
      "own_host": null,
      "autosendcmd": "/msg NickServ identify password",
      "usermode": "+iw",
      "sasl_mechanism": "PLAIN",
      "sasl_username": "myaccount",
      "sasl_password": "***",
      "max_kicks": 4,
      "max_msgs": 3,
      "max_modes": 6,
      "max_whois": 10,
      "max_cmds_at_once": 5,
      "cmd_queue_speed": 2200,
      "max_query_chans": 10
    },
    {
      "name": "OFTC",
      "chat_type": "IRC",
      "nick": "anotherusername",
      "alternate_nick": null,
      "username": null,
      "realname": null,
      "own_host": null,
      "autosendcmd": null,
      "usermode": null,
      "sasl_mechanism": null,
      "sasl_username": null,
      "sasl_password": null,
      "max_kicks": 1,
      "max_msgs": 1,
      "max_modes": 3,
      "max_whois": 1,
      "max_cmds_at_once": 0,
      "cmd_queue_speed": 0,
      "max_query_chans": 0
    }
  ]
}
```

**Field Descriptions:**
- `name`: Network identifier (unique)
- `chat_type`: Protocol type (always "IRC" for now)
- `nick`: Default nickname for this network
- `alternate_nick`: Alternative nickname if primary is taken
- `username`: IRC username (ident)
- `realname`: Real name field
- `own_host`: Bind to specific local address
- `autosendcmd`: Commands to run after connecting
- `usermode`: User mode to set on connect
- `sasl_mechanism`: SASL auth mechanism (PLAIN, EXTERNAL, SCRAM-SHA-256, etc.)
- `sasl_username`: SASL authentication username
- `sasl_password`: SASL password (masked as "***" in responses)
- `max_kicks`: Maximum kicks per command
- `max_msgs`: Maximum messages per command
- `max_modes`: Maximum mode changes per command
- `max_whois`: Maximum WHOIS queries per command
- `max_cmds_at_once`: Max commands before flood protection
- `cmd_queue_speed`: Milliseconds between commands
- `max_query_chans`: Max channels in MODE/WHO sync

**Note**: `null` values indicate the field is not set (uses irssi defaults).

---

### 2. server_list (Request)

**Purpose**: Query all configured IRC servers.

**Request (Node.js → irssi):**
```json
{
  "type": "server_list",
  "id": "req-uuid-23456",
  "network": null  // Optional: filter by network name
}
```

**Response (irssi → Node.js):**
```json
{
  "type": "server_list_response",
  "id": "msg-uuid-78901",
  "response_to": "req-uuid-23456",
  "timestamp": 1697385600,
  "servers": [
    {
      "address": "irc.libera.chat",
      "port": 6697,
      "chatnet": "Libera.Chat",
      "password": null,
      "autoconnect": true,
      "use_tls": true,
      "tls_verify": true,
      "tls_cert": null,
      "tls_pkey": null,
      "tls_pass": null,
      "tls_cafile": null,
      "tls_capath": null,
      "tls_ciphers": null,
      "tls_pinned_cert": null,
      "tls_pinned_pubkey": null,
      "own_host": null,
      "family": 0,
      "max_cmds_at_once": 0,
      "cmd_queue_speed": 0,
      "max_query_chans": 0,
      "starttls": 0,
      "no_cap": false,
      "no_proxy": false,
      "last_failed": false,
      "banned": false,
      "dns_error": false
    },
    {
      "address": "irc.libera.chat",
      "port": 6667,
      "chatnet": "Libera.Chat",
      "password": null,
      "autoconnect": false,
      "use_tls": false,
      "tls_verify": false,
      "tls_cert": null,
      "tls_pkey": null,
      "tls_pass": null,
      "tls_cafile": null,
      "tls_capath": null,
      "tls_ciphers": null,
      "tls_pinned_cert": null,
      "tls_pinned_pubkey": null,
      "own_host": null,
      "family": 0,
      "max_cmds_at_once": 0,
      "cmd_queue_speed": 0,
      "max_query_chans": 0,
      "starttls": 0,
      "no_cap": false,
      "no_proxy": false,
      "last_failed": false,
      "banned": false,
      "dns_error": false
    }
  ]
}
```

**Field Descriptions:**
- `address`: Server hostname or IP address
- `port`: Server port number
- `chatnet`: Associated network name
- `password`: Server password (null if not set, never send actual password in response)
- `autoconnect`: Auto-connect on irssi startup
- `use_tls`: Use TLS/SSL connection
- `tls_verify`: Verify TLS certificate
- `tls_cert`: Client certificate path
- `tls_pkey`: Client private key path
- `tls_pass`: Client key password (masked)
- `tls_cafile`: CA certificate file
- `tls_capath`: CA certificate directory
- `tls_ciphers`: Custom cipher list
- `tls_pinned_cert`: Pinned certificate fingerprint
- `tls_pinned_pubkey`: Pinned public key
- `own_host`: Bind to specific local address
- `family`: Address family (0=default, 2=IPv4, 10=IPv6)
- `max_cmds_at_once`: Override network setting
- `cmd_queue_speed`: Override network setting
- `max_query_chans`: Override network setting
- `starttls`: STARTTLS mode (-1=disallow, 0=notset, 1=enabled)
- `no_cap`: Disable CAP negotiation
- `no_proxy`: Don't use proxy for this server
- `last_failed`: Last connection attempt failed
- `banned`: Banned from this server
- `dns_error`: DNS resolution failed

---

### 3. network_add (Request)

**Purpose**: Add or modify an IRC network configuration.

**Request (Node.js → irssi):**
```json
{
  "type": "network_add",
  "id": "req-uuid-34567",
  "network": {
    "name": "MyNetwork",
    "nick": "mynick",
    "alternate_nick": "mynick_",
    "username": "myuser",
    "realname": "My Real Name",
    "own_host": null,
    "autosendcmd": "/msg NickServ identify mypass",
    "usermode": "+iw",
    "sasl_mechanism": "PLAIN",
    "sasl_username": "myaccount",
    "sasl_password": "secretpass",
    "max_kicks": 4,
    "max_msgs": 3,
    "max_modes": 6,
    "max_whois": 10,
    "max_cmds_at_once": 5,
    "cmd_queue_speed": 2200,
    "max_query_chans": 10
  }
}
```

**Response (irssi → Node.js):**
```json
{
  "type": "command_result",
  "id": "msg-uuid-89012",
  "response_to": "req-uuid-34567",
  "timestamp": 1697385600,
  "success": true,
  "message": "Network 'MyNetwork' added successfully",
  "error_code": null
}
```

**Error Response:**
```json
{
  "type": "command_result",
  "id": "msg-uuid-89013",
  "response_to": "req-uuid-34567",
  "timestamp": 1697385600,
  "success": false,
  "message": "Network 'MyNetwork' already exists. Use modify operation.",
  "error_code": "NETWORK_EXISTS"
}
```

**Notes:**
- Only `name` is required; all other fields are optional
- If network exists, it will be modified (unless you want separate add/modify operations)
- Use `null` or omit fields to keep existing values (on modify) or use defaults (on add)

---

### 4. network_remove (Request)

**Purpose**: Remove an IRC network configuration.

**Request (Node.js → irssi):**
```json
{
  "type": "network_remove",
  "id": "req-uuid-45678",
  "name": "MyNetwork"
}
```

**Response (irssi → Node.js):**
```json
{
  "type": "command_result",
  "id": "msg-uuid-90123",
  "response_to": "req-uuid-45678",
  "timestamp": 1697385600,
  "success": true,
  "message": "Network 'MyNetwork' removed successfully",
  "error_code": null
}
```

**Error Response:**
```json
{
  "type": "command_result",
  "id": "msg-uuid-90124",
  "response_to": "req-uuid-45678",
  "timestamp": 1697385600,
  "success": false,
  "message": "Network 'MyNetwork' not found",
  "error_code": "NETWORK_NOT_FOUND"
}
```

**Notes:**
- Removing a network also removes all associated server and channel setups
- Active connections to servers in this network are NOT disconnected automatically

---

### 5. server_add (Request)

**Purpose**: Add or modify an IRC server configuration.

**Request (Node.js → irssi):**
```json
{
  "type": "server_add",
  "id": "req-uuid-56789",
  "server": {
    "address": "irc.example.com",
    "port": 6697,
    "chatnet": "MyNetwork",
    "password": null,
    "autoconnect": true,
    "use_tls": true,
    "tls_verify": true,
    "tls_cert": null,
    "tls_pkey": null,
    "tls_pass": null,
    "tls_cafile": "/etc/ssl/certs/ca-certificates.crt",
    "tls_capath": null,
    "tls_ciphers": null,
    "tls_pinned_cert": null,
    "tls_pinned_pubkey": null,
    "own_host": null,
    "family": 0,
    "max_cmds_at_once": 0,
    "cmd_queue_speed": 0,
    "max_query_chans": 0,
    "starttls": 0,
    "no_cap": false,
    "no_proxy": false
  }
}
```

**Response (irssi → Node.js):**
```json
{
  "type": "command_result",
  "id": "msg-uuid-01234",
  "response_to": "req-uuid-56789",
  "timestamp": 1697385600,
  "success": true,
  "message": "Server 'irc.example.com:6697' added successfully",
  "error_code": null
}
```

**Error Response:**
```json
{
  "type": "command_result",
  "id": "msg-uuid-01235",
  "response_to": "req-uuid-56789",
  "timestamp": 1697385600,
  "success": false,
  "message": "Network 'MyNetwork' does not exist",
  "error_code": "NETWORK_NOT_FOUND"
}
```

**Notes:**
- `address` and `port` are required
- `chatnet` is optional but recommended
- If server exists (same address + port), it will be modified
- Set numeric values to 0 to use network/global defaults

---

### 6. server_remove (Request)

**Purpose**: Remove an IRC server configuration.

**Request (Node.js → irssi):**
```json
{
  "type": "server_remove",
  "id": "req-uuid-67890",
  "address": "irc.example.com",
  "port": 6697,
  "chatnet": "MyNetwork"
}
```

**Response (irssi → Node.js):**
```json
{
  "type": "command_result",
  "id": "msg-uuid-12345",
  "response_to": "req-uuid-67890",
  "timestamp": 1697385600,
  "success": true,
  "message": "Server 'irc.example.com:6697' removed successfully",
  "error_code": null
}
```

**Error Response:**
```json
{
  "type": "command_result",
  "id": "msg-uuid-12346",
  "response_to": "req-uuid-67890",
  "timestamp": 1697385600,
  "success": false,
  "message": "Server 'irc.example.com:6697' not found",
  "error_code": "SERVER_NOT_FOUND"
}
```

**Notes:**
- `address` and `port` are used to identify the server
- `chatnet` is optional but helps narrow down the match
- Active connections to this server are NOT disconnected automatically

---

## Error Codes

| Code | Description |
|------|-------------|
| `NETWORK_NOT_FOUND` | Requested network does not exist |
| `NETWORK_EXISTS` | Network already exists (on add operation) |
| `SERVER_NOT_FOUND` | Requested server does not exist |
| `SERVER_EXISTS` | Server already exists (on add operation) |
| `INVALID_PARAMETER` | Invalid parameter value |
| `MISSING_REQUIRED_FIELD` | Required field not provided |
| `INTERNAL_ERROR` | Internal irssi error |

---

## Configuration Persistence

All network and server operations modify irssi's in-memory configuration. To persist changes to disk:

**Option 1: Auto-save (recommended)**
- irssi fe-web automatically calls `/SAVE` after each successful operation

**Option 2: Manual save**
- The Lounge sends a command message: `{"type": "command", "command": "SAVE"}`

**Current config file**: `~/.irssi/config`

---

## Implementation Notes

### Security Considerations

1. **Password Masking**: Never send actual passwords in responses
   - Network listing: show "***" for `sasl_password`
   - Server listing: omit `password` field or show "***"
   
2. **Input Validation**: Validate all JSON input
   - Check required fields
   - Validate data types
   - Sanitize strings (prevent injection)

3. **Authentication**: Ensure WebSocket client is authenticated before allowing network/server modifications

### Memory Management

All operations follow irssi's memory management conventions:
- Use `g_new0()`, `g_malloc()`, `g_strdup()` for allocations
- Use `g_free()`, `g_free_not_null()` for deallocation
- Structures are freed by irssi's internal functions (`chatnet_destroy()`, etc.)

### Thread Safety

- All operations are executed in irssi's main thread
- No thread synchronization needed

---

## Integration Guide for The Lounge

### 1. Query Networks on Startup

```javascript
// Send request
const request = {
  type: 'network_list',
  id: generateUUID()
};
websocket.send(JSON.stringify(request));

// Handle response
websocket.on('message', (data) => {
  const msg = JSON.parse(data);
  if (msg.type === 'network_list_response' && msg.response_to === request.id) {
    msg.networks.forEach(network => {
      console.log(`Network: ${network.name}`);
      console.log(`  Nick: ${network.nick}`);
      console.log(`  SASL: ${network.sasl_mechanism || 'disabled'}`);
    });
  }
});
```

### 2. Query Servers for a Network

```javascript
const request = {
  type: 'server_list',
  id: generateUUID(),
  network: 'Libera.Chat'  // Optional filter
};
websocket.send(JSON.stringify(request));
```

### 3. Add a New Network

```javascript
const request = {
  type: 'network_add',
  id: generateUUID(),
  network: {
    name: 'Libera.Chat',
    nick: 'myuser',
    sasl_mechanism: 'PLAIN',
    sasl_username: 'myaccount',
    sasl_password: 'mypassword'
  }
};
websocket.send(JSON.stringify(request));

// Check response
websocket.on('message', (data) => {
  const msg = JSON.parse(data);
  if (msg.type === 'command_result' && msg.response_to === request.id) {
    if (msg.success) {
      console.log('Network added:', msg.message);
    } else {
      console.error('Error:', msg.message, msg.error_code);
    }
  }
});
```

### 4. Add a Server to Network

```javascript
const request = {
  type: 'server_add',
  id: generateUUID(),
  server: {
    address: 'irc.libera.chat',
    port: 6697,
    chatnet: 'Libera.Chat',
    autoconnect: true,
    use_tls: true,
    tls_verify: true
  }
};
websocket.send(JSON.stringify(request));
```

### 5. Remove Network (and all its servers)

```javascript
const request = {
  type: 'network_remove',
  id: generateUUID(),
  name: 'Libera.Chat'
};
websocket.send(JSON.stringify(request));
```

### 6. Remove Specific Server

```javascript
const request = {
  type: 'server_remove',
  id: generateUUID(),
  address: 'irc.libera.chat',
  port: 6697,
  chatnet: 'Libera.Chat'
};
websocket.send(JSON.stringify(request));
```

---

## Testing Scenarios

### Test 1: List Networks
1. Start irssi with existing networks
2. Connect The Lounge
3. Send `network_list` request
4. Verify response contains all networks with correct data

### Test 2: Add Network
1. Send `network_add` with new network
2. Verify `command_result` success=true
3. Send `network_list` to confirm
4. Check `~/.irssi/config` file has new network

### Test 3: Add Server
1. Send `server_add` with new server
2. Verify success response
3. Send `server_list` to confirm
4. Check config file

### Test 4: Remove Network
1. Send `network_remove` for existing network
2. Verify success response
3. Send `network_list` to confirm removal
4. Verify associated servers are also removed

### Test 5: Error Handling
1. Try to remove non-existent network
2. Verify error response with correct error_code
3. Try to add network without required fields
4. Verify validation error

### Test 6: Password Security
1. Add network with SASL password
2. Query network_list
3. Verify password is masked ("***") in response

### Test 7: Config Persistence
1. Add network and server
2. Restart irssi
3. Verify network and server still exist

---

## C Implementation Checklist

- [ ] Add message type enums to `fe-web.h`
- [ ] Implement `fe_web_handle_network_list()` in `fe-web-client.c`
- [ ] Implement `fe_web_handle_server_list()` in `fe-web-client.c`
- [ ] Implement `fe_web_handle_network_add()` in `fe-web-client.c`
- [ ] Implement `fe_web_handle_network_remove()` in `fe-web-client.c`
- [ ] Implement `fe_web_handle_server_add()` in `fe-web-client.c`
- [ ] Implement `fe_web_handle_server_remove()` in `fe-web-client.c`
- [ ] Implement `fe_web_build_network_json()` in `fe-web-json.c`
- [ ] Implement `fe_web_build_server_json()` in `fe-web-json.c`
- [ ] Implement `fe_web_send_command_result()` in `fe-web-json.c`
- [ ] Add message routing in `fe_web_client_handle_message()`
- [ ] Add auto-save after each operation
- [ ] Add input validation for all operations
- [ ] Add comprehensive error handling
- [ ] Test memory management (no leaks)
- [ ] Test with valgrind
- [ ] Document functions with comments
- [ ] Create integration tests

---

## Appendix: irssi Internal API Reference

### Networks (Chatnets)

```c
extern GSList *chatnets;  // Global list of all networks

// Find network by name
CHATNET_REC *chatnet_find(const char *name);

// Create/add network
void chatnet_create(CHATNET_REC *chatnet);

// Remove network (also saves config)
void chatnet_remove(CHATNET_REC *chatnet);

// IRC-specific network creation
void ircnet_create(IRC_CHATNET_REC *rec);

// Find IRC network
#define irc_chatnet_find(name) IRC_CHATNET(chatnet_find(name))
```

### Servers

```c
extern GSList *setupservers;  // Global list of all server setups

// Find server setup
SERVER_SETUP_REC *server_setup_find(const char *address, int port, const char *chatnet);

// Add server
void server_setup_add(SERVER_SETUP_REC *rec);

// Modify server
void server_setup_modify(SERVER_SETUP_REC *rec, int old_port, const char *old_chatnet);

// Remove server
void server_setup_remove(SERVER_SETUP_REC *rec);

// Remove all servers for network
void server_setup_remove_chatnet(const char *chatnet);
```

### Configuration

```c
// Save configuration to disk
signal_emit("save config", 0);
// Or use command:
signal_emit("send command", 3, "SAVE", NULL, NULL);
```

---

**End of Specification**
