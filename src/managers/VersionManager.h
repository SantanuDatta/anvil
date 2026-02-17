#ifndef VERSIONMANAGER_H
#define VERSIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include "services/BaseService.h"
#include "models/PHPVersion.h"

namespace Anvil
{
    namespace Services
    {
        class PHPService;
        class NodeService;
    }

    namespace Managers
    {
        /**
         * @brief Manages PHP and Node.js versions across sites
         *
         * Handles version switching, per-site version configuration,
         * and maintains version-to-site mappings.
         */
        class VersionManager : public QObject
        {
            Q_OBJECT

        public:
            explicit VersionManager(QObject *parent = nullptr);
            ~VersionManager();

            // Initialization
            bool initialize();
            bool isInitialized() const { return m_initialized; }

            // ============================================================
            // Global Version Management
            // ============================================================

            // PHP Global Versions
            Services::ServiceResult<bool> switchGlobalPhpVersion(const QString &version);
            QString globalPhpVersion() const;
            Services::ServiceResult<QList<Models::PHPVersion>> listInstalledPhpVersions();
            Services::ServiceResult<QStringList> listAvailablePhpVersions();

            // Node Global Versions
            Services::ServiceResult<bool> switchGlobalNodeVersion(const QString &version);
            QString globalNodeVersion() const;
            Services::ServiceResult<QStringList> listInstalledNodeVersions();
            Services::ServiceResult<QStringList> listAvailableNodeVersions();

            // ============================================================
            // Per-Site Version Management
            // ============================================================

            // PHP per-site
            Services::ServiceResult<bool> setSitePhpVersion(const QString &siteId,
                                                            const QString &version);
            Services::ServiceResult<QString> getSitePhpVersion(const QString &siteId) const;

            // Node per-site
            Services::ServiceResult<bool> setSiteNodeVersion(const QString &siteId,
                                                             const QString &version);
            Services::ServiceResult<QString> getSiteNodeVersion(const QString &siteId) const;

            // ============================================================
            // Version Installation
            // ============================================================

            Services::ServiceResult<bool> installPhpVersion(const QString &version);
            Services::ServiceResult<bool> uninstallPhpVersion(const QString &version);
            Services::ServiceResult<bool> updatePhpVersion(const QString &version);

            Services::ServiceResult<bool> installNodeVersion(const QString &version);
            Services::ServiceResult<bool> uninstallNodeVersion(const QString &version);

            // ============================================================
            // Version Information
            // ============================================================

            // Check if versions are installed
            bool isPhpVersionInstalled(const QString &version) const;
            bool isNodeVersionInstalled(const QString &version) const;

            // Get detailed version info
            Services::ServiceResult<Models::PHPVersion> getPhpVersionInfo(const QString &version) const;

            // Get sites using a specific version
            QStringList getSitesUsingPhpVersion(const QString &version) const;
            QStringList getSitesUsingNodeVersion(const QString &version) const;

            // Version validation
            bool isValidPhpVersion(const QString &version) const;
            bool isValidNodeVersion(const QString &version) const;

            // ============================================================
            // Bulk Operations
            // ============================================================

            // Update all sites to use a specific version
            Services::ServiceResult<bool> updateAllSitesToPhpVersion(const QString &version);
            Services::ServiceResult<bool> updateAllSitesToNodeVersion(const QString &version);

            // Detect and sync versions from file system
            Services::ServiceResult<bool> syncVersions();

            // ============================================================
            // Persistence
            // ============================================================

            bool save();
            bool load();

        signals:
            void globalPhpVersionChanged(const QString &version);
            void globalNodeVersionChanged(const QString &version);
            void sitePhpVersionChanged(const QString &siteId, const QString &version);
            void siteNodeVersionChanged(const QString &siteId, const QString &version);
            void phpVersionInstalled(const QString &version);
            void phpVersionUninstalled(const QString &version);
            void nodeVersionInstalled(const QString &version);
            void nodeVersionUninstalled(const QString &version);
            void error(const QString &message);

        private:
            // Service accessors
            Services::PHPService *phpService() const;
            Services::NodeService *nodeService() const;

            // Version normalization
            QString normalizePhpVersion(const QString &version) const;
            QString normalizeNodeVersion(const QString &version) const;

            // Validation helpers
            Services::ServiceResult<bool> validatePhpVersion(const QString &version) const;
            Services::ServiceResult<bool> validateNodeVersion(const QString &version) const;

            // Persistence helpers
            QString getConfigFilePath() const;
            QJsonObject toJson() const;
            void fromJson(const QJsonObject &json);

            // Site-to-version mappings
            QMap<QString, QString> m_sitePhpVersions;  // siteId -> phpVersion
            QMap<QString, QString> m_siteNodeVersions; // siteId -> nodeVersion

            // Global versions cache
            QString m_globalPhpVersion;
            QString m_globalNodeVersion;

            bool m_initialized;
        };
    }
}
#endif
