#include "models/Site.h"
#include "utils/FileSystem.h"
#include <QUuid>
#include <QDir>

namespace Anvil::Models
{
    Site::Site()
        : m_phpVersion("8.3"), m_sslEnabled(false), m_isParked(false), m_createdAt(QDateTime::currentDateTime()), m_lastAccessed(QDateTime::currentDateTime())
    {
        generateId();
    }
    Site::Site(const QString &name, const QString &path, const QString &domain)
        : m_name(name), m_path(path), m_domain(domain), m_phpVersion("8.3"), m_sslEnabled(false), m_isParked(false), m_createdAt(QDateTime::currentDateTime()), m_lastAccessed(QDateTime::currentDateTime())
    {
        generateId();
    }
    QString Site::url() const
    {
        return QString("http://%1").arg(m_domain);
    }
    QString Site::secureUrl() const
    {
        return QString("https://%1").arg(m_domain);
    }
    bool Site::isValid() const
    {
        return validate().isEmpty();
    }
    QStringList Site::validate() const
    {
        QStringList errors;
        if (m_name.isEmpty())
        {
            errors << "Site name is required";
        }
        if (m_path.isEmpty())
        {
            errors << "Site path is required";
        }
        else if (!Utils::FileSystem::isDirectory(m_path))
        {
            errors << "Site path does not exist or is not a directory";
        }
        if (m_domain.isEmpty())
        {
            errors << "Domain is required";
        }
        if (m_phpVersion.isEmpty())
        {
            errors << "PHP version is required";
        }
        return errors;
    }
    QJsonObject Site::toJson() const
    {
        QJsonObject json;
        json["id"] = m_id;
        json["name"] = m_name;
        json["path"] = m_path;
        json["domain"] = m_domain;
        json["phpVersion"] = m_phpVersion;
        json["sslEnabled"] = m_sslEnabled;
        json["isParked"] = m_isParked;
        json["createdAt"] = m_createdAt.toString(Qt::ISODate);
        json["lastAccessed"] = m_lastAccessed.toString(Qt::ISODate);
        return json;
    }
    Site Site::fromJson(const QJsonObject &json)
    {
        Site site;
        site.m_id = json["id"].toString();
        site.m_name = json["name"].toString();
        site.m_path = json["path"].toString();
        site.m_domain = json["domain"].toString();
        site.m_phpVersion = json["phpVersion"].toString("8.3");
        site.m_sslEnabled = json["sslEnabled"].toBool(false);
        site.m_isParked = json["isParked"].toBool(false);
        QString createdAtStr = json["createdAt"].toString();
        if (!createdAtStr.isEmpty())
        {
            site.m_createdAt = QDateTime::fromString(createdAtStr, Qt::ISODate);
        }
        QString lastAccessedStr = json["lastAccessed"].toString();
        if (!lastAccessedStr.isEmpty())
        {
            site.m_lastAccessed = QDateTime::fromString(lastAccessedStr, Qt::ISODate);
        }
        return site;
    }
    bool Site::operator==(const Site &other) const
    {
        return m_id == other.m_id;
    }
    bool Site::operator!=(const Site &other) const
    {
        return !(*this == other);
    }
    void Site::generateId()
    {
        m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
}
