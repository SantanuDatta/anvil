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
anvil/
├── src/
│   ├── main.cpp              # Application entry point
│   ├── core/                 # Core business logic
│   │   ├── ServiceManager    # Manage system services
│   │   ├── PhpManager        # PHP version management
│   │   ├── NginxManager      # Nginx configuration
│   │   └── SiteManager       # Site/domain management
│   ├── ui/                   # Qt UI components
│   │   ├── MainWindow
│   │   ├── TrayIcon
│   │   └── Dialogs
│   └── utils/                # Utility classes
├── resources/                # Icons, UI files, etc.
├── tests/                    # Unit and integration tests
├── docs/                     # Documentation
└── CMakeLists.txt           # Build configuration
```

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
