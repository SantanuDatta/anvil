#include "managers/SiteManager.h"
#include "core/ServiceManager.h"
#include "core/ConfigManager.h"
#include "services/NginxService.h"
#include "services/DatabaseService.h"
#include "services/DnsService.h"
#include "services/PHPService.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QRegularExpression>
#include <QSet>

namespace
{
    bool isLaravelApp(const QString &path)
    {
        QString artisanPath = Anvil::Utils::FileSystem::joinPath(path, "artisan");
        QString bootstrapPath = Anvil::Utils::FileSystem::joinPath(path, "bootstrap/app.php");
        if (Anvil::Utils::FileSystem::fileExists(artisanPath) &&
            Anvil::Utils::FileSystem::fileExists(bootstrapPath))
        {
            return true;
        }

        QString composerPath = Anvil::Utils::FileSystem::joinPath(path, "composer.json");
        if (!Anvil::Utils::FileSystem::fileExists(composerPath))
        {
            return false;
        }

        auto result = Anvil::Utils::FileSystem::readFile(composerPath);
        return result.isSuccess() && result.data.contains("\"laravel/framework\"");
    }

    bool isPhpApp(const QString &path)
    {
        QString indexPath = Anvil::Utils::FileSystem::joinPath(path, "index.php");
        QString publicIndexPath = Anvil::Utils::FileSystem::joinPath(path, "public/index.php");
        return Anvil::Utils::FileSystem::fileExists(indexPath) ||
               Anvil::Utils::FileSystem::fileExists(publicIndexPath);
    }

    QString resolveDocumentRoot(const QString &path)
    {
        if (isLaravelApp(path))
        {
            QString publicPath = Anvil::Utils::FileSystem::joinPath(path, "public");
            if (Anvil::Utils::FileSystem::isDirectory(publicPath))
            {
                return publicPath;
            }
        }

        QString publicIndexPath = Anvil::Utils::FileSystem::joinPath(path, "public/index.php");
        if (Anvil::Utils::FileSystem::fileExists(publicIndexPath))
        {
            QString publicPath = Anvil::Utils::FileSystem::joinPath(path, "public");
            if (Anvil::Utils::FileSystem::isDirectory(publicPath))
            {
                return publicPath;
            }
        }

        return path;
    }
}

namespace Anvil::Managers
{
    SiteManager::SiteManager(QObject *parent)
        : QObject(parent), m_initialized(false)
    {
    }

    SiteManager::~SiteManager()
    {
        if (m_initialized)
        {
            save();
        }
    }

    bool SiteManager::initialize()
    {
        if (m_initialized)
        {
            LOG_WARNING("SiteManager already initialized");
            return true;
        }

        LOG_INFO("Initializing SiteManager");

        // Ensure ServiceManager is initialized
        auto *sm = serviceManager();
        if (!sm)
        {
            LOG_ERROR("ServiceManager instance is null");
            return false;
        }

        if (!sm->isInitialized())
        {
            LOG_INFO("ServiceManager not initialized, attempting to initialize");
            if (!sm->initialize())
            {
                LOG_ERROR("ServiceManager failed to initialize");
                return false;
            }
        }

        // Load existing sites
        if (!load())
        {
            LOG_ERROR("Failed to load sites");
            return false;
        }

        m_initialized = true;
        LOG_INFO("SiteManager initialized successfully");

        return true;
    }

    Services::ServiceResult<bool> SiteManager::createSite(const Models::Site &site)
    {
        LOG_INFO(QString("Creating site: %1 (%2)").arg(site.name(), site.domain()));

        // Validate site
        auto validationResult = validateSite(site);
        if (validationResult.isError())
        {
            return validationResult;
        }

        // Check for duplicates
        if (siteExists(site.id()))
        {
            return Services::ServiceResult<bool>::Err(
                QString("Site with ID %1 already exists").arg(site.id()));
        }

        if (domainExists(site.domain()))
        {
            return Services::ServiceResult<bool>::Err(
                QString("Domain %1 is already in use").arg(site.domain()));
        }

        // Create site with transaction-like rollback
        auto result = createSiteWithRollback(site);

        if (result.isSuccess())
        {
            // Store site in memory
            m_sites[site.id()] = site;
            m_domainToSiteId[site.domain()] = site.id();

            // Persist to disk
            if (!save())
            {
                LOG_ERROR("Site was created but failed to save sites.json. Rolling back in-memory state.");

                // In-memory rollback (best effort)
                m_domainToSiteId.remove(site.domain());
                m_sites.remove(site.id());

                return Services::ServiceResult<bool>::Err("Failed to save sites file. Check permissions.");
            }

            emit siteCreated(site.id());
            LOG_INFO(QString("Site created successfully: %1").arg(site.name()));
        }

        return result;
    }

    Services::ServiceResult<bool> SiteManager::createSiteWithRollback(const Models::Site &site)
    {
        SiteCreationState state;
        state.domain = site.domain();
        state.siteId = site.id();
        state.databaseName = generateDatabaseName(site.domain());

        try
        {
            // Step 1: DNS Entry
            LOG_DEBUG("Creating DNS entry");
            auto *dns = dnsService();
            if (!dns)
            {
                LOG_WARNING("DNS service not available; domain may not resolve until DNS is configured");
            }
            else
            {
                auto dnsResult = dns->addEntry(site.domain(), "127.0.0.1");
                if (dnsResult.isError())
                {
                    throw std::runtime_error(
                        QString("DNS creation failed: %1").arg(dnsResult.error).toStdString());
                }
                state.dnsCreated = true;
                LOG_DEBUG("✓ DNS entry created");
            }

            // Step 2: Database (optional but recommended)
            LOG_DEBUG("Creating database");
            auto *db = databaseService();
            if (db && db->isInstalled())
            {
                auto dbResult = db->createDatabase(state.databaseName);
                if (dbResult.isError())
                {
                    // Database creation is not critical, log warning but continue
                    LOG_WARNING(QString("Database creation failed: %1").arg(dbResult.error));
                }
                else
                {
                    state.databaseCreated = true;
                    LOG_DEBUG(QString("✓ Database created: %1").arg(state.databaseName));
                }
            }

            // Step 3: Nginx Configuration
            LOG_DEBUG("Configuring Nginx");
            auto *nginx = nginxService();
            if (!nginx)
            {
                throw std::runtime_error("Nginx service not available");
            }

            auto nginxResult = nginx->addSite(site);
            if (nginxResult.isError())
            {
                throw std::runtime_error(
                    QString("Nginx configuration failed: %1").arg(nginxResult.error).toStdString());
            }
            state.nginxConfigured = true;
            LOG_DEBUG("✓ Nginx configured");

            // Success!
            return Services::ServiceResult<bool>::Ok(true);
        }
        catch (const std::exception &e)
        {
            // Rollback everything
            LOG_ERROR(QString("Site creation failed, rolling back: %1").arg(e.what()));
            rollbackSiteCreation(state);

            return Services::ServiceResult<bool>::Err(e.what());
        }
    }

    void SiteManager::rollbackSiteCreation(const SiteCreationState &state)
    {
        LOG_WARNING(QString("Rolling back site creation for: %1").arg(state.domain));

        // Rollback in reverse order

        // 3. Remove Nginx configuration
        if (state.nginxConfigured)
        {
            auto *nginx = nginxService();
            if (nginx)
            {
                nginx->removeSite(state.siteId);
                LOG_DEBUG("✓ Nginx configuration removed");
            }
        }

        // 2. Remove database
        if (state.databaseCreated)
        {
            auto *db = databaseService();
            if (db)
            {
                db->dropDatabase(state.databaseName);
                LOG_DEBUG(QString("✓ Database removed: %1").arg(state.databaseName));
            }
        }

        // 1. Remove DNS entry
        if (state.dnsCreated)
        {
            auto *dns = dnsService();
            if (dns)
            {
                dns->removeEntry(state.domain);
                LOG_DEBUG("✓ DNS entry removed");
            }
        }

        LOG_INFO("Rollback completed");
    }

    Services::ServiceResult<bool> SiteManager::removeSite(const QString &siteId)
    {
        LOG_INFO(QString("Removing site: %1").arg(siteId));

        if (!siteExists(siteId))
        {
            return Services::ServiceResult<bool>::Err("Site does not exist");
        }

        Models::Site site = m_sites[siteId];
        QString dbName = generateDatabaseName(site.domain());

        // Remove from services (no rollback needed for deletion)

        // 1. Nginx
        auto *nginx = nginxService();
        if (nginx)
        {
            auto result = nginx->removeSite(siteId);
            if (result.isError())
            {
                LOG_WARNING(QString("Failed to remove Nginx config: %1").arg(result.error));
            }
        }

        // 2. Database (optional)
        auto *db = databaseService();
        if (db && db->isInstalled())
        {
            auto result = db->dropDatabase(dbName);
            if (result.isError())
            {
                LOG_WARNING(QString("Failed to remove database: %1").arg(result.error));
            }
        }

        // 3. DNS
        auto *dns = dnsService();
        if (dns)
        {
            auto result = dns->removeEntry(site.domain());
            if (result.isError())
            {
                LOG_WARNING(QString("Failed to remove DNS entry: %1").arg(result.error));
            }
        }

        // Remove from memory
        m_domainToSiteId.remove(site.domain());
        m_sites.remove(siteId);

        // Persist changes
        save();

        emit siteRemoved(siteId);
        LOG_INFO(QString("Site removed: %1").arg(site.name()));

        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<bool> SiteManager::updateSite(const Models::Site &site)
    {
        LOG_INFO(QString("Updating site: %1").arg(site.name()));

        if (!siteExists(site.id()))
        {
            return Services::ServiceResult<bool>::Err("Site does not exist");
        }

        // Validate updated site
        auto validationResult = validateSite(site);
        if (validationResult.isError())
        {
            return validationResult;
        }

        Models::Site oldSite = m_sites[site.id()];

        // If domain changed, check for conflicts
        if (site.domain() != oldSite.domain())
        {
            if (domainExists(site.domain()))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Domain %1 is already in use").arg(site.domain()));
            }
        }

        // Update Nginx configuration
        auto *nginx = nginxService();
        if (nginx)
        {
            auto result = nginx->updateSite(site);
            if (result.isError())
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Failed to update Nginx: %1").arg(result.error));
            }
        }

        // If domain changed, update DNS
        if (site.domain() != oldSite.domain())
        {
            auto *dns = dnsService();
            if (dns)
            {
                dns->removeEntry(oldSite.domain());
                dns->addEntry(site.domain(), "127.0.0.1");
            }

            // Update domain mapping
            m_domainToSiteId.remove(oldSite.domain());
            m_domainToSiteId[site.domain()] = site.id();
        }

        // Update in memory
        m_sites[site.id()] = site;

        // Persist changes
        save();

        emit siteUpdated(site.id());
        LOG_INFO(QString("Site updated: %1").arg(site.name()));

        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<Models::Site> SiteManager::getSite(const QString &siteId) const
    {
        if (!siteExists(siteId))
        {
            return Services::ServiceResult<Models::Site>::Err("Site not found");
        }

        return Services::ServiceResult<Models::Site>::Ok(m_sites[siteId]);
    }

    Services::ServiceResult<QList<Models::Site>> SiteManager::listSites() const
    {
        QList<Models::Site> sites = m_sites.values();
        return Services::ServiceResult<QList<Models::Site>>::Ok(sites);
    }

    Services::ServiceResult<QList<Models::Site>> SiteManager::listSitesByDomain(const QString &domain) const
    {
        QList<Models::Site> matchingSites;

        for (const Models::Site &site : m_sites.values())
        {
            if (site.domain().contains(domain, Qt::CaseInsensitive))
            {
                matchingSites.append(site);
            }
        }

        return Services::ServiceResult<QList<Models::Site>>::Ok(matchingSites);
    }

    bool SiteManager::siteExists(const QString &siteId) const
    {
        return m_sites.contains(siteId);
    }

    bool SiteManager::domainExists(const QString &domain) const
    {
        return m_domainToSiteId.contains(domain);
    }

    Services::ServiceResult<bool> SiteManager::parkDirectory(const QString &path)
    {
        LOG_INFO(QString("Parking directory: %1").arg(path));

        // Validate path
        auto pathValidation = validatePath(path);
        if (pathValidation.isError())
        {
            return pathValidation;
        }

        if (isDirectoryParked(path))
        {
            return Services::ServiceResult<bool>::Err("Directory is already parked");
        }

        // Scan directory for subdirectories
        auto scanResult = scanParkedDirectory(path);
        if (scanResult.isError())
        {
            return Services::ServiceResult<bool>::Err(scanResult.error);
        }

        QStringList subdirs = scanResult.data;
        Core::ConfigManager &config = Core::ConfigManager::instance();
        QString defaultDomain = config.defaultDomain();

        // Create a site for each subdirectory
        for (const QString &subdir : subdirs)
        {
            bool isLaravel = isLaravelApp(subdir);
            bool isPhp = isLaravel || isPhpApp(subdir);

            if (!isPhp)
            {
                LOG_INFO(QString("Skipping non-PHP project: %1").arg(subdir));
                continue;
            }

            QString siteName = generateSiteNameFromPath(subdir);
            QString domain = QFileInfo(subdir).fileName() + "." + defaultDomain;
            QString sitePath = resolveDocumentRoot(subdir);

            Models::Site site(siteName, sitePath, domain);
            site.setPhpVersion(config.defaultPhpVersion());
            site.setIsParked(true);

            // Create site (failures are logged but don't stop parking)
            auto result = createSite(site);
            if (result.isError())
            {
                LOG_WARNING(QString("Failed to create site for %1: %2")
                                .arg(subdir, result.error));
            }
        }

        // Add to parked directories
        m_parkedDirectories.append(path);
        save();

        LOG_INFO(QString("Directory parked with %1 sites").arg(subdirs.size()));
        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<bool> SiteManager::unparkDirectory(const QString &path)
    {
        LOG_INFO(QString("Unparking directory: %1").arg(path));

        if (!isDirectoryParked(path))
        {
            return Services::ServiceResult<bool>::Err("Directory is not parked");
        }

        // Remove all sites from this directory
        QList<QString> sitesToRemove;
        for (const Models::Site &site : m_sites.values())
        {
            if (site.path().startsWith(path))
            {
                sitesToRemove.append(site.id());
            }
        }

        for (const QString &siteId : sitesToRemove)
        {
            removeSite(siteId);
        }

        // Remove from parked directories
        m_parkedDirectories.removeAll(path);
        save();

        LOG_INFO(QString("Directory unparked, removed %1 sites").arg(sitesToRemove.size()));
        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<QStringList> SiteManager::listParkedDirectories() const
    {
        return Services::ServiceResult<QStringList>::Ok(m_parkedDirectories);
    }

    bool SiteManager::isDirectoryParked(const QString &path) const
    {
        return m_parkedDirectories.contains(path);
    }

    Services::ServiceResult<bool> SiteManager::enableSite(const QString &siteId)
    {
        LOG_INFO(QString("Enabling site: %1").arg(siteId));

        if (!siteExists(siteId))
        {
            return Services::ServiceResult<bool>::Err("Site does not exist");
        }

        auto *nginx = nginxService();
        if (!nginx)
        {
            return Services::ServiceResult<bool>::Err("Nginx service not available");
        }

        auto result = nginx->enableSite(siteId);
        if (result.isSuccess())
        {
            emit siteEnabled(siteId);
        }

        return result;
    }

    Services::ServiceResult<bool> SiteManager::disableSite(const QString &siteId)
    {
        LOG_INFO(QString("Disabling site: %1").arg(siteId));

        if (!siteExists(siteId))
        {
            return Services::ServiceResult<bool>::Err("Site does not exist");
        }

        auto *nginx = nginxService();
        if (!nginx)
        {
            return Services::ServiceResult<bool>::Err("Nginx service not available");
        }

        auto result = nginx->disableSite(siteId);
        if (result.isSuccess())
        {
            emit siteDisabled(siteId);
        }

        return result;
    }

    bool SiteManager::isSiteEnabled(const QString &siteId) const
    {
        if (!siteExists(siteId))
        {
            return false;
        }

        auto *nginx = nginxService();
        if (!nginx)
        {
            return false;
        }

        QString enabledPath = nginx->getSiteEnabledPath(siteId);
        return Utils::FileSystem::fileExists(enabledPath);
    }

    Services::ServiceResult<bool> SiteManager::enableSsl(const QString &siteId)
    {
        LOG_INFO(QString("Enabling SSL for site: %1").arg(siteId));

        if (!siteExists(siteId))
        {
            return Services::ServiceResult<bool>::Err("Site does not exist");
        }

        Models::Site site = m_sites[siteId];

        // Generate self-signed certificate
        auto certResult = generateSelfSignedCert(siteId);
        if (certResult.isError())
        {
            return certResult;
        }

        // Update site to enable SSL
        site.setSslEnabled(true);
        return updateSite(site);
    }

    Services::ServiceResult<bool> SiteManager::disableSsl(const QString &siteId)
    {
        LOG_INFO(QString("Disabling SSL for site: %1").arg(siteId));

        if (!siteExists(siteId))
        {
            return Services::ServiceResult<bool>::Err("Site does not exist");
        }

        Models::Site site = m_sites[siteId];
        site.setSslEnabled(false);
        return updateSite(site);
    }

    Services::ServiceResult<bool> SiteManager::generateSelfSignedCert(const QString &siteId)
    {
        if (!siteExists(siteId))
        {
            return Services::ServiceResult<bool>::Err("Site does not exist");
        }

        Models::Site site = m_sites[siteId];

        auto *nginx = nginxService();
        if (!nginx)
        {
            return Services::ServiceResult<bool>::Err("Nginx service not available");
        }

        return nginx->generateSelfSignedCert(site.domain());
    }

    Services::ServiceResult<bool> SiteManager::removeAllSites()
    {
        LOG_WARNING("Removing all sites");

        QList<QString> siteIds = m_sites.keys();
        for (const QString &siteId : siteIds)
        {
            removeSite(siteId);
        }

        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<bool> SiteManager::disableAllSites()
    {
        LOG_INFO("Disabling all sites");

        for (const QString &siteId : m_sites.keys())
        {
            disableSite(siteId);
        }

        return Services::ServiceResult<bool>::Ok(true);
    }

    bool SiteManager::save()
    {
        QString filePath = getSitesFilePath();
        QJsonObject json = sitesToJson();

        QJsonDocument doc(json);
        QString jsonString = doc.toJson(QJsonDocument::Indented);

        auto result = Utils::FileSystem::writeFile(filePath, jsonString);

        if (result.isError())
        {
            LOG_ERROR(QString("Failed to save sites: %1").arg(result.error));
            return false;
        }

        LOG_DEBUG("Sites saved successfully");
        return true;
    }

    bool SiteManager::load()
    {
        QString filePath = getSitesFilePath();

        if (!Utils::FileSystem::fileExists(filePath))
        {
            LOG_INFO("No existing sites file found");
            return true; // Not an error
        }

        auto result = Utils::FileSystem::readFile(filePath);

        if (result.isError())
        {
            LOG_ERROR(QString("Failed to load sites: %1").arg(result.error));
            return false;
        }

        QJsonDocument doc = QJsonDocument::fromJson(result.data.toUtf8());

        if (!doc.isObject())
        {
            LOG_ERROR("Invalid sites file format");
            return false;
        }

        sitesFromJson(doc.object());

        if (m_parkedDirectories.isEmpty())
        {
            QSet<QString> inferredRoots;
            for (const Models::Site &site : m_sites.values())
            {
                if (!site.isParked())
                {
                    continue;
                }

                QDir siteDir(site.path());
                if (siteDir.dirName() == "public")
                {
                    siteDir.cdUp();
                }

                QDir projectDir(siteDir.absolutePath());
                if (projectDir.cdUp())
                {
                    inferredRoots.insert(projectDir.absolutePath());
                }
                else
                {
                    inferredRoots.insert(siteDir.absolutePath());
                }
            }

            if (!inferredRoots.isEmpty())
            {
                m_parkedDirectories = QStringList(inferredRoots.begin(), inferredRoots.end());
                save();
            }
        }
        LOG_INFO(QString("Loaded %1 sites").arg(m_sites.size()));

        return true;
    }

    Services::ServiceResult<bool> SiteManager::validateSite(const Models::Site &site) const
    {
        QStringList errors = site.validate();

        if (!errors.isEmpty())
        {
            return Services::ServiceResult<bool>::Err(errors.join(", "));
        }

        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<bool> SiteManager::validateDomain(const QString &domain) const
    {
        // Allows: myapp, myapp.test, foo.bar.test
        // Blocks spaces, underscores, weird characters.
        QRegularExpression regex(
            "^[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?"
            "(\\.[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$");

        if (!regex.match(domain).hasMatch())
        {
            return Services::ServiceResult<bool>::Err("Invalid domain format");
        }

        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<bool> SiteManager::validatePath(const QString &path) const
    {
        if (!Utils::FileSystem::isDirectory(path))
        {
            return Services::ServiceResult<bool>::Err("Path does not exist or is not a directory");
        }

        if (!Utils::FileSystem::isReadable(path))
        {
            return Services::ServiceResult<bool>::Err("Path is not readable");
        }

        return Services::ServiceResult<bool>::Ok(true);
    }

    QString SiteManager::generateDatabaseName(const QString &domain) const
    {
        QString dbName = domain;
        dbName.replace('.', '_');
        dbName.replace('-', '_');
        return sanitizeDatabaseName(dbName);
    }

    QString SiteManager::sanitizeDatabaseName(const QString &name) const
    {
        QString sanitized = name;
        // Remove any characters not allowed in database names
        sanitized.replace(QRegularExpression("[^a-zA-Z0-9_]"), "");
        // Ensure it doesn't start with a number
        if (!sanitized.isEmpty() && sanitized[0].isDigit())
        {
            sanitized = "db_" + sanitized;
        }
        return sanitized;
    }

    Models::Site SiteManager::createDefaultSite(const QString &path, const QString &domain) const
    {
        Core::ConfigManager &config = Core::ConfigManager::instance();

        QString siteName = QFileInfo(path).fileName();
        Models::Site site(siteName, path, domain);
        site.setPhpVersion(config.defaultPhpVersion());
        site.setSslEnabled(config.enableSslByDefault());

        return site;
    }

    Services::ServiceResult<QStringList> SiteManager::scanParkedDirectory(const QString &path) const
    {
        QDir dir(path);
        if (!dir.exists())
        {
            return Services::ServiceResult<QStringList>::Err("Directory does not exist");
        }

        QStringList subdirs;
        QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QFileInfo &entry : entries)
        {
            subdirs.append(entry.absoluteFilePath());
        }

        return Services::ServiceResult<QStringList>::Ok(subdirs);
    }

    QString SiteManager::generateSiteNameFromPath(const QString &path) const
    {
        return QFileInfo(path).fileName();
    }

    QString SiteManager::getSitesFilePath() const
    {
        Core::ConfigManager &config = Core::ConfigManager::instance();
        return Utils::FileSystem::joinPath(config.sitesPath(), "sites.json");
    }

    QJsonObject SiteManager::sitesToJson() const
    {
        QJsonObject json;

        // Sites array
        QJsonArray sitesArray;
        for (const Models::Site &site : m_sites.values())
        {
            sitesArray.append(site.toJson());
        }
        json["sites"] = sitesArray;

        // Parked directories
        QJsonArray parkedArray;
        for (const QString &path : m_parkedDirectories)
        {
            parkedArray.append(path);
        }
        json["parkedDirectories"] = parkedArray;

        return json;
    }

    void SiteManager::sitesFromJson(const QJsonObject &json)
    {
        m_sites.clear();
        m_domainToSiteId.clear();
        m_parkedDirectories.clear();

        // Load sites
        if (json.contains("sites") && json["sites"].isArray())
        {
            QJsonArray sitesArray = json["sites"].toArray();
            for (const QJsonValue &value : sitesArray)
            {
                if (value.isObject())
                {
                    Models::Site site = Models::Site::fromJson(value.toObject());
                    m_sites[site.id()] = site;
                    m_domainToSiteId[site.domain()] = site.id();
                }
            }
        }

        // Load parked directories
        if (json.contains("parkedDirectories") && json["parkedDirectories"].isArray())
        {
            QJsonArray parkedArray = json["parkedDirectories"].toArray();
            for (const QJsonValue &value : parkedArray)
            {
                m_parkedDirectories.append(value.toString());
            }
        }
    }

    Services::NginxService *SiteManager::nginxService() const
    {
        auto *sm = Core::ServiceManager::instance();
        return sm ? sm->nginxService() : nullptr;
    }

    Services::DatabaseService *SiteManager::databaseService() const
    {
        auto *sm = Core::ServiceManager::instance();
        return sm ? sm->databaseService() : nullptr;
    }

    Services::DnsService *SiteManager::dnsService() const
    {
        auto *sm = Core::ServiceManager::instance();
        return sm ? sm->dnsService() : nullptr;
    }

    Services::PHPService *SiteManager::phpService() const
    {
        auto *sm = Core::ServiceManager::instance();
        return sm ? sm->phpService() : nullptr;
    }

    Anvil::Core::ServiceManager *SiteManager::serviceManager() const
    {
        return Anvil::Core::ServiceManager::instance();
    }
}
