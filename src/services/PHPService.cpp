#include "services/PHPService.h"
#include "core/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>

namespace Anvil::Services
{
    namespace
    {
        // CRITICAL FIX: Centralized version normalization
        QString normalizeVersion(const QString &version)
        {
            QStringList parts = version.split('.');
            if (parts.size() >= 2)
            {
                return parts[0] + "." + parts[1]; // "8.3.12" -> "8.3"
            }
            return version; // Already normalized or invalid
        }
    }

    PHPService::PHPService(QObject *parent)
        : BaseService("php", parent)
    {
        setDisplayName("PHP");
        setDescription("PHP Programming Language");

        // Detect package manager
        m_packageManager = detectPackageManager();

        // Set base path
        Core::ConfigManager &config = Core::ConfigManager::instance();
        m_phpBasePath = config.phpPath();

        // Scan for installed versions
        scanInstalledVersions();

        // Detect current system PHP version
        auto versionResult = detectSystemPhpVersion();
        if (versionResult.isSuccess())
        {
            // CRITICAL FIX: Normalize version before storing
            m_currentVersion = normalizeVersion(versionResult.data);
            setVersion(m_currentVersion);
        }
    }

    PHPService::~PHPService()
    {
    }

    ServiceResult<bool> PHPService::start()
    {
        LOG_INFO("Starting PHP-FPM service");

        if (m_currentVersion.isEmpty())
        {
            return ServiceResult<bool>::Err("No PHP version selected");
        }

        return startFpm(m_currentVersion);
    }

    ServiceResult<bool> PHPService::stop()
    {
        LOG_INFO("Stopping PHP-FPM service");

        if (m_currentVersion.isEmpty())
        {
            return ServiceResult<bool>::Err("No PHP version selected");
        }

        return stopFpm(m_currentVersion);
    }

    bool PHPService::isInstalled() const
    {
        return checkProgramExists("php");
    }

    bool PHPService::isRunning() const
    {
        if (m_currentVersion.isEmpty())
        {
            return false;
        }

        return isFpmRunning(m_currentVersion);
    }

    ServiceResult<bool> PHPService::install()
    {
        LOG_INFO("Installing PHP");

        // Install latest stable version (8.3)
        return installVersion("8.3");
    }

    ServiceResult<bool> PHPService::uninstall()
    {
        LOG_INFO("Uninstalling PHP");

        // Uninstall all versions
        for (const QString &version : m_installedVersions.keys())
        {
            uninstallVersion(version);
        }

        return ServiceResult<bool>::Ok(true);
    }

    ServiceResult<bool> PHPService::configure()
    {
        LOG_INFO("Configuring PHP");

        if (m_currentVersion.isEmpty())
        {
            return ServiceResult<bool>::Err("No PHP version selected");
        }

        // Generate php.ini
        QString iniContent = generatePhpIniConfig(m_currentVersion);
        auto iniResult = setPhpIni(m_currentVersion, iniContent);
        if (iniResult.isError())
        {
            return iniResult;
        }

        // Generate FPM pool config
        QString fpmContent = generateFpmPoolConfig(m_currentVersion);
        auto fpmResult = setFpmConfig(m_currentVersion, fpmContent);

        return fpmResult;
    }

    QString PHPService::configPath() const
    {
        return phpIniPath(m_currentVersion);
    }

    ServiceResult<QString> PHPService::detectVersion()
    {
        return detectSystemPhpVersion();
    }

    ServiceResult<QList<Models::PHPVersion>> PHPService::listAvailableVersions()
    {
        QList<Models::PHPVersion> versions;

        // Common available PHP versions
        QStringList availableVersions = {"7.4", "8.0", "8.1", "8.2", "8.3", "8.4"};

        for (const QString &ver : availableVersions)
        {
            Models::PHPVersion phpVersion(ver);
            versions.append(phpVersion);
        }

        return ServiceResult<QList<Models::PHPVersion>>::Ok(versions);
    }

    ServiceResult<QList<Models::PHPVersion>> PHPService::listInstalledVersions()
    {
        QList<Models::PHPVersion> versions = m_installedVersions.values();
        return ServiceResult<QList<Models::PHPVersion>>::Ok(versions);
    }

    ServiceResult<bool> PHPService::installVersion(const QString &version)
    {
        // CRITICAL FIX: Normalize input version
        QString normalizedVersion = normalizeVersion(version);

        LOG_INFO(QString("Installing PHP %1").arg(normalizedVersion));

        emit installProgress(0, QString("Preparing to install PHP %1...").arg(normalizedVersion));

        if (isVersionInstalled(normalizedVersion))
        {
            return ServiceResult<bool>::Err(QString("PHP %1 is already installed").arg(normalizedVersion));
        }

        ServiceResult<bool> result;

        // Try repository installation first
        emit installProgress(10, "Checking repositories...");
        result = installFromRepository(normalizedVersion);

        if (result.isError())
        {
            LOG_WARNING(QString("Repository installation failed: %1").arg(result.error));
            LOG_INFO("Attempting source installation...");

            emit installProgress(20, "Downloading PHP source...");
            result = installFromSource(normalizedVersion);
        }

        if (result.isSuccess())
        {
            emit installProgress(100, QString("PHP %1 installed successfully").arg(normalizedVersion));

            // Rescan installed versions
            scanInstalledVersions();

            // Configure the new installation
            configure();

            emit versionInstalled(normalizedVersion);
            LOG_INFO(QString("PHP %1 installed successfully").arg(normalizedVersion));
        }
        else
        {
            emit installProgress(0, QString("Installation failed: %1").arg(result.error));
        }

        return result;
    }

    ServiceResult<bool> PHPService::uninstallVersion(const QString &version)
    {
        // CRITICAL FIX: Normalize input version
        QString normalizedVersion = normalizeVersion(version);

        LOG_INFO(QString("Uninstalling PHP %1").arg(normalizedVersion));

        if (!isVersionInstalled(normalizedVersion))
        {
            return ServiceResult<bool>::Err(QString("PHP %1 is not installed").arg(normalizedVersion));
        }

        // Stop FPM if running
        if (isFpmRunning(normalizedVersion))
        {
            stopFpm(normalizedVersion);
        }

        // Uninstall via package manager
        QStringList args;

        if (m_packageManager == "apt")
        {
            args << "remove" << "-y" << QString("php%1").arg(normalizedVersion);
            auto result = executeAsRoot("apt", args);

            if (result.isSuccess())
            {
                m_installedVersions.remove(normalizedVersion);
                emit versionUninstalled(normalizedVersion);
                return result;
            }
        }
        else if (m_packageManager == "dnf" || m_packageManager == "yum")
        {
            QString compactVersion = normalizedVersion;
            compactVersion.remove('.');

            args << "remove" << "-y" << QString("php%1").arg(compactVersion);
            auto result = executeAsRoot(m_packageManager, args);

            if (result.isSuccess())
            {
                m_installedVersions.remove(normalizedVersion);
                emit versionUninstalled(normalizedVersion);
                return result;
            }
        }
        else if (m_packageManager == "pacman")
        {
            args << "-R" << "--noconfirm" << "php" << "php-fpm";
            auto result = executeAsRoot("pacman", args);

            if (result.isSuccess())
            {
                m_installedVersions.remove(normalizedVersion);
                emit versionUninstalled(normalizedVersion);
                return result;
            }
        }

        return ServiceResult<bool>::Err(
            QString("Uninstallation not supported for package manager: %1")
                .arg(m_packageManager));
    }

    ServiceResult<bool> PHPService::switchVersion(const QString &version)
    {
        // CRITICAL FIX: Normalize input version
        QString normalizedVersion = normalizeVersion(version);

        LOG_INFO(QString("Switching to PHP %1").arg(normalizedVersion));

        if (!isVersionInstalled(normalizedVersion))
        {
            return ServiceResult<bool>::Err(QString("PHP %1 is not installed").arg(normalizedVersion));
        }

        // Stop current FPM
        if (!m_currentVersion.isEmpty() && isFpmRunning(m_currentVersion))
        {
            stopFpm(m_currentVersion);
        }

        // Update symlinks
        updateSymlinks(normalizedVersion);

        // Start new FPM
        auto result = startFpm(normalizedVersion);

        if (result.isSuccess())
        {
            m_currentVersion = normalizedVersion;
            setVersion(normalizedVersion);

            // Update config
            Core::ConfigManager &config = Core::ConfigManager::instance();
            config.setDefaultPhpVersion(normalizedVersion);

            emit versionSwitched(normalizedVersion);
            LOG_INFO(QString("Switched to PHP %1").arg(normalizedVersion));
        }

        return result;
    }

    QString PHPService::defaultVersion() const
    {
        Core::ConfigManager &config = Core::ConfigManager::instance();
        return config.defaultPhpVersion();
    }

    Models::PHPVersion PHPService::getVersionInfo(const QString &version) const
    {
        // CRITICAL FIX: Normalize lookup key
        QString normalizedVersion = normalizeVersion(version);
        return m_installedVersions.value(normalizedVersion, Models::PHPVersion());
    }

    bool PHPService::isVersionInstalled(const QString &version) const
    {
        // CRITICAL FIX: Normalize lookup key
        QString normalizedVersion = normalizeVersion(version);
        return m_installedVersions.contains(normalizedVersion);
    }

    ServiceResult<bool> PHPService::startFpm(const QString &version)
    {
        QString fpmBinary = phpFpmBinaryPath(version);

        if (fpmBinary.isEmpty())
        {
            return ServiceResult<bool>::Err(QString("PHP-FPM %1 not found").arg(version));
        }

        if (isFpmRunning(version))
        {
            return ServiceResult<bool>::Ok(true);
        }

        QStringList args;
        args << "--fpm-config" << fpmConfigPath(version);

        bool started = process()->start(fpmBinary, args);

        if (started)
        {
            updateStatus(Models::ServiceStatus::Running);
            return ServiceResult<bool>::Ok(true);
        }

        return ServiceResult<bool>::Err("Failed to start PHP-FPM");
    }

    ServiceResult<bool> PHPService::stopFpm(const QString &version)
    {
        if (!isFpmRunning(version))
        {
            return ServiceResult<bool>::Ok(true);
        }

        bool stopped = process()->stop();

        if (stopped)
        {
            updateStatus(Models::ServiceStatus::Stopped);
            return ServiceResult<bool>::Ok(true);
        }

        return ServiceResult<bool>::Err("Failed to stop PHP-FPM");
    }

    ServiceResult<bool> PHPService::restartFpm(const QString &version)
    {
        auto stopResult = stopFpm(version);
        if (stopResult.isError())
        {
            return stopResult;
        }

        QThread::msleep(500);

        return startFpm(version);
    }

    bool PHPService::isFpmRunning(const QString &version) const
    {
        // Normalize version for checking
        QString normalizedVersion = normalizeVersion(version);

        if (process()->isRunning())
        {
            return true;
        }

        // Try possible service names
        QStringList serviceNames;
        serviceNames << QString("php%1-fpm").arg(normalizedVersion)
                     << "php-fpm";

        for (const QString &name : serviceNames)
        {
            auto result = executeAndCapture("systemctl", QStringList() << "is-active" << name);
            if (result.isSuccess() && result.data.trimmed() == "active")
            {
                return true;
            }
        }

        return false;
    }

    ServiceResult<QStringList> PHPService::listAvailableExtensions(const QString &version)
    {
        Q_UNUSED(version)

        QStringList extensions = {
            "curl", "gd", "mbstring", "xml", "zip", "mysql", "pgsql",
            "sqlite3", "redis", "imagick", "intl", "bcmath", "soap"};

        return ServiceResult<QStringList>::Ok(extensions);
    }

    ServiceResult<QStringList> PHPService::listInstalledExtensions(const QString &version)
    {
        QString phpBinary = phpBinaryPath(version);

        if (phpBinary.isEmpty())
        {
            return ServiceResult<QStringList>::Err("PHP binary not found");
        }

        auto result = executeAndCapture(phpBinary, QStringList() << "-m");

        if (result.isError())
        {
            return ServiceResult<QStringList>::Err(result.error);
        }

        QStringList extensions = result.data.split('\n', Qt::SkipEmptyParts);
        extensions.removeAll("[PHP Modules]");
        extensions.removeAll("[Zend Modules]");

        return ServiceResult<QStringList>::Ok(extensions);
    }

    ServiceResult<bool> PHPService::installExtension(const QString &version, const QString &extension)
    {
        // CRITICAL FIX: Normalize version
        QString normalizedVersion = normalizeVersion(version);

        LOG_INFO(QString("Installing PHP %1 extension: %2").arg(normalizedVersion, extension));

        QStringList args;

        if (m_packageManager == "apt")
        {
            args << "install" << "-y" << QString("php%1-%2").arg(normalizedVersion, extension);
            auto result = executeAsRoot("apt", args);

            if (result.isSuccess())
            {
                emit extensionInstalled(normalizedVersion, extension);
                return result;
            }
        }
        else if (m_packageManager == "dnf" || m_packageManager == "yum")
        {
            // Fedora/RHEL: php83-php-mysqlnd format
            QString compactVersion = normalizedVersion;
            compactVersion.remove('.');

            QString pkgName = QString("php%1-php-%2")
                                  .arg(compactVersion, extension);
            args << "install" << "-y" << pkgName;

            auto result = executeAsRoot(m_packageManager, args);

            if (result.isSuccess())
            {
                emit extensionInstalled(normalizedVersion, extension);
                return result;
            }
        }
        else if (m_packageManager == "pacman")
        {
            // Arch: php-{extension} format
            QString pkgName = QString("php-%1").arg(extension);
            args << "-S" << "--noconfirm" << pkgName;

            auto result = executeAsRoot("pacman", args);

            if (result.isSuccess())
            {
                emit extensionInstalled(normalizedVersion, extension);
                return result;
            }
        }

        return ServiceResult<bool>::Err(
            QString("Extension installation not supported for: %1")
                .arg(m_packageManager));
    }

    ServiceResult<bool> PHPService::uninstallExtension(const QString &version, const QString &extension)
    {
        // CRITICAL FIX: Normalize version
        QString normalizedVersion = normalizeVersion(version);

        LOG_INFO(QString("Uninstalling PHP %1 extension: %2").arg(normalizedVersion, extension));

        QStringList args;

        if (m_packageManager == "apt")
        {
            args << "remove" << "-y" << QString("php%1-%2").arg(normalizedVersion, extension);
            return executeAsRoot("apt", args);
        }
        else if (m_packageManager == "dnf" || m_packageManager == "yum")
        {
            QString compactVersion = normalizedVersion;
            compactVersion.remove('.');

            QString pkgName = QString("php%1-php-%2")
                                  .arg(compactVersion, extension);
            args << "remove" << "-y" << pkgName;
            return executeAsRoot(m_packageManager, args);
        }
        else if (m_packageManager == "pacman")
        {
            QString pkgName = QString("php-%1").arg(extension);
            args << "-R" << "--noconfirm" << pkgName;
            return executeAsRoot("pacman", args);
        }

        return ServiceResult<bool>::Err(
            QString("Extension removal not supported for: %1")
                .arg(m_packageManager));
    }

    ServiceResult<QString> PHPService::getPhpIni(const QString &version)
    {
        QString path = phpIniPath(version);
        auto fileResult = Utils::FileSystem::readFile(path);

        if (fileResult.isError())
        {
            return ServiceResult<QString>::Err(fileResult.error);
        }

        return ServiceResult<QString>::Ok(fileResult.data);
    }

    ServiceResult<bool> PHPService::setPhpIni(const QString &version, const QString &content)
    {
        QString path = phpIniPath(version);
        auto fileResult = Utils::FileSystem::writeFile(path, content);

        if (fileResult.isError())
        {
            return ServiceResult<bool>::Err(fileResult.error);
        }

        return ServiceResult<bool>::Ok(true);
    }

    ServiceResult<QString> PHPService::getFpmConfig(const QString &version)
    {
        QString path = fpmConfigPath(version);
        auto fileResult = Utils::FileSystem::readFile(path);

        if (fileResult.isError())
        {
            return ServiceResult<QString>::Err(fileResult.error);
        }

        return ServiceResult<QString>::Ok(fileResult.data);
    }

    ServiceResult<bool> PHPService::setFpmConfig(const QString &version, const QString &content)
    {
        QString path = fpmConfigPath(version);
        auto fileResult = Utils::FileSystem::writeFile(path, content);

        if (fileResult.isError())
        {
            return ServiceResult<bool>::Err(fileResult.error);
        }

        return ServiceResult<bool>::Ok(true);
    }

    QString PHPService::phpBinaryPath(const QString &version) const
    {
        // Normalize version
        QString normalizedVersion = normalizeVersion(version);

        // Check custom installation first
        QString customPath = Utils::FileSystem::joinPath(getPhpBasePath(version), "bin/php");
        if (Utils::FileSystem::fileExists(customPath))
        {
            return customPath;
        }

        // Check system installation
        QString systemPath = getProgramPath(QString("php%1").arg(normalizedVersion));
        if (!systemPath.isEmpty())
        {
            return systemPath;
        }

        // Fallback to default php
        return getProgramPath("php");
    }

    QString PHPService::phpFpmBinaryPath(const QString &version) const
    {
        // Normalize version
        QString normalizedVersion = normalizeVersion(version);

        QString customPath = Utils::FileSystem::joinPath(getPhpBasePath(version), "sbin/php-fpm");
        if (Utils::FileSystem::fileExists(customPath))
        {
            return customPath;
        }

        QString bin = getProgramPath(QString("php-fpm%1").arg(normalizedVersion));
        if (bin.isEmpty())
        {
            bin = getProgramPath("php-fpm");
        }

        return bin;
    }

    QString PHPService::phpIniPath(const QString &version) const
    {
        return Utils::FileSystem::joinPath(getPhpBasePath(version), "php.ini");
    }

    QString PHPService::fpmConfigPath(const QString &version) const
    {
        return Utils::FileSystem::joinPath(getPhpBasePath(version), "php-fpm.conf");
    }

    ServiceResult<bool> PHPService::installFromRepository(const QString &version)
    {
        if (m_packageManager == "apt")
        {
            // Ubuntu/Debian - Try ondrej/php PPA first
            if (isUbuntuPpaAvailable())
            {
                LOG_INFO("Adding ondrej/php PPA for newer PHP versions");
                executeAsRoot("add-apt-repository", QStringList() << "-y" << "ppa:ondrej/php");
                executeAsRoot("apt", QStringList() << "update");
            }

            QStringList installArgs;
            installArgs << "install" << "-y"
                        << QString("php%1").arg(version)
                        << QString("php%1-fpm").arg(version)
                        << QString("php%1-cli").arg(version)
                        << QString("php%1-common").arg(version);

            return executeAsRoot("apt", installArgs);
        }
        else if (m_packageManager == "dnf")
        {
            // Fedora - Enable Remi's repository for newer PHP versions
            LOG_INFO("Installing PHP from Fedora repositories");

            QString compactVersion = version;
            compactVersion.remove('.');

            QStringList installArgs;
            installArgs << "install" << "-y"
                        << QString("php%1").arg(compactVersion) // php83
                        << QString("php%1-php-fpm").arg(compactVersion);

            return executeAsRoot("dnf", installArgs);
        }
        else if (m_packageManager == "yum")
        {
            // CentOS/RHEL - Enable Remi's repository
            LOG_INFO("Installing PHP from YUM repositories");

            QString compactVersion = version;
            compactVersion.remove('.');

            QStringList installArgs;
            installArgs << "install" << "-y"
                        << QString("php%1").arg(compactVersion)
                        << QString("php%1-php-fpm").arg(compactVersion);

            return executeAsRoot("yum", installArgs);
        }
        else if (m_packageManager == "pacman")
        {
            // Arch Linux - Use official repos (usually has latest)
            LOG_INFO("Installing PHP from Arch repositories");

            QStringList installArgs;
            installArgs << "-S" << "--noconfirm"
                        << "php"
                        << "php-fpm";

            return executeAsRoot("pacman", installArgs);
        }

        return ServiceResult<bool>::Err(
            QString("Package manager '%1' is not supported for PHP installation")
                .arg(m_packageManager));
    }

    ServiceResult<bool> PHPService::installFromSource(const QString &version)
    {
        Q_UNUSED(version)
        return ServiceResult<bool>::Err("Source installation not yet implemented");
    }

    ServiceResult<bool> PHPService::downloadPhp(const QString &version, const QString &destination)
    {
        Q_UNUSED(version)
        Q_UNUSED(destination)
        return ServiceResult<bool>::Err("Not implemented");
    }

    ServiceResult<bool> PHPService::compilePhp(const QString &version, const QString &sourceDir)
    {
        Q_UNUSED(version)
        Q_UNUSED(sourceDir)
        return ServiceResult<bool>::Err("Not implemented");
    }

    ServiceResult<QString> PHPService::detectSystemPhpVersion()
    {
        auto result = executeAndCapture("php", QStringList() << "--version");

        if (result.isError())
        {
            return result;
        }

        // Parse version from output like "PHP 8.3.12 (cli)"
        QRegularExpression regex("PHP (\\d+\\.\\d+\\.\\d+)");
        QRegularExpressionMatch match = regex.match(result.data);

        if (match.hasMatch())
        {
            QString fullVersion = match.captured(1); // "8.3.12"
            // CRITICAL FIX: Return normalized version
            return ServiceResult<QString>::Ok(normalizeVersion(fullVersion));
        }

        return ServiceResult<QString>::Err("Could not detect PHP version");
    }

    void PHPService::scanInstalledVersions()
    {
        m_installedVersions.clear();

        QSet<QString> shortVersions; // like "8.3", "8.4"

        // 1) Scan custom base path: $phpPath/8.3, 8.4, etc.
        QRegularExpression verRegex(R"(^(\d+)\.(\d+)$)");

        QDir baseDir(m_phpBasePath);
        if (baseDir.exists())
        {
            const QFileInfoList entries =
                baseDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

            for (const QFileInfo &fi : entries)
            {
                const QString name = fi.fileName();
                if (verRegex.match(name).hasMatch())
                {
                    shortVersions.insert(name);
                }
            }
        }

        // 2) Scan for phpX.Y binaries on the system
        for (int major = 5; major <= 9; ++major)
        {
            for (int minor = 0; minor <= 10; ++minor)
            {
                const QString shortVer = QString("%1.%2").arg(major).arg(minor);
                const QString binName = QString("php%1").arg(shortVer);

                if (checkProgramExists(binName))
                {
                    shortVersions.insert(shortVer);
                }
            }
        }

        // 3) CRITICAL FIX: For each short version, resolve full version and register
        //    but ALWAYS use short version as the key
        for (const QString &shortVer : shortVersions)
        {
            QString phpBinary = phpBinaryPath(shortVer);
            if (phpBinary.isEmpty() || !Utils::FileSystem::fileExists(phpBinary))
            {
                continue;
            }

            // Get full version (e.g., "8.3.12")
            auto result = executeAndCapture(phpBinary,
                                            QStringList() << "-r" << "echo PHP_VERSION;");
            QString fullVer = result.isSuccess() ? result.data.trimmed() : shortVer;

            // Create PHPVersion object with full version info
            Models::PHPVersion phpVersion(fullVer, phpBinary);
            phpVersion.setIsInstalled(true);

            // CRITICAL FIX: Store under SHORT version key
            m_installedVersions[shortVer] = phpVersion;

            LOG_DEBUG(QString("Found PHP %1 (full: %2) at %3")
                          .arg(shortVer, fullVer, phpBinary));
        }

        LOG_INFO(QString("Found %1 installed PHP versions").arg(m_installedVersions.size()));
    }

    QString PHPService::generatePhpIniConfig(const QString &version)
    {
        Q_UNUSED(version) // Add this line

        return R"(
                    [PHP]
                    engine = On
                    short_open_tag = Off
                    precision = 14
                    output_buffering = 4096
                    zlib.output_compression = Off
                    implicit_flush = Off
                    serialize_precision = -1
                    disable_functions =
                    disable_classes =
                    zend.enable_gc = On

                    expose_php = Off

                    max_execution_time = 300
                    max_input_time = 60
                    memory_limit = 512M

                    error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
                    display_errors = Off
                    display_startup_errors = Off
                    log_errors = On
                    error_log = /var/log/php_errors.log

                    post_max_size = 100M
                    upload_max_filesize = 100M
                    max_file_uploads = 20

                    [Date]
                    date.timezone = UTC
                    )";
    }

    QString PHPService::generateFpmPoolConfig(const QString &version)
    {
        return QString(R"(
[www]
user = www-data
group = www-data

listen = /run/php/php%1-fpm.sock
listen.owner = www-data
listen.group = www-data

pm = dynamic
pm.max_children = 10
pm.start_servers = 2
pm.min_spare_servers = 1
pm.max_spare_servers = 3
)")
            .arg(version);
    }

    QString PHPService::getPhpBasePath(const QString &version) const
    {
        return Utils::FileSystem::joinPath(m_phpBasePath, version);
    }

    void PHPService::updateSymlinks(const QString &version)
    {
        QString phpBinary = phpBinaryPath(version);
        QString phpFpmBinary = phpFpmBinaryPath(version);

        Utils::FileSystem::createSymlink(phpBinary, "/usr/local/bin/php");
        Utils::FileSystem::createSymlink(phpFpmBinary, "/usr/local/bin/php-fpm");
    }

    QString PHPService::detectPackageManager() const
    {
        // Detect in order of popularity
        if (checkProgramExists("apt"))
        {
            return "apt";
        }
        else if (checkProgramExists("dnf"))
        {
            return "dnf";
        }
        else if (checkProgramExists("yum"))
        {
            return "yum";
        }
        else if (checkProgramExists("pacman"))
        {
            return "pacman";
        }
        else if (checkProgramExists("zypper"))
        {
            return "zypper";
        }
        else if (checkProgramExists("apk"))
        {
            return "apk";
        }

        LOG_WARNING("No supported package manager detected");
        return QString();
    }

    bool PHPService::isUbuntuPpaAvailable() const
    {
        // Only Ubuntu/Debian-based systems support PPAs
        if (m_packageManager != "apt")
        {
            return false;
        }

        return checkProgramExists("add-apt-repository");
    }
}
