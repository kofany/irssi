# Evolved Irssi (erssi) 🚀

[![GitHub stars](https://img.shields.io/github/stars/kofany/irssi.svg?style=social&label=Stars)](https://github.com/kofany/irssi/tree/evolved-irssi)
[![License](https://img.shields.io/badge/License-GPL--2.0--or--later-blue.svg)](https://opensource.org/licenses/GPL-2.0)
[![IRC Network](https://img.shields.io/badge/Chat-IRC-green.svg)](irc://irc.ircnet.com)
[![Build Status](https://img.shields.io/badge/Build-Meson%2BNinja-orange.svg)](https://mesonbuild.com/)

## What is Evolved Irssi?

Evolved Irssi (erssi) is a next-generation IRC client that builds upon the robust foundation of the classic irssi, introducing modern features and enhanced user experience without sacrificing the simplicity and power that made irssi legendary.

🎯 **Mission**: Modernizing IRC, one feature at a time, while preserving the soul of irssi.

## 🌟 Key Features

### 🎨 Advanced Sidepanel System
- **Smart Redraw Logic**: Separate functions for left panel (`redraw_left_panels_only`), right panel (`redraw_right_panels_only`), and both (`redraw_both_panels_only`)
- **Event-Specific Updates**: Different panel updates based on IRC events (joins, parts, nick changes, channel activity)
- **Batched Mass Events**: Hybrid batching system for mass joins/parts with timer fallback and immediate sync triggers
- **Debug Logging**: Comprehensive event tracking with file-based debug output (`/tmp/irssi_sidepanels.log`)

### 🎯 Enhanced Nick Display
- **Dynamic Nick Alignment**: Intelligent padding and truncation with `+` indicator for long nicks
- **Hash-Based Nick Coloring**: Consistent colors per nick per channel with configurable palette and reset events
- **Color Reset System**: Configurable events (quit/part/nickchange) that reassign colors, plus manual `/nickhash shift` command
- **Real-Time Updates**: All formatting applied dynamically as messages appear

### Enhanced User Experience
- **Whois in Active Window**: Say goodbye to context switching! Whois responses appear directly in your current chat window
- **Optimized Sidepanels with Mouse Support**: Intelligent redraw system for left (window list) and right (nicklist) panels with batching for mass events
- **Enhanced Nick Display**: Advanced nick alignment, intelligent truncation, and hash-based nick coloring system with separate mode colors
- **Performance Optimizations**: Granular panel redraws instead of full refreshes, batched updates for mass join/part events
- **Separate Configuration**: Uses `~/.erssi/` directory, allowing coexistence with standard irssi

### Full Compatibility
- **100% Perl Script Compatible**: All existing irssi Perl scripts work without modification
- **Theme Compatible**: Use any irssi theme seamlessly
- **Configuration Compatible**: Import your existing irssi configuration effortlessly

### 🎨 Premium Themes Collection
- **Nexus Steel Theme**: Modern cyberpunk-inspired theme with advanced statusbar configurations
- **Default Theme**: Enhanced version of classic irssi theme with improved readability
- **Colorless Theme**: Clean, minimalist theme for terminal environments with limited color support
- **iTerm2 Color Scheme**: Premium `material-erssi.itermcolors` terminal color scheme optimized for erssi
- **Auto-Installation**: Themes are automatically copied to `~/.erssi/` on first startup
- **Custom Location**: All themes located in `themes/` directory for easy customization

### Modern Build System
- **Meson + Ninja**: Fast, reliable builds with comprehensive dependency management
- **Full Feature Support**: Perl scripting, OTR messaging, UTF8proc, SSL/TLS out of the box
- **Cross-Platform**: Native support for macOS and Linux distributions

## 🚀 Quick Start

### One-Line Installation

```bash
# Clone and run the installation script
git clone https://github.com/kofany/irssi.git -b evolved-irssi
cd irssi
./install-irssi.sh
```

The installation script will:
- ✅ Detect your system (macOS/Linux)
- ✅ Install all required dependencies automatically
- ✅ Offer installation as `irssi` or `erssi`
- ✅ Choose global (`/opt/`) or local (`~/.local`) installation
- ✅ Build with full feature support

### Installation Options

**Option 1: Standard irssi replacement**
```bash
./install-irssi.sh
# Choose: 1 (irssi) → replaces system irssi
```

**Option 2: Independent erssi installation (Recommended)**
```bash
./install-irssi.sh
# Choose: 2 (erssi) → installs alongside existing irssi
```

## 📦 Manual Installation

For advanced users who prefer manual control:

```bash
# Install dependencies (varies by system)
# See INSTALL-SCRIPT.md for complete package lists

# For erssi (evolved version)
./erssi-convert.sh
meson setup Build --prefix=/opt/erssi -Dwith-perl=yes -Dwith-otr=yes
ninja -C Build
sudo ninja -C Build install

# For standard irssi
meson setup Build --prefix=/opt/irssi -Dwith-perl=yes -Dwith-otr=yes
ninja -C Build  
sudo ninja -C Build install
```

## 🔧 Dependencies

### Automatically Installed
The installation script handles all dependencies:

**macOS (Homebrew):**
- meson, ninja, pkg-config, glib, openssl@3, ncurses
- utf8proc, libgcrypt, libotr, perl

**Linux (APT/DNF/Pacman):**
- Build tools, meson, ninja, pkg-config
- libglib2.0-dev, libssl-dev, libncurses-dev
- libperl-dev, libutf8proc-dev, libgcrypt-dev, libotr-dev

### Build Features Enabled
- ✅ **Perl Scripting**: Full embedded Perl support
- ✅ **OTR Messaging**: Off-The-Record encrypted messaging
- ✅ **UTF8proc**: Enhanced Unicode handling
- ✅ **SSL/TLS**: Secure connections
- ✅ **Terminal UI**: Full ncurses support with mouse interaction

## 📋 System Requirements

### Supported Systems
- **macOS**: 10.15+ (Catalina and later)
- **Linux**: Ubuntu 18.04+, Debian 10+, Fedora 30+, Arch Linux

### Build Requirements
- Meson ≥ 0.53
- Ninja ≥ 1.5
- GLib ≥ 2.32
- Modern C compiler (GCC/Clang)

## 🏃‍♂️ Running Evolved Irssi

```bash
# For erssi installation
erssi

# For irssi replacement
irssi

```

### First Run
- **erssi**: Creates `~/.erssi/` configuration directory
- **irssi**: Uses existing `~/.irssi/` or creates new one

## ⚙️ Configuration

### 🎨 Nick Display Settings

Evolved Irssi includes advanced nick formatting features that can be configured:

```bash
# Enable nick column alignment and truncation
/set nick_column_enabled on
/set nick_column_width 12

# Enable hash-based nick coloring (coming soon)
/set nick_color_enabled on
/set nick_mode_color_enabled on
```

### 🔧 Sidepanel Debug (Advanced)

For troubleshooting sidepanel performance:

```bash
# Enable debug logging to /tmp/irssi_sidepanels.log
/set debug_sidepanel_redraws on

# View real-time redraw events
tail -f /tmp/irssi_sidepanels.log
```

### Left Sidepanel Setup

For optimal experience with the left sidepanel feature, we recommend configuring separate status windows for each IRC network. This creates clean visual separators in the sidepanel and improves navigation.

Add this to your `~/.erssi/config` (or `~/.irssi/config`):

```
windows = {
  1 = {
    immortal = "yes";
    name = "Notices";
    level = "NOTICES";
    servertag = "Notices";
  };
  2 = {
    name = "IRCnet";
    level = "ALL -NOTICES";
    servertag = "IRCnet";
  };
  3 = {
    name = "IRCnet2";
    level = "ALL -NOTICES";
    servertag = "IRCnet2";
  };
};
```

**Key benefits:**
- **Network Separation**: Each IRC network gets its own status window
- **Visual Organization**: Sidepanel shows clear network boundaries
- **Better Navigation**: Easy switching between different networks
- **Servertag Matching**: The `servertag` field should match your server connection names

**Best Practices:**
- Create one status window per IRC network
- Use descriptive names matching your server tags
- Keep the "Notices" window for general system messages
- Adjust `level` settings based on what you want to see in each window

## 🔍 Verification & Troubleshooting

Check your installation:
```bash
./check-installation.sh          # Full system check
./check-installation.sh --quiet  # Minimal output
./check-installation.sh --verbose # Detailed diagnostics
```

For detailed troubleshooting, see [INSTALL-SCRIPT.md](INSTALL-SCRIPT.md).

## 📁 Project Structure

```
irssi/
├── install-irssi.sh       # Main installation script
├── check-installation.sh  # Installation checker
├── erssi-convert.sh      # Irssi → Erssi converter
├── INSTALL-SCRIPT.md     # Detailed installation guide
├── src/                  # Source code
│   ├── fe-text/
│   │   ├── sidepanels.c  # Enhanced sidepanel system with optimized redraws
│   │   └── sidepanels.h  # Sidepanel definitions
│   └── fe-common/core/
│       └── fe-expandos.c # Nick formatting expandos (alignment, truncation, coloring)
├── themes/              # Premium theme collection
│   ├── nexus.theme      # Modern cyberpunk theme (recommended)
│   ├── default.theme    # Enhanced classic irssi theme
│   └── colorless.theme  # Minimalist theme for limited color terminals
├── startup              # Evolved banner displayed on erssi startup
├── material-erssi.itermcolors # Premium iTerm2 color scheme for optimal erssi experience
└── docs/               # Documentation
```

## 🤝 Contributing

We welcome contributions of all kinds!

### Quick Start
```bash
git clone https://github.com/kofany/irssi.git -b evolved-irssi
cd irssi
# Make your changes
git add .
git commit -m "feat: your awesome feature"
git push origin feature/your-feature
```

### Areas for Contribution
- 🐛 **Bug Reports**: Found an issue? Let us know!
- ✨ **New Features**: Ideas for enhancing the IRC experience
- 📚 **Documentation**: Help improve guides and tutorials
- 🎨 **Themes**: Create beautiful themes for the community
- 🔧 **Platform Support**: Extend support to more systems

## 📈 Performance

Evolved Irssi maintains the legendary performance of classic irssi:
- **Memory**: Minimal additional footprint (~2-5MB)
- **CPU**: Negligible performance overhead
- **Startup**: Fast boot times maintained
- **Network**: Efficient IRC protocol handling

## 🆚 irssi vs erssi

| Feature | Standard irssi | Evolved erssi |
|---------|---------------|---------------|
| **Configuration** | `~/.irssi/` | `~/.erssi/` |
| **Binary Name** | `irssi` | `erssi` |
| **Whois Display** | Separate window | Active window |
| **Mouse Support** | Limited | Full sidepanel support |
| **Nick Alignment** | Basic | Enhanced alignment |
| **Perl Scripts** | ✅ Compatible | ✅ Compatible |
| **Themes** | ✅ Compatible | ✅ Compatible |
| **Coexistence** | N/A | ✅ Runs alongside irssi |

## 📜 Version History

### v1.5-evolved (Current)
- **🎨 Advanced Sidepanel System**: Granular redraw optimizations with event-specific panel updates
- **⚡ Performance Improvements**: Batched mass event handling for channels with 400+ users
- **🎯 Enhanced Nick Display**: Hash-based coloring system with separate mode colors and intelligent truncation
- **🔧 Technical Enhancements**:
  - Replaced global `redraw_all()` with targeted `redraw_*_only()` functions
  - Hybrid batching system (timer + event marker) for mass joins/parts
  - Safety checks for window creation/destruction events
  - C89/C90 compliance for broader compatibility
  - Debug logging system with file output
- **📦 Installation & Build**:
  - Enhanced installation system with cross-platform support
  - Automatic dependency management
  - Improved build configuration
  - Comprehensive documentation

### Previous Versions
- Based on irssi current from irssi master branch
- Incremental feature additions
- Community-driven enhancements

## 🏆 Acknowledgments

- **[Irssi Core Team](https://irssi.org/)**: Original irssi project and continued excellence
- **[Open Source Community](https://github.com/irssi/irssi)**: Inspiration and collaborative spirit
- **Contributors**: Everyone who made evolved irssi possible

## 📞 Contact & Support

- **🐛 Issues**: [GitHub Issues](https://github.com/kofany/irssi/issues)
- **💬 Discussion**: [GitHub Discussions](https://github.com/kofany/irssi/discussions)
- **📧 IRC**: `#erssi` on IRCnet
- **📖 Documentation**: [Installation Guide](INSTALL-SCRIPT.md)

## 📄 License

Evolved Irssi is released under the **GNU General Public License v2.0 or later** (GPL-2.0-or-later), consistent with the original irssi project.

---

<div align="center">

**🎯 Evolved Irssi: Where Classic Meets Modern** 

*Preserving the power of irssi while embracing the future of IRC*

[⭐ Star us on GitHub](https://github.com/kofany/irssi/tree/evolved-irssi) • [📚 Read the Docs](INSTALL-SCRIPT.md) • [🚀 Get Started](#quick-start)

</div>