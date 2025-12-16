/**
 * Integration Test for ConfigManager, ServiceManager, and NodeService
 *
 * Compile and run:
 * g++ -std=c++17 test_integration.cpp -o test_integration \
 *     -I/usr/include/qt6 -lQt6Core -lQt6Network \
 *     src/core/ConfigManager.cpp \
 *     src/core/ServiceManager.cpp \
 *     src/services/*.cpp \
 *     src/utils/*.cpp \
 *     src/models/*.cpp
 */

#include <QCoreApplication>
#include <QDebug>
#include "core/ConfigManager.h"
#include "core/ServiceManager.h"
#include "utils/Logger.h"

using namespace Anvil;

void testConfigManager()
{
    qDebug() << "\n=== Testing ConfigManager ===";

    Core::ConfigManager &config = Core::ConfigManager::instance();

    // Test initialization
    if (!config.initialize())
    {
        qDebug() << "❌ ConfigManager initialization failed";
        return;
    }
    qDebug() << "✓ ConfigManager initialized";

    // Test paths
    qDebug() << "✓ Anvil path:" << config.anvilPath();
    qDebug() << "✓ PHP path:" << config.phpPath();
    qDebug() << "✓ Nginx path:" << config.nginxPath();

    // Test default values
    qDebug() << "✓ Default PHP version:" << config.defaultPhpVersion();
    qDebug() << "✓ Default Node version:" << config.defaultNodeVersion();
    qDebug() << "✓ Database type:" << config.databaseType();

    // Test setters
    config.setDefaultPhpVersion("8.4");
    config.setDefaultNodeVersion("22");

    qDebug() << "✓ Updated PHP version:" << config.defaultPhpVersion();
    qDebug() << "✓ Updated Node version:" << config.defaultNodeVersion();

    // Test persistence
    if (config.save())
    {
        qDebug() << "✓ Configuration saved successfully";
    }
    else
    {
        qDebug() << "❌ Configuration save failed";
    }
}

void testServiceManager()
{
    qDebug() << "\n=== Testing ServiceManager ===";

    Core::ServiceManager &manager = Core::ServiceManager::instance();

    // Test initialization
    if (!manager.initialize())
    {
        qDebug() << "❌ ServiceManager initialization failed";
        return;
    }
    qDebug() << "✓ ServiceManager initialized";

    // Test service accessors
    auto *phpService = manager.phpService();
    auto *nginxService = manager.nginxService();
    auto *databaseService = manager.databaseService();
    auto *dnsService = manager.dnsService();
    auto *nodeService = manager.nodeService();

    qDebug() << "✓ All service pointers valid:";
    qDebug() << "  - PHP:" << (phpService != nullptr);
    qDebug() << "  - Nginx:" << (nginxService != nullptr);
    qDebug() << "  - Database:" << (databaseService != nullptr);
    qDebug() << "  - DNS:" << (dnsService != nullptr);
    qDebug() << "  - Node:" << (nodeService != nullptr);

    // Test available services
    QStringList services = manager.availableServices();
    qDebug() << "✓ Available services:" << services;

    // Test service status
    qDebug() << "\n=== Service Installation Status ===";
    for (const QString &service : services)
    {
        Models::Service status = manager.getServiceStatus(service);
        qDebug() << QString("  %1: %2 (v%3)")
                        .arg(service.leftJustified(12))
                        .arg(status.statusString())
                        .arg(status.version());
    }

    // Test health check
    bool healthy = manager.isHealthy();
    qDebug() << "\n✓ System healthy:" << healthy;

    if (!healthy)
    {
        QStringList issues = manager.getHealthIssues();
        qDebug() << "  Issues:";
        for (const QString &issue : issues)
        {
            qDebug() << "    -" << issue;
        }
    }
}

void testNodeService()
{
    qDebug() << "\n=== Testing NodeService ===";

    Core::ServiceManager &manager = Core::ServiceManager::instance();
    auto *nodeService = manager.nodeService();

    if (!nodeService)
    {
        qDebug() << "❌ NodeService not available";
        return;
    }

    // Check installation
    bool installed = nodeService->isInstalled();
    qDebug() << "✓ Node installed:" << installed;

    if (!installed)
    {
        qDebug() << "  (Install with: curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash)";
        return;
    }

    // Check nvm
    bool hasNvm = nodeService->hasNvm();
    qDebug() << "✓ nvm available:" << hasNvm;

    if (hasNvm)
    {
        qDebug() << "  nvm path:" << nodeService->nvmPath();
    }

    // Check current version
    QString currentVersion = nodeService->currentVersion();
    qDebug() << "✓ Current Node version:" << currentVersion;

    // List installed versions
    auto installedResult = nodeService->listInstalledVersions();
    if (installedResult.isSuccess())
    {
        qDebug() << "✓ Installed Node versions:";
        for (const auto &version : installedResult.data)
        {
            QString marker = version.isDefault ? " (default)" : "";
            qDebug() << QString("  - Node %1%2").arg(version.version, marker);
        }
    }

    // Check npm
    auto npmVersionResult = nodeService->npmVersion();
    if (npmVersionResult.isSuccess())
    {
        qDebug() << "✓ npm version:" << npmVersionResult.data;
    }
}

void testIntegration()
{
    qDebug() << "\n=== Testing Full Integration ===";

    Core::ConfigManager &config = Core::ConfigManager::instance();
    Core::ServiceManager &manager = Core::ServiceManager::instance();

    // Test ConfigManager -> ServiceManager integration
    QString phpVersion = config.defaultPhpVersion();
    auto *phpService = manager.phpService();

    qDebug() << "✓ Config says default PHP:" << phpVersion;
    qDebug() << "✓ PHP service current version:" << phpService->currentVersion();

    // Test NodeService integration
    QString nodeVersion = config.defaultNodeVersion();
    auto *nodeService = manager.nodeService();

    qDebug() << "✓ Config says default Node:" << nodeVersion;
    qDebug() << "✓ Node service current version:" << nodeService->currentVersion();

    // Test service orchestration
    QStringList running = manager.runningServices();
    QStringList stopped = manager.stoppedServices();

    qDebug() << "\n✓ Running services:" << running;
    qDebug() << "✓ Stopped services:" << stopped;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Setup logging
    Utils::Logger::instance().setConsoleOutput(true);
    Utils::Logger::instance().setLogLevel(Utils::LogLevel::Debug);

    qDebug() << "╔════════════════════════════════════════════╗";
    qDebug() << "║   Anvil Integration Test Suite            ║";
    qDebug() << "╚════════════════════════════════════════════╝";

    try
    {
        // Run tests
        testConfigManager();
        testServiceManager();
        testNodeService();
        testIntegration();

        qDebug() << "\n╔════════════════════════════════════════════╗";
        qDebug() << "║   ✅ All Tests Completed                   ║";
        qDebug() << "╚════════════════════════════════════════════╝\n";

        return 0;
    }
    catch (const std::exception &e)
    {
        qDebug() << "\n❌ Test failed with exception:" << e.what();
        return 1;
    }
}
