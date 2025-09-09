# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

Irssi uses the **Meson** build system with **Ninja** as the backend.

### Essential Commands

```bash
# Initial setup
meson Build

# Build the project
ninja -C Build

# Install (requires sudo for system-wide install)
sudo ninja -C Build install

# Run tests
ninja -C Build test

# Clean build
rm -rf Build
```

### Build Configuration

Common meson options (pass to `meson Build -Doption=value`):

- `-Dwith-perl=yes/no` - Enable/disable Perl scripting support (default: auto)
- `-Dwith-proxy=yes` - Build irssi-proxy module
- `-Dwith-bot=yes` - Build irssi-bot
- `-Dwithout-textui=yes` - Build without text frontend
- `-Dwith-otr=yes/no` - Enable OTR encryption support
- `-Dstatic-dependency=yes` - Request static dependencies

## Architecture Overview

Irssi is a modular IRC client with a layered architecture:

### Core Layers

1. **Core (`src/core/`)** - Base functionality (servers, channels, networking, protocols)
2. **Chat Protocols (`src/irc/core/`)** - IRC-specific protocol handling
3. **Frontend Common (`src/fe-common/`)** - Shared frontend logic
4. **Frontend Implementations** - Multiple frontends:
   - **Text UI (`src/fe-text/`)** - Traditional terminal interface
   - **Web UI (`src/fe-web/`)** - WebSocket-based web interface (custom development)
   - **None (`src/fe-none/`)** - Headless mode

### Key Components

- **Modules System** - Dynamic loading via `src/core/modules.c`
- **Signal System** - Event-driven architecture in `src/core/signals.c`
- **Settings** - Configuration management in `src/core/settings.c`
- **Perl Integration** - Extensive scripting support in `src/perl/`
- **Plugin Architecture** - Loadable modules for DCC, flood control, notifications, etc.

### Web Frontend (fe-web)

Current development focus on a modern web-based frontend:

- **WebSocket Server** (`fe-web-server.c`) - Real-time communication
- **JSON API** (`fe-web-api.c`) - Serialization of Irssi objects
- **Signal Bridge** (`fe-web-signals.c`) - Maps Irssi events to web messages
- **Client Management** (`fe-web-client.c`) - WebSocket client handling
- **Frontend Prototype** (`web_proto/irssi-web/`) - Next.js React application

## Development Workflow

1. **Code Changes** - Modify source in `src/`
2. **Build** - `ninja -C Build`
3. **Test** - `ninja -C Build test` (limited test coverage currently)
4. **Install** - For system testing: `sudo ninja -C Build install`

### Module Development

- New modules go in appropriate subdirectories under `src/`
- Each module needs `meson.build` file
- Use existing modules as templates (e.g., `src/irc/dcc/`)
- Register with module system via `module.h`

## Project Structure

```
src/
├── core/           # Core Irssi functionality
├── fe-common/      # Shared frontend code
│   ├── core/       # Common UI components
│   └── irc/        # IRC-specific UI
├── fe-text/        # Terminal interface
├── fe-web/         # Web interface (active development)
├── irc/            # IRC protocol implementation
│   ├── core/       # Core IRC handling  
│   ├── dcc/        # Direct Client-to-Client
│   ├── flood/      # Flood protection
│   └── notifylist/ # Friend notifications
├── perl/           # Perl scripting engine
└── lib-config/     # Configuration parser
```

## Dependencies

- **GLib 2.32+** - Core data structures and utilities
- **OpenSSL** - TLS/SSL support
- **Perl 5.8+** - For scripting support (optional)
- **ncurses/terminfo** - For text frontend
- **Meson 0.53+ & Ninja 1.8+** - Build system

## Special Notes

- **Cross-platform** - Supports Linux, macOS, FreeBSD, etc.
- **Modular Design** - Components can be built/excluded independently
- **Signal-driven** - Heavy use of signal/callback patterns
- **Memory Management** - Manual memory management throughout
- **Thread Safety** - Generally single-threaded design
- **Plugin API** - Rich API for both C modules and Perl scripts

The `fe-web` branch contains active development of a modern web interface to complement the traditional terminal UI.