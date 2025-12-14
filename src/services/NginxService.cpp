#include "services/NginxService.h"
#include "core/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"
#include <QRegularExpression>
#include <QDir>

namespace Anvil
{
    namespace Services
    {
        NginxService::NginxService(QObject *parent)
            : BaseService("nginx", parent)
        {

            setDisplayName("Nginx Web Server");
            setDescription("High-performance HTTP server and reverse proxy");

            Core::ConfigManager &config = Core::ConfigManager::instance();
            m_nginxBasePath = config.nginxPath();
            m_sitesAvailablePath = Utils::FileSystem::joinPath(m_nginxBasePath, "sites-available");
            m_sitesEnabledPath = Utils::FileSystem::joinPath(m_nginxBasePath, "sites-enabled");
            m_sslPath = Utils::FileSystem::joinPath(m_nginxBasePath, "ssl");
            m_logsPath = config.logsPath();

            // Detect package manager
            m_packageManager = detectPackageManager();

            // Detect nginx
            detectNginx();

            // Detect version
            auto versionResult = detectVersion();
            if (versionResult.isSuccess())
            {
                setVersion(versionResult.data);
            }
        }

        NginxService::~NginxService()
        {
        }

        ServiceResult<bool> NginxService::start()
        {
            LOG_INFO("Starting Nginx");

            if (!isInstalled())
            {
                return ServiceResult<bool>::Err("Nginx is not installed");
            }

            if (isRunning())
            {
                LOG_INFO("Nginx is already running");
                return ServiceResult<bool>::Ok(true);
            }

            // Test config before starting
            auto testResult = testConfig();
            if (testResult.isError())
            {
                return ServiceResult<bool>::Err(QString("Config test failed: %1").arg(testResult.error));
            }

            auto result = executeAsRoot("systemctl", QStringList() << "start" << "nginx");

            if (result.isSuccess())
            {
                updateStatus(Models::ServiceStatus::Running);
                LOG_INFO("Nginx started successfully");
            }

            return result;
        }

        ServiceResult<bool> NginxService::stop()
        {
            LOG_INFO("Stopping Nginx");

            if (!isRunning())
            {
                return ServiceResult<bool>::Ok(true);
            }

            auto result = executeAsRoot("systemctl", QStringList() << "stop" << "nginx");

            if (result.isSuccess())
            {
                updateStatus(Models::ServiceStatus::Stopped);
                LOG_INFO("Nginx stopped successfully");
            }

            return result;
        }

        bool NginxService::isInstalled() const
        {
            return checkProgramExists("nginx");
        }

        bool NginxService::isRunning() const
        {
            auto result = executeAndCapture("systemctl", QStringList() << "is-active" << "nginx");
            return result.isSuccess() && result.data.trimmed() == "active";
        }

        ServiceResult<bool> NginxService::install()
        {
            LOG_INFO("Installing Nginx");

            if (isInstalled())
            {
                return ServiceResult<bool>::Err("Nginx is already installed");
            }

            if (m_packageManager == "apt")
            {
                auto result = executeAsRoot("apt", QStringList() << "install" << "-y" << "nginx");

                if (result.isSuccess())
                {
                    detectNginx();
                    configure();
                    LOG_INFO("Nginx installed successfully");
                }

                return result;
            }

            return ServiceResult<bool>::Err("Unsupported package manager");
        }

        ServiceResult<bool> NginxService::uninstall()
        {
            LOG_INFO("Uninstalling Nginx");

            if (!isInstalled())
            {
                return ServiceResult<bool>::Ok(true);
            }

            // Stop nginx first
            if (isRunning())
            {
                stop();
            }

            if (m_packageManager == "apt")
            {
                return executeAsRoot("apt", QStringList() << "remove" << "-y" << "nginx");
            }

            return ServiceResult<bool>::Err("Unsupported package manager");
        }

        ServiceResult<bool> NginxService::configure()
        {
            LOG_INFO("Configuring Nginx");

            // Ensure directories exist
            Utils::FileSystem::ensureDirectoryExists(m_nginxBasePath);
            Utils::FileSystem::ensureDirectoryExists(m_sitesAvailablePath);
            Utils::FileSystem::ensureDirectoryExists(m_sitesEnabledPath);
            Utils::FileSystem::ensureDirectoryExists(m_sslPath);
            Utils::FileSystem::ensureDirectoryExists(m_logsPath);

            // Generate main config
            QString mainConfig = generateMainConfig();
            QString mainConfigPath = Utils::FileSystem::joinPath(m_nginxBasePath, "nginx.conf");

            auto result = Utils::FileSystem::writeFile(mainConfigPath, mainConfig);
            if (result.isError())
            {
                return ServiceResult<bool>::Err(result.error);
            }

            // Test configuration
            return testConfig();
        }

        QString NginxService::configPath() const
        {
            return Utils::FileSystem::joinPath(m_nginxBasePath, "nginx.conf");
        }

        ServiceResult<QString> NginxService::detectVersion()
        {
            auto result = executeAndCapture("nginx", QStringList() << "-v");

            // nginx outputs version to stderr
            QString output = result.isSuccess() ? result.data : QString();

            QRegularExpression regex("nginx version: nginx/([\\d.]+)");
            QRegularExpressionMatch match = regex.match(output);

            if (match.hasMatch())
            {
                return ServiceResult<QString>::Ok(match.captured(1));
            }

            return ServiceResult<QString>::Err("Could not detect nginx version");
        }

        ServiceResult<bool> NginxService::reload()
        {
            LOG_INFO("Reloading Nginx configuration");

            if (!isRunning())
            {
                return ServiceResult<bool>::Err("Nginx is not running");
            }

            // Test config before reloading
            auto testResult = testConfig();
            if (testResult.isError())
            {
                return ServiceResult<bool>::Err(QString("Config test failed: %1").arg(testResult.error));
            }

            auto result = executeAsRoot("systemctl", QStringList() << "reload" << "nginx");

            if (result.isSuccess())
            {
                emit configReloaded();
                LOG_INFO("Nginx configuration reloaded");
            }

            return result;
        }

        ServiceResult<bool> NginxService::testConfig()
        {
            auto result = executeAndCapture("nginx", QStringList() << "-t");

            if (result.isSuccess())
            {
                LOG_DEBUG("Nginx configuration test passed");
                return ServiceResult<bool>::Ok(true);
            }

            LOG_ERROR(QString("Nginx configuration test failed: %1").arg(result.error));
            return ServiceResult<bool>::Err(result.error);
        }

        ServiceResult<bool> NginxService::addSite(const Models::Site &site)
        {
            LOG_INFO(QString("Adding site: %1 (%2)").arg(site.name(), site.domain()));

            // Validate site
            auto validateResult = validateSiteConfig(site);
            if (validateResult.isError())
            {
                return validateResult;
            }

            // Generate site configuration
            QString config = generateSiteConfig(site);
            QString configPath = getSiteConfigPath(site.id());

            // Write configuration file
            auto writeResult = Utils::FileSystem::writeFile(configPath, config);
            if (writeResult.isError())
            {
                return ServiceResult<bool>::Err(writeResult.error);
            }

            // Enable site (create symlink)
            auto enableResult = enableSite(site.id());
            if (enableResult.isError())
            {
                return enableResult;
            }

            // Test and reload
            auto testResult = testConfig();
            if (testResult.isError())
            {
                // Rollback
                removeSite(site.id());
                return ServiceResult<bool>::Err(QString("Config test failed: %1").arg(testResult.error));
            }

            if (isRunning())
            {
                reload();
            }

            emit siteAdded(site.id());
            LOG_INFO(QString("Site added successfully: %1").arg(site.name()));

            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> NginxService::removeSite(const QString &siteId)
        {
            LOG_INFO(QString("Removing site: %1").arg(siteId));

            // Disable site first
            disableSite(siteId);

            // Remove configuration file
            QString configPath = getSiteConfigPath(siteId);

            if (Utils::FileSystem::fileExists(configPath))
            {
                if (!Utils::FileSystem::deleteFile(configPath))
                {
                    return ServiceResult<bool>::Err("Failed to delete site configuration");
                }
            }

            if (isRunning())
            {
                reload();
            }

            emit siteRemoved(siteId);
            LOG_INFO(QString("Site removed: %1").arg(siteId));

            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> NginxService::updateSite(const Models::Site &site)
        {
            LOG_INFO(QString("Updating site: %1").arg(site.name()));

            // Remove old configuration
            auto removeResult = removeSite(site.id());
            if (removeResult.isError())
            {
                return removeResult;
            }

            // Add new configuration
            return addSite(site);
        }

        ServiceResult<bool> NginxService::enableSite(const QString &siteId)
        {
            QString configPath = getSiteConfigPath(siteId);
            QString enabledPath = getSiteEnabledPath(siteId);

            if (!Utils::FileSystem::fileExists(configPath))
            {
                return ServiceResult<bool>::Err("Site configuration does not exist");
            }

            if (Utils::FileSystem::fileExists(enabledPath))
            {
                LOG_DEBUG(QString("Site already enabled: %1").arg(siteId));
                return ServiceResult<bool>::Ok(true);
            }

            if (!Utils::FileSystem::createSymlink(configPath, enabledPath))
            {
                return ServiceResult<bool>::Err("Failed to create symlink");
            }

            emit siteEnabled(siteId);
            LOG_INFO(QString("Site enabled: %1").arg(siteId));

            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> NginxService::disableSite(const QString &siteId)
        {
            QString enabledPath = getSiteEnabledPath(siteId);

            if (!Utils::FileSystem::fileExists(enabledPath))
            {
                return ServiceResult<bool>::Ok(true);
            }

            if (!Utils::FileSystem::deleteFile(enabledPath))
            {
                return ServiceResult<bool>::Err("Failed to remove symlink");
            }

            emit siteDisabled(siteId);
            LOG_INFO(QString("Site disabled: %1").arg(siteId));

            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> NginxService::generateSelfSignedCert(const QString &domain)
        {
            LOG_INFO(QString("Generating self-signed certificate for: %1").arg(domain));

            ensureSslDirectory();

            QString certPath = getSslCertPath(domain);
            QString keyPath = getSslKeyPath(domain);

            QStringList args;
            args << "req" << "-x509" << "-nodes" << "-days" << "365"
                 << "-newkey" << "rsa:2048"
                 << "-keyout" << keyPath
                 << "-out" << certPath
                 << "-subj" << QString("/CN=%1").arg(domain);

            auto result = executeCommand("openssl", args);

            if (result.isSuccess())
            {
                LOG_INFO(QString("Certificate generated: %1").arg(domain));
            }

            return result;
        }

        ServiceResult<bool> NginxService::installCertificate(const QString &domain,
                                                             const QString &certPath,
                                                             const QString &keyPath)
        {
            LOG_INFO(QString("Installing certificate for: %1").arg(domain));

            if (!Utils::FileSystem::fileExists(certPath))
            {
                return ServiceResult<bool>::Err("Certificate file does not exist");
            }

            if (!Utils::FileSystem::fileExists(keyPath))
            {
                return ServiceResult<bool>::Err("Key file does not exist");
            }

            ensureSslDirectory();

            QString destCertPath = getSslCertPath(domain);
            QString destKeyPath = getSslKeyPath(domain);

            if (!Utils::FileSystem::copyFile(certPath, destCertPath, true))
            {
                return ServiceResult<bool>::Err("Failed to copy certificate");
            }

            if (!Utils::FileSystem::copyFile(keyPath, destKeyPath, true))
            {
                return ServiceResult<bool>::Err("Failed to copy key");
            }

            LOG_INFO(QString("Certificate installed: %1").arg(domain));
            return ServiceResult<bool>::Ok(true);
        }

        QString NginxService::getSiteConfigPath(const QString &siteId) const
        {
            return Utils::FileSystem::joinPath(m_sitesAvailablePath, siteId + ".conf");
        }

        QString NginxService::getSiteEnabledPath(const QString &siteId) const
        {
            return Utils::FileSystem::joinPath(m_sitesEnabledPath, siteId + ".conf");
        }

        ServiceResult<bool> NginxService::validateSiteConfig(const Models::Site &site)
        {
            QStringList errors = site.validate();

            if (!errors.isEmpty())
            {
                return ServiceResult<bool>::Err(errors.join(", "));
            }

            return ServiceResult<bool>::Ok(true);
        }

        QString NginxService::generateSiteConfig(const Models::Site &site)
        {
            QString config;
            QTextStream stream(&config);

            // HTTP server block
            stream << "server {\n";
            stream << "    listen 80;\n";
            stream << "    listen [::]:80;\n";
            stream << "    server_name " << site.domain() << ";\n";
            stream << "\n";
            stream << "    root " << site.path() << ";\n";
            stream << "    index index.php index.html index.htm;\n";
            stream << "\n";
            stream << "    access_log " << Utils::FileSystem::joinPath(m_logsPath, site.domain() + "-access.log") << ";\n";
            stream << "    error_log " << Utils::FileSystem::joinPath(m_logsPath, site.domain() + "-error.log") << ";\n";
            stream << "\n";

            // SSL redirect if enabled
            if (site.sslEnabled())
            {
                stream << "    # Redirect to HTTPS\n";
                stream << "    return 301 https://$server_name$request_uri;\n";
                stream << "}\n\n";

                // HTTPS server block
                stream << "server {\n";
                stream << "    listen 443 ssl http2;\n";
                stream << "    listen [::]:443 ssl http2;\n";
                stream << "    server_name " << site.domain() << ";\n";
                stream << "\n";
                stream << "    root " << site.path() << ";\n";
                stream << "    index index.php index.html index.htm;\n";
                stream << "\n";
                stream << "    access_log " << Utils::FileSystem::joinPath(m_logsPath, site.domain() + "-ssl-access.log") << ";\n";
                stream << "    error_log " << Utils::FileSystem::joinPath(m_logsPath, site.domain() + "-ssl-error.log") << ";\n";
                stream << "\n";
                stream << generateSslBlock(site.domain());
                stream << "\n";
            }

            // Location blocks
            stream << "    location / {\n";
            stream << "        try_files $uri $uri/ /index.php?$query_string;\n";
            stream << "    }\n";
            stream << "\n";

            // PHP-FPM block
            stream << generatePhpFpmBlock(site.phpVersion());

            // Static files
            stream << "    location ~* \\.(jpg|jpeg|png|gif|ico|css|js|svg|woff|woff2|ttf|eot)$ {\n";
            stream << "        expires 1y;\n";
            stream << "        add_header Cache-Control \"public, immutable\";\n";
            stream << "    }\n";
            stream << "\n";

            // Deny access to hidden files
            stream << "    location ~ /\\.(?!well-known).* {\n";
            stream << "        deny all;\n";
            stream << "    }\n";

            stream << "}\n";

            return config;
        }

        QString NginxService::generatePhpFpmBlock(const QString &phpVersion)
        {
            QString block;
            QTextStream stream(&block);

            QString socketPath = QString("/run/php/php%1-fpm.sock").arg(phpVersion);

            stream << "    location ~ \\.php$ {\n";
            stream << "        fastcgi_split_path_info ^(.+\\.php)(/.+)$;\n";
            stream << "        fastcgi_pass unix:" << socketPath << ";\n";
            stream << "        fastcgi_index index.php;\n";
            stream << "        include fastcgi_params;\n";
            stream << "        fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;\n";
            stream << "        fastcgi_param PATH_INFO $fastcgi_path_info;\n";
            stream << "    }\n";

            return block;
        }

        QString NginxService::generateSslBlock(const QString &domain)
        {
            QString block;
            QTextStream stream(&block);

            stream << "    ssl_certificate " << getSslCertPath(domain) << ";\n";
            stream << "    ssl_certificate_key " << getSslKeyPath(domain) << ";\n";
            stream << "    ssl_protocols TLSv1.2 TLSv1.3;\n";
            stream << "    ssl_ciphers HIGH:!aNULL:!MD5;\n";
            stream << "    ssl_prefer_server_ciphers on;\n";

            return block;
        }

        QString NginxService::generateMainConfig()
        {
            QString config;
            QTextStream stream(&config);

            stream << "user www-data;\n";
            stream << "worker_processes auto;\n";
            stream << "pid /run/nginx.pid;\n";
            stream << "\n";
            stream << "events {\n";
            stream << "    worker_connections 768;\n";
            stream << "}\n";
            stream << "\n";
            stream << "http {\n";
            stream << "    sendfile on;\n";
            stream << "    tcp_nopush on;\n";
            stream << "    tcp_nodelay on;\n";
            stream << "    keepalive_timeout 65;\n";
            stream << "    types_hash_max_size 2048;\n";
            stream << "\n";
            stream << "    include /etc/nginx/mime.types;\n";
            stream << "    default_type application/octet-stream;\n";
            stream << "\n";
            stream << "    ssl_protocols TLSv1.2 TLSv1.3;\n";
            stream << "    ssl_prefer_server_ciphers on;\n";
            stream << "\n";
            stream << "    access_log " << Utils::FileSystem::joinPath(m_logsPath, "access.log") << ";\n";
            stream << "    error_log " << Utils::FileSystem::joinPath(m_logsPath, "error.log") << ";\n";
            stream << "\n";
            stream << "    gzip on;\n";
            stream << "\n";
            stream << "    include " << m_sitesEnabledPath << "/*;\n";
            stream << "}\n";

            return config;
        }

        ServiceResult<bool> NginxService::ensureSslDirectory()
        {
            if (!Utils::FileSystem::ensureDirectoryExists(m_sslPath))
            {
                return ServiceResult<bool>::Err("Failed to create SSL directory");
            }
            return ServiceResult<bool>::Ok(true);
        }

        bool NginxService::hasSslCertificate(const QString &domain) const
        {
            return Utils::FileSystem::fileExists(getSslCertPath(domain)) &&
                   Utils::FileSystem::fileExists(getSslKeyPath(domain));
        }

        QString NginxService::getSslCertPath(const QString &domain) const
        {
            return Utils::FileSystem::joinPath(m_sslPath, domain + ".crt");
        }

        QString NginxService::getSslKeyPath(const QString &domain) const
        {
            return Utils::FileSystem::joinPath(m_sslPath, domain + ".key");
        }

        QString NginxService::getNginxBasePath() const
        {
            return m_nginxBasePath;
        }

        QString NginxService::getNginxBinary() const
        {
            return getProgramPath("nginx");
        }

        QString NginxService::detectPackageManager() const
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

        ServiceResult<bool> NginxService::detectNginx()
        {
            if (!isInstalled())
            {
                return ServiceResult<bool>::Err("Nginx is not installed");
            }

            return ServiceResult<bool>::Ok(true);
        }
    }
}
