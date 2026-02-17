#ifndef PHPSERVICE_H
#define PHPSERVICE_H

#include "services/BaseService.h"
#include "models/PHPVersion.h"
#include <QMap>

namespace Anvil::Services
{
    class PHPService : public BaseService
    {
        Q_OBJECT

    public:
        explicit PHPService(QObject *parent = nullptr);
        ~PHPService() override;

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

        // PHP-specific operations
        ServiceResult<QList<Models::PHPVersion>> listAvailableVersions();
        ServiceResult<QList<Models::PHPVersion>> listInstalledVersions();
        ServiceResult<bool> installVersion(const QString &version);
        ServiceResult<bool> uninstallVersion(const QString &version);
        ServiceResult<bool> switchVersion(const QString &version);

        // Version info
        QString currentVersion() const { return m_currentVersion; }
        QString defaultVersion() const;
        Models::PHPVersion getVersionInfo(const QString &version) const;
        bool isVersionInstalled(const QString &version) const;

        // PHP-FPM operations
        ServiceResult<bool> startFpm(const QString &version);
        ServiceResult<bool> stopFpm(const QString &version);
        ServiceResult<bool> restartFpm(const QString &version);
        bool isFpmRunning(const QString &version) const;

        // Extensions
        ServiceResult<QStringList> listAvailableExtensions(const QString &version);
        ServiceResult<QStringList> listInstalledExtensions(const QString &version);
        ServiceResult<bool> installExtension(const QString &version, const QString &extension);
        ServiceResult<bool> uninstallExtension(const QString &version, const QString &extension);

        // Configuration
        ServiceResult<QString> getPhpIni(const QString &version);
        ServiceResult<bool> setPhpIni(const QString &version, const QString &content);
        ServiceResult<QString> getFpmConfig(const QString &version);
        ServiceResult<bool> setFpmConfig(const QString &version, const QString &content);

        // Paths
        QString phpBinaryPath(const QString &version) const;
        QString phpFpmBinaryPath(const QString &version) const;
        QString phpIniPath(const QString &version) const;
        QString fpmConfigPath(const QString &version) const;

    signals:
        void versionInstalled(const QString &version);
        void versionUninstalled(const QString &version);
        void versionSwitched(const QString &version);
        void extensionInstalled(const QString &version, const QString &extension);
        void installProgress(int percentage, const QString &message);

    private:
        // Installation helpers
        ServiceResult<bool> installFromRepository(const QString &version);
        ServiceResult<bool> installFromSource(const QString &version);
        ServiceResult<bool> downloadPhp(const QString &version, const QString &destination);
        ServiceResult<bool> compilePhp(const QString &version, const QString &sourceDir);

        // Version detection
        ServiceResult<QString> detectSystemPhpVersion();
        void scanInstalledVersions();

        // Configuration helpers
        QString generatePhpIniConfig(const QString &version);
        QString generateFpmPoolConfig(const QString &version);

        // Path helpers
        QString getPhpBasePath(const QString &version) const;
        void updateSymlinks(const QString &version);

        // Repository detection
        // Repository discovery helpers
        QStringList discoverRepositoryPhpVersions() const;
        QStringList discoverInstalledPhpVersions() const;
        QStringList parsePhpVersionsFromOutput(const QString &output) const;
        QString normalizeVersionToken(const QString &token) const;

        QString detectPackageManager() const;
        bool isUbuntuPpaAvailable() const;

        QString m_currentVersion;
        QMap<QString, Models::PHPVersion> m_installedVersions;
        QString m_phpBasePath;
        QString m_packageManager;
    };
}
#endif
