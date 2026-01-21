#pragma once

#include <QString>
#include <QJsonObject>
#include <QMutex>

namespace Anvil::Core
{
    class ConfigManager
    {
    public:
        static ConfigManager &instance();

        // Initialization
        bool initialize();
        bool isInitialized() const;

        // Paths (read-only)
        QString anvilPath() const;
        QString phpPath() const;
        QString nginxPath() const;
        QString dataPath() const;
        QString logsPath() const;
        QString sitesPath() const;

        // Service settings
        QString defaultPhpVersion() const;
        void setDefaultPhpVersion(const QString &version);

        QString defaultNodeVersion() const;
        void setDefaultNodeVersion(const QString &version);

        QString databaseType() const;
        void setDatabaseType(const QString &type);

        int databasePort() const;
        void setDatabasePort(int port);

        // Site defaults
        QString defaultDomain() const;
        void setDefaultDomain(const QString &domain);

        bool autoStartServices() const;
        void setAutoStartServices(bool enabled);

        bool enableSslByDefault() const;
        void setEnableSslByDefault(bool enabled);

        // Nginx
        int nginxPort() const;
        void setNginxPort(int port);

        int nginxSslPort() const;
        void setNginxSslPort(int port);

        // PHP
        int maxPhpMemory() const;
        void setMaxPhpMemory(int mb);

        int maxUploadSize() const;
        void setMaxUploadSize(int mb);

        QString timezone() const;
        void setTimezone(const QString &timezone);

        // Persistence
        bool save();
        bool load();

        // Backup / Restore
        bool exportConfig(const QString &filePath);
        bool importConfig(const QString &filePath);

        // Reset
        void resetToDefaults();

        // Disable copying
        ConfigManager(const ConfigManager &) = delete;
        ConfigManager &operator=(const ConfigManager &) = delete;

    private:
        ConfigManager();
        ~ConfigManager();

        void createDefaultDirectories();
        void setDefaultValues();
        QString getConfigFilePath() const;

        QJsonObject toJson() const;
        void fromJson(const QJsonObject &json);

        // Paths
        QString m_anvilPath;
        QString m_phpPath;
        QString m_nginxPath;
        QString m_dataPath;
        QString m_logsPath;
        QString m_sitesPath;

        // Settings
        QString m_defaultPhpVersion;
        QString m_defaultNodeVersion;
        QString m_databaseType;
        int m_databasePort = 3306;

        QString m_defaultDomain;
        bool m_autoStartServices = false;
        bool m_enableSslByDefault = false;

        int m_nginxPort = 80;
        int m_nginxSslPort = 443;

        int m_maxPhpMemory = 512;
        int m_maxUploadSize = 64;
        QString m_timezone = "UTC";

        bool m_initialized = false;
        mutable QMutex m_mutex;
    };
}
