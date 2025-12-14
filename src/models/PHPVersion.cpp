#include "models/PHPVersion.h"
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonValue>

namespace Anvil
{
    namespace Models
    {
        PHPVersion::PHPVersion()
            : m_isInstalled(false), m_isDefault(false)
        {
        }

        PHPVersion::PHPVersion(const QString &version)
            : m_version(version), m_isInstalled(false), m_isDefault(false)
        {
            parseVersion(version);
        }

        PHPVersion::PHPVersion(const QString &version, const QString &binaryPath)
            : m_version(version), m_binaryPath(binaryPath), m_isInstalled(true), m_isDefault(false)
        {
            parseVersion(version);
        }

        int PHPVersion::majorVersion() const
        {
            QStringList parts = m_version.split('.');
            return parts.isEmpty() ? 0 : parts[0].toInt();
        }

        int PHPVersion::minorVersion() const
        {
            QStringList parts = m_version.split('.');
            return parts.size() < 2 ? 0 : parts[1].toInt();
        }

        int PHPVersion::patchVersion() const
        {
            QStringList parts = m_version.split('.');
            return parts.size() < 3 ? 0 : parts[2].toInt();
        }

        QString PHPVersion::shortVersion() const
        {
            return QString("%1.%2").arg(majorVersion()).arg(minorVersion());
        }

        QString PHPVersion::fullVersion() const
        {
            return m_version;
        }

        bool PHPVersion::isValid() const
        {
            return isValidVersion(m_version);
        }

        bool PHPVersion::isValidVersion(const QString &version)
        {
            QRegularExpression regex("^\\d+\\.\\d+(\\.\\d+)?$");
            return regex.match(version).hasMatch();
        }

        bool PHPVersion::operator<(const PHPVersion &other) const
        {
            return compareVersions(other) < 0;
        }

        bool PHPVersion::operator>(const PHPVersion &other) const
        {
            return compareVersions(other) > 0;
        }

        bool PHPVersion::operator==(const PHPVersion &other) const
        {
            return m_version == other.m_version;
        }

        bool PHPVersion::operator!=(const PHPVersion &other) const
        {
            return !(*this == other);
        }

        QJsonObject PHPVersion::toJson() const
        {
            QJsonObject json;
            json["version"] = m_version;
            json["binaryPath"] = m_binaryPath;
            json["configPath"] = m_configPath;
            json["extensionPath"] = m_extensionPath;
            json["isInstalled"] = m_isInstalled;
            json["isDefault"] = m_isDefault;

            QJsonArray extensions;
            for (const QString &ext : m_installedExtensions)
            {
                extensions.append(ext);
            }
            json["installedExtensions"] = extensions;

            return json;
        }

        PHPVersion PHPVersion::fromJson(const QJsonObject &json)
        {
            PHPVersion version;
            version.m_version = json["version"].toString();
            version.m_binaryPath = json["binaryPath"].toString();
            version.m_configPath = json["configPath"].toString();
            version.m_extensionPath = json["extensionPath"].toString();
            version.m_isInstalled = json["isInstalled"].toBool(false);
            version.m_isDefault = json["isDefault"].toBool(false);

            QJsonArray extensions = json["installedExtensions"].toArray();
            for (const QJsonValue &ext : extensions)
            {
                version.m_installedExtensions << ext.toString();
            }

            return version;
        }

        QList<PHPVersion> PHPVersion::parseVersionList(const QString &output)
        {
            QList<PHPVersion> versions;

            QStringList lines = output.split('\n');
            for (const QString &line : lines)
            {
                PHPVersion version = parse(line);
                if (version.isValid())
                {
                    versions << version;
                }
            }

            return versions;
        }

        PHPVersion PHPVersion::parse(const QString &versionString)
        {
            // Extract version from strings like "PHP 8.3.12 (cli)" or just "8.3.12"
            QRegularExpression regex("(\\d+\\.\\d+(?:\\.\\d+)?)");
            QRegularExpressionMatch match = regex.match(versionString);

            if (match.hasMatch())
            {
                return PHPVersion(match.captured(1));
            }

            return PHPVersion();
        }

        void PHPVersion::parseVersion(const QString &version)
        {
            m_version = version;
        }

        int PHPVersion::compareVersions(const PHPVersion &other) const
        {
            if (majorVersion() != other.majorVersion())
            {
                return majorVersion() - other.majorVersion();
            }

            if (minorVersion() != other.minorVersion())
            {
                return minorVersion() - other.minorVersion();
            }

            return patchVersion() - other.patchVersion();
        }
    }
}
