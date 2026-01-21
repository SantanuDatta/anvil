#ifndef NODESERVICE_H
#define NODESERVICE_H

#include "services/BaseService.h"
#include <QMap>

namespace Anvil::Services
{
    struct NodeVersion
    {
        QString version; // e.g., "20.11.0"
        QString alias;   // e.g., "lts/iron", "node", "default"
        bool isInstalled;
        bool isDefault;
        QString path;

        NodeVersion() : isInstalled(false), isDefault(false) {}

        QString shortVersion() const
        {
            // "20.11.0" -> "20"
            return version.split('.').first();
        }
    };

    class NodeService : public BaseService
    {
        Q_OBJECT

    public:
        explicit NodeService(QObject *parent = nullptr);
        ~NodeService() override;

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

        // Check if nvm is installed
        bool hasNvm() const;
        QString nvmPath() const;

        // Node version management
        ServiceResult<QList<NodeVersion>> listAvailableVersions();
        ServiceResult<QList<NodeVersion>> listInstalledVersions();
        ServiceResult<bool> installVersion(const QString &version);
        ServiceResult<bool> uninstallVersion(const QString &version);
        ServiceResult<bool> useVersion(const QString &version);
        ServiceResult<bool> setDefaultVersion(const QString &version);

        // Current version
        QString currentVersion() const { return m_currentVersion; }
        QString defaultVersion() const;

        // LTS versions
        ServiceResult<QStringList> listLTSVersions();
        QString latestLTS() const;

        // npm management
        ServiceResult<QString> npmVersion();
        ServiceResult<bool> updateNpm();

        // Package management (for site)
        ServiceResult<bool> npmInstall(const QString &projectPath);
        ServiceResult<bool> npmUpdate(const QString &projectPath);
        ServiceResult<bool> npmRun(const QString &projectPath, const QString &script);

        // .nvmrc support
        ServiceResult<QString> readNvmrc(const QString &projectPath);
        ServiceResult<bool> useNvmrc(const QString &projectPath);

    signals:
        void versionInstalled(const QString &version);
        void versionUninstalled(const QString &version);
        void versionSwitched(const QString &version);

    private:
        // nvm operations
        ServiceResult<QString> executeNvm(const QStringList &args);
        ServiceResult<QString> executeNode(const QStringList &args);
        ServiceResult<QString> executeNpm(const QStringList &args);

        // Installation
        ServiceResult<bool> installNvm();
        QString getNvmInstallScript();

        // Detection
        ServiceResult<bool> detectNvm();
        void scanInstalledVersions();
        QString detectCurrentVersion();

        // Path helpers
        QString getNodePath(const QString &version) const;
        QString getNpmPath(const QString &version) const;
        QString getNvmDir() const;

        // Parsing
        QList<NodeVersion> parseVersionList(const QString &output);
        NodeVersion parseVersionString(const QString &line);

        bool m_hasNvm;
        QString m_nvmDir;
        QString m_currentVersion;
        QString m_defaultVersion;
        QMap<QString, NodeVersion> m_installedVersions;
    };
}
#endif
