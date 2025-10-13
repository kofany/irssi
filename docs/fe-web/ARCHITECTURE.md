# fe-web Architecture Diagrams

## Overview

This document provides architecture diagrams for different deployment scenarios of fe-web.

---

## Scenario 1: Backend Server → irssi (Dual-Layer Security)

**Use case:** Node.js/Python backend connecting to irssi for persistent IRC connection.

```
┌──────────────────────────────────────────────────────────────┐
│                         Browser                              │
│  - User interface (React/Vue/etc.)                           │
│  - Displays IRC messages                                     │
│  - Sends user commands                                       │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 │ HTTPS (Let's Encrypt)
                 │ + WebSocket (wss://)
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│                    Frontend Server                           │
│  - Serves static files (HTML/CSS/JS)                         │
│  - nginx or similar                                          │
│  - Valid SSL certificate (Let's Encrypt)                     │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 │ WebSocket (wss://)
                 │ Let's Encrypt certificate
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│                    Backend Server                            │
│  - Node.js/Python/Go                                         │
│  - Maintains persistent connection to irssi                  │
│  - Caches IRC history in database                            │
│  - Handles multiple browser clients                          │
│  - Implements encryption layer                               │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 │ wss:// (self-signed cert)
                 │ + AES-256-GCM encryption
                 │ DUAL-LAYER SECURITY
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│                    irssi + fe-web                            │
│  - IRC client (irssi)                                        │
│  - WebSocket server (fe-web module)                          │
│  - SSL/TLS enabled (self-signed)                             │
│  - Encryption enabled (AES-256-GCM)                          │
│                                                              │
│  Configuration:                                              │
│  /SET fe_web_ssl ON                                          │
│  /SET fe_web_encryption ON                                   │
│  /SET fe_web_password "strong-password"                      │
└──────────────────────────────────────────────────────────────┘
```

**Security layers:**
1. **Browser ↔ Frontend**: HTTPS with valid certificate (Let's Encrypt)
2. **Frontend ↔ Backend**: WSS with valid certificate (Let's Encrypt)
3. **Backend ↔ irssi**: WSS with self-signed certificate (Layer 1)
4. **Backend ↔ irssi**: AES-256-GCM encryption (Layer 2)

**Benefits:**
- ✅ No certificate warnings for browser users
- ✅ Backend can accept self-signed certificate
- ✅ Dual-layer security for backend ↔ irssi
- ✅ Persistent IRC connection
- ✅ Message history in database

---

## Scenario 2: Dedicated Mobile/Desktop App → irssi (Dual-Layer Security)

**Use case:** Native iOS/Android/Electron app connecting directly to irssi.

```
┌──────────────────────────────────────────────────────────────┐
│              Dedicated App (iOS/Android/Desktop)             │
│  - Native UI (Swift/Kotlin/Electron)                         │
│  - WebSocket client                                          │
│  - Accepts self-signed certificates                          │
│  - Implements encryption layer                               │
│  - Local message cache                                       │
│  - Push notifications                                        │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 │ wss:// (self-signed cert)
                 │ + AES-256-GCM encryption
                 │ DUAL-LAYER SECURITY
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│                    irssi + fe-web                            │
│  - IRC client (irssi)                                        │
│  - WebSocket server (fe-web module)                          │
│  - SSL/TLS enabled (self-signed)                             │
│  - Encryption enabled (AES-256-GCM)                          │
│                                                              │
│  Configuration:                                              │
│  /SET fe_web_ssl ON                                          │
│  /SET fe_web_encryption ON                                   │
│  /SET fe_web_password "strong-password"                      │
└──────────────────────────────────────────────────────────────┘
```

**Security layers:**
1. **App ↔ irssi**: WSS with self-signed certificate (Layer 1)
2. **App ↔ irssi**: AES-256-GCM encryption (Layer 2)

**Benefits:**
- ✅ Direct connection (no backend needed)
- ✅ Dual-layer security
- ✅ App can accept self-signed certificate (not browser)
- ✅ Native UI/UX
- ✅ Push notifications
- ✅ Offline support

---

## Scenario 3: Browser → Backend → irssi (Encryption Only)

**Use case:** Web app with backend, encryption only (no SSL on irssi side).

```
┌──────────────────────────────────────────────────────────────┐
│                         Browser                              │
│  - User interface (React/Vue/etc.)                           │
│  - Displays IRC messages                                     │
│  - Sends user commands                                       │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 │ HTTPS (Let's Encrypt)
                 │ + WebSocket (wss://)
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│              Frontend + Backend (Combined)                   │
│  - Next.js/Express/Django                                    │
│  - Serves web UI                                             │
│  - WebSocket proxy                                           │
│  - Valid SSL certificate (Let's Encrypt)                     │
│  - Implements encryption layer                               │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 │ ws:// (plain WebSocket)
                 │ + AES-256-GCM encryption
                 │ ENCRYPTION ONLY
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│                    irssi + fe-web                            │
│  - IRC client (irssi)                                        │
│  - WebSocket server (fe-web module)                          │
│  - SSL/TLS disabled                                          │
│  - Encryption enabled (AES-256-GCM)                          │
│                                                              │
│  Configuration:                                              │
│  /SET fe_web_ssl OFF                                         │
│  /SET fe_web_encryption ON                                   │
│  /SET fe_web_password "strong-password"                      │
└──────────────────────────────────────────────────────────────┘
```

**Security layers:**
1. **Browser ↔ Backend**: HTTPS with valid certificate (Let's Encrypt)
2. **Backend ↔ irssi**: AES-256-GCM encryption (application-level)

**Benefits:**
- ✅ No certificate warnings for browser users
- ✅ Simpler setup (no SSL on irssi side)
- ✅ End-to-end encryption via application layer
- ✅ Good for localhost/LAN deployments

---

## Scenario 4: The Lounge Style (All-in-One)

**Use case:** The Lounge-style IRC client with integrated web UI.

```
┌──────────────────────────────────────────────────────────────┐
│                         Browser                              │
│  - The Lounge web UI                                         │
│  - Displays IRC messages                                     │
│  - Sends user commands                                       │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 │ HTTPS (Let's Encrypt)
                 │ + WebSocket (wss://)
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│                      The Lounge                              │
│  - Web UI (HTML/CSS/JS)                                      │
│  - IRC client (built-in)                                     │
│  - WebSocket server                                          │
│  - Message history (SQLite/PostgreSQL)                       │
│  - User management                                           │
│  - Valid SSL certificate (Let's Encrypt)                     │
│                                                              │
│  Optional: Connect to external irssi for advanced features   │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 │ wss:// (self-signed cert)
                 │ + AES-256-GCM encryption
                 │ DUAL-LAYER SECURITY
                 │ (Optional connection)
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│                    irssi + fe-web                            │
│  - IRC client (irssi)                                        │
│  - WebSocket server (fe-web module)                          │
│  - Advanced IRC features (scripts, plugins)                  │
│  - SSL/TLS enabled (self-signed)                             │
│  - Encryption enabled (AES-256-GCM)                          │
│                                                              │
│  Configuration:                                              │
│  /SET fe_web_ssl ON                                          │
│  /SET fe_web_encryption ON                                   │
│  /SET fe_web_password "strong-password"                      │
└──────────────────────────────────────────────────────────────┘
```

**Security layers:**
1. **Browser ↔ The Lounge**: HTTPS with valid certificate (Let's Encrypt)
2. **The Lounge ↔ irssi** (optional): WSS with self-signed certificate (Layer 1)
3. **The Lounge ↔ irssi** (optional): AES-256-GCM encryption (Layer 2)

**Benefits:**
- ✅ All-in-one solution
- ✅ No external dependencies
- ✅ Optional irssi integration for advanced features
- ✅ User management built-in

---

## Security Comparison

| Scenario | Browser Security | Backend Security | irssi Security | Complexity |
|----------|------------------|------------------|----------------|------------|
| 1. Backend → irssi | HTTPS (valid) | WSS (valid) | WSS (self) + Enc | High |
| 2. App → irssi | N/A | N/A | WSS (self) + Enc | Low |
| 3. Browser → Backend | HTTPS (valid) | ws:// + Enc | ws:// + Enc | Medium |
| 4. The Lounge | HTTPS (valid) | N/A | WSS (self) + Enc | Medium |

**Legend:**
- **HTTPS (valid)**: HTTPS with Let's Encrypt certificate
- **WSS (valid)**: WebSocket Secure with Let's Encrypt certificate
- **WSS (self)**: WebSocket Secure with self-signed certificate
- **ws:// + Enc**: Plain WebSocket with AES-256-GCM encryption
- **N/A**: Not applicable

---

## Deployment Recommendations

### For Production (Public Internet)

**Recommended:** Scenario 1 or 3
- Use Let's Encrypt for browser-facing connections
- Use dual-layer or encryption for backend ↔ irssi
- Consider reverse proxy (nginx/Caddy)

### For Mobile/Desktop Apps

**Recommended:** Scenario 2
- Direct connection to irssi
- Dual-layer security
- App can accept self-signed certificates

### For Localhost/LAN

**Recommended:** Scenario 3
- Encryption only (no SSL on irssi)
- Simpler setup
- Still secure (application-level encryption)

### For The Lounge Users

**Recommended:** Scenario 4
- Use The Lounge as primary client
- Optional irssi integration for advanced features
- Best user experience

---

## See Also

- [DUAL-LAYER-SECURITY.md](DUAL-LAYER-SECURITY.md) - Complete security guide
- [CLIENT-SPEC.md](CLIENT-SPEC.md) - Client implementation specification
- [THE-LOUNGE-INTEGRATION.md](THE-LOUNGE-INTEGRATION.md) - The Lounge integration guide

