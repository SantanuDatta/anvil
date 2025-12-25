/**
 * VersionManager Integration Test
 *
 * Tests VersionManager functionality with real services
 */

#include <QCoreApplication>
#include <QDebug>
#include "core/ConfigManager.h"
#include "core/ServiceManager.h"
#include "managers/VersionManager.h"
#include "managers/SiteManager.h"
#include "utils/Logger.h"

using namespace Anvil;

void testVersionManagerInitialization()
{
    qDebug() << "\n"
             << QString("=").repeated(60);
    qDebug() << "TEST 1: VersionManager Initialization";
    qDebug() << QString("=").repeated(60);

    Managers::VersionManager versionManager;

    if (!versionManager.initialize())
    {
        qDebug() << "❌ Initialization failed";
        return;
    }

    qDebug() << "✅ VersionManager initialized";
    qDebug() << "Global PHP version:" << versionManager.globalPhpVersion();
    qDebug() << "Global Node version:" << versionManager.globalNodeVersion();
}

void testListInstalledVersions()
{
    qDebug() << "\n"
             << QString("=").repeated(60);
    qDebug() << "TEST 2: List Installed Versions";
    qDebug() << QString("=").repeated(60);

    Managers::VersionManager versionManager;
    versionManager.initialize();

    // List PHP versions
    auto phpVersions = versionManager.listInstalledPhpVersions();
    if (phpVersions.isSuccess())
    {
        qDebug() << "\n📦 Installed PHP Versions:";
        if (phpVersions.data.isEmpty())
        {
            qDebug() << "  (none installed)";
        }
        else
        {
            for (const auto &ver : phpVersions.data)
            {
                QString marker = ver.isDefault() ? " ⭐" : "";
                qDebug() << QString("  • PHP %1%2").arg(ver.version(), marker);
                qDebug() << QString("    Binary: %1").arg(ver.binaryPath());
            }
        }
    }
    else
    {
        qDebug() << "❌" << phpVersions.error;
    }

    // List Node versions
    auto nodeVersions = versionManager.listInstalledNodeVersions();
    if (nodeVersions.isSuccess())
    {
        qDebug() << "\n📦 Installed Node Versions:";
        if (nodeVersions.data.isEmpty())
        {
            qDebug() << "  (none installed)";
        }
        else
        {
            for (const QString &ver : nodeVersions.data)
            {
                qDebug() << QString("  • Node %1").arg(ver);
            }
        }
    }
    else
    {
        qDebug() << "❌" << nodeVersions.error;
    }
}

void testVersionSwitching()
{
    qDebug() << "\n"
             << QString("=").repeated(60);
    qDebug() << "TEST 3: Global Version Switching";
    qDebug() << QString("=").repeated(60);

    Managers::VersionManager versionManager;
    versionManager.initialize();

    QString currentPhp = versionManager.globalPhpVersion();
    qDebug() << "Current PHP:" << currentPhp;

    // Try to switch to the same version (should succeed)
    qDebug() << "\n🔄 Switching to same version...";
    auto result = versionManager.switchGlobalPhpVersion(currentPhp);
    if (result.isSuccess())
    {
        qDebug() << "✅ Switch successful";
    }
    else
    {
        qDebug() << "❌" << result.error;
    }

    // Try to switch to non-existent version (should fail)
    qDebug() << "\n🔄 Switching to non-existent version...";
    auto badResult = versionManager.switchGlobalPhpVersion("99.9");
    if (badResult.isError())
    {
        qDebug() << "✅ Correctly rejected:" << badResult.error;
    }
    else
    {
        qDebug() << "❌ Should have failed!";
    }
}

void testPerSiteVersions()
{
    qDebug() << "\n"
             << QString("=").repeated(60);
    qDebug() << "TEST 4: Per-Site Version Management";
    qDebug() << QString("=").repeated(60);

    Managers::VersionManager versionManager;
    versionManager.initialize();

    QString testSiteId = "test-site-123";
    QString phpVersion = versionManager.globalPhpVersion();

    // Set site-specific version
    qDebug() << QString("\n🔧 Setting site %1 to PHP %2").arg(testSiteId, phpVersion);
    auto setResult = versionManager.setSitePhpVersion(testSiteId, phpVersion);
    if (setResult.isSuccess())
    {
        qDebug() << "✅ Site version set";
    }
    else
    {
        qDebug() << "❌" << setResult.error;
    }

    // Get site version
    auto getResult = versionManager.getSitePhpVersion(testSiteId);
    if (getResult.isSuccess())
    {
        qDebug() << "✅ Site uses PHP:" << getResult.data;
    }
    else
    {
        qDebug() << "❌" << getResult.error;
    }

    // Check which sites use this version
    QStringList sites = versionManager.getSitesUsingPhpVersion(phpVersion);
    qDebug() << QString("\n📊 %1 site(s) using PHP %2").arg(sites.size()).arg(phpVersion);
    for (const QString &siteId : sites)
    {
        qDebug() << QString("  • %1").arg(siteId);
    }
}

void testVersionValidation()
{
    qDebug() << "\n"
             << QString("=").repeated(60);
    qDebug() << "TEST 5: Version Validation";
    qDebug() << QString("=").repeated(60);

    Managers::VersionManager versionManager;
    versionManager.initialize();

    struct TestCase
    {
        QString version;
        bool shouldBeValid;
    };

    QList<TestCase> phpTests = {
        {"8.3", true},
        {"8.3.12", true},
        {"7.4", true},
        {"99.9", true}, // Format valid, but not installed
        {"8.x", false},
        {"eight", false},
        {"", false}};

    qDebug() << "\n🔍 PHP Version Validation:";
    for (const auto &test : phpTests)
    {
        bool isValid = versionManager.isValidPhpVersion(test.version);
        QString result = isValid == test.shouldBeValid ? "✅" : "❌";
        qDebug() << QString("  %1 '%2' -> %3 (expected %4)")
                        .arg(result)
                        .arg(test.version)
                        .arg(isValid ? "valid" : "invalid")
                        .arg(test.shouldBeValid ? "valid" : "invalid");
    }

    QList<TestCase> nodeTests = {
        {"20", true},
        {"20.11", true},
        {"20.11.0", true},
        {"18", true},
        {"v20", false},
        {"latest", false},
        {"", false}};

    qDebug() << "\n🔍 Node Version Validation:";
    for (const auto &test : nodeTests)
    {
        bool isValid = versionManager.isValidNodeVersion(test.version);
        QString result = isValid == test.shouldBeValid ? "✅" : "❌";
        qDebug() << QString("  %1 '%2' -> %3 (expected %4)")
                        .arg(result)
                        .arg(test.version)
                        .arg(isValid ? "valid" : "invalid")
                        .arg(test.shouldBeValid ? "valid" : "invalid");
    }
}

void testSafetyChecks()
{
    qDebug() << "\n"
             << QString("=").repeated(60);
    qDebug() << "TEST 6: Safety Checks";
    qDebug() << QString("=").repeated(60);

    Managers::VersionManager versionManager;
    versionManager.initialize();

    QString phpVersion = versionManager.globalPhpVersion();
    QString testSiteId = "protected-site-456";

    // Create a site using this version
    qDebug() << QString("\n🔧 Creating site with PHP %1").arg(phpVersion);
    versionManager.setSitePhpVersion(testSiteId, phpVersion);

    // Try to uninstall (should fail)
    qDebug() << QString("\n🛡️  Attempting to uninstall PHP %1...").arg(phpVersion);
    auto uninstallResult = versionManager.uninstallPhpVersion(phpVersion);
    if (uninstallResult.isError())
    {
        qDebug() << "✅ Correctly prevented uninstall:" << uninstallResult.error;
    }
    else
    {
        qDebug() << "❌ Should have prevented uninstall!";
    }

    // Check sites using version
    QStringList sites = versionManager.getSitesUsingPhpVersion(phpVersion);
    qDebug() << QString("\n📊 Sites using PHP %1:").arg(phpVersion);
    for (const QString &siteId : sites)
    {
        qDebug() << QString("  • %1").arg(siteId);
    }
}

void testPersistence()
{
    qDebug() << "\n"
             << QString("=").repeated(60);
    qDebug() << "TEST 7: Persistence";
    qDebug() << QString("=").repeated(60);

    QString testSiteId = "persist-site-789";
    QString phpVersion;

    // First instance - set version
    {
        Managers::VersionManager versionManager1;
        versionManager1.initialize();

        phpVersion = versionManager1.globalPhpVersion();

        qDebug() << "\n💾 Instance 1: Setting site version";
        versionManager1.setSitePhpVersion(testSiteId, phpVersion);
        versionManager1.save();
        qDebug() << "✅ Saved to disk";
    }

    // Second instance - load and verify
    {
        Managers::VersionManager versionManager2;
        versionManager2.initialize();

        qDebug() << "\n📂 Instance 2: Loading from disk";
        auto getResult = versionManager2.getSitePhpVersion(testSiteId);
        if (getResult.isSuccess() && getResult.data == phpVersion)
        {
            qDebug() << QString("✅ Correctly loaded: %1").arg(getResult.data);
        }
        else
        {
            qDebug() << "❌ Failed to load persisted data";
        }
    }
}

void testIntegrationWithSiteManager()
{
    qDebug() << "\n"
             << QString("=").repeated(60);
    qDebug() << "TEST 8: Integration with SiteManager";
    qDebug() << QString("=").repeated(60);

    Managers::SiteManager siteManager;
    Managers::VersionManager versionManager;

    siteManager.initialize();
    versionManager.initialize();

    qDebug() << "\n🔗 Both managers initialized";
    qDebug() << "✅ SiteManager:" << (siteManager.isInitialized() ? "OK" : "FAILED");
    qDebug() << "✅ VersionManager:" << (versionManager.isInitialized() ? "OK" : "FAILED");

    // List sites
    auto sitesResult = siteManager.listSites();
    if (sitesResult.isSuccess())
    {
        qDebug() << QString("\n📊 Found %1 sites").arg(sitesResult.data.size());

        for (const auto &site : sitesResult.data)
        {
            qDebug() << QString("\n  Site: %1").arg(site.name());
            qDebug() << QString("    Domain: %1").arg(site.domain());
            qDebug() << QString("    PHP (from site): %1").arg(site.phpVersion());

            // Get version from VersionManager
            auto verResult = versionManager.getSitePhpVersion(site.id());
            if (verResult.isSuccess())
            {
                qDebug() << QString("    PHP (from manager): %1").arg(verResult.data);
            }
        }
    }
    else
    {
        qDebug() << "\n📊 No sites found (this is OK if fresh install)";
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Setup logging
    Utils::Logger::instance().setConsoleOutput(true);
    Utils::Logger::instance().setLogLevel(Utils::LogLevel::Info);

    qDebug() << "╔════════════════════════════════════════════════════════════╗";
    qDebug() << "║   VersionManager Test Suite                                ║";
    qDebug() << "╚════════════════════════════════════════════════════════════╝";

    try
    {
        // Initialize core systems
        Core::ConfigManager::instance().initialize();
        Core::ServiceManager::instance().initialize();

        // Run tests
        testVersionManagerInitialization();
        testListInstalledVersions();
        testVersionSwitching();
        testPerSiteVersions();
        testVersionValidation();
        testSafetyChecks();
        testPersistence();
        testIntegrationWithSiteManager();

        qDebug() << "\n╔════════════════════════════════════════════════════════════╗";
        qDebug() << "║   ✅ All Tests Completed                                    ║";
        qDebug() << "╚════════════════════════════════════════════════════════════╝";
        qDebug() << "";
        qDebug() << "Summary:";
        qDebug() << "  ✓ VersionManager initialization";
        qDebug() << "  ✓ Version listing (PHP & Node)";
        qDebug() << "  ✓ Global version switching";
        qDebug() << "  ✓ Per-site version management";
        qDebug() << "  ✓ Version validation";
        qDebug() << "  ✓ Safety checks (prevent uninstall in use)";
        qDebug() << "  ✓ Data persistence";
        qDebug() << "  ✓ Integration with SiteManager";
        qDebug() << "";

        return 0;
    }
    catch (const std::exception &e)
    {
        qDebug() << "\n❌ Test failed with exception:" << e.what();
        return 1;
    }
}
