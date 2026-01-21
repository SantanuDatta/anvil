# Anvil

<div align="center">

A memory-efficient, robust PHP development environment manager for Linux - inspired by Laravel Herd.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Qt Version](https://img.shields.io/badge/Qt-6.x-green.svg)](https://www.qt.io/)
[![C++ Standard](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

</div>

## Overview

Anvil is a native Linux application built with Qt that provides a seamless PHP development environment management experience. It allows you to easily manage PHP versions, serve Laravel applications, and handle local development domains with minimal resource usage.

## Features

- **Multiple PHP Version Management**: Install and switch between PHP versions effortlessly
- **Local Domain Management**: Serve sites on `.test` domains automatically
- **Database Management**: Built-in support for MySQL/MariaDB and PostgreSQL
- **SSL/TLS Support**: Automatic HTTPS certificates for local development
- **System Tray Integration**: Quick access to all features from the system tray
- **Memory Efficient**: Optimized for low memory footprint
- **Service Management**: Start/stop PHP-FPM, Nginx, and database services
- **Site Parking**: Park directories to automatically serve all subdirectories

## Requirements

- **OS**: Linux (Ubuntu 20.04+, Debian 11+, Fedora 35+, or equivalent)
- **Qt**: 6.2 or higher
- **Compiler**: GCC 9+ or Clang 10+ with C++17 support
- **CMake**: 3.16 or higher
- **RAM**: 512MB minimum (1GB recommended)

## Building from Source

### Install Dependencies

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev qt6-tools-dev \
    qt6-tools-dev-tools libqt6svg6-dev libgl1-mesa-dev libssl-dev
```

#### Fedora
```bash
sudo dnf install -y gcc-c++ cmake qt6-qtbase-devel qt6-qttools-devel \
    qt6-qtsvg-devel mesa-libGL-devel openssl-devel
```

#### Arch Linux
```bash
sudo pacman -S base-devel cmake qt6-base qt6-tools qt6-svg openssl
```

### Build Steps

```bash
# Clone the repository
git clone https://github.com/yourusername/anvil.git
cd anvil

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . -j$(nproc)

# Install (optional)
sudo cmake --install .
```

## Installation

### From Release Package

Download the latest release package for your distribution:

```bash
# Debian/Ubuntu
sudo dpkg -i anvil_*.deb

# Fedora/RHEL
sudo rpm -i anvil-*.rpm

# Arch Linux
sudo pacman -U anvil-*.pkg.tar.zst
```

### Post-Installation

After installation, run the initial setup:

```bash
anvil-cli setup
```

This will:
- Install required PHP versions
- Configure Nginx
- Set up SSL certificates
- Configure system services

## Usage

### GUI Application

Launch Anvil from your application menu or run:

```bash
anvil
```

### CLI Commands

```bash
# Start services
anvil-cli start

# Stop services
anvil-cli stop

# Park a directory
anvil-cli park /path/to/projects

# Link a specific site
anvil-cli link mysite

# List all sites
anvil-cli sites

# Switch PHP version
anvil-cli php 8.3

# Install PHP version
anvil-cli php:install 8.4
```

## Project Structure

```
Anvil/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── build/                        # Build artifacts
│   └── Anvil                     # Executable
├── src/
│   ├── main.cpp                  # Application entry point
│   │
│   ├── core/                     # Core Foundation (Complete)
│   │   ├── ConfigManager.h       # Configuration management
│   │   ├── ConfigManager.cpp     # (~550 lines)
│   │   ├── ServiceManager.h      # Service orchestration
│   │   └── ServiceManager.cpp    # (~400 lines)
│   │
│   ├── services/                 # System Services (Complete)
│   │   ├── BaseService.h/cpp     # Base class (~300 lines)
│   │   ├── PHPService.h/cpp      # PHP version management (~800 lines)
│   │   ├── NodeService.h/cpp     # Node.js/nvm management (~600 lines)
│   │   ├── NginxService.h/cpp    # Web server config (~700 lines)
│   │   ├── DatabaseService.h/cpp # MySQL/PostgreSQL (~900 lines)
│   │   └── DnsService.h/cpp      # /etc/hosts management (~400 lines)
│   │
│   ├── managers/                 # Business Logic Orchestration
│   │   ├── SiteManager.h/cpp     # Site lifecycle (~700 lines)
│   │   ├── VersionManager.h/cpp  # Done
│   │   └── ProcessManager.h/cpp  # Done
│   │
│   ├── models/                  # Data Models (Complete)
│   │   ├── Site.h/cpp           # Site representation (~200 lines)
│   │   ├── PHPVersion.h/cpp     # PHP version info (~250 lines)
│   │   └── Service.h/cpp        # Service status (~150 lines)
│   │
│   ├── utils/                   # Utilities (Complete)
│   │   ├── Logger.h/cpp         # Logging system (~200 lines)
│   │   ├── Process.h/cpp        # Process execution (~600 lines)
│   │   ├── FileSystem.h/cpp     # File operations (~800 lines)
│   │   ├── Network.h/cpp        # Network utilities (~700 lines)
│   │   ├── DistroDetector.h/cpp # Linux distro detection (~300 lines)
│   │   └── resources/           # TODO
│   │       ├── icons/           # Application icons
│   │       ├── scripts/         # Helper scripts
│   │       └── templates/       # Config templates
│   │
│   ├── ui/                      # User Interface (TODO)
│   │   ├── Theme.h/cpp          # Theme system interface & imeplementation
│   │   ├── MainWindow.h/cpp     # Main application window
│   │   ├── TrayIcon.h/cpp       # System tray integration
│   │   └── dialogs/
│   │       ├── AddSiteDialog.h/cpp     # Create site wizard
│   │       └── SettingsDialog.h/cpp    # Application settings
│   │
│   └── tests/                    # Testing (Partial)
│       ├── integration/          # Integration tests (2 files)
│       │   ├── test_integration.cpp
│       │   ├── test_integration_2.cpp
│       │   ├── test_prcess_manager.cpp
│       │   └── test_version_manager.cpp
│       └── unit/                 # Unit tests (TODO)
```

## 📊 Implementation Status

### Phase 1: Core Foundation ✅ (100% Complete)
- [x] ConfigManager - JSON-based configuration
- [x] ServiceManager - Service lifecycle orchestration
- [x] All Services (PHP, Node, Nginx, Database, DNS)
- [x] Data Models (Site, PHPVersion, Service)
- [x] Utilities (Logger, Process, FileSystem, Network)
- [x] Security hardening (root command whitelist)

### Phase 2: Site Management ✅ (100% Complete)
- [x] SiteManager - Site creation with rollback
- [x] VersionManager - PHP/Node version switching
- [x] ProcessManager - Advanced process management
- [x] Transaction-like site operations
- [x] DNS + Database + Nginx coordination

### Phase 3: User Interface ⏳ (0% Complete)
- [ ] MainWindow with tabs
- [ ] AddSiteDialog for site creation
- [ ] SettingsDialog for preferences
- [ ] System tray integration
- [ ] Service status indicators

### Phase 4: Testing & Polish ⏳ (20% Complete)
- [x] Integration test framework
- [ ] Unit tests
- [ ] CI/CD pipeline
- [ ] Documentation
- [ ] Packaging (deb/rpm/AppImage)

## Architecture

Anvil follows a clean architecture pattern with separation of concerns:

- **Core Layer**: Business logic independent of Qt UI
- **UI Layer**: Qt-based user interface
- **Service Layer**: System interaction and service management
- **Utils Layer**: Helper functions and utilities

## Contributing

Contributions are welcome! Please read our [Contributing Guidelines](CONTRIBUTING.md) before submitting PRs.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Development Setup

```bash
# Install development dependencies
sudo apt install -y qt6-base-dev-tools clang-format clang-tidy cppcheck

# Build in debug mode
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON ..
cmake --build .

# Run tests
ctest --output-on-failure
```

## Troubleshooting

### Services won't start
```bash
# Check service status
sudo systemctl status anvil-nginx
sudo systemctl status anvil-php-fpm

# Check logs
journalctl -u anvil-nginx -f
```

### Permission issues
```bash
# Add user to anvil group
sudo usermod -aG anvil $USER
# Log out and back in
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Inspired by [Laravel Herd](https://herd.laravel.com/)
- Built with [Qt Framework](https://www.qt.io/)
- Community contributors and testers

## Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/anvil/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/anvil/discussions)
- **Documentation**: [Wiki](https://github.com/yourusername/anvil/wiki)

---

Made with ❤️ for the Laravel and Linux communities
