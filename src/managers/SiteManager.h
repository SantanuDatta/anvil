#ifndef SITEMANAGER_H
#define SITEMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QJsonObject>
#include "models/Site.h"
#include "services/BaseService.h"

namespace Anvil
{
    namespace Services
    {
        class NginxService;
        class DatabaseService;
        class DnsService;
        class PHPService;
    }

    namespace Managers
    {
        /**
         * @brief Orchestrates site creation across multiple services
         *
         * Provides transaction-like behavior with automatic rollback on failure.
         * Coordinates between Nginx, DNS, Database, and PHP services.
         */
        class SiteManager : public QObject
        {
            Q_OBJECT

        public:
            explicit SiteManager(QObject *parent = nullptr);
            ~SiteManager();

            // Initialization
            bool initialize();

            // Site operations
            Services::ServiceResult<bool> createSite(const Models::Site &site);
            Services::ServiceResult<bool> removeSite(const QString &siteId);
            Services::ServiceResult<bool> updateSite(const Models::Site &site);

            // Site queries
            Services::ServiceResult<Models::Site> getSite(const QString &siteId) const;
            Services::ServiceResult<QList<Models::Site>> listSites() const;
            Services::ServiceResult<QList<Models::Site>> listSitesByDomain(const QString &domain) const;
            bool siteExists(const QString &siteId) const;
            bool domainExists(const QString &domain) const;

            // Directory parking
            Services::ServiceResult<bool> parkDirectory(const QString &path);
            Services::ServiceResult<bool> unparkDirectory(const QString &path);
            Services::ServiceResult<QStringList> listParkedDirectories() const;
            bool isDirectoryParked(const QString &path) const;

            // Site state management
            Services::ServiceResult<bool> enableSite(const QString &siteId);
            Services::ServiceResult<bool> disableSite(const QString &siteId);
            bool isSiteEnabled(const QString &siteId) const;

            // SSL operations
            Services::ServiceResult<bool> enableSsl(const QString &siteId);
            Services::ServiceResult<bool> disableSsl(const QString &siteId);
            Services::ServiceResult<bool> generateSelfSignedCert(const QString &siteId);

            // Bulk operations
            Services::ServiceResult<bool> removeAllSites();
            Services::ServiceResult<bool> disableAllSites();

            // Persistence
            bool save();
            bool load();

        signals:
            void siteCreated(const QString &siteId);
            void siteRemoved(const QString &siteId);
            void siteUpdated(const QString &siteId);
            void siteEnabled(const QString &siteId);
            void siteDisabled(const QString &siteId);
            void error(const QString &message);

        private:
            // Transaction-like site creation
            struct SiteCreationState
            {
                bool dnsCreated = false;
                bool databaseCreated = false;
                bool nginxConfigured = false;
                QString databaseName;
                QString domain;
                QString siteId;
            };

            Services::ServiceResult<bool> createSiteWithRollback(const Models::Site &site);
            void rollbackSiteCreation(const SiteCreationState &state);

            // Validation
            Services::ServiceResult<bool> validateSite(const Models::Site &site) const;
            Services::ServiceResult<bool> validateDomain(const QString &domain) const;
            Services::ServiceResult<bool> validatePath(const QString &path) const;

            // Helper methods
            QString generateDatabaseName(const QString &domain) const;
            QString sanitizeDatabaseName(const QString &name) const;
            Models::Site createDefaultSite(const QString &path, const QString &domain) const;

            // Parking helpers
            Services::ServiceResult<QStringList> scanParkedDirectory(const QString &path) const;
            QString generateSiteNameFromPath(const QString &path) const;

            // Persistence helpers
            QString getSitesFilePath() const;
            QJsonObject sitesToJson() const;
            void sitesFromJson(const QJsonObject &json);

            // Service accessors
            Services::NginxService *nginxService() const;
            Services::DatabaseService *databaseService() const;
            Services::DnsService *dnsService() const;
            Services::PHPService *phpService() const;

            // Data storage
            QMap<QString, Models::Site> m_sites;     // siteId -> Site
            QMap<QString, QString> m_domainToSiteId; // domain -> siteId
            QStringList m_parkedDirectories;
            bool m_initialized;
        };
    }
}

#endif
