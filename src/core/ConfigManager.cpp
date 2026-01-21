#include "core/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QMutexLocker>

namespace Anvil::Core
{
    ConfigManager::ConfigManager()
        : m_defaultPhpVersion("8.3"), m_defaultNodeVersion("20"), m_databaseType("MySQL"), m_databasePort(3306), m_defaultDomain("test"), m_autoStartServices(false), m_enableSslByDefault(false), m_nginxPort(80), m_nginxSslPort(443), m_maxPhpMemory(512), m_maxUploadSize(100), m_timezone("UTC"), m_initialized(false)
    {
        // Set base path
        QString home = QDir::homePath();
        m_anvilPath = home + "/.anvil";

        // Set service paths
        m_phpPath = m_anvilPath + "/php";
        m_nginxPath = m_anvilPath + "/nginx";
        m_dataPath = m_anvilPath + "/data";
        m_logsPath = m_anvilPath + "/logs";
        m_sitesPath = m_anvilPath + "/sites";
    }

    ConfigManager::~ConfigManager() = default;

    ConfigManager &ConfigManager::instance()
    {
        static ConfigManager instance;
        return instance;
    }

    bool ConfigManager::initialize()
    {
        QMutexLocker locker(&m_mutex);

        if (m_initialized)
        {
            LOG_WARNING("ConfigManager already initialized");
            return true;
        }

        LOG_INFO("Initializing ConfigManager");

        // Create directory structure
        createDefaultDirectories();

        // Load existing config or create defaults
        if (Utils::FileSystem::fileExists(getConfigFilePath()))
        {
            if (!load())
            {
                LOG_WARNING("Failed to load config, using defaults");
                setDefaultValues();
            }
        }
        else
        {
            LOG_INFO("No existing config found, creating defaults");
            setDefaultValues();
            save();
        }

        m_initialized = true;
        LOG_INFO(QString("ConfigManager initialized at: %1").arg(m_anvilPath));

        return true;
    }

    void ConfigManager::createDefaultDirectories()
    {
        QStringList directories = {
            m_anvilPath,
            m_phpPath,
            m_nginxPath,
            m_nginxPath + "/sites-available",
            m_nginxPath + "/sites-enabled",
            m_nginxPath + "/ssl",
            m_dataPath,
            m_logsPath,
            m_sitesPath};

        for (const QString &dir : directories)
        {
            if (!Utils::FileSystem::ensureDirectoryExists(dir))
            {
                LOG_ERROR(QString("Failed to create directory: %1").arg(dir));
            }
        }
    }

    void ConfigManager::setDefaultValues()
    {
        m_defaultPhpVersion = "8.3";
        m_defaultNodeVersion = "20";
        m_databaseType = "MySQL";
        m_databasePort = 3306;
        m_defaultDomain = "test";
        m_autoStartServices = false;
        m_enableSslByDefault = false;
        m_nginxPort = 80;
        m_nginxSslPort = 443;
        m_maxPhpMemory = 512;
        m_maxUploadSize = 100;
        m_timezone = "UTC";
    }

    QString ConfigManager::getConfigFilePath() const
    {
        return m_anvilPath + "/config.json";
    }

    // Getters and Setters
    QString ConfigManager::defaultPhpVersion() const
    {
        QMutexLocker locker(&m_mutex);
        return m_defaultPhpVersion;
    }

    void ConfigManager::setDefaultPhpVersion(const QString &version)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_defaultPhpVersion != version)
            {
                m_defaultPhpVersion = version;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Default PHP version set to: %1").arg(version));
            save();
        }
    }

    QString ConfigManager::defaultNodeVersion() const
    {
        QMutexLocker locker(&m_mutex);
        return m_defaultNodeVersion;
    }

    void ConfigManager::setDefaultNodeVersion(const QString &version)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_defaultNodeVersion != version)
            {
                m_defaultNodeVersion = version;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Default Node version set to: %1").arg(version));
            save();
        }
    }

    QString ConfigManager::databaseType() const
    {
        QMutexLocker locker(&m_mutex);
        return m_databaseType;
    }

    void ConfigManager::setDatabaseType(const QString &type)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_databaseType != type)
            {
                m_databaseType = type;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Database type set to: %1").arg(type));
            save();
        }
    }

    int ConfigManager::databasePort() const
    {
        QMutexLocker locker(&m_mutex);
        return m_databasePort;
    }

    void ConfigManager::setDatabasePort(int port)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_databasePort != port)
            {
                m_databasePort = port;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Database port set to: %1").arg(port));
            save();
        }
    }

    QString ConfigManager::defaultDomain() const
    {
        QMutexLocker locker(&m_mutex);
        return m_defaultDomain;
    }

    void ConfigManager::setDefaultDomain(const QString &domain)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_defaultDomain != domain)
            {
                m_defaultDomain = domain;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Default domain set to: .%1").arg(domain));
            save();
        }
    }

    bool ConfigManager::autoStartServices() const
    {
        QMutexLocker locker(&m_mutex);
        return m_autoStartServices;
    }

    void ConfigManager::setAutoStartServices(bool enabled)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_autoStartServices != enabled)
            {
                m_autoStartServices = enabled;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Auto-start services: %1").arg(enabled ? "enabled" : "disabled"));
            save();
        }
    }

    bool ConfigManager::enableSslByDefault() const
    {
        QMutexLocker locker(&m_mutex);
        return m_enableSslByDefault;
    }

    void ConfigManager::setEnableSslByDefault(bool enabled)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_enableSslByDefault != enabled)
            {
                m_enableSslByDefault = enabled;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Enable SSL by default: %1").arg(enabled ? "enabled" : "disabled"));
            save();
        }
    }

    void ConfigManager::setNginxPort(int port)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_nginxPort != port)
            {
                m_nginxPort = port;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Nginx port set to: %1").arg(port));
            save();
        }
    }

    void ConfigManager::setNginxSslPort(int port)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_nginxSslPort != port)
            {
                m_nginxSslPort = port;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Nginx SSL port set to: %1").arg(port));
            save();
        }
    }

    int ConfigManager::maxPhpMemory() const
    {
        QMutexLocker locker(&m_mutex);
        return m_maxPhpMemory;
    }

    void ConfigManager::setMaxPhpMemory(int mb)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_maxPhpMemory != mb)
            {
                m_maxPhpMemory = mb;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Max PHP memory set to: %1MB").arg(mb));
            save();
        }
    }

    int ConfigManager::maxUploadSize() const
    {
        QMutexLocker locker(&m_mutex);
        return m_maxUploadSize;
    }

    void ConfigManager::setMaxUploadSize(int mb)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_maxUploadSize != mb)
            {
                m_maxUploadSize = mb;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Max upload size set to: %1MB").arg(mb));
            save();
        }
    }

    QString ConfigManager::timezone() const
    {
        QMutexLocker locker(&m_mutex);
        return m_timezone;
    }

    void ConfigManager::setTimezone(const QString &timezone)
    {
        bool changed = false;

        {
            QMutexLocker locker(&m_mutex);
            if (m_timezone != timezone)
            {
                m_timezone = timezone;
                changed = true;
            }
        }

        if (changed)
        {
            LOG_INFO(QString("Timezone set to: %1").arg(timezone));
            save();
        }
    }

    // Persistence
    bool ConfigManager::save()
    {
        QMutexLocker locker(&m_mutex);

        QString configPath = getConfigFilePath();
        QJsonObject json = toJson();

        QJsonDocument doc(json);
        QString jsonString = doc.toJson(QJsonDocument::Indented);

        auto result = Utils::FileSystem::writeFile(configPath, jsonString);

        if (result.isError())
        {
            LOG_ERROR(QString("Failed to save config: %1").arg(result.error));
            return false;
        }

        LOG_DEBUG("Configuration saved successfully");
        return true;
    }

    bool ConfigManager::load()
    {
        QMutexLocker locker(&m_mutex);

        QString configPath = getConfigFilePath();

        if (!Utils::FileSystem::fileExists(configPath))
        {
            LOG_WARNING("Config file does not exist");
            return false;
        }

        auto result = Utils::FileSystem::readFile(configPath);

        if (result.isError())
        {
            LOG_ERROR(QString("Failed to read config: %1").arg(result.error));
            return false;
        }

        QJsonDocument doc = QJsonDocument::fromJson(result.data.toUtf8());

        if (!doc.isObject())
        {
            LOG_ERROR("Invalid config file format");
            return false;
        }

        fromJson(doc.object());
        LOG_INFO("Configuration loaded successfully");

        return true;
    }

    QJsonObject ConfigManager::toJson() const
    {
        QJsonObject json;

        // Paths
        json["anvilPath"] = m_anvilPath;
        json["phpPath"] = m_phpPath;
        json["nginxPath"] = m_nginxPath;
        json["dataPath"] = m_dataPath;
        json["logsPath"] = m_logsPath;
        json["sitesPath"] = m_sitesPath;

        // Service settings
        json["defaultPhpVersion"] = m_defaultPhpVersion;
        json["defaultNodeVersion"] = m_defaultNodeVersion;
        json["databaseType"] = m_databaseType;
        json["databasePort"] = m_databasePort;

        // Site defaults
        json["defaultDomain"] = m_defaultDomain;
        json["autoStartServices"] = m_autoStartServices;
        json["enableSslByDefault"] = m_enableSslByDefault;

        // Nginx
        json["nginxPort"] = m_nginxPort;
        json["nginxSslPort"] = m_nginxSslPort;

        // PHP settings
        json["maxPhpMemory"] = m_maxPhpMemory;
        json["maxUploadSize"] = m_maxUploadSize;
        json["timezone"] = m_timezone;

        return json;
    }

    void ConfigManager::fromJson(const QJsonObject &json)
    {
        // Service settings (don't override paths)
        if (json.contains("defaultPhpVersion"))
            m_defaultPhpVersion = json["defaultPhpVersion"].toString("8.3");

        if (json.contains("defaultNodeVersion"))
            m_defaultNodeVersion = json["defaultNodeVersion"].toString("20");

        if (json.contains("databaseType"))
            m_databaseType = json["databaseType"].toString("MySQL");

        if (json.contains("databasePort"))
            m_databasePort = json["databasePort"].toInt(3306);

        // Site defaults
        if (json.contains("defaultDomain"))
            m_defaultDomain = json["defaultDomain"].toString("test");

        if (json.contains("autoStartServices"))
            m_autoStartServices = json["autoStartServices"].toBool(false);

        if (json.contains("enableSslByDefault"))
            m_enableSslByDefault = json["enableSslByDefault"].toBool(false);

        // Nginx
        if (json.contains("nginxPort"))
            m_nginxPort = json["nginxPort"].toInt(80);

        if (json.contains("nginxSslPort"))
            m_nginxSslPort = json["nginxSslPort"].toInt(443);

        // PHP settings
        if (json.contains("maxPhpMemory"))
            m_maxPhpMemory = json["maxPhpMemory"].toInt(512);

        if (json.contains("maxUploadSize"))
            m_maxUploadSize = json["maxUploadSize"].toInt(100);

        if (json.contains("timezone"))
            m_timezone = json["timezone"].toString("UTC");
    }

    bool ConfigManager::exportConfig(const QString &filePath)
    {
        QMutexLocker locker(&m_mutex);

        QJsonObject json = toJson();
        QJsonDocument doc(json);

        auto result = Utils::FileSystem::writeFile(filePath,
                                                   doc.toJson(QJsonDocument::Indented));

        if (result.isSuccess())
        {
            LOG_INFO(QString("Config exported to: %1").arg(filePath));
            return true;
        }

        LOG_ERROR(QString("Failed to export config: %1").arg(result.error));
        return false;
    }

    bool ConfigManager::importConfig(const QString &filePath)
    {
        QMutexLocker locker(&m_mutex);

        auto result = Utils::FileSystem::readFile(filePath);

        if (result.isError())
        {
            LOG_ERROR(QString("Failed to import config: %1").arg(result.error));
            return false;
        }

        QJsonDocument doc = QJsonDocument::fromJson(result.data.toUtf8());

        if (!doc.isObject())
        {
            LOG_ERROR("Invalid config file format");
            return false;
        }

        fromJson(doc.object());
        save();

        LOG_INFO(QString("Config imported from: %1").arg(filePath));
        return true;
    }

    void ConfigManager::resetToDefaults()
    {
        QMutexLocker locker(&m_mutex);

        LOG_INFO("Resetting configuration to defaults");
        setDefaultValues();
        save();
    }
}
