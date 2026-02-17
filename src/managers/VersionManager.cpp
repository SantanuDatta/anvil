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
#include <algorithm>

namespace Anvil::Managers
{
    namespace
    {
        QString normalizeVersion(const QString &version)
        {
            QStringList parts = version.split('.');
            return parts.size() >= 2 ? parts[0] + "." + parts[1] : version;
        }
    }

    VersionManager::VersionManager(QObject *parent)
        : QObject(parent), m_initialized(false) {}

    VersionManager::~VersionManager()
    {
        if (m_initialized)
            save();
    }

    bool VersionManager::initialize()
    {
        if (m_initialized)
        {
            LOG_WARNING("VersionManager already initialized");
            return true;
        }
        LOG_INFO("Initializing VersionManager");
        Core::ServiceManager *serviceManager = Core::ServiceManager::instance();
        if (!serviceManager || !serviceManager->isInitialized())
        {
            LOG_ERROR("ServiceManager not initialized");
            return false;
        }
        Core::ConfigManager &config = Core::ConfigManager::instance();
        m_globalPhpVersion = config.defaultPhpVersion();
        m_globalNodeVersion = config.defaultNodeVersion();
        load();
        m_initialized = true;
        LOG_INFO("VersionManager initialized successfully");
        LOG_INFO(QString("Global PHP: %1, Global Node: %2").arg(m_globalPhpVersion, m_globalNodeVersion));
        return true;
    }

    Services::ServiceResult<bool> VersionManager::switchGlobalPhpVersion(const QString &version)
    {
        LOG_INFO(QString("Switching global PHP version to: %1").arg(version));
        auto validation = validatePhpVersion(version);
        if (validation.isError())
            return validation;
        QString normalized = normalizeVersion(version);
        if (!isPhpVersionInstalled(normalized))
        {
            return Services::ServiceResult<bool>::Err(QString("PHP %1 is not installed").arg(normalized));
        }
        auto *php = phpService();
        if (!php)
            return Services::ServiceResult<bool>::Err("PHP service not available");
        auto result = php->switchVersion(normalized);
        if (result.isError())
            return result;
        m_globalPhpVersion = normalized;
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
        return php ? php->listInstalledVersions() : Services::ServiceResult<QList<Models::PHPVersion>>::Err("PHP service not available");
    }

    Services::ServiceResult<QStringList> VersionManager::listAvailablePhpVersions()
    {
        QStringList versions;

        auto *php = phpService();
        if (php)
        {
            auto availableResult = php->listAvailableVersions();
            if (availableResult.isSuccess())
            {
                for (const auto &phpVer : availableResult.data)
                    versions.append(normalizeVersion(phpVer.shortVersion()));
            }
        }

        auto installedResult = listInstalledPhpVersions();
        if (installedResult.isSuccess())
        {
            for (const auto &phpVer : installedResult.data)
                versions.append(normalizeVersion(phpVer.shortVersion()));
        }

        versions.removeAll(QString());
        versions.removeDuplicates();

        std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b)
                  {
                      const QStringList aParts = a.split('.');
                      const QStringList bParts = b.split('.');
                      const int aMajor = aParts.value(0).toInt();
                      const int bMajor = bParts.value(0).toInt();
                      if (aMajor != bMajor)
                          return aMajor > bMajor;

                      const int aMinor = aParts.value(1).toInt();
                      const int bMinor = bParts.value(1).toInt();
                      return aMinor > bMinor;
                  });

        return versions.isEmpty()
                   ? Services::ServiceResult<QStringList>::Err("No available PHP versions found")
                   : Services::ServiceResult<QStringList>::Ok(versions);
    }

    Services::ServiceResult<bool> VersionManager::switchGlobalNodeVersion(const QString &version)
    {
        LOG_INFO(QString("Switching global Node version to: %1").arg(version));
        auto validation = validateNodeVersion(version);
        if (validation.isError())
            return validation;
        QString normalized = normalizeVersion(version);
        if (!isNodeVersionInstalled(normalized))
        {
            return Services::ServiceResult<bool>::Err(QString("Node.js %1 is not installed").arg(normalized));
        }
        auto *node = nodeService();
        if (!node)
            return Services::ServiceResult<bool>::Err("Node service not available");
        auto result = node->useVersion(normalized);
        if (result.isError())
            return result;
        m_globalNodeVersion = normalized;
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
            return Services::ServiceResult<QStringList>::Err("Node service not available");
        auto result = node->listInstalledVersions();
        if (result.isError())
            return Services::ServiceResult<QStringList>::Err(result.error);
        QStringList versions;
        for (const auto &nodeVer : result.data)
            versions.append(nodeVer.version);
        return Services::ServiceResult<QStringList>::Ok(versions);
    }

    Services::ServiceResult<QStringList> VersionManager::listAvailableNodeVersions()
    {
        auto *node = nodeService();
        if (!node)
            return Services::ServiceResult<QStringList>::Err("Node service not available");
        auto result = node->listLTSVersions();
        return result.isSuccess() ? result : Services::ServiceResult<QStringList>::Ok({"18", "20", "22"});
    }

    Services::ServiceResult<bool> VersionManager::setSitePhpVersion(const QString &siteId, const QString &version)
    {
        LOG_INFO(QString("Setting PHP version for site %1: %2").arg(siteId, version));
        auto validation = validatePhpVersion(version);
        if (validation.isError())
            return validation;
        QString normalized = normalizeVersion(version);
        if (!isPhpVersionInstalled(normalized))
        {
            return Services::ServiceResult<bool>::Err(QString("PHP %1 is not installed").arg(normalized));
        }
        m_sitePhpVersions[siteId] = normalized;
        save();
        emit sitePhpVersionChanged(siteId, normalized);
        LOG_INFO(QString("Site %1 now uses PHP %2").arg(siteId, normalized));
        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<QString> VersionManager::getSitePhpVersion(const QString &siteId) const
    {
        return Services::ServiceResult<QString>::Ok(
            m_sitePhpVersions.contains(siteId) ? m_sitePhpVersions[siteId] : m_globalPhpVersion);
    }

    Services::ServiceResult<bool> VersionManager::setSiteNodeVersion(const QString &siteId, const QString &version)
    {
        LOG_INFO(QString("Setting Node version for site %1: %2").arg(siteId, version));
        auto validation = validateNodeVersion(version);
        if (validation.isError())
            return validation;
        QString normalized = normalizeVersion(version);
        if (!isNodeVersionInstalled(normalized))
        {
            return Services::ServiceResult<bool>::Err(QString("Node.js %1 is not installed").arg(normalized));
        }
        m_siteNodeVersions[siteId] = normalized;
        save();
        emit siteNodeVersionChanged(siteId, normalized);
        LOG_INFO(QString("Site %1 now uses Node %2").arg(siteId, normalized));
        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<QString> VersionManager::getSiteNodeVersion(const QString &siteId) const
    {
        return Services::ServiceResult<QString>::Ok(
            m_siteNodeVersions.contains(siteId) ? m_siteNodeVersions[siteId] : m_globalNodeVersion);
    }

    Services::ServiceResult<bool> VersionManager::installPhpVersion(const QString &version)
    {
        LOG_INFO(QString("Installing PHP %1").arg(version));
        QString normalized = normalizeVersion(version);
        auto *php = phpService();
        if (!php)
            return Services::ServiceResult<bool>::Err("PHP service not available");
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
        QString normalized = normalizeVersion(version);
        QStringList sites = getSitesUsingPhpVersion(normalized);
        if (!sites.isEmpty())
        {
            return Services::ServiceResult<bool>::Err(
                QString("Cannot uninstall PHP %1: Used by %2 site(s)").arg(normalized).arg(sites.size()));
        }
        auto *php = phpService();
        if (!php)
            return Services::ServiceResult<bool>::Err("PHP service not available");
        auto result = php->uninstallVersion(normalized);
        if (result.isSuccess())
        {
            emit phpVersionUninstalled(normalized);
            LOG_INFO(QString("PHP %1 uninstalled successfully").arg(normalized));
        }
        return result;
    }

    Services::ServiceResult<bool> VersionManager::updatePhpVersion(const QString &version)
    {
        LOG_INFO(QString("Updating PHP %1").arg(version));
        QString normalized = normalizeVersion(version);

        auto *php = phpService();
        if (!php)
            return Services::ServiceResult<bool>::Err("PHP service not available");

        auto result = php->updateVersion(normalized);
        if (result.isSuccess())
            LOG_INFO(QString("PHP %1 updated successfully").arg(normalized));

        return result;
    }

    Services::ServiceResult<bool> VersionManager::installNodeVersion(const QString &version)
    {
        LOG_INFO(QString("Installing Node.js %1").arg(version));
        QString normalized = normalizeVersion(version);
        auto *node = nodeService();
        if (!node)
            return Services::ServiceResult<bool>::Err("Node service not available");
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
        QString normalized = normalizeVersion(version);
        QStringList sites = getSitesUsingNodeVersion(normalized);
        if (!sites.isEmpty())
        {
            return Services::ServiceResult<bool>::Err(
                QString("Cannot uninstall Node.js %1: Used by %2 site(s)").arg(normalized).arg(sites.size()));
        }
        auto *node = nodeService();
        if (!node)
            return Services::ServiceResult<bool>::Err("Node service not available");
        auto result = node->uninstallVersion(normalized);
        if (result.isSuccess())
        {
            emit nodeVersionUninstalled(normalized);
            LOG_INFO(QString("Node.js %1 uninstalled successfully").arg(normalized));
        }
        return result;
    }

    bool VersionManager::isPhpVersionInstalled(const QString &version) const
    {
        auto *php = phpService();
        return php ? php->isVersionInstalled(normalizeVersion(version)) : false;
    }

    bool VersionManager::isNodeVersionInstalled(const QString &version) const
    {
        auto *node = nodeService();
        if (!node)
            return false;
        QString normalized = normalizeVersion(version);
        auto result = node->listInstalledVersions();
        if (result.isError())
            return false;
        for (const auto &nodeVer : result.data)
        {
            if (normalizeVersion(nodeVer.version) == normalized)
                return true;
        }
        return false;
    }

    Services::ServiceResult<Models::PHPVersion> VersionManager::getPhpVersionInfo(const QString &version) const
    {
        auto *php = phpService();
        if (!php)
            return Services::ServiceResult<Models::PHPVersion>::Err("PHP service not available");
        Models::PHPVersion info = php->getVersionInfo(normalizeVersion(version));
        return info.version().isEmpty() ? Services::ServiceResult<Models::PHPVersion>::Err("Version not found") : Services::ServiceResult<Models::PHPVersion>::Ok(info);
    }

    QStringList VersionManager::getSitesUsingPhpVersion(const QString &version) const
    {
        QStringList sites;
        QString normalized = normalizeVersion(version);
        for (auto it = m_sitePhpVersions.begin(); it != m_sitePhpVersions.end(); ++it)
        {
            if (normalizeVersion(it.value()) == normalized)
                sites.append(it.key());
        }
        return sites;
    }

    QStringList VersionManager::getSitesUsingNodeVersion(const QString &version) const
    {
        QStringList sites;
        QString normalized = normalizeVersion(version);
        for (auto it = m_siteNodeVersions.begin(); it != m_siteNodeVersions.end(); ++it)
        {
            if (normalizeVersion(it.value()) == normalized)
                sites.append(it.key());
        }
        return sites;
    }

    bool VersionManager::isValidPhpVersion(const QString &version) const
    {
        return QRegularExpression(R"(^\d+\.\d+(\.\d+)?$)").match(version).hasMatch();
    }

    bool VersionManager::isValidNodeVersion(const QString &version) const
    {
        return QRegularExpression(R"(^\d+(\.\d+)?(\.\d+)?$)").match(version).hasMatch();
    }

    Services::ServiceResult<bool> VersionManager::updateAllSitesToPhpVersion(const QString &version)
    {
        LOG_INFO(QString("Updating all sites to PHP %1").arg(version));
        QString normalized = normalizeVersion(version);
        auto validation = validatePhpVersion(normalized);
        if (validation.isError())
            return validation;
        for (const QString &siteId : m_sitePhpVersions.keys())
        {
            m_sitePhpVersions[siteId] = normalized;
            emit sitePhpVersionChanged(siteId, normalized);
        }
        auto switchResult = switchGlobalPhpVersion(normalized);
        if (switchResult.isError())
            return switchResult;
        save();
        LOG_INFO(QString("All sites updated to PHP %1").arg(normalized));
        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<bool> VersionManager::updateAllSitesToNodeVersion(const QString &version)
    {
        LOG_INFO(QString("Updating all sites to Node.js %1").arg(version));
        QString normalized = normalizeVersion(version);
        auto validation = validateNodeVersion(normalized);
        if (validation.isError())
            return validation;
        for (const QString &siteId : m_siteNodeVersions.keys())
        {
            m_siteNodeVersions[siteId] = normalized;
            emit siteNodeVersionChanged(siteId, normalized);
        }
        auto switchResult = switchGlobalNodeVersion(normalized);
        if (switchResult.isError())
            return switchResult;
        save();
        LOG_INFO(QString("All sites updated to Node.js %1").arg(normalized));
        return Services::ServiceResult<bool>::Ok(true);
    }

    Services::ServiceResult<bool> VersionManager::syncVersions()
    {
        LOG_INFO("Syncing versions from services");
        auto *php = phpService();
        if (php && php->isInstalled())
        {
            QString currentPhp = php->currentVersion();
            if (!currentPhp.isEmpty())
                m_globalPhpVersion = normalizeVersion(currentPhp);
        }
        auto *node = nodeService();
        if (node && node->isInstalled())
        {
            QString currentNode = node->currentVersion();
            if (!currentNode.isEmpty())
                m_globalNodeVersion = normalizeVersion(currentNode);
        }
        save();
        LOG_INFO(QString("Versions synced: PHP %1, Node %2").arg(m_globalPhpVersion, m_globalNodeVersion));
        return Services::ServiceResult<bool>::Ok(true);
    }

    bool VersionManager::save()
    {
        QString filePath = getConfigFilePath();
        QJsonObject json = toJson();
        QJsonDocument doc(json);
        auto result = Utils::FileSystem::writeFile(filePath, doc.toJson(QJsonDocument::Indented));
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
            return true;
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

    Services::PHPService *VersionManager::phpService() const
    {
        auto *serviceManager = Core::ServiceManager::instance();
        return serviceManager ? serviceManager->phpService() : nullptr;
    }

    Services::NodeService *VersionManager::nodeService() const
    {
        auto *serviceManager = Core::ServiceManager::instance();
        return serviceManager ? serviceManager->nodeService() : nullptr;
    }

    Services::ServiceResult<bool> VersionManager::validatePhpVersion(const QString &version) const
    {
        return isValidPhpVersion(version) ? Services::ServiceResult<bool>::Ok(true) : Services::ServiceResult<bool>::Err("Invalid PHP version format");
    }

    Services::ServiceResult<bool> VersionManager::validateNodeVersion(const QString &version) const
    {
        return isValidNodeVersion(version) ? Services::ServiceResult<bool>::Ok(true) : Services::ServiceResult<bool>::Err("Invalid Node version format");
    }

    QString VersionManager::getConfigFilePath() const
    {
        Core::ConfigManager &config = Core::ConfigManager::instance();
        return Utils::FileSystem::joinPath(config.dataPath(), "version-mappings.json");
    }

    QJsonObject VersionManager::toJson() const
    {
        QJsonObject json;
        json["globalPhpVersion"] = m_globalPhpVersion;
        json["globalNodeVersion"] = m_globalNodeVersion;
        QJsonObject phpMappings;
        for (auto it = m_sitePhpVersions.begin(); it != m_sitePhpVersions.end(); ++it)
        {
            phpMappings[it.key()] = it.value();
        }
        json["sitePhpVersions"] = phpMappings;
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
        if (json.contains("globalPhpVersion"))
        {
            m_globalPhpVersion = json["globalPhpVersion"].toString();
        }
        if (json.contains("globalNodeVersion"))
        {
            m_globalNodeVersion = json["globalNodeVersion"].toString();
        }
        if (json.contains("sitePhpVersions"))
        {
            QJsonObject phpMappings = json["sitePhpVersions"].toObject();
            for (auto it = phpMappings.begin(); it != phpMappings.end(); ++it)
            {
                m_sitePhpVersions[it.key()] = it.value().toString();
            }
        }
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
