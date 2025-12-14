# Contributing to Anvil

First off, thank you for considering contributing to Anvil! It's people like you that make Anvil such a great tool for the Laravel and Linux communities.

## Code of Conduct

This project and everyone participating in it is governed by our Code of Conduct. By participating, you are expected to uphold this code. Please report unacceptable behavior to the project maintainers.

## How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check the existing issues to avoid duplicates. When you create a bug report, include as many details as possible:

- **Use a clear and descriptive title**
- **Describe the exact steps to reproduce the problem**
- **Provide specific examples**
- **Describe the behavior you observed and what you expected**
- **Include screenshots if relevant**
- **Include your environment details** (Linux distro, Qt version, etc.)

### Suggesting Enhancements

Enhancement suggestions are tracked as GitHub issues. When creating an enhancement suggestion:

- **Use a clear and descriptive title**
- **Provide a detailed description of the suggested enhancement**
- **Explain why this enhancement would be useful**
- **List any alternatives you've considered**

### Pull Requests

1. **Fork the repo** and create your branch from `main`
2. **Follow the coding standards** (see below)
3. **Add tests** if you've added code
4. **Ensure the test suite passes**
5. **Update documentation** as needed
6. **Write clear commit messages**

## Development Process

### Setting Up Development Environment

```bash
# Clone your fork
git clone https://github.com/your-username/anvil.git
cd anvil

# Add upstream remote
git remote add upstream https://github.com/original/anvil.git

# Create a feature branch
git checkout -b feature/my-new-feature

# Install development tools
sudo apt install clang-format clang-tidy cppcheck qt6-base-dev-tools
```

### Coding Standards

We follow modern C++ best practices and Qt conventions:

#### C++ Style Guide

- **Indentation**: 4 spaces (no tabs)
- **Braces**: Allman style for classes/functions, K&R for control flow
- **Naming Conventions**:
  - Classes: `PascalCase` (e.g., `ServiceManager`)
  - Functions: `camelCase` (e.g., `startService()`)
  - Variables: `camelCase` (e.g., `phpVersion`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_RETRY_COUNT`)
  - Private members: prefix with `m_` (e.g., `m_isRunning`)

#### Code Formatting

We use `clang-format` for consistent formatting:

```bash
# Format all source files
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting
clang-format --dry-run --Werror src/**/*.{cpp,h}
```

#### Example Code Style

```cpp
// Good
class ServiceManager : public QObject
{
    Q_OBJECT

public:
    explicit ServiceManager(QObject* parent = nullptr);

    bool startService(const QString& serviceName);
    void stopAllServices();

signals:
    void serviceStarted(const QString& name);
    void serviceError(const QString& error);

private slots:
    void handleServiceStatus();

private:
    bool m_isRunning{false};
    QMap<QString, Service*> m_services;

    void initializeServices();
    bool validateServiceName(const QString& name) const;
};
```

### Qt Best Practices

- Use Qt's memory management (parent-child relationships)
- Prefer `QSharedPointer` and `QScopedPointer` for ownership
- Use signals/slots for loose coupling
- Leverage Qt containers (`QVector`, `QMap`, etc.)
- Use `QThread` and `QtConcurrent` for threading
- Implement proper RAII patterns

### Memory Efficiency Guidelines

1. **Avoid memory leaks**: Always use smart pointers or Qt parent ownership
2. **Use lazy loading**: Load resources only when needed
3. **Implement caching wisely**: Cache with size limits and expiration
4. **Profile regularly**: Use Valgrind and Qt profiling tools
5. **Minimize copies**: Use references and move semantics

### Testing

Write tests for all new features and bug fixes:

```cpp
// tests/test_service_manager.cpp
#include <QtTest/QtTest>
#include "core/ServiceManager.h"

class TestServiceManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testStartService();
    void testStopService();
    void cleanupTestCase();

private:
    ServiceManager* m_manager{nullptr};
};
```

Run tests before submitting:

```bash
cd build
cmake -DENABLE_TESTING=ON ..
cmake --build .
ctest --output-on-failure
```

### Documentation

- Update README.md for user-facing changes
- Add inline documentation for complex logic
- Use Doxygen-style comments for public APIs:

```cpp
/**
 * @brief Starts a system service
 *
 * This method attempts to start the specified service and returns
 * whether the operation was successful.
 *
 * @param serviceName The name of the service to start
 * @return true if service started successfully, false otherwise
 * @throws ServiceException if service name is invalid
 */
bool startService(const QString& serviceName);
```

## Commit Message Guidelines

Write clear, concise commit messages:

```
feat: add PHP 8.4 support

- Implement PHP 8.4 installation process
- Update configuration templates
- Add tests for new version

Closes #123
```

### Commit Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

## Project Structure Guidelines

When adding new features, follow this structure:

```
src/
├── core/           # Business logic (no Qt UI dependencies)
├── ui/             # Qt UI components
├── services/       # System service interactions
├── utils/          # Helper utilities
└── models/         # Data models
```

## Performance Considerations

- Profile before optimizing
- Use Qt's benchmark tools for performance-critical code
- Minimize system calls
- Cache expensive operations
- Use async operations for I/O

## Security Considerations

- Validate all user input
- Use secure methods for privilege escalation
- Never store sensitive data in plain text
- Sanitize file paths and system commands
- Use Qt's secure coding practices

## Questions?

Feel free to open a discussion on GitHub or reach out to the maintainers.

Thank you for contributing to Anvil! 🔨
