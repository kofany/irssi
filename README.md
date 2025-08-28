# Evolved Irssi (erssi) 🚀

[![GitHub stars](https://img.shields.io/github/stars/kofany/irssi.svg?style=social&label=Stars)](https://github.com/kofany/irssi)
[![License](https://img.shields.io/badge/License-GPL--2.0--or--later-blue.svg)](https://opensource.org/licenses/GPL-2.0)
[![IRC Network](https://img.shields.io/badge/Chat-IRC-green.svg)](irc://irc.libera.chat)
[![Build Status](https://img.shields.io/badge/Build-Meson%2BNinja-orange.svg)](https://mesonbuild.com/)

## What is Evolved Irssi?

Evolved Irssi (erssi) is a next-generation IRC client that builds upon the robust foundation of the classic irssi, introducing modern features and enhanced user experience without sacrificing the simplicity and power that made irssi legendary.

🎯 **Mission**: Modernizing IRC, one feature at a time, while preserving the soul of irssi.

## 🌟 Key Features

### Enhanced User Experience
- **Whois in Active Window**: Say goodbye to context switching! Whois responses appear directly in your current chat window
- **Native Sidepanels with Mouse Support**: Full mouse interaction for channel and user lists
- **Native Nick Alignment**: Perfectly aligned nicknames for enhanced readability across all chat windows
- **Separate Configuration**: Uses `~/.erssi/` directory, allowing coexistence with standard irssi

### Full Compatibility
- **100% Perl Script Compatible**: All existing irssi Perl scripts work without modification
- **Theme Compatible**: Use any irssi theme seamlessly
- **Configuration Compatible**: Import your existing irssi configuration effortlessly

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

# Check installation status
./check-installation.sh
```

### First Run
- **erssi**: Creates `~/.erssi/` configuration directory
- **irssi**: Uses existing `~/.irssi/` or creates new one

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
├── themes/              # Theme files
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
- Enhanced installation system with cross-platform support
- Automatic dependency management
- Improved build configuration
- Comprehensive documentation

### Previous Versions
- Based on irssi 1.4.x series
- Incremental feature additions
- Community-driven enhancements

## 🏆 Acknowledgments

- **[Irssi Core Team](https://irssi.org/)**: Original irssi project and continued excellence
- **[Open Source Community](https://github.com/irssi/irssi)**: Inspiration and collaborative spirit
- **Contributors**: Everyone who made evolved irssi possible

## 📞 Contact & Support

- **🐛 Issues**: [GitHub Issues](https://github.com/kofany/irssi/issues)
- **💬 Discussion**: [GitHub Discussions](https://github.com/kofany/irssi/discussions)
- **📧 IRC**: `#erssi` on Libera Chat
- **📖 Documentation**: [Installation Guide](INSTALL-SCRIPT.md)

## 📄 License

Evolved Irssi is released under the **GNU General Public License v2.0 or later** (GPL-2.0-or-later), consistent with the original irssi project.

---

<div align="center">

**🎯 Evolved Irssi: Where Classic Meets Modern** 

*Preserving the power of irssi while embracing the future of IRC*

[⭐ Star us on GitHub](https://github.com/kofany/irssi) • [📚 Read the Docs](INSTALL-SCRIPT.md) • [🚀 Get Started](#quick-start)

</div>