#include "managers/VersionManager.h"
#include "core/ServiceManager.h"
#include "core/ConfigManager.h"
#include "services/PHPService.h"
#include "services/NodeService.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

namespace Anvil
{
    namespace Managers
    {
        VersionManager::VersionManager(QObject *parent)
            : QObject(parent), m_initialized(false)
        {
        }

        VersionManager::~VersionManager()
        {
            if (m_initialized)
            {
                save();
            }
        }

        bool VersionManager::initialize()
        {
            if (m_initialized)
            {
                LOG_WARNING("VersionManager already initialized");
                return true;
            }

            LOG_INFO("Initializing VersionManager");

            // Ensure ServiceManager is initialized
            Core::ServiceManager &serviceManager = Core::ServiceManager::instance();
            if (!serviceManager.isInitialized())
            {
                LOG_ERROR("ServiceManager not initialized");
                return false;
            }

            // Load global versions from ConfigManager
            Core::ConfigManager &config = Core::ConfigManager::instance();
            m_globalPhpVersion = config.defaultPhpVersion();
            m_globalNodeVersion = config.defaultNodeVersion();

            // Load per-site version mappings
            load();

            m_initialized = true;
            LOG_INFO("VersionManager initialized successfully");
            LOG_INFO(QString("Global PHP: %1, Global Node: %2")
                         .arg(m_globalPhpVersion, m_globalNodeVersion));

            return true;
        }

        // ============================================================
        // Global Version Management - PHP
        // ============================================================

        Services::ServiceResult<bool> VersionManager::switchGlobalPhpVersion(const QString &version)
        {
            LOG_INFO(QString("Switching global PHP version to: %1").arg(version));

            // Validate version
            auto validation = validatePhpVersion(version);
            if (validation.isError())
            {
                return validation;
            }

            QString normalized = normalizePhpVersion(version);

            // Check if version is installed
            if (!isPhpVersionInstalled(normalized))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("PHP %1 is not installed").arg(normalized));
            }

            // Switch version via PHPService
            auto *php = phpService();
            if (!php)
            {
                return Services::ServiceResult<bool>::Err("PHP service not available");
            }

            auto result = php->switchVersion(normalized);
            if (result.isError())
            {
                return result;
            }

            // Update global version
            m_globalPhpVersion = normalized;

            // Update ConfigManager
            Core::ConfigManager &config = Core::ConfigManager::instance();
            config.setDefaultPhpVersion(normalized);

            emit globalPhpVersionChanged(normalized);
            LOG_INFO(QString("Global PHP version changed to: %1").arg(normalized));

            return Services::ServiceResult<bool>::Ok(true);
        }

        QString VersionManager::globalPhpVersion() const
        {
            return m_globalPhpVersion;
        }

        Services::ServiceResult<QList<Models::PHPVersion>> VersionManager::listInstalledPhpVersions()
        {
            auto *php = phpService();
            if (!php)
            {
                return Services::ServiceResult<QList<Models::PHPVersion>>::Err(
                    "PHP service not available");
            }

            return php->listInstalledVersions();
        }

        Services::ServiceResult<QStringList> VersionManager::listAvailablePhpVersions()
        {
            // Common PHP versions
            QStringList versions = {"7.4", "8.0", "8.1", "8.2", "8.3", "8.4"};
            return Services::ServiceResult<QStringList>::Ok(versions);
        }

        // ============================================================
        // Global Version Management - Node
        // ============================================================

        Services::ServiceResult<bool> VersionManager::switchGlobalNodeVersion(const QString &version)
        {
            LOG_INFO(QString("Switching global Node version to: %1").arg(version));

            // Validate version
            auto validation = validateNodeVersion(version);
            if (validation.isError())
            {
                return validation;
            }

            QString normalized = normalizeNodeVersion(version);

            // Check if version is installed
            if (!isNodeVersionInstalled(normalized))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Node.js %1 is not installed").arg(normalized));
            }

            // Switch version via NodeService
            auto *node = nodeService();
            if (!node)
            {
                return Services::ServiceResult<bool>::Err("Node service not available");
            }

            auto result = node->useVersion(normalized);
            if (result.isError())
            {
                return result;
            }

            // Update global version
            m_globalNodeVersion = normalized;

            // Update ConfigManager
            Core::ConfigManager &config = Core::ConfigManager::instance();
            config.setDefaultNodeVersion(normalized);

            emit globalNodeVersionChanged(normalized);
            LOG_INFO(QString("Global Node version changed to: %1").arg(normalized));

            return Services::ServiceResult<bool>::Ok(true);
        }

        QString VersionManager::globalNodeVersion() const
        {
            return m_globalNodeVersion;
        }

        Services::ServiceResult<QStringList> VersionManager::listInstalledNodeVersions()
        {
            auto *node = nodeService();
            if (!node)
            {
                return Services::ServiceResult<QStringList>::Err("Node service not available");
            }

            auto result = node->listInstalledVersions();
            if (result.isError())
            {
                return Services::ServiceResult<QStringList>::Err(result.error);
            }

            QStringList versions;
            for (const auto &nodeVer : result.data)
            {
                versions.append(nodeVer.version);
            }

            return Services::ServiceResult<QStringList>::Ok(versions);
        }

        Services::ServiceResult<QStringList> VersionManager::listAvailableNodeVersions()
        {
            auto *node = nodeService();
            if (!node)
            {
                return Services::ServiceResult<QStringList>::Err("Node service not available");
            }

            // Get LTS versions
            auto result = node->listLTSVersions();
            if (result.isSuccess())
            {
                return result;
            }

            // Fallback to common versions
            QStringList versions = {"18", "20", "22"};
            return Services::ServiceResult<QStringList>::Ok(versions);
        }

        // ============================================================
        // Per-Site Version Management
        // ============================================================

        Services::ServiceResult<bool> VersionManager::setSitePhpVersion(const QString &siteId,
                                                                        const QString &version)
        {
            LOG_INFO(QString("Setting PHP version for site %1: %2").arg(siteId, version));

            // Validate version
            auto validation = validatePhpVersion(version);
            if (validation.isError())
            {
                return validation;
            }

            QString normalized = normalizePhpVersion(version);

            // Check if version is installed
            if (!isPhpVersionInstalled(normalized))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("PHP %1 is not installed").arg(normalized));
            }

            // Store mapping
            m_sitePhpVersions[siteId] = normalized;

            // Persist changes
            save();

            emit sitePhpVersionChanged(siteId, normalized);
            LOG_INFO(QString("Site %1 now uses PHP %2").arg(siteId, normalized));

            return Services::ServiceResult<bool>::Ok(true);
        }

        Services::ServiceResult<QString> VersionManager::getSitePhpVersion(const QString &siteId) const
        {
            if (m_sitePhpVersions.contains(siteId))
            {
                return Services::ServiceResult<QString>::Ok(m_sitePhpVersions[siteId]);
            }

            // Fall back to global version
            return Services::ServiceResult<QString>::Ok(m_globalPhpVersion);
        }

        Services::ServiceResult<bool> VersionManager::setSiteNodeVersion(const QString &siteId,
                                                                         const QString &version)
        {
            LOG_INFO(QString("Setting Node version for site %1: %2").arg(siteId, version));

            // Validate version
            auto validation = validateNodeVersion(version);
            if (validation.isError())
            {
                return validation;
            }

            QString normalized = normalizeNodeVersion(version);

            // Check if version is installed
            if (!isNodeVersionInstalled(normalized))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Node.js %1 is not installed").arg(normalized));
            }

            // Store mapping
            m_siteNodeVersions[siteId] = normalized;

            // Persist changes
            save();

            emit siteNodeVersionChanged(siteId, normalized);
            LOG_INFO(QString("Site %1 now uses Node %2").arg(siteId, normalized));

            return Services::ServiceResult<bool>::Ok(true);
        }

        Services::ServiceResult<QString> VersionManager::getSiteNodeVersion(const QString &siteId) const
        {
            if (m_siteNodeVersions.contains(siteId))
            {
                return Services::ServiceResult<QString>::Ok(m_siteNodeVersions[siteId]);
            }

            // Fall back to global version
            return Services::ServiceResult<QString>::Ok(m_globalNodeVersion);
        }

        // ============================================================
        // Version Installation
        // ============================================================

        Services::ServiceResult<bool> VersionManager::installPhpVersion(const QString &version)
        {
            LOG_INFO(QString("Installing PHP %1").arg(version));

            QString normalized = normalizePhpVersion(version);

            auto *php = phpService();
            if (!php)
            {
                return Services::ServiceResult<bool>::Err("PHP service not available");
            }

            auto result = php->installVersion(normalized);

            if (result.isSuccess())
            {
                emit phpVersionInstalled(normalized);
                LOG_INFO(QString("PHP %1 installed successfully").arg(normalized));
            }

            return result;
        }

        Services::ServiceResult<bool> VersionManager::uninstallPhpVersion(const QString &version)
        {
            LOG_INFO(QString("Uninstalling PHP %1").arg(version));

            QString normalized = normalizePhpVersion(version);

            // Check if any sites are using this version
            QStringList sites = getSitesUsingPhpVersion(normalized);
            if (!sites.isEmpty())
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Cannot uninstall PHP %1: Used by %2 site(s)")
                        .arg(normalized)
                        .arg(sites.size()));
            }

            auto *php = phpService();
            if (!php)
            {
                return Services::ServiceResult<bool>::Err("PHP service not available");
            }

            auto result = php->uninstallVersion(normalized);

            if (result.isSuccess())
            {
                emit phpVersionUninstalled(normalized);
                LOG_INFO(QString("PHP %1 uninstalled successfully").arg(normalized));
            }

            return result;
        }

        Services::ServiceResult<bool> VersionManager::installNodeVersion(const QString &version)
        {
            LOG_INFO(QString("Installing Node.js %1").arg(version));

            QString normalized = normalizeNodeVersion(version);

            auto *node = nodeService();
            if (!node)
            {
                return Services::ServiceResult<bool>::Err("Node service not available");
            }

            auto result = node->installVersion(normalized);

            if (result.isSuccess())
            {
                emit nodeVersionInstalled(normalized);
                LOG_INFO(QString("Node.js %1 installed successfully").arg(normalized));
            }

            return result;
        }

        Services::ServiceResult<bool> VersionManager::uninstallNodeVersion(const QString &version)
        {
            LOG_INFO(QString("Uninstalling Node.js %1").arg(version));

            QString normalized = normalizeNodeVersion(version);

            // Check if any sites are using this version
            QStringList sites = getSitesUsingNodeVersion(normalized);
            if (!sites.isEmpty())
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Cannot uninstall Node.js %1: Used by %2 site(s)")
                        .arg(normalized)
                        .arg(sites.size()));
            }

            auto *node = nodeService();
            if (!node)
            {
                return Services::ServiceResult<bool>::Err("Node service not available");
            }

            auto result = node->uninstallVersion(normalized);

            if (result.isSuccess())
            {
                emit nodeVersionUninstalled(normalized);
                LOG_INFO(QString("Node.js %1 uninstalled successfully").arg(normalized));
            }

            return result;
        }

        // ============================================================
        // Version Information
        // ============================================================

        bool VersionManager::isPhpVersionInstalled(const QString &version) const
        {
            auto *php = phpService();
            if (!php)
            {
                return false;
            }

            QString normalized = normalizePhpVersion(version);
            return php->isVersionInstalled(normalized);
        }

        bool VersionManager::isNodeVersionInstalled(const QString &version) const
        {
            auto *node = nodeService();
            if (!node)
            {
                return false;
            }

            QString normalized = normalizeNodeVersion(version);

            auto result = node->listInstalledVersions();
            if (result.isError())
            {
                return false;
            }

            for (const auto &nodeVer : result.data)
            {
                if (normalizeNodeVersion(nodeVer.version) == normalized)
                {
                    return true;
                }
            }

            return false;
        }

        Services::ServiceResult<Models::PHPVersion> VersionManager::getPhpVersionInfo(const QString &version) const
        {
            auto *php = phpService();
            if (!php)
            {
                return Services::ServiceResult<Models::PHPVersion>::Err("PHP service not available");
            }

            QString normalized = normalizePhpVersion(version);
            Models::PHPVersion info = php->getVersionInfo(normalized);

            if (info.version().isEmpty())
            {
                return Services::ServiceResult<Models::PHPVersion>::Err("Version not found");
            }

            return Services::ServiceResult<Models::PHPVersion>::Ok(info);
        }

        QStringList VersionManager::getSitesUsingPhpVersion(const QString &version) const
        {
            QStringList sites;
            QString normalized = normalizePhpVersion(version);

            for (auto it = m_sitePhpVersions.begin(); it != m_sitePhpVersions.end(); ++it)
            {
                if (normalizePhpVersion(it.value()) == normalized)
                {
                    sites.append(it.key());
                }
            }

            return sites;
        }

        QStringList VersionManager::getSitesUsingNodeVersion(const QString &version) const
        {
            QStringList sites;
            QString normalized = normalizeNodeVersion(version);

            for (auto it = m_siteNodeVersions.begin(); it != m_siteNodeVersions.end(); ++it)
            {
                if (normalizeNodeVersion(it.value()) == normalized)
                {
                    sites.append(it.key());
                }
            }

            return sites;
        }

        bool VersionManager::isValidPhpVersion(const QString &version) const
        {
            // Match X.Y or X.Y.Z format
            QRegularExpression regex(R"(^\d+\.\d+(\.\d+)?$)");
            return regex.match(version).hasMatch();
        }

        bool VersionManager::isValidNodeVersion(const QString &version) const
        {
            // Match X, X.Y, or X.Y.Z format
            QRegularExpression regex(R"(^\d+(\.\d+)?(\.\d+)?$)");
            return regex.match(version).hasMatch();
        }

        // ============================================================
        // Bulk Operations
        // ============================================================

        Services::ServiceResult<bool> VersionManager::updateAllSitesToPhpVersion(const QString &version)
        {
            LOG_INFO(QString("Updating all sites to PHP %1").arg(version));

            QString normalized = normalizePhpVersion(version);

            // Validate version
            auto validation = validatePhpVersion(normalized);
            if (validation.isError())
            {
                return validation;
            }

            // Update all site mappings
            QStringList siteIds = m_sitePhpVersions.keys();
            for (const QString &siteId : siteIds)
            {
                m_sitePhpVersions[siteId] = normalized;
                emit sitePhpVersionChanged(siteId, normalized);
            }

            // Switch global version
            auto switchResult = switchGlobalPhpVersion(normalized);
            if (switchResult.isError())
            {
                return switchResult;
            }

            save();

            LOG_INFO(QString("All sites updated to PHP %1").arg(normalized));
            return Services::ServiceResult<bool>::Ok(true);
        }

        Services::ServiceResult<bool> VersionManager::updateAllSitesToNodeVersion(const QString &version)
        {
            LOG_INFO(QString("Updating all sites to Node.js %1").arg(version));

            QString normalized = normalizeNodeVersion(version);

            // Validate version
            auto validation = validateNodeVersion(normalized);
            if (validation.isError())
            {
                return validation;
            }

            // Update all site mappings
            QStringList siteIds = m_siteNodeVersions.keys();
            for (const QString &siteId : siteIds)
            {
                m_siteNodeVersions[siteId] = normalized;
                emit siteNodeVersionChanged(siteId, normalized);
            }

            // Switch global version
            auto switchResult = switchGlobalNodeVersion(normalized);
            if (switchResult.isError())
            {
                return switchResult;
            }

            save();

            LOG_INFO(QString("All sites updated to Node.js %1").arg(normalized));
            return Services::ServiceResult<bool>::Ok(true);
        }

        Services::ServiceResult<bool> VersionManager::syncVersions()
        {
            LOG_INFO("Syncing versions from services");

            // Sync PHP versions
            auto *php = phpService();
            if (php && php->isInstalled())
            {
                QString currentPhp = php->currentVersion();
                if (!currentPhp.isEmpty())
                {
                    m_globalPhpVersion = normalizePhpVersion(currentPhp);
                }
            }

            // Sync Node versions
            auto *node = nodeService();
            if (node && node->isInstalled())
            {
                QString currentNode = node->currentVersion();
                if (!currentNode.isEmpty())
                {
                    m_globalNodeVersion = normalizeNodeVersion(currentNode);
                }
            }

            save();

            LOG_INFO(QString("Versions synced: PHP %1, Node %2")
                         .arg(m_globalPhpVersion, m_globalNodeVersion));

            return Services::ServiceResult<bool>::Ok(true);
        }

        // ============================================================
        // Persistence
        // ============================================================

        bool VersionManager::save()
        {
            QString filePath = getConfigFilePath();
            QJsonObject json = toJson();

            QJsonDocument doc(json);
            QString jsonString = doc.toJson(QJsonDocument::Indented);

            auto result = Utils::FileSystem::writeFile(filePath, jsonString);

            if (result.isError())
            {
                LOG_ERROR(QString("Failed to save version mappings: %1").arg(result.error));
                return false;
            }

            LOG_DEBUG("Version mappings saved successfully");
            return true;
        }

        bool VersionManager::load()
        {
            QString filePath = getConfigFilePath();

            if (!Utils::FileSystem::fileExists(filePath))
            {
                LOG_INFO("No existing version mappings file found");
                return true; // Not an error
            }

            auto result = Utils::FileSystem::readFile(filePath);

            if (result.isError())
            {
                LOG_ERROR(QString("Failed to load version mappings: %1").arg(result.error));
                return false;
            }

            QJsonDocument doc = QJsonDocument::fromJson(result.data.toUtf8());

            if (!doc.isObject())
            {
                LOG_ERROR("Invalid version mappings file format");
                return false;
            }

            fromJson(doc.object());
            LOG_INFO(QString("Loaded version mappings for %1 PHP sites, %2 Node sites")
                         .arg(m_sitePhpVersions.size())
                         .arg(m_siteNodeVersions.size()));

            return true;
        }

        // ============================================================
        // Private Helper Methods
        // ============================================================

        Services::PHPService *VersionManager::phpService() const
        {
            return Core::ServiceManager::instance().phpService();
        }

        Services::NodeService *VersionManager::nodeService() const
        {
            return Core::ServiceManager::instance().nodeService();
        }

        QString VersionManager::normalizePhpVersion(const QString &version) const
        {
            // "8.3.12" -> "8.3"
            // "8.3" -> "8.3"
            QStringList parts = version.split('.');
            if (parts.size() >= 2)
            {
                return parts[0] + "." + parts[1];
            }
            return version;
        }

        QString VersionManager::normalizeNodeVersion(const QString &version) const
        {
            // "20.11.0" -> "20"
            // "20" -> "20"
            QStringList parts = version.split('.');
            if (!parts.isEmpty())
            {
                return parts[0];
            }
            return version;
        }

        Services::ServiceResult<bool> VersionManager::validatePhpVersion(const QString &version) const
        {
            if (!isValidPhpVersion(version))
            {
                return Services::ServiceResult<bool>::Err("Invalid PHP version format");
            }

            return Services::ServiceResult<bool>::Ok(true);
        }

        Services::ServiceResult<bool> VersionManager::validateNodeVersion(const QString &version) const
        {
            if (!isValidNodeVersion(version))
            {
                return Services::ServiceResult<bool>::Err("Invalid Node version format");
            }

            return Services::ServiceResult<bool>::Ok(true);
        }

        QString VersionManager::getConfigFilePath() const
        {
            Core::ConfigManager &config = Core::ConfigManager::instance();
            return Utils::FileSystem::joinPath(config.dataPath(), "version-mappings.json");
        }

        QJsonObject VersionManager::toJson() const
        {
            QJsonObject json;

            // Global versions
            json["globalPhpVersion"] = m_globalPhpVersion;
            json["globalNodeVersion"] = m_globalNodeVersion;

            // PHP site mappings
            QJsonObject phpMappings;
            for (auto it = m_sitePhpVersions.begin(); it != m_sitePhpVersions.end(); ++it)
            {
                phpMappings[it.key()] = it.value();
            }
            json["sitePhpVersions"] = phpMappings;

            // Node site mappings
            QJsonObject nodeMappings;
            for (auto it = m_siteNodeVersions.begin(); it != m_siteNodeVersions.end(); ++it)
            {
                nodeMappings[it.key()] = it.value();
            }
            json["siteNodeVersions"] = nodeMappings;

            return json;
        }

        void VersionManager::fromJson(const QJsonObject &json)
        {
            // Load global versions
            if (json.contains("globalPhpVersion"))
            {
                m_globalPhpVersion = json["globalPhpVersion"].toString();
            }

            if (json.contains("globalNodeVersion"))
            {
                m_globalNodeVersion = json["globalNodeVersion"].toString();
            }

            // Load PHP site mappings
            if (json.contains("sitePhpVersions"))
            {
                QJsonObject phpMappings = json["sitePhpVersions"].toObject();
                for (auto it = phpMappings.begin(); it != phpMappings.end(); ++it)
                {
                    m_sitePhpVersions[it.key()] = it.value().toString();
                }
            }

            // Load Node site mappings
            if (json.contains("siteNodeVersions"))
            {
                QJsonObject nodeMappings = json["siteNodeVersions"].toObject();
                for (auto it = nodeMappings.begin(); it != nodeMappings.end(); ++it)
                {
                    m_siteNodeVersions[it.key()] = it.value().toString();
                }
            }
        }
    }
}
