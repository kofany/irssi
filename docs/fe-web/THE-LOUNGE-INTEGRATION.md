# The Lounge Integration Guide for fe-web

## Overview

This document provides instructions for integrating The Lounge IRC client with irssi's fe-web WebSocket module.

**Target audience**: Developers working on The Lounge integration with fe-web.

---

## Quick Start

### 1. Server Setup (irssi)

**Option A: Encryption Only (Recommended for browser-based clients)**
```
/LOAD fe-web
/SET fe_web_enabled ON
/SET fe_web_port 9001
/SET fe_web_bind 127.0.0.1
/SET fe_web_password yourpassword
/SET fe_web_ssl OFF
/SET fe_web_encryption ON
/SAVE
```

**Option B: Dual-Layer Security (Recommended for backend/dedicated apps)**
```
/LOAD fe-web
/SET fe_web_enabled ON
/SET fe_web_port 9001
/SET fe_web_bind 127.0.0.1
/SET fe_web_password yourpassword
/SET fe_web_ssl ON
/SET fe_web_encryption ON
/SAVE
```

### 2. Client Connection (The Lounge)

**For encryption only (Option A):**
```
ws://127.0.0.1:9001/?password=yourpassword
```

**For dual-layer security (Option B):**
```
wss://127.0.0.1:9001/?password=yourpassword
```

**Note**: When using wss://, backend must accept self-signed certificates.

---

## Security Options

### Version 1.4 Changes

fe-web now supports **dual-layer security**:
- **Layer 1**: SSL/TLS (wss://) - optional, self-signed certificate
- **Layer 2**: Application-level encryption (AES-256-GCM) - optional, enabled by default

### Configuration Matrix

| SSL | Encryption | Protocol | Use Case |
|-----|------------|----------|----------|
| OFF | ON         | ws://    | **Browser-based clients** (The Lounge web UI) |
| ON  | ON         | wss://   | **Backend/dedicated apps** (The Lounge server) |
| ON  | OFF        | wss://   | Legacy compatibility |
| OFF | OFF        | ws://    | Debug only (NOT SECURE) |

### Why Dual-Layer?

**For browser-based clients:**
- Use encryption only (ws://)
- No certificate warnings
- Zero configuration

**For backend/dedicated apps:**
- Use dual-layer (wss:// + encryption)
- Defense in depth
- Backend can easily accept self-signed certs

**Key points:**
- Encryption is **enabled by default** - controlled by `/SET fe_web_encryption ON/OFF` in irssi
- SSL/TLS is **disabled by default** - controlled by `/SET fe_web_ssl ON/OFF` in irssi
- Password is used for **both authentication and encryption key derivation**
- All messages encrypted with **AES-256-GCM** (authenticated encryption)
- **Binary WebSocket frames** (opcode 0x2) for encrypted data
- **Same WebSocket protocol** - only message payload is encrypted

### Implementation in The Lounge

#### Encryption Helper Class

Create a reusable encryption helper:

```javascript
const crypto = require('crypto').webcrypto || require('crypto');

class IrssiEncryption {
    constructor(password) {
        this.password = password;
        this.key = null;
    }

    async deriveKey() {
        const encoder = new TextEncoder();
        const passwordData = encoder.encode(this.password);

        const keyMaterial = await crypto.subtle.importKey(
            'raw', passwordData, 'PBKDF2', false, ['deriveKey']
        );

        this.key = await crypto.subtle.deriveKey(
            {
                name: 'PBKDF2',
                salt: encoder.encode('irssi-fe-web-v1'),
                iterations: 10000,
                hash: 'SHA-256'
            },
            keyMaterial,
            { name: 'AES-GCM', length: 256 },
            false,
            ['encrypt', 'decrypt']
        );
    }

    async encrypt(plaintext) {
        const iv = crypto.getRandomValues(new Uint8Array(12));
        const encoder = new TextEncoder();

        const ciphertext = await crypto.subtle.encrypt(
            { name: 'AES-GCM', iv: iv },
            this.key,
            encoder.encode(plaintext)
        );

        // Build message: IV + ciphertext (includes tag)
        const message = new Uint8Array(12 + ciphertext.byteLength);
        message.set(iv, 0);
        message.set(new Uint8Array(ciphertext), 12);

        return message;
    }

    async decrypt(data) {
        const dataArray = new Uint8Array(data);
        const iv = dataArray.slice(0, 12);
        const ciphertext = dataArray.slice(12);

        const plaintext = await crypto.subtle.decrypt(
            { name: 'AES-GCM', iv: iv },
            this.key,
            ciphertext
        );

        const decoder = new TextDecoder();
        return decoder.decode(plaintext);
    }
}
```

#### Option 1: Node.js WebSocket Client (Recommended)

If The Lounge uses Node.js `ws` library:

```javascript
const WebSocket = require('ws');

// Configuration
const config = {
    host: '127.0.0.1',
    port: 9001,
    password: 'yourpassword',
    encryption: true  // User preference
};

// Build URL (always ws://)
const url = `ws://${config.host}:${config.port}/?password=${encodeURIComponent(config.password)}`;

// Initialize encryption
let encryption = null;
if (config.encryption) {
    encryption = new IrssiEncryption(config.password);
    await encryption.deriveKey();
}

// Connect
const ws = new WebSocket(url);
ws.binaryType = 'arraybuffer';  // For encrypted messages

ws.on('open', () => {
    console.log('Connected to irssi fe-web');
});

ws.on('message', async (data) => {
    let msg;

    if (config.encryption && data instanceof ArrayBuffer) {
        // Decrypt binary message
        const json = await encryption.decrypt(data);
        msg = JSON.parse(json);
    } else {
        // Plain text message
        msg = JSON.parse(data);
    }

    handleMessage(msg);
});

ws.on('error', (error) => {
    console.error('WebSocket error:', error);
});

ws.on('close', (code, reason) => {
    console.log(`Connection closed: ${code} - ${reason}`);
});

// Send encrypted message
async function sendMessage(obj) {
    const json = JSON.stringify(obj);

    if (config.encryption) {
        const encrypted = await encryption.encrypt(json);
        ws.send(encrypted);
    } else {
        ws.send(json);
    }
}
```

#### Option 2: Browser WebSocket API

If The Lounge runs in browser:

```javascript
// Browser WebSocket API
const config = {
    host: '127.0.0.1',
    port: 9001,
    password: 'yourpassword',
    encryption: true
};

const url = `ws://${config.host}:${config.port}/?password=${encodeURIComponent(config.password)}`;

// Initialize encryption
let encryption = null;
if (config.encryption) {
    encryption = new IrssiEncryption(config.password);
    await encryption.deriveKey();
}

const ws = new WebSocket(url);
ws.binaryType = 'arraybuffer';  // For encrypted messages

ws.onopen = () => {
    console.log('Connected to irssi fe-web');
};

ws.onmessage = async (event) => {
    let msg;

    if (config.encryption && event.data instanceof ArrayBuffer) {
        // Decrypt binary message
        const json = await encryption.decrypt(event.data);
        msg = JSON.parse(json);
    } else {
        // Plain text message
        msg = JSON.parse(event.data);
    }

    handleMessage(msg);
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
};

ws.onclose = (event) => {
    console.log(`Connection closed: ${event.code}`);
};

// Send encrypted message
async function sendMessage(obj) {
    const json = JSON.stringify(obj);

    if (config.encryption) {
        const encrypted = await encryption.encrypt(json);
        ws.send(encrypted);
    } else {
        ws.send(json);
    }
}
```

### User Configuration

Add encryption toggle to The Lounge settings:

```javascript
// Example configuration schema
{
    "irssi_fe_web": {
        "enabled": true,
        "host": "127.0.0.1",
        "port": 9001,
        "password": "yourpassword",
        "encryption": true  // NEW: Application-level encryption (default: true)
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
│ ☑ Use encryption (AES-256-GCM)      │
│   (Recommended - enabled by default)│
│                                     │
│ [Connect]  [Cancel]                 │
└─────────────────────────────────────┘
```

**Connection status indicator:**
```
🔒 Connected (encrypted)  - Encryption enabled
🔓 Connected (plain)      - Encryption disabled (not recommended)
```

---

## Protocol Details

### WebSocket Protocol with Encryption

The WebSocket protocol uses **plain ws://** with **application-level encryption**:

- **Transport**: Plain WebSocket (ws://)
- **Handshake**: Standard WebSocket handshake (unencrypted)
- **Authentication**: Password in query parameter (unencrypted handshake)
- **Messages**: Encrypted JSON payloads (binary frames)

**Key changes from plain JSON:**
- **Binary frames (opcode 0x2)** instead of text frames (opcode 0x1)
- **Message format**: `[IV (12 bytes)] [Ciphertext] [Auth Tag (16 bytes)]`
- **Encryption**: AES-256-GCM with PBKDF2-derived key

### Authentication Flow

**With encryption enabled:**
```
1. Client: Derive key from password (PBKDF2)
2. Client → Server: GET /?password=yourpassword HTTP/1.1
                     Upgrade: websocket
                     ...
3. Server → Client: HTTP/1.1 101 Switching Protocols
4. Server → Client: Binary frame with encrypted {"type": "auth_ok", ...}
5. Client: Decrypt and verify message
```

**Without encryption (not recommended):**
```
1. Client → Server: GET /?password=yourpassword HTTP/1.1
                     Upgrade: websocket
                     ...
2. Server → Client: HTTP/1.1 101 Switching Protocols
3. Server → Client: Text frame with plain {"type": "auth_ok", ...}
```

---

## Testing

### Test with wscat (Plain JSON only)

**Note**: wscat doesn't support custom encryption, so test with encryption disabled:

```bash
# In irssi
/SET fe_web_encryption OFF

# Install wscat
npm install -g wscat

# Test plain connection
wscat -c "ws://127.0.0.1:9001/?password=yourpassword"
```

### Test with Node.js (With Encryption)

```javascript
// test-encryption.js
const WebSocket = require('ws');
const crypto = require('crypto').webcrypto || require('crypto');

class IrssiEncryption {
    constructor(password) {
        this.password = password;
        this.key = null;
    }

    async deriveKey() {
        const encoder = new TextEncoder();
        const passwordData = encoder.encode(this.password);

        const keyMaterial = await crypto.subtle.importKey(
            'raw', passwordData, 'PBKDF2', false, ['deriveKey']
        );

        this.key = await crypto.subtle.deriveKey(
            {
                name: 'PBKDF2',
                salt: encoder.encode('irssi-fe-web-v1'),
                iterations: 10000,
                hash: 'SHA-256'
            },
            keyMaterial,
            { name: 'AES-GCM', length: 256 },
            false,
            ['encrypt', 'decrypt']
        );
    }

    async decrypt(data) {
        const dataArray = new Uint8Array(data);
        const iv = dataArray.slice(0, 12);
        const ciphertext = dataArray.slice(12);

        const plaintext = await crypto.subtle.decrypt(
            { name: 'AES-GCM', iv: iv },
            this.key,
            ciphertext
        );

        const decoder = new TextDecoder();
        return decoder.decode(plaintext);
    }
}

async function testConnection(useEncryption) {
    const url = 'ws://127.0.0.1:9001/?password=yourpassword';

    let encryption = null;
    if (useEncryption) {
        encryption = new IrssiEncryption('yourpassword');
        await encryption.deriveKey();
        console.log('🔒 Encryption enabled');
    } else {
        console.log('🔓 Plain connection');
    }

    const ws = new WebSocket(url);
    ws.binaryType = 'arraybuffer';

    ws.on('open', () => {
        console.log('✅ Connected successfully');
    });

    ws.on('message', async (data) => {
        let msg;

        if (useEncryption && data instanceof ArrayBuffer) {
            const json = await encryption.decrypt(data);
            msg = JSON.parse(json);
            console.log('🔓 Decrypted:', msg.type);
        } else {
            msg = JSON.parse(data);
            console.log('📨 Received:', msg.type);
        }

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

// After (configurable encryption)
const url = `ws://${config.host}:${config.port}/?password=${config.password}`;
const ws = new WebSocket(url);
ws.binaryType = 'arraybuffer';  // For encrypted messages

// Initialize encryption if enabled
if (config.encryption) {
    const encryption = new IrssiEncryption(config.password);
    await encryption.deriveKey();
}
```

### Backward Compatibility

The Lounge should support **both** encrypted and plain connections:

- Default to **encryption enabled** (recommended)
- Allow user to disable encryption in settings (for debugging)
- Gracefully handle both binary (encrypted) and text (plain) frames

**Frame type detection:**
```javascript
ws.on('message', async (data) => {
    let msg;

    if (data instanceof ArrayBuffer) {
        // Binary frame = encrypted
        const json = await encryption.decrypt(data);
        msg = JSON.parse(json);
    } else {
        // Text frame = plain JSON
        msg = JSON.parse(data);
    }

    handleMessage(msg);
});
```

---

## Security Recommendations

### For Users

**Development/Testing (localhost):**
- ✅ Encryption enabled (default) - recommended
- ✅ Encryption disabled - acceptable for debugging

**Production (remote server):**
- ✅ **Always use encryption** (default)
- ✅ Consider additional transport security (VPN/SSH tunnel)
- ✅ Or use reverse proxy for additional TLS layer

### For The Lounge Developers

**Display warnings:**
- Warn when encryption is disabled with non-localhost host
- Show encryption status in connection indicator
- Recommend keeping encryption enabled

**Example warning:**
```javascript
if (!config.encryption && config.host !== 'localhost' && config.host !== '127.0.0.1') {
    console.warn('⚠️ WARNING: Encryption disabled for remote connection!');
    console.warn('   Password and all data will be sent in plain text.');
    console.warn('   Enable encryption in settings.');
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

