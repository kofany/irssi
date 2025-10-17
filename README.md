# Irssi FE-Web Module v1.0.0

WebSocket-based frontend module for Irssi IRC client.

## Features

- **WebSocket Server**: Secure wss:// connections with auto-generated SSL certificates
- **End-to-End Encryption**: AES-256-GCM application-level encryption
- **Real-time Sync**: Live IRC state synchronization (channels, queries, nicklists, messages)
- **Network Management**: Add/remove IRC networks and servers via web interface
- **Password Protected**: Required authentication for all connections
- **Modern Security**: TLS 1.2+ enforcement, dual-layer encryption

## Requirements

- Irssi 1.0.0 or later
- GLib 2.32+
- OpenSSL 3.0+
- Meson 0.53+ and Ninja 1.8+

## Installation

```bash
git clone https://github.com/kofany/irssi-fe-web
cd irssi-fe-web
meson Build --prefix=$HOME/.local
ninja -C Build
ninja -C Build install
```

## Configuration

In Irssi:

```
/LOAD fe-web
/SET fe_web_password your-strong-password-here
/SET fe_web_port 9001
/SET fe_web_bind 127.0.0.1
/FE_WEB START
```

**Security Note**: Always use a strong password! Generate one with:
```bash
openssl rand -base64 32
```

## Usage

### Start Server

```
/FE_WEB START
```

### Check Status

```
/FE_WEB STATUS
```

### Stop Server

```
/FE_WEB STOP
```

### Connect from Web Client

```
wss://localhost:9001/?password=your-password
```

The module will automatically:
- Generate self-signed SSL certificates
- Derive encryption keys from your password
- Handle WebSocket handshake and authentication
- Stream IRC events to connected clients

## API / Protocol

See [docs/CLIENT-SPEC.md](docs/CLIENT-SPEC.md) for WebSocket protocol documentation.

## Architecture

- **fe-web.c**: Main module, commands, initialization
- **fe-web-server.c**: WebSocket server, SSL/TLS, handshake
- **fe-web-ssl.c**: SSL certificate generation and management
- **fe-web-crypto.c**: AES-256-GCM encryption/decryption
- **fe-web-client.c**: Client connection handling
- **fe-web-signals.c**: IRC event handlers
- **fe-web-netserver.c**: Network/server management
- **fe-web-utils.c**: Message serialization and utilities

## Security

- **Dual-Layer Security**: SSL/TLS + AES-256-GCM application encryption
- **Password Required**: No default password, must be explicitly set
- **Self-Signed Certs**: Auto-generated 2048-bit RSA certificates
- **Key Derivation**: PBKDF2 with 100,000 iterations
- **Modern TLS**: Enforces TLS 1.2 minimum

## License

GPL-2.0 - Same as Irssi

## Author

Created by kofany

## Links

- Irssi: https://irssi.org/
- GitHub: https://github.com/kofany/irssi-fe-web
