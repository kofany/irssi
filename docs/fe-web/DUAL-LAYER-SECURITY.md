# Dual-Layer Security Architecture

## Overview

fe-web **enforces mandatory dual-layer security** combining both SSL/TLS and application-level encryption for defense-in-depth.

**⚠️ IMPORTANT**: As of version 1.5, both security layers are **MANDATORY** and cannot be disabled.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Layer 1: Transport Security (SSL/TLS)                  │
│  ├─ Protocol: wss:// (WebSocket Secure)                 │
│  ├─ Certificate: Self-signed (auto-generated)           │
│  ├─ Encryption: TLS 1.2/1.3                             │
│  └─ Purpose: Protect against network sniffing           │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Layer 2: Application Security (AES-256-GCM)            │
│  ├─ Algorithm: AES-256-GCM                              │
│  ├─ Key Derivation: PBKDF2-HMAC-SHA256 (10000 iter)    │
│  ├─ Message Format: [IV][Ciphertext][Tag]              │
│  └─ Purpose: End-to-end encryption + authentication     │
└─────────────────────────────────────────────────────────┘
```

## Security Configuration

**⚠️ MANDATORY**: Both SSL/TLS and encryption are **ALWAYS enabled** - no configuration options to disable.

### irssi Configuration (Required)

```
/SET fe_web_password "your-strong-password"  # REQUIRED
/SET fe_web_enabled ON
/SET fe_web_port 9001
/SET fe_web_bind 127.0.0.1
/SAVE
```

**Generate strong password:**
```bash
openssl rand -base64 32
```

**Server will REFUSE to start if:**
- ❌ Password is not set
- ❌ SSL initialization fails
- ❌ Encryption initialization fails

**No longer available:**
- ❌ `/SET fe_web_ssl ON/OFF` - removed, always ON
- ❌ `/SET fe_web_encryption ON/OFF` - removed, always ON

## Connection Flow (Dual-Layer)

```
1. Client initiates TLS connection
   └─> wss://irssi-server:9001/?password=secret

2. TLS handshake
   ├─> Client Hello
   ├─> Server Hello (self-signed cert)
   ├─> Client accepts certificate
   └─> TLS session established

3. WebSocket handshake (over TLS)
   ├─> GET /?password=secret HTTP/1.1
   │   Upgrade: websocket
   │   ...
   └─> HTTP/1.1 101 Switching Protocols

4. Application-level encryption
   ├─> Client derives key: PBKDF2(password)
   ├─> Server derives key: PBKDF2(password)
   └─> Both ready for encrypted communication

5. Message exchange
   ├─> Client: Encrypt JSON → Binary frame → TLS → Server
   └─> Server: Decrypt → Process → Encrypt → TLS → Client
```

## Security Benefits

### Defense in Depth

**If TLS is compromised:**
- ✅ Application-level encryption still protects data
- ✅ Attacker sees encrypted binary frames
- ✅ Cannot read or modify messages (authenticated encryption)

**If application encryption is compromised:**
- ✅ TLS still protects transport
- ✅ Prevents network sniffing
- ✅ Provides additional layer of security

### Authentication

**Dual authentication:**
1. **Password authentication** - WebSocket handshake
2. **Encryption authentication** - AES-GCM tag verification

Attacker must:
- Know the password (for WebSocket auth)
- Derive correct encryption key (for message decryption)
- Pass GCM tag verification (for message integrity)

## Use Cases

### 1. Backend Server → irssi

**Scenario:** Node.js/Python backend connecting to irssi

**Configuration:**
```javascript
// Backend (Node.js)
const ws = new WebSocket('wss://irssi:9001/?password=secret', {
    rejectUnauthorized: false  // Accept self-signed cert
});
ws.binaryType = 'arraybuffer';

// Implement encryption layer
const encryption = new IrssiEncryption(password);
await encryption.deriveKey();
```

**Benefits:**
- ✅ TLS protects backend ↔ irssi connection
- ✅ Encryption provides end-to-end security
- ✅ Backend can easily accept self-signed cert

### 2. Dedicated Mobile/Desktop App → irssi

**Scenario:** iOS/Android/Electron app connecting to irssi

**Configuration:**
```swift
// iOS
let ws = WebSocket(url: "wss://irssi:9001/?password=secret")
ws.allowSelfSignedCertificates = true

// Implement encryption layer
let encryption = IrssiEncryption(password: password)
encryption.deriveKey()
```

**Benefits:**
- ✅ TLS protects app ↔ irssi connection
- ✅ Encryption provides end-to-end security
- ✅ App can easily accept self-signed cert (not browser)

### 3. Browser → Backend → irssi

**Scenario:** Web app with backend proxy

**Architecture:**
```
Browser → HTTPS (Let's Encrypt) → Backend → wss:// (self-signed) → irssi
          ├─ Valid cert                    ├─ Self-signed cert
          └─ No encryption needed          └─ Encryption enabled
```

**Benefits:**
- ✅ Browser gets valid certificate (no warnings)
- ✅ Backend ↔ irssi uses dual-layer security
- ✅ End-to-end security from browser to irssi

## Performance Impact

### SSL/TLS Overhead

- **Handshake:** ~1-2 RTT (one-time per connection)
- **Encryption:** ~5-10% CPU overhead
- **Throughput:** Minimal impact on modern CPUs

### Application Encryption Overhead

- **Key derivation:** ~10ms (one-time, PBKDF2 10000 iterations)
- **Encryption:** ~1-2% CPU overhead (AES-256-GCM is hardware-accelerated)
- **Message size:** +28 bytes per message (IV + tag)

### Combined Overhead

- **Total:** ~6-12% CPU overhead
- **Latency:** Negligible (<1ms per message)
- **Acceptable for:** All use cases (IRC is low-bandwidth)

## Security Considerations

### Self-Signed Certificates

**Limitations:**
- ❌ Does NOT verify server identity
- ❌ Vulnerable to MITM if attacker has network access
- ✅ Protects against passive sniffing

**Mitigation:**
- ✅ Application-level encryption provides end-to-end security
- ✅ Even if TLS is MITM'd, attacker cannot read/modify messages
- ✅ GCM authentication tag prevents tampering

### Password Security

**Critical:**
- Password is used for both authentication and encryption key
- **Use strong passwords** (16+ characters, random)
- Consider using password manager or key derivation

**Example strong password:**
```bash
# Generate random password
openssl rand -base64 32
# Output: 8xK9mP2vL5nQ7wR4tY6uI3oA1sD0fG8hJ9kL2mN5pQ==
```

### Network Security

**Recommended:**
- ✅ Use dual-layer (SSL + encryption) for remote connections
- ✅ Use encryption only for localhost/LAN
- ✅ Consider VPN/SSH tunnel for additional security
- ✅ Use reverse proxy with Let's Encrypt for production

## Troubleshooting

### SSL Handshake Fails

**Symptoms:**
```
fe-web: [xxx] SSL handshake failed
```

**Solutions:**
1. Check client accepts self-signed certificates
2. Verify OpenSSL is installed on server
3. Check firewall allows port 9001

### Encryption Fails

**Symptoms:**
```
fe-web: [xxx] Decryption failed - wrong password or tampered data
```

**Solutions:**
1. Verify password matches on client and server
2. Check PBKDF2 parameters match (salt, iterations)
3. Verify message format: [IV (12)][Ciphertext][Tag (16)]

### Both Enabled But Only One Works

**Check:**
```
# In irssi
/SET fe_web_ssl
/SET fe_web_encryption

# Should show:
fe_web_ssl = ON
fe_web_encryption = ON
```

**Verify logs:**
```
fe-web: New connection from 127.0.0.1 (id: xxx) [SSL] [ENCRYPTED]
```

Both `[SSL]` and `[ENCRYPTED]` tags should appear.

## Migration Guide

### From SSL-only to Dual-Layer

**Before:**
```
/SET fe_web_ssl ON
/SET fe_web_encryption OFF
```

**After:**
```
/SET fe_web_encryption ON
/SAVE
```

**Client changes:**
- Add encryption layer (see CLIENT-SPEC.md)
- Keep wss:// URL
- Add binary frame support

### From Encryption-only to Dual-Layer

**Before:**
```
/SET fe_web_ssl OFF
/SET fe_web_encryption ON
```

**After:**
```
/SET fe_web_ssl ON
/SAVE
```

**Client changes:**
- Change ws:// to wss://
- Add self-signed certificate acceptance
- Keep encryption layer unchanged

## References

- [CLIENT-SPEC.md](CLIENT-SPEC.md) - Complete client specification
- [THE-LOUNGE-INTEGRATION.md](THE-LOUNGE-INTEGRATION.md) - Integration guide
- [RFC 6455](https://tools.ietf.org/html/rfc6455) - WebSocket Protocol
- [RFC 5246](https://tools.ietf.org/html/rfc5246) - TLS 1.2
- [NIST SP 800-38D](https://csrc.nist.gov/publications/detail/sp/800-38d/final) - AES-GCM

