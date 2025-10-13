# fe-web Client Implementation Guide

## Version 1.5 - Mandatory Dual-Layer Security

**Target audience**: Developers implementing WebSocket clients for fe-web.

**Last updated**: 2025-10-12

---

## ⚠️ CRITICAL SECURITY REQUIREMENTS

**As of version 1.5, fe-web enforces MANDATORY dual-layer security:**

1. **SSL/TLS (wss://) is MANDATORY** - cannot be disabled
2. **AES-256-GCM encryption is MANDATORY** - cannot be disabled
3. **Password authentication is REQUIRED** - server refuses to start without it

**Your client MUST:**
- ✅ Connect via `wss://` (NOT `ws://`)
- ✅ Accept self-signed certificates
- ✅ Implement AES-256-GCM encryption layer
- ✅ Use BINARY WebSocket frames (opcode 0x2) for all messages

---

## Connection Flow

### Step-by-Step Connection Process

```
1. Derive encryption key from password
   ├─> Algorithm: PBKDF2-HMAC-SHA256
   ├─> Salt: "irssi-fe-web-v1" (15 bytes, fixed)
   ├─> Iterations: 10,000
   ├─> Output: 32-byte key (256 bits)
   └─> Store key for encrypting/decrypting messages

2. Establish TLS connection
   ├─> Protocol: wss://
   ├─> Server uses self-signed certificate
   ├─> Client MUST accept self-signed cert
   └─> TLS handshake completes

3. WebSocket handshake (over TLS)
   ├─> Send: GET /?password=<password> HTTP/1.1
   ├─> Include: Sec-WebSocket-Key (random 16 bytes, base64)
   ├─> Server validates password
   └─> Server responds: HTTP/1.1 101 Switching Protocols

4. Receive auth_ok message
   ├─> Server sends encrypted auth_ok
   ├─> Frame type: BINARY (opcode 0x2)
   ├─> Decrypt using derived key
   └─> Parse JSON: {"type":"auth_ok",...}

5. Ready for bidirectional communication
   ├─> All messages encrypted with AES-256-GCM
   ├─> All frames are BINARY (opcode 0x2)
   └─> Connection is fully secure
```

---

## Encryption Implementation

### Key Derivation (PBKDF2)

**Parameters:**
- **Algorithm**: PBKDF2-HMAC-SHA256
- **Password**: From user input (same as WebSocket auth)
- **Salt**: `"irssi-fe-web-v1"` (15 bytes, UTF-8 encoded, **FIXED**)
- **Iterations**: 10,000
- **Key length**: 32 bytes (256 bits)

**JavaScript Example:**
```javascript
async function deriveKey(password) {
    const encoder = new TextEncoder();
    const passwordBuffer = encoder.encode(password);
    const saltBuffer = encoder.encode("irssi-fe-web-v1");
    
    // Import password as key material
    const keyMaterial = await crypto.subtle.importKey(
        'raw',
        passwordBuffer,
        'PBKDF2',
        false,
        ['deriveBits', 'deriveKey']
    );
    
    // Derive AES-GCM key
    const key = await crypto.subtle.deriveKey(
        {
            name: 'PBKDF2',
            salt: saltBuffer,
            iterations: 10000,
            hash: 'SHA-256'
        },
        keyMaterial,
        { name: 'AES-GCM', length: 256 },
        true,
        ['encrypt', 'decrypt']
    );
    
    return key;
}
```

**Python Example:**
```python
import hashlib
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.backends import default_backend

def derive_key(password: str) -> bytes:
    kdf = PBKDF2HMAC(
        algorithm=hashlib.sha256(),
        length=32,
        salt=b"irssi-fe-web-v1",
        iterations=10000,
        backend=default_backend()
    )
    return kdf.derive(password.encode('utf-8'))
```

---

### Message Encryption (AES-256-GCM)

**Encrypted Message Format:**
```
[IV (12 bytes)] [Ciphertext (variable)] [Authentication Tag (16 bytes)]
```

**Parameters:**
- **Algorithm**: AES-256-GCM
- **Key**: 32 bytes (from PBKDF2)
- **IV**: 12 bytes (random, generated per message)
- **Tag**: 16 bytes (authentication tag, appended by GCM)

**JavaScript Encryption Example:**
```javascript
async function encryptMessage(key, jsonString) {
    const encoder = new TextEncoder();
    const plaintext = encoder.encode(jsonString);
    
    // Generate random IV (12 bytes for GCM)
    const iv = crypto.getRandomValues(new Uint8Array(12));
    
    // Encrypt
    const ciphertext = await crypto.subtle.encrypt(
        {
            name: 'AES-GCM',
            iv: iv,
            tagLength: 128  // 16 bytes
        },
        key,
        plaintext
    );
    
    // Combine: IV + ciphertext (includes tag at end)
    const encrypted = new Uint8Array(12 + ciphertext.byteLength);
    encrypted.set(iv, 0);
    encrypted.set(new Uint8Array(ciphertext), 12);
    
    return encrypted;
}
```

**JavaScript Decryption Example:**
```javascript
async function decryptMessage(key, encryptedData) {
    // Extract IV (first 12 bytes)
    const iv = encryptedData.slice(0, 12);
    
    // Extract ciphertext + tag (rest of data)
    const ciphertext = encryptedData.slice(12);
    
    // Decrypt
    const plaintext = await crypto.subtle.decrypt(
        {
            name: 'AES-GCM',
            iv: iv,
            tagLength: 128
        },
        key,
        ciphertext
    );
    
    // Decode to string
    const decoder = new TextDecoder();
    return decoder.decode(plaintext);
}
```

**Python Encryption Example:**
```python
import os
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

def encrypt_message(key: bytes, json_string: str) -> bytes:
    # Generate random IV (12 bytes)
    iv = os.urandom(12)
    
    # Create AESGCM cipher
    aesgcm = AESGCM(key)
    
    # Encrypt (returns ciphertext + tag)
    ciphertext = aesgcm.encrypt(iv, json_string.encode('utf-8'), None)
    
    # Combine: IV + ciphertext + tag
    return iv + ciphertext

def decrypt_message(key: bytes, encrypted_data: bytes) -> str:
    # Extract IV (first 12 bytes)
    iv = encrypted_data[:12]
    
    # Extract ciphertext + tag (rest)
    ciphertext = encrypted_data[12:]
    
    # Create AESGCM cipher
    aesgcm = AESGCM(key)
    
    # Decrypt
    plaintext = aesgcm.decrypt(iv, ciphertext, None)
    
    return plaintext.decode('utf-8')
```

---

## WebSocket Frame Format

### Sending Messages (Client → Server)

**Frame structure:**
```
FIN=1, Opcode=0x2 (BINARY), MASK=1
Masking-key: [4 random bytes]
Payload: [Encrypted message]
```

**Steps:**
1. Create JSON message: `{"type":"command","command":"/join #irssi"}`
2. Encrypt JSON using AES-256-GCM
3. Create BINARY WebSocket frame (opcode 0x2)
4. Apply masking (REQUIRED for client→server)
5. Send frame

**JavaScript Example:**
```javascript
async function sendMessage(ws, key, messageObj) {
    // 1. Convert to JSON
    const json = JSON.stringify(messageObj);
    
    // 2. Encrypt
    const encrypted = await encryptMessage(key, json);
    
    // 3. Send as binary frame
    ws.send(encrypted);
}
```

### Receiving Messages (Server → Client)

**Frame structure:**
```
FIN=1, Opcode=0x2 (BINARY), MASK=0
Payload: [Encrypted message]
```

**Steps:**
1. Receive BINARY frame (opcode 0x2)
2. Extract payload (no unmasking needed)
3. Decrypt using AES-256-GCM
4. Parse JSON
5. Handle message

**JavaScript Example:**
```javascript
ws.binaryType = 'arraybuffer';  // IMPORTANT!

ws.onmessage = async (event) => {
    // 1. Get encrypted data
    const encrypted = new Uint8Array(event.data);
    
    // 2. Decrypt
    const json = await decryptMessage(key, encrypted);
    
    // 3. Parse JSON
    const message = JSON.parse(json);
    
    // 4. Handle message
    console.log('Received:', message);
    
    if (message.type === 'auth_ok') {
        console.log('Authenticated!');
    } else if (message.type === 'message') {
        console.log(`[${message.channel}] <${message.nick}> ${message.text}`);
    }
};
```

---

## SSL/TLS Configuration

### Accepting Self-Signed Certificates

**Node.js (ws library):**
```javascript
const WebSocket = require('ws');

const ws = new WebSocket('wss://127.0.0.1:9001/?password=secret', {
    rejectUnauthorized: false  // Accept self-signed cert
});
```

**Python (websocket-client):**
```python
import websocket
import ssl

ws = websocket.WebSocket(sslopt={"cert_reqs": ssl.CERT_NONE})
ws.connect("wss://127.0.0.1:9001/?password=secret")
```

**Browser (cannot disable cert validation):**
```javascript
// Browsers CANNOT accept self-signed certs programmatically
// Solution: Use backend proxy with valid cert (Let's Encrypt)

// Browser → HTTPS (valid cert) → Backend → wss:// (self-signed) → irssi
```

---

## Complete Implementation Example

See next section for full working examples in:
- JavaScript (Node.js)
- Python
- Browser (with encryption helper class)

**Continue to**: [CLIENT-SPEC.md](CLIENT-SPEC.md) for complete examples and message format details.

---

## Common Pitfalls

### ❌ Mistake 1: Using ws:// instead of wss://
```javascript
// WRONG - will fail
const ws = new WebSocket('ws://127.0.0.1:9001/?password=secret');
```

**Fix:**
```javascript
// CORRECT
const ws = new WebSocket('wss://127.0.0.1:9001/?password=secret', {
    rejectUnauthorized: false
});
```

### ❌ Mistake 2: Sending TEXT frames instead of BINARY
```javascript
// WRONG - server expects BINARY frames
ws.send(JSON.stringify(message));  // Sends TEXT frame
```

**Fix:**
```javascript
// CORRECT - encrypt and send as BINARY
const encrypted = await encryptMessage(key, JSON.stringify(message));
ws.send(encrypted);  // Sends BINARY frame
```

### ❌ Mistake 3: Wrong salt for PBKDF2
```javascript
// WRONG - incorrect salt
const salt = encoder.encode("my-custom-salt");
```

**Fix:**
```javascript
// CORRECT - MUST use exact salt
const salt = encoder.encode("irssi-fe-web-v1");
```

### ❌ Mistake 4: Not setting binaryType
```javascript
// WRONG - default is 'blob', causes issues
const ws = new WebSocket('wss://...');
```

**Fix:**
```javascript
// CORRECT - set to 'arraybuffer'
const ws = new WebSocket('wss://...');
ws.binaryType = 'arraybuffer';
```

---

## Testing Your Implementation

### 1. Test Key Derivation

```javascript
const password = "test123";
const key = await deriveKey(password);
const keyHex = Array.from(new Uint8Array(await crypto.subtle.exportKey('raw', key)))
    .map(b => b.toString(16).padStart(2, '0'))
    .join('');

console.log('Key (hex):', keyHex);
// Should be consistent for same password
```

### 2. Test Encryption/Decryption

```javascript
const message = '{"type":"ping"}';
const encrypted = await encryptMessage(key, message);
const decrypted = await decryptMessage(key, encrypted);

console.log('Original:', message);
console.log('Decrypted:', decrypted);
// Should match
```

### 3. Test Connection

```javascript
const ws = new WebSocket('wss://127.0.0.1:9001/?password=test123', {
    rejectUnauthorized: false
});
ws.binaryType = 'arraybuffer';

ws.onopen = () => console.log('Connected!');
ws.onmessage = async (event) => {
    const encrypted = new Uint8Array(event.data);
    const json = await decryptMessage(key, encrypted);
    const msg = JSON.parse(json);
    console.log('Received:', msg);
    
    if (msg.type === 'auth_ok') {
        console.log('✅ Authentication successful!');
    }
};
```

---

## Security Considerations

### Password Strength

**Generate strong passwords:**
```bash
# 32-byte random password (base64)
openssl rand -base64 32

# Example output:
# 8xK9mP2vL5nQ7wR4tY6uI3oA1sD0fG8hJ9kL2mN5pQ==
```

### Key Storage

**DO NOT:**
- ❌ Store derived key in localStorage (browser)
- ❌ Log encryption keys to console
- ❌ Send keys over network

**DO:**
- ✅ Derive key on-demand from password
- ✅ Store password securely (keychain/credential manager)
- ✅ Clear key from memory when done

### IV Reuse

**CRITICAL**: Never reuse IV with same key!

```javascript
// WRONG - reusing same IV
const iv = new Uint8Array(12);  // All zeros!

// CORRECT - random IV per message
const iv = crypto.getRandomValues(new Uint8Array(12));
```

---

## Troubleshooting

### Connection Refused

**Error**: `ECONNREFUSED` or connection timeout

**Check:**
1. Is irssi running?
2. Is fe-web loaded? (`/LOAD fe-web`)
3. Is server enabled? (`/SET fe_web_enabled ON`)
4. Is password set? (`/SET fe_web_password <password>`)
5. Check irssi logs for error messages

### Authentication Failed

**Error**: Server closes connection immediately

**Check:**
1. Password in URL matches server password
2. Password is URL-encoded if contains special chars
3. Server logs show: "Authentication failed"

### Decryption Failed

**Error**: Cannot decrypt messages

**Check:**
1. PBKDF2 salt is exactly `"irssi-fe-web-v1"`
2. Iterations = 10,000
3. Key length = 32 bytes
4. IV size = 12 bytes
5. Tag size = 16 bytes

### SSL Certificate Error

**Error**: `CERT_HAS_EXPIRED` or `DEPTH_ZERO_SELF_SIGNED_CERT`

**Fix:**
```javascript
// Node.js
const ws = new WebSocket('wss://...', {
    rejectUnauthorized: false
});

// Python
ws.connect("wss://...", sslopt={"cert_reqs": ssl.CERT_NONE})
```

---

## Next Steps

1. **Read**: [CLIENT-SPEC.md](CLIENT-SPEC.md) for complete message format
2. **Review**: [DUAL-LAYER-SECURITY.md](DUAL-LAYER-SECURITY.md) for security details
3. **Check**: [THE-LOUNGE-INTEGRATION.md](THE-LOUNGE-INTEGRATION.md) for integration examples
4. **Test**: Use provided examples to verify your implementation

---

## Support

If you encounter issues:
1. Check server logs in irssi
2. Verify all security requirements are met
3. Test with provided examples
4. Review troubleshooting section

**Remember**: Both SSL/TLS and encryption are MANDATORY - there is no fallback to insecure modes.

