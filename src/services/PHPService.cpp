#include "services/PHPService.h"
#include "core/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>

namespace Anvil
{
    namespace Services
    {
        namespace
        {
            QString majorMinor(const QString &version)
            {
                QStringList parts = version.split('.');
                if (parts.size() >= 2)
                {
                    return parts[0] + "." + parts[1]; // "8.3.12" -> "8.3"
                }
                return version; // fallback
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
                m_currentVersion = versionResult.data;
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
            LOG_INFO(QString("Installing PHP %1").arg(version));

            emit installProgress(0, QString("Preparing to install PHP %1...").arg(version));

            if (isVersionInstalled(version))
            {
                return ServiceResult<bool>::Err(QString("PHP %1 is already installed").arg(version));
            }

            ServiceResult<bool> result;

            // Try repository installation first
            emit installProgress(10, "Checking repositories...");
            result = installFromRepository(version);

            if (result.isError())
            {
                LOG_WARNING(QString("Repository installation failed: %1").arg(result.error));
                LOG_INFO("Attempting source installation...");

                emit installProgress(20, "Downloading PHP source...");
                result = installFromSource(version);
            }

            if (result.isSuccess())
            {
                emit installProgress(100, QString("PHP %1 installed successfully").arg(version));

                // Rescan installed versions
                scanInstalledVersions();

                // Configure the new installation
                configure();

                emit versionInstalled(version);
                LOG_INFO(QString("PHP %1 installed successfully").arg(version));
            }
            else
            {
                emit installProgress(0, QString("Installation failed: %1").arg(result.error));
            }

            return result;
        }

        ServiceResult<bool> PHPService::uninstallVersion(const QString &version)
        {
            LOG_INFO(QString("Uninstalling PHP %1").arg(version));

            if (!isVersionInstalled(version))
            {
                return ServiceResult<bool>::Err(QString("PHP %1 is not installed").arg(version));
            }

            // Stop FPM if running
            if (isFpmRunning(version))
            {
                stopFpm(version);
            }

            // Uninstall via package manager
            QStringList args;

            if (m_packageManager == "apt")
            {
                const QString shortVer = majorMinor(version);
                args << "remove" << "-y" << QString("php%1").arg(shortVer);
                auto result = executeAsRoot("apt", args);

                if (result.isSuccess())
                {
                    m_installedVersions.remove(version);
                    emit versionUninstalled(version);
                    return result;
                }
            }

            return ServiceResult<bool>::Err("Uninstallation failed");
        }

        ServiceResult<bool> PHPService::switchVersion(const QString &version)
        {
            LOG_INFO(QString("Switching to PHP %1").arg(version));

            if (!isVersionInstalled(version))
            {
                return ServiceResult<bool>::Err(QString("PHP %1 is not installed").arg(version));
            }

            // Stop current FPM
            if (!m_currentVersion.isEmpty() && isFpmRunning(m_currentVersion))
            {
                stopFpm(m_currentVersion);
            }

            // Update symlinks
            updateSymlinks(version);

            // Start new FPM
            auto result = startFpm(version);

            if (result.isSuccess())
            {
                m_currentVersion = version;
                setVersion(version);

                // Update config
                Core::ConfigManager &config = Core::ConfigManager::instance();
                config.setDefaultPhpVersion(version);

                emit versionSwitched(version);
                LOG_INFO(QString("Switched to PHP %1").arg(version));
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
            return m_installedVersions.value(version, Models::PHPVersion());
        }

        bool PHPService::isVersionInstalled(const QString &version) const
        {
            return m_installedVersions.contains(version);
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
            // prefer full version for internal state – process() is your own worker
            if (process()->isRunning())
            {
                return true;
            }

            const QString shortVer = majorMinor(version);

            // Try a few possible service names
            QStringList serviceNames;
            serviceNames << QString("php%1-fpm").arg(shortVer) // php8.3-fpm
                         << QString("php%1-fpm").arg(version)  // php8.3.12-fpm (just in case)
                         << "php-fpm";                         // generic

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
            LOG_INFO(QString("Installing PHP %1 extension: %2").arg(version, extension));

            QStringList args;

            if (m_packageManager == "apt")
            {
                const QString shortVer = majorMinor(version);
                args << "install" << "-y" << QString("php%1-%2").arg(shortVer, extension);
                auto result = executeAsRoot("apt", args);

                if (result.isSuccess())
                {
                    emit extensionInstalled(version, extension);
                    return result;
                }
            }

            return ServiceResult<bool>::Err("Extension installation failed");
        }

        ServiceResult<bool> PHPService::uninstallExtension(const QString &version, const QString &extension)
        {
            LOG_INFO(QString("Uninstalling PHP %1 extension: %2").arg(version, extension));

            QStringList args;

            if (m_packageManager == "apt")
            {
                const QString shortVer = majorMinor(version);
                args << "remove" << "-y" << QString("php%1-%2").arg(shortVer, extension);
                return executeAsRoot("apt", args);
            }

            return ServiceResult<bool>::Err("Extension uninstallation failed");
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
            // Check custom installation first
            QString customPath = Utils::FileSystem::joinPath(getPhpBasePath(version), "bin/php");
            if (Utils::FileSystem::fileExists(customPath))
            {
                return customPath;
            }

            const QString shortVer = majorMinor(version);

            // Check system installation (php8.3, php8.3.12, etc.)
            QString systemPath = getProgramPath(QString("php%1").arg(shortVer));
            if (systemPath.isEmpty())
            {
                systemPath = getProgramPath(QString("php%1").arg(version));
            }
            if (!systemPath.isEmpty())
            {
                return systemPath;
            }

            // Fallback to default php
            return getProgramPath("php");
        }

        QString PHPService::phpFpmBinaryPath(const QString &version) const
        {
            // Your own custom install path will likely use full version – keep it that way.
            QString customPath = Utils::FileSystem::joinPath(getPhpBasePath(version), "sbin/php-fpm");
            if (Utils::FileSystem::fileExists(customPath))
            {
                return customPath;
            }

            const QString shortVer = majorMinor(version);

            // Distro-style binaries usually use only major.minor
            QString bin = getProgramPath(QString("php-fpm%1").arg(shortVer)); // php-fpm8.3
            if (bin.isEmpty())
            {
                bin = getProgramPath(QString("php-fpm%1").arg(version)); // php-fpm8.3.12 (rare)
            }
            if (bin.isEmpty())
            {
                bin = getProgramPath("php-fpm"); // generic
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
                const QString shortVer = majorMinor(version);

                // Add Ondřej’s PPA, update, etc...
                QStringList installArgs;
                installArgs << "install" << "-y"
                            << QString("php%1").arg(shortVer)
                            << QString("php%1-fpm").arg(shortVer)
                            << QString("php%1-cli").arg(shortVer)
                            << QString("php%1-common").arg(shortVer);

                return executeAsRoot("apt", installArgs);
            }

            return ServiceResult<bool>::Err("Unsupported package manager");
        }

        ServiceResult<bool> PHPService::installFromSource(const QString &version)
        {
            // This would involve downloading and compiling PHP from source
            // For now, return error - source installation is complex
            return ServiceResult<bool>::Err("Source installation not yet implemented");
        }

        ServiceResult<bool> PHPService::downloadPhp(const QString &version, const QString &destination)
        {
            // Download PHP source
            return ServiceResult<bool>::Err("Not implemented");
        }

        ServiceResult<bool> PHPService::compilePhp(const QString &version, const QString &sourceDir)
        {
            // Compile PHP from source
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
                return ServiceResult<QString>::Ok(match.captured(1)); // "8.3.12"
            }

            return ServiceResult<QString>::Err("Could not detect PHP version");
        }

        void PHPService::scanInstalledVersions()
        {
            m_installedVersions.clear();

            QSet<QString> shortVersions; // like "8.3", "8.4"

            // 1) Scan custom base path: $phpPath/8.3, 8.4, etc.
            QRegularExpression verRegex(R"(^(\\d+)\\.(\\d+)$)"); // "8.3"

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

            // 2) Scan for phpX.Y binaries on the system (php8.3, php8.4, etc.)
            for (int major = 5; major <= 9; ++major)
            {
                for (int minor = 0; minor <= 10; ++minor)
                {
                    const QString shortVer = QString("%1.%2").arg(major).arg(minor);
                    const QString binName = QString("php%1").arg(shortVer); // php8.3

                    if (checkProgramExists(binName))
                    {
                        shortVersions.insert(shortVer);
                    }
                }
            }

            // 3) For each short version, resolve full version and register it
            for (const QString &shortVer : shortVersions)
            {
                QString phpBinary = phpBinaryPath(shortVer);
                if (phpBinary.isEmpty() || !Utils::FileSystem::fileExists(phpBinary))
                {
                    continue;
                }

                auto result = executeAndCapture(phpBinary,
                                                QStringList() << "-r" << "echo PHP_VERSION;");
                QString fullVer = result.isSuccess()
                                      ? result.data.trimmed()
                                      : shortVer;

                Models::PHPVersion phpVersion(fullVer, phpBinary);
                phpVersion.setIsInstalled(true);

                // store under short key so "8.3" maps to full info
                m_installedVersions[shortVer] = phpVersion;
            }

            LOG_INFO(QString("Found %1 installed PHP versions").arg(m_installedVersions.size()));
        }

        QString PHPService::generatePhpIniConfig(const QString &version)
        {
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

            // Create symlinks for easy access
            Utils::FileSystem::createSymlink(phpBinary, "/usr/local/bin/php");
            Utils::FileSystem::createSymlink(phpFpmBinary, "/usr/local/bin/php-fpm");
        }

        QString PHPService::detectPackageManager() const
        {
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

            return QString();
        }

        bool PHPService::isUbuntuPpaAvailable() const
        {
            return checkProgramExists("add-apt-repository");
        }
    }
}
