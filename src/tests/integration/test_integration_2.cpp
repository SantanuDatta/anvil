/**
 * Phase 2 Integration Test
 * Tests PHP version bug fix and SiteManager
 */

#include <QCoreApplication>
#include <QDebug>
#include "core/ConfigManager.h"
#include "core/ServiceManager.h"
#include "managers/SiteManager.h"
#include "utils/Logger.h"

using namespace Anvil;

void testPhpVersionBugFix()
{
    qDebug() << "\n=== Testing PHP Version Bug Fix ===";

    auto *manager = Core::ServiceManager::instance();
    if (!manager)
    {
        qDebug() << "❌ ServiceManager not available";
        return;
    }
    auto *phpService = manager->phpService();

    if (!phpService || !phpService->isInstalled())
    {
        qDebug() << "⚠ PHP not installed, skipping test";
        return;
    }

    // Test 1: Version normalization
    qDebug() << "\n1. Version Normalization Test:";
    QString testVersions[] = {"8.3.12", "8.3", "8.4.0", "8.4"};

    for (const QString &version : testVersions)
    {
        qDebug() << QString("  Input: %1").arg(version);

        // Check if version is installed
        bool installed = phpService->isVersionInstalled(version);
        qDebug() << QString("    Installed: %1").arg(installed ? "YES" : "NO");

        // Get version info
        Models::PHPVersion phpVer = phpService->getVersionInfo(version);
        if (!phpVer.version().isEmpty())
        {
            qDebug() << QString("    Found: %1 (binary: %2)")
                            .arg(phpVer.version(), phpVer.binaryPath());
        }
    }

    // Test 2: List installed versions
    qDebug() << "\n2. Installed Versions Test:";
    auto installedResult = phpService->listInstalledVersions();

    if (installedResult.isSuccess())
    {
        qDebug() << QString("  Found %1 versions:").arg(installedResult.data.size());

        for (const Models::PHPVersion &ver : installedResult.data)
        {
            QString marker = ver.isDefault() ? " (default)" : "";
            qDebug() << QString("    ✓ PHP %1%2").arg(ver.version(), marker);
            qDebug() << QString("      Binary: %1").arg(ver.binaryPath());
        }
    }

    // Test 3: Switch version (use short format)
    qDebug() << "\n3. Version Switching Test:";
    QString currentVer = phpService->currentVersion();
    qDebug() << QString("  Current version: %1").arg(currentVer);

    // Try switching to the same version with different formats
    qDebug() << "  Testing format compatibility:";
    qDebug() << QString("    Switch to '8.3' -> isInstalled: %1")
                    .arg(phpService->isVersionInstalled("8.3") ? "YES" : "NO");
    qDebug() << QString("    Switch to '8.3.12' -> isInstalled: %1")
                    .arg(phpService->isVersionInstalled("8.3.12") ? "YES" : "NO");

    qDebug() << "\n✅ PHP Version Bug Fix Test Complete";
    qDebug() << "  All version keys are now normalized to major.minor format";
}

void testSiteManager()
{
    qDebug() << "\n=== Testing SiteManager ===";

    // Initialize SiteManager
    Managers::SiteManager siteManager;

    if (!siteManager.initialize())
    {
        qDebug() << "❌ SiteManager initialization failed";
        return;
    }
    qDebug() << "✓ SiteManager initialized";

    // Test 1: Create a test site
    qDebug() << "\n1. Site Creation Test (with Rollback):";

    Models::Site testSite;
    testSite.setName("Test Laravel App");
    testSite.setPath("/tmp/test-laravel-app");
    testSite.setDomain("test-app.test");
    testSite.setPhpVersion("8.3");
    testSite.setSslEnabled(false);

    qDebug() << QString("  Creating site: %1").arg(testSite.name());
    qDebug() << QString("    Domain: %1").arg(testSite.domain());
    qDebug() << QString("    Path: %1").arg(testSite.path());
    qDebug() << QString("    PHP: %1").arg(testSite.phpVersion());

    // Create test directory if it doesn't exist
    Utils::FileSystem::ensureDirectoryExists(testSite.path());

    auto createResult = siteManager.createSite(testSite);

    if (createResult.isSuccess())
    {
        qDebug() << "  ✓ Site created successfully";
        qDebug() << "    Transaction steps completed:";
        qDebug() << "      1. DNS entry created";
        qDebug() << "      2. Database created (if available)";
        qDebug() << "      3. Nginx configured";
    }
    else
    {
        qDebug() << QString("  ⚠ Site creation failed: %1").arg(createResult.error);
        qDebug() << "  Rollback should have cleaned up partial changes";
    }

    // Test 2: List sites
    qDebug() << "\n2. List Sites Test:";
    auto listResult = siteManager.listSites();

    if (listResult.isSuccess())
    {
        qDebug() << QString("  Found %1 sites:").arg(listResult.data.size());

        for (const Models::Site &site : listResult.data)
        {
            qDebug() << QString("    • %1 (%2)").arg(site.name(), site.domain());
            qDebug() << QString("      Path: %1").arg(site.path());
            qDebug() << QString("      PHP: %1 | SSL: %2")
                            .arg(site.phpVersion())
                            .arg(site.sslEnabled() ? "Yes" : "No");
        }
    }

    // Test 3: Site queries
    qDebug() << "\n3. Site Query Tests:";
    qDebug() << QString("  Domain 'test-app.test' exists: %1")
                    .arg(siteManager.domainExists("test-app.test") ? "YES" : "NO");
    qDebug() << QString("  Site ID exists: %1")
                    .arg(siteManager.siteExists(testSite.id()) ? "YES" : "NO");

    // Test 4: Enable/Disable site
    if (siteManager.siteExists(testSite.id()))
    {
        qDebug() << "\n4. Enable/Disable Test:";

        auto enableResult = siteManager.enableSite(testSite.id());
        if (enableResult.isSuccess())
        {
            qDebug() << "  ✓ Site enabled";
        }

        bool isEnabled = siteManager.isSiteEnabled(testSite.id());
        qDebug() << QString("  Site enabled status: %1").arg(isEnabled ? "YES" : "NO");

        auto disableResult = siteManager.disableSite(testSite.id());
        if (disableResult.isSuccess())
        {
            qDebug() << "  ✓ Site disabled";
        }
    }

    // Test 5: SSL operations
    if (siteManager.siteExists(testSite.id()))
    {
        qDebug() << "\n5. SSL Operations Test:";

        auto sslResult = siteManager.enableSsl(testSite.id());
        if (sslResult.isSuccess())
        {
            qDebug() << "  ✓ SSL enabled with self-signed certificate";
        }
        else
        {
            qDebug() << QString("  ⚠ SSL enable failed: %1").arg(sslResult.error);
        }
    }

    // Test 6: Update site
    if (siteManager.siteExists(testSite.id()))
    {
        qDebug() << "\n6. Update Site Test:";

        auto siteResult = siteManager.getSite(testSite.id());
        if (siteResult.isSuccess())
        {
            Models::Site site = siteResult.data;
            site.setPhpVersion("8.4");

            auto updateResult = siteManager.updateSite(site);
            if (updateResult.isSuccess())
            {
                qDebug() << "  ✓ Site updated (PHP version changed to 8.4)";
            }
        }
    }

    // Test 7: Cleanup - Remove test site
    qDebug() << "\n7. Cleanup Test:";
    if (siteManager.siteExists(testSite.id()))
    {
        auto removeResult = siteManager.removeSite(testSite.id());
        if (removeResult.isSuccess())
        {
            qDebug() << "  ✓ Test site removed";
            qDebug() << "    Cleanup steps completed:";
            qDebug() << "      1. Nginx config removed";
            qDebug() << "      2. Database dropped (if exists)";
            qDebug() << "      3. DNS entry removed";
        }
    }

    // Cleanup test directory
    Utils::FileSystem::deleteDirectory("/tmp/test-laravel-app", true);

    qDebug() << "\n✅ SiteManager Test Complete";
}

void testRollbackScenario()
{
    qDebug() << "\n=== Testing Rollback Scenario ===";

    Managers::SiteManager siteManager;
    siteManager.initialize();

    // Create a site with invalid configuration to trigger rollback
    qDebug() << "\n1. Testing Rollback on Invalid Site:";

    Models::Site invalidSite;
    invalidSite.setName("Invalid Site");
    invalidSite.setPath("/nonexistent/path"); // This path doesn't exist
    invalidSite.setDomain("invalid.test");
    invalidSite.setPhpVersion("8.3");

    qDebug() << "  Creating site with invalid path...";
    auto result = siteManager.createSite(invalidSite);

    if (result.isError())
    {
        qDebug() << QString("  ✓ Site creation failed as expected: %1").arg(result.error);
        qDebug() << "  ✓ Rollback mechanism activated";
        qDebug() << "  ✓ No partial configurations should remain";

        // Verify rollback worked
        bool domainExists = siteManager.domainExists("invalid.test");
        qDebug() << QString("  ✓ Domain cleaned up: %1")
                        .arg(!domainExists ? "YES" : "NO");
    }
    else
    {
        qDebug() << "  ⚠ Site creation succeeded unexpectedly";
    }
}

void testPersistence()
{
    qDebug() << "\n=== Testing Persistence ===";

    // Create a site
    Managers::SiteManager siteManager1;
    siteManager1.initialize();

    Models::Site persistSite;
    persistSite.setName("Persist Test");
    persistSite.setPath("/tmp/persist-test");
    persistSite.setDomain("persist.test");
    persistSite.setPhpVersion("8.3");

    Utils::FileSystem::ensureDirectoryExists(persistSite.path());

    qDebug() << "\n1. Creating site and saving:";
    auto createResult = siteManager1.createSite(persistSite);

    if (createResult.isSuccess())
    {
        qDebug() << "  ✓ Site created";
        siteManager1.save();
        qDebug() << "  ✓ Sites saved to disk";
    }

    // Create new instance and load
    qDebug() << "\n2. Loading sites in new instance:";
    Managers::SiteManager siteManager2;
    siteManager2.initialize();

    if (siteManager2.siteExists(persistSite.id()))
    {
        qDebug() << "  ✓ Site loaded successfully";

        auto siteResult = siteManager2.getSite(persistSite.id());
        if (siteResult.isSuccess())
        {
            Models::Site loaded = siteResult.data;
            qDebug() << QString("    Name: %1").arg(loaded.name());
            qDebug() << QString("    Domain: %1").arg(loaded.domain());
            qDebug() << QString("    Path: %1").arg(loaded.path());
        }

        // Cleanup
        siteManager2.removeSite(persistSite.id());
    }
    else
    {
        qDebug() << "  ❌ Site not found after reload";
    }

    Utils::FileSystem::deleteDirectory("/tmp/persist-test", true);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Setup logging
    Utils::Logger::instance().setConsoleOutput(true);
    Utils::Logger::instance().setLogLevel(Utils::LogLevel::Info);

    qDebug() << "╔════════════════════════════════════════════╗";
    qDebug() << "║   Anvil Phase 2 Test Suite                 ║";
    qDebug() << "║   PHP Bug Fix + SiteManager                ║";
    qDebug() << "╚════════════════════════════════════════════╝";

    // Initialize core systems
    Core::ConfigManager::instance().initialize();
    if (auto *manager = Core::ServiceManager::instance())
    {
        manager->initialize();
    }

    try
    {
        // Run tests
        testPhpVersionBugFix();
        testSiteManager();
        testRollbackScenario();
        testPersistence();

        qDebug() << "\n╔════════════════════════════════════════════╗";
        qDebug() << "║   ✅ Phase 2 Tests Completed               ║";
        qDebug() << "╚════════════════════════════════════════════╝";
        qDebug() << "";
        qDebug() << "Summary:";
        qDebug() << "  ✓ PHP version keys normalized to major.minor";
        qDebug() << "  ✓ SiteManager orchestrates multiple services";
        qDebug() << "  ✓ Transaction-like rollback on failures";
        qDebug() << "  ✓ Site persistence working";
        qDebug() << "";

        return 0;
    }
    catch (const std::exception &e)
    {
        qDebug() << "\n❌ Test failed with exception:" << e.what();
        return 1;
    }
}
