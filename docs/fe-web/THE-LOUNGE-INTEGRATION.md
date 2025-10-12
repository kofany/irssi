# The Lounge Integration Guide for fe-web

## Overview

This document provides instructions for integrating The Lounge IRC client with irssi's fe-web WebSocket module.

**Target audience**: Developers working on The Lounge integration with fe-web.

---

## Quick Start

### 1. Server Setup (irssi)

```
/LOAD fe-web
/SET fe_web_enabled ON
/SET fe_web_port 9001
/SET fe_web_bind 127.0.0.1
/SET fe_web_password yourpassword
/SET fe_web_ssl ON
/SAVE
```

### 2. Client Connection (The Lounge)

**WebSocket URL format:**
```
wss://127.0.0.1:9001/?password=yourpassword
```

**For plain connection (no SSL):**
```
ws://127.0.0.1:9001/?password=yourpassword
```

---

## SSL/TLS Implementation

### What Changed in Version 1.2

fe-web now supports **optional SSL/TLS encryption** (wss://) with auto-generated self-signed certificates.

**Key points:**
- SSL is **optional** - controlled by `/SET fe_web_ssl ON/OFF` in irssi
- Certificates are **auto-generated** at startup (no manual certificate management)
- Certificate is **self-signed** - clients must accept/ignore certificate errors
- **Same WebSocket protocol** - only transport layer changes (TLS vs plain TCP)

### Implementation in The Lounge

#### Option 1: Node.js WebSocket Client (Recommended)

If The Lounge uses Node.js `ws` library:

```javascript
const WebSocket = require('ws');

// Configuration
const config = {
    host: '127.0.0.1',
    port: 9001,
    password: 'yourpassword',
    ssl: true  // User preference
};

// Build URL
const protocol = config.ssl ? 'wss' : 'ws';
const url = `${protocol}://${config.host}:${config.port}/?password=${encodeURIComponent(config.password)}`;

// WebSocket options for SSL
const wsOptions = config.ssl ? {
    rejectUnauthorized: false  // Accept self-signed certificates
} : {};

// Connect
const ws = new WebSocket(url, wsOptions);

ws.on('open', () => {
    console.log('Connected to irssi fe-web');
});

ws.on('message', (data) => {
    const msg = JSON.parse(data);
    handleMessage(msg);
});

ws.on('error', (error) => {
    console.error('WebSocket error:', error);
});

ws.on('close', (code, reason) => {
    console.log(`Connection closed: ${code} - ${reason}`);
});
```

#### Option 2: Browser WebSocket API

If The Lounge runs in browser:

```javascript
// Browser WebSocket API
const config = {
    host: '127.0.0.1',
    port: 9001,
    password: 'yourpassword',
    ssl: true
};

const protocol = config.ssl ? 'wss' : 'ws';
const url = `${protocol}://${config.host}:${config.port}/?password=${encodeURIComponent(config.password)}`;

const ws = new WebSocket(url);

ws.onopen = () => {
    console.log('Connected to irssi fe-web');
};

ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    handleMessage(msg);
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
};

ws.onclose = (event) => {
    console.log(`Connection closed: ${event.code}`);
};
```

**Note**: Browser will show security warning for self-signed certificates. User must manually accept the certificate.

### User Configuration

Add SSL toggle to The Lounge settings:

```javascript
// Example configuration schema
{
    "irssi_fe_web": {
        "enabled": true,
        "host": "127.0.0.1",
        "port": 9001,
        "password": "yourpassword",
        "ssl": true,  // NEW: SSL/TLS toggle
        "ssl_verify": false  // NEW: Certificate verification (false for self-signed)
    }
}
```

### UI Considerations

**Connection settings UI:**
```
┌─────────────────────────────────────┐
│ irssi fe-web Connection             │
├─────────────────────────────────────┤
│ Host:     [127.0.0.1            ]   │
│ Port:     [9001                 ]   │
│ Password: [••••••••••••         ]   │
│                                     │
│ ☑ Use SSL/TLS (wss://)              │
│ ☑ Accept self-signed certificates  │
│                                     │
│ [Connect]  [Cancel]                 │
└─────────────────────────────────────┘
```

**Warning message for self-signed certificates:**
```
⚠️ Warning: Self-signed certificate detected

The server is using a self-signed SSL certificate.
This provides encryption but does not verify server identity.

For production use, consider:
- Using a reverse proxy (nginx) with Let's Encrypt
- Using a VPN/SSH tunnel
- Generating a CA-signed certificate

[ ] Don't show this warning again
[Continue]  [Cancel]
```

---

## Protocol Details

### No Changes to WebSocket Protocol

The WebSocket protocol remains **identical** whether using `ws://` or `wss://`:

- Same JSON message format
- Same authentication flow (password in query parameter)
- Same message types (auth_ok, server_add, channel_add, etc.)
- Same client commands (sync_server, send_message, etc.)

**Only difference**: Transport layer encryption (TLS vs plain TCP)

### Authentication Flow

**With SSL (wss://):**
```
1. Client → Server: TLS handshake
2. Client → Server: GET /?password=yourpassword HTTP/1.1 (over TLS)
                     Upgrade: websocket
                     ...
3. Server → Client: HTTP/1.1 101 Switching Protocols (over TLS)
4. Server → Client: {"type": "auth_ok", ...} (over TLS)
```

**Without SSL (ws://):**
```
1. Client → Server: GET /?password=yourpassword HTTP/1.1
                     Upgrade: websocket
                     ...
2. Server → Client: HTTP/1.1 101 Switching Protocols
3. Server → Client: {"type": "auth_ok", ...}
```

---

## Testing

### Test with wscat

```bash
# Install wscat
npm install -g wscat

# Test plain connection
wscat -c "ws://127.0.0.1:9001/?password=yourpassword"

# Test SSL connection (ignore certificate)
wscat -c "wss://127.0.0.1:9001/?password=yourpassword" --no-check
```

### Test with Node.js

```javascript
// test-ssl.js
const WebSocket = require('ws');

async function testConnection(useSSL) {
    const protocol = useSSL ? 'wss' : 'ws';
    const url = `${protocol}://127.0.0.1:9001/?password=yourpassword`;
    
    const options = useSSL ? { rejectUnauthorized: false } : {};
    
    console.log(`Testing ${protocol}:// connection...`);
    
    const ws = new WebSocket(url, options);
    
    ws.on('open', () => {
        console.log('✅ Connected successfully');
    });
    
    ws.on('message', (data) => {
        const msg = JSON.parse(data);
        console.log('📨 Received:', msg.type);
        
        if (msg.type === 'auth_ok') {
            console.log('✅ Authentication successful');
            ws.close();
        }
    });
    
    ws.on('error', (error) => {
        console.error('❌ Error:', error.message);
    });
    
    ws.on('close', () => {
        console.log('Connection closed');
    });
}

// Test both
testConnection(false);  // ws://
setTimeout(() => testConnection(true), 2000);  // wss://
```

Run:
```bash
node test-ssl.js
```

---

## Migration Guide

### From ws:// to wss://

**No code changes required** if you follow this pattern:

```javascript
// Before (hardcoded ws://)
const ws = new WebSocket('ws://127.0.0.1:9001/?password=secret');

// After (configurable protocol)
const protocol = config.ssl ? 'wss' : 'ws';
const url = `${protocol}://${config.host}:${config.port}/?password=${config.password}`;
const options = config.ssl ? { rejectUnauthorized: false } : {};
const ws = new WebSocket(url, options);
```

### Backward Compatibility

The Lounge should support **both** `ws://` and `wss://`:

- Default to `ws://` for backward compatibility
- Allow user to enable `wss://` in settings
- Auto-detect SSL support (try `wss://`, fallback to `ws://`)

**Auto-detection example:**
```javascript
async function connectWithFallback(config) {
    // Try SSL first
    if (config.ssl) {
        try {
            return await connectSSL(config);
        } catch (error) {
            console.warn('SSL connection failed, falling back to plain ws://');
        }
    }
    
    // Fallback to plain
    return await connectPlain(config);
}
```

---

## Security Recommendations

### For Users

**Development/Testing (localhost):**
- ✅ `ws://` is acceptable
- ✅ `wss://` with self-signed cert is acceptable

**Production (remote server):**
- ❌ **Never use `ws://`** over internet
- ✅ Use reverse proxy (nginx/caddy) with Let's Encrypt
- ✅ Or use VPN/SSH tunnel

### For The Lounge Developers

**Display warnings:**
- Warn when using `ws://` with non-localhost host
- Warn when using self-signed certificates over internet
- Suggest reverse proxy for production

**Example warning:**
```javascript
if (!config.ssl && config.host !== 'localhost' && config.host !== '127.0.0.1') {
    console.warn('⚠️ WARNING: Using unencrypted connection (ws://) to remote host!');
    console.warn('   Password and all data will be sent in plain text.');
    console.warn('   Enable SSL or use a reverse proxy.');
}
```

---

## Complete Reference

For complete protocol specification, see:
- **CLIENT-SPEC.md** - Full WebSocket protocol documentation
- **MESSAGE_FORMATS.md** - All JSON message types (if available)
- **AUTHENTICATION.md** - Authentication details (if available)

---

## Support

For questions or issues:
- GitHub: https://github.com/kofany/irssi
- IRC: #irssi on Libera.Chat

---

**Last updated**: 2025-10-12 (Version 1.2 - SSL/TLS support)

