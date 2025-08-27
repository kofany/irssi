# Evolved Irssi (erssi) 🚀

[![GitHub stars](https://img.shields.io/github/stars/kofany/irssi.svg?style=social&label=Stars)](https://github.com/kofany/irssi)
[![License](https://img.shields.io/badge/License-GPL-blue.svg)](https://opensource.org/licenses/GPL-3.0)
[![IRC Network](https://img.shields.io/badge/Chat-IRC-green.svg)](irc://irc.libera.chat)

## What is Evolved Irssi?

Evolved Irssi (erssi) is a next-generation IRC client that builds upon the robust foundation of the classic irssi, introducing modern features and enhanced user experience without sacrificing the simplicity and power of the original.

## Key Features

### 1. Whois in Active Window
Say goodbye to context switching! Whois responses now appear directly in your current chat window, keeping your workflow smooth and uninterrupted.

### 2. Native Sidepanels with Mouse Support
- Full mouse interaction for channel and user lists
- Seamless navigation without leaving keyboard mode
- Intuitive panel management

### 3. Native Nick Alignment for Chat Windows
- Perfectly aligned nicknames for enhanced readability
- Consistent formatting across different chat windows
- Improved conversation tracking

## Installation

### Prerequisites
- Git
- Build essentials (gcc, make, etc.)
- Perl development libraries

### Quick Installation

```bash
# Clone the repository
git clone https://github.com/kofany/irssi.git
cd irssi

# Run the prekonfigurator
./prekonfigurator.sh

# Install evolved irssi
make
make install
```

### Running Evolved Irssi

After installation, you can run erssi using:

```bash
erssi  # Launches the evolved version
irssi  # Still launches the standard version
```

## Configuration

Evolved Irssi maintains full compatibility with:
- Existing irssi configuration files
- Perl scripts
- Themes

Your current `~/.irssi/config` will work seamlessly with erssi.

## Compatibility

- **Perl Scripts**: 100% compatible with existing scripts
- **Themes**: All standard irssi themes work without modifications
- **System Irssi**: Can coexist with standard irssi installation

## Contributing

We welcome contributions! Here's how you can help:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/awesome-enhancement`)
3. Commit your changes (`git commit -m 'Add some awesome feature'`)
4. Push to the branch (`git push origin feature/awesome-enhancement`)
5. Open a Pull Request

### Reporting Issues
- Use GitHub Issues
- Provide detailed description
- Include your system configuration
- If possible, provide reproducible steps

## Performance & Resource Usage

Evolved Irssi is designed to be lightweight:
- Minimal additional memory footprint
- Negligible performance overhead
- Maintains the speed of classic irssi

## License

Evolved Irssi is released under the GNU General Public License (GPL), consistent with the original irssi project.

## Acknowledgments

- [Irssi Core Team](https://irssi.org/) - Original irssi project
- [Open Source Community](https://github.com/irssi/irssi) - Continuous inspiration and support

## Contact & Support

- **GitHub**: [kofany/irssi](https://github.com/kofany/irssi)
- **IRC**: `#erssi` on Libera Chat
- **Email**: [project maintainer email]

---

**Evolved Irssi**: Modernizing IRC, One Feature at a Time. 💬