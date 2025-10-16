# Network/Server Management Implementation Guide

## Overview

This document provides implementation details for the network/server management feature in irssi fe-web. It complements the main specification document with C code examples and integration notes.

## Files Modified/Created

### New Files
1. **`/Users/kfn/irssi/docs/fe-web/NETWORK-SERVER-MANAGEMENT-SPEC.md`**
   - Complete API specification
   - JSON message schemas
   - Integration guide for The Lounge

2. **`/Users/kfn/irssi/src/fe-web/fe-web-netserver.c`**
   - Network/server management request handlers
   - Business logic for add/remove/list operations

### Modified Files
1. **`/Users/kfn/irssi/src/fe-web/fe-web.h`**
   - Added new message type enums (WEB_MSG_NETWORK_LIST, etc.)
   - Added function declarations for handlers
   - Added includes for chatnet and server-setup headers

2. **`/Users/kfn/irssi/src/fe-web/fe-web-json.c`**
   - Added `fe_web_build_network_json()` - builds JSON for IRC_CHATNET_REC
   - Added `fe_web_build_server_json()` - builds JSON for IRC_SERVER_SETUP_REC
   - Added `fe_web_build_command_result_json()` - builds success/error responses

3. **`/Users/kfn/irssi/src/fe-web/fe-web-client.c`**
   - Updated `fe_web_client_handle_message()` to route new message types
   - Added message type dispatching for network/server operations

4. **`/Users/kfn/irssi/src/fe-web/meson.build`**
   - Added fe-web-netserver.c to build

## Implementation Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    fe-web-client.c                          │
│  fe_web_client_handle_message()                             │
│    ├─> Parses incoming JSON                                 │
│    ├─> Routes by message type                               │
│    └─> Dispatches to handler functions                      │
└────────────┬────────────────────────────────────────────────┘
             │
             ├──> network_list ────────┐
             ├──> server_list ─────────┤
             ├──> network_add ─────────┤
             ├──> network_remove ──────┤
             ├──> server_add ──────────┤
             └──> server_remove ───────┤
                                       │
┌──────────────────────────────────────▼──────────────────────┐
│                  fe-web-netserver.c                          │
│  Handler Functions:                                          │
│    ├─> fe_web_handle_network_list()                         │
│    ├─> fe_web_handle_server_list()                          │
│    ├─> fe_web_handle_network_add()                          │
│    ├─> fe_web_handle_network_remove()                       │
│    ├─> fe_web_handle_server_add()                           │
│    └─> fe_web_handle_server_remove()                        │
└────────────┬────────────────────────────────────────────────┘
             │
             ├──> Uses: chatnets (global GSList)
             ├──> Uses: setupservers (global GSList)
             ├──> Calls: ircnet_create(), chatnet_remove()
             ├──> Calls: server_setup_add(), server_setup_remove()
             └──> Emits: signal("save config")
             
┌─────────────────────────────────────────────────────────────┐
│                    fe-web-json.c                            │
│  JSON Building Functions:                                    │
│    ├─> fe_web_build_network_json()                          │
│    ├─> fe_web_build_server_json()                           │
│    └─> fe_web_build_command_result_json()                   │
└─────────────────────────────────────────────────────────────┘
```

## Key Implementation Details

### 1. Direct API Access (No Text Parsing!)

The implementation uses irssi's internal C APIs directly:

```c
// Networks are stored in global list
extern GSList *chatnets;

// Iterate over networks
for (tmp = chatnets; tmp != NULL; tmp = tmp->next) {
    IRC_CHATNET_REC *rec = IRC_CHATNET(tmp->data);
    // ... build JSON
}

// Find network by name
IRC_CHATNET_REC *rec = irc_chatnet_find("Libera.Chat");

// Create/update network
ircnet_create(rec);  // Also saves to config

// Remove network
chatnet_remove(CHATNET(rec));
```

```c
// Servers are stored in global list
extern GSList *setupservers;

// Find server
SERVER_SETUP_REC *rec = server_setup_find(address, port, chatnet);

// Add server
server_setup_add(SERVER_SETUP(rec));

// Remove server
server_setup_remove(rec);
```

### 2. Password Security

Passwords are **never** sent in responses:

```c
/* In fe_web_build_network_json() */
if (rec->sasl_password != NULL) {
    g_string_append(json, "\"sasl_password\":\"***\",");
} else {
    g_string_append(json, "\"sasl_password\":null,");
}

/* In fe_web_build_server_json() */
if (rec->password != NULL) {
    g_string_append(json, "\"password\":\"***\",");
} else {
    g_string_append(json, "\"password\":null,");
}
```

When receiving passwords from client:
```c
char *sasl_password = fe_web_json_get_string(json_str, "sasl_password");
if (sasl_password != NULL && g_strcmp0(sasl_password, "***") != 0) {
    // Only update if not the masked placeholder
    g_free_not_null(rec->sasl_password);
    rec->sasl_password = g_strdup(sasl_password);
}
```

### 3. Auto-Save Configuration

After each successful operation:

```c
/* Auto-save configuration to disk */
signal_emit("save config", 0);
```

This ensures changes persist to `~/.irssi/config` immediately.

### 4. Memory Management

All string allocations use GLib:

```c
// Allocate
char *str = g_strdup("value");
IRC_CHATNET_REC *rec = g_new0(IRC_CHATNET_REC, 1);

// Free
g_free(str);
g_free_not_null(rec->nick);  // Safe if NULL

// GString management
GString *json = g_string_new("{");
g_string_append(json, "data");
g_string_free(json, TRUE);  // TRUE = free character data too
```

### 5. JSON Building Pattern

All JSON is built manually (no external JSON library):

```c
GString *json = g_string_new("{");

// String field (with escaping)
g_string_append_printf(json, "\"name\":\"%s\",", 
                      fe_web_escape_json(rec->name));

// Null field
g_string_append(json, "\"alternate_nick\":null,");

// Boolean field
g_string_append_printf(json, "\"autoconnect\":%s,", 
                      rec->autoconnect ? "true" : "false");

// Numeric field
g_string_append_printf(json, "\"port\":%d,", rec->port);

g_string_append(json, "}");
```

### 6. WebSocket Message Sending

All responses are sent as WebSocket frames:

```c
/* Create WebSocket frame */
guchar *frame;
gsize frame_len;
frame = fe_web_websocket_create_frame(0x01,  /* text frame */
                                     (const guchar *)json->str,
                                     json->len, &frame_len);

/* Send via network buffer */
if (frame != NULL) {
    net_sendbuffer_send(client->handle, frame, frame_len);
    g_free(frame);
    client->messages_sent++;
}
```

### 7. Error Handling Pattern

Consistent error responses:

```c
if (name == NULL || *name == '\0') {
    send_command_result(client, request_id, FALSE,
                       "Network name is required", 
                       "MISSING_REQUIRED_FIELD");
    g_free(name);
    g_free(request_id);
    return;
}

// On success
char *msg = g_strdup_printf("Network '%s' added successfully", name);
send_command_result(client, request_id, TRUE, msg, NULL);
g_free(msg);
```

## Compilation

Build irssi with meson:

```bash
cd /Users/kfn/irssi
meson setup build
cd build
meson compile
```

The fe-web module will be compiled and installed to the modules directory.

## Testing

### Manual Testing with WebSocket Client

Use a WebSocket client (e.g., `wscat`) to test:

```bash
# Install wscat
npm install -g wscat

# Connect to irssi fe-web
wscat -c ws://localhost:8080

# Send network list request
{"type":"network_list","id":"test-123"}

# Expected response:
{
  "type":"network_list_response",
  "id":"msg-456",
  "response_to":"test-123",
  "timestamp":1697385600,
  "networks":[...]
}
```

### Testing with The Lounge

1. Configure The Lounge to connect to irssi fe-web
2. Implement network/server management UI
3. Send requests as documented in NETWORK-SERVER-MANAGEMENT-SPEC.md
4. Verify responses and configuration persistence

## Debugging

Enable debug logging in irssi:

```irssi
/SET debug_level ALL
/SET -clear
```

fe-web logs to irssi using:

```c
printtext(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
          "fe-web: [%s] Handling network_list request", client->id);
```

Check logs in irssi's status window or log files.

## Known Limitations

### 1. Simplified JSON Parsing

Current implementation uses simple string parsing for JSON. For production:
- Consider using a proper JSON library (e.g., json-glib)
- Current parser doesn't handle nested objects fully
- Doesn't validate JSON structure deeply

### 2. No Nested Object Parsing

The `network_add` request expects fields at root level:
```json
{"type":"network_add","id":"123","name":"Net","nick":"user"}
```

Instead of nested:
```json
{"type":"network_add","id":"123","network":{"name":"Net","nick":"user"}}
```

**TODO**: Implement proper nested object extraction.

### 3. Boolean Parsing

Current implementation uses `fe_web_json_get_int()` for booleans:
```c
rec->autoconnect = fe_web_json_get_int(json_str, "autoconnect", 0);
```

**TODO**: Add `fe_web_json_get_bool()` for proper boolean handling.

### 4. No Request Validation Schema

Currently validates fields individually. Consider adding:
- JSON schema validation
- Required field checking
- Type validation
- Range validation for numeric fields

## Future Enhancements

### 1. Batch Operations

Allow adding multiple networks/servers in one request:
```json
{
  "type": "network_add_batch",
  "networks": [...]
}
```

### 2. Network/Server Querying by Criteria

```json
{
  "type": "server_list",
  "filter": {
    "chatnet": "Libera.Chat",
    "use_tls": true,
    "autoconnect": true
  }
}
```

### 3. Real-time Configuration Updates

Broadcast configuration changes to all connected clients:
```json
{
  "type": "config_changed",
  "change_type": "network_added",
  "network": {...}
}
```

### 4. Validation Before Save

Add pre-save validation:
- Check if network name is unique
- Validate SASL mechanism is supported
- Verify TLS certificate paths exist
- Test server connectivity (optional)

### 5. Import/Export Configuration

```json
{
  "type": "config_export",
  "format": "json"
}

{
  "type": "config_import",
  "config": {...},
  "merge_mode": "replace" | "merge"
}
```

## Integration with The Lounge

### Recommended UI Flow

1. **Network Management Screen**
   - List all networks (query on page load)
   - Add/Edit/Delete buttons
   - Form for network properties

2. **Server Management Screen**
   - List servers grouped by network
   - Add/Edit/Delete buttons
   - Form for server properties

3. **Auto-sync**
   - Query network/server list on WebSocket connect
   - Cache in Node.js memory
   - Refresh on configuration changes

### Example React Component Structure

```javascript
// NetworkList.jsx
function NetworkList() {
  const [networks, setNetworks] = useState([]);
  
  useEffect(() => {
    // Query networks on mount
    const request = {
      type: 'network_list',
      id: generateUUID()
    };
    websocket.send(JSON.stringify(request));
    
    // Handle response
    websocket.on('message', handleNetworkListResponse);
  }, []);
  
  return (
    <div>
      {networks.map(net => (
        <NetworkCard key={net.name} network={net} />
      ))}
      <AddNetworkButton />
    </div>
  );
}
```

## Security Considerations

### 1. Authentication

Ensure WebSocket client is authenticated before allowing network/server modifications:

```c
void fe_web_handle_network_add(WEB_CLIENT_REC *client, const char *json_str)
{
    if (!client->authenticated) {
        send_command_result(client, request_id, FALSE,
                           "Authentication required", "UNAUTHORIZED");
        return;
    }
    // ... proceed with operation
}
```

### 2. Input Sanitization

All user input is escaped before use:

```c
char *escaped = fe_web_escape_json(user_input);
g_string_append_printf(json, "\"field\":\"%s\"", escaped);
g_free(escaped);
```

### 3. Configuration File Protection

Ensure `~/.irssi/config` has proper permissions:
```bash
chmod 600 ~/.irssi/config
```

### 4. TLS for WebSocket

Use TLS for WebSocket connection in production:
- Protects passwords in transit
- Prevents MITM attacks
- Required for public deployments

## Performance Considerations

### 1. Network List Caching

For large network lists, consider caching:
- Cache in Node.js (The Lounge)
- Invalidate on configuration changes
- Reduces load on irssi

### 2. Bulk Operations

Send multiple operations in batch when possible:
- Import configuration = multiple network_add + server_add
- Can be slow if done one-by-one
- Consider transaction-like API

### 3. Config Save Frequency

Auto-save after each operation may be I/O intensive:
- Consider batching saves
- Debounce save operations
- Save only after batch completion

## Troubleshooting

### Issue: Networks not showing up

**Check:**
1. Is chatnets list populated? `/NETWORK LIST` in irssi
2. Are networks IRC type? (filters non-IRC networks)
3. Check irssi debug logs

### Issue: Password changes not persisting

**Check:**
1. Is config file writable?
2. Was `/SAVE` called?
3. Check config file after operation

### Issue: JSON parsing errors

**Check:**
1. Is JSON valid? Use JSON validator
2. Are fields at correct nesting level?
3. Are strings properly escaped?

### Issue: Memory leaks

**Check:**
1. All `g_strdup()` have matching `g_free()`
2. GString freed with `g_string_free(str, TRUE)`
3. Run with valgrind: `valgrind --leak-check=full irssi`

## References

- Main Spec: `/Users/kfn/irssi/docs/fe-web/NETWORK-SERVER-MANAGEMENT-SPEC.md`
- irssi Source: `/Users/kfn/irssi/src/`
- WebSocket RFC: https://tools.ietf.org/html/rfc6455
- The Lounge: https://thelounge.chat/

---

**Implementation Status**: Complete ✅

All handler functions implemented. Ready for testing and integration with The Lounge.
