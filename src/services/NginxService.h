#ifndef NGINXSERVICE_H
#define NGINXSERVICE_H

#include "services/BaseService.h"
#include "models/Site.h"
#include <QMap>

namespace Anvil {
    namespace Services {
        class NginxService : public BaseService {
            Q_OBJECT

        public:
            explicit NginxService(QObject* parent = nullptr);
            ~NginxService() override;

            // BaseService implementation
            ServiceResult<bool> start() override;
            ServiceResult<bool> stop() override;
            bool isInstalled() const override;
            bool isRunning() const override;
            ServiceResult<bool> install() override;
            ServiceResult<bool> uninstall() override;
            ServiceResult<bool> configure() override;
            QString configPath() const override;
            ServiceResult<QString> detectVersion() override;

            // Nginx-specific operations
            ServiceResult<bool> reload();
            ServiceResult<bool> testConfig();

            // Site configuration
            ServiceResult<bool> addSite(const Models::Site& site);
            ServiceResult<bool> removeSite(const QString& siteId);
            ServiceResult<bool> updateSite(const Models::Site& site);
            ServiceResult<bool> enableSite(const QString& siteId);
            ServiceResult<bool> disableSite(const QString& siteId);

            // SSL operations
            ServiceResult<bool> generateSelfSignedCert(const QString& domain);
            ServiceResult<bool> installCertificate(const QString& domain,
                                                   const QString& certPath,
                                                   const QString& keyPath);

            // Configuration paths
            QString sitesAvailablePath() const { return m_sitesAvailablePath; }
            QString sitesEnabledPath() const { return m_sitesEnabledPath; }
            QString sslPath() const { return m_sslPath; }
            QString logsPath() const { return m_logsPath; }

            // Site config file paths
            QString getSiteConfigPath(const QString& siteId) const;
            QString getSiteEnabledPath(const QString& siteId) const;

            // Validation
            ServiceResult<bool> validateSiteConfig(const Models::Site& site);

        signals:
            void siteAdded(const QString& siteId);
            void siteRemoved(const QString& siteId);
            void siteEnabled(const QString& siteId);
            void siteDisabled(const QString& siteId);
            void configReloaded();

        private:
            // Configuration generation
            QString generateSiteConfig(const Models::Site& site);
            QString generatePhpFpmBlock(const QString& phpVersion);
            QString generateSslBlock(const QString& domain);
            QString generateMainConfig();

            // SSL helpers
            ServiceResult<bool> ensureSslDirectory();
            bool hasSslCertificate(const QString& domain) const;
            QString getSslCertPath(const QString& domain) const;
            QString getSslKeyPath(const QString& domain) const;

            // Path helpers
            QString getNginxBasePath() const;
            QString getNginxBinary() const;

            // Detection
            QString detectPackageManager() const;
            ServiceResult<bool> detectNginx();

            QString m_nginxBasePath;
            QString m_sitesAvailablePath;
            QString m_sitesEnabledPath;
            QString m_sslPath;
            QString m_logsPath;
            QString m_packageManager;
        };
    }
}
#endif
