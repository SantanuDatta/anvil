#ifndef PHPVERSION_H
#define PHPVERSION_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QList>

namespace Anvil
{
    namespace Models
    {
        class PHPVersion
        {
        public:
            PHPVersion();
            explicit PHPVersion(const QString &version);
            PHPVersion(const QString &version, const QString &binaryPath);

            // Getters
            QString version() const { return m_version; }
            QString binaryPath() const { return m_binaryPath; }
            QString configPath() const { return m_configPath; }
            QString extensionPath() const { return m_extensionPath; }
            bool isInstalled() const { return m_isInstalled; }
            bool isDefault() const { return m_isDefault; }
            QStringList installedExtensions() const { return m_installedExtensions; }

            // Setters
            void setVersion(const QString &version) { m_version = version; }
            void setBinaryPath(const QString &path) { m_binaryPath = path; }
            void setConfigPath(const QString &path) { m_configPath = path; }
            void setExtensionPath(const QString &path) { m_extensionPath = path; }
            void setIsInstalled(bool installed) { m_isInstalled = installed; }
            void setIsDefault(bool isDefault) { m_isDefault = isDefault; }
            void setInstalledExtensions(const QStringList &extensions) { m_installedExtensions = extensions; }

            // Version operations
            int majorVersion() const;
            int minorVersion() const;
            int patchVersion() const;
            QString shortVersion() const; // e.g., "8.3"
            QString fullVersion() const;  // e.g., "8.3.12"

            // Validation
            bool isValid() const;
            static bool isValidVersion(const QString &version);

            // Comparison
            bool operator<(const PHPVersion &other) const;
            bool operator>(const PHPVersion &other) const;
            bool operator==(const PHPVersion &other) const;
            bool operator!=(const PHPVersion &other) const;

            // Serialization
            QJsonObject toJson() const;
            static PHPVersion fromJson(const QJsonObject &json);

            // Static helpers
            static QList<PHPVersion> parseVersionList(const QString &output);
            static PHPVersion parse(const QString &versionString);

        private:
            QString m_version;
            QString m_binaryPath;
            QString m_configPath;
            QString m_extensionPath;
            bool m_isInstalled;
            bool m_isDefault;
            QStringList m_installedExtensions;

            void parseVersion(const QString &version);
            int compareVersions(const PHPVersion &other) const;
        };
    }
}

#endif
