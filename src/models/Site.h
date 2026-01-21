#ifndef SITE_H
#define SITE_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>

namespace Anvil::Models
{
    class Site
    {
    public:
        Site();
        Site(const QString &name, const QString &path, const QString &domain);

        // Getters
        QString id() const { return m_id; }
        QString name() const { return m_name; }
        QString path() const { return m_path; }
        QString domain() const { return m_domain; }
        QString phpVersion() const { return m_phpVersion; }
        bool sslEnabled() const { return m_sslEnabled; }
        bool isSecured() const { return m_sslEnabled; }
        bool isParked() const { return m_isParked; }
        bool isLinked() const { return !m_isParked; }
        QDateTime createdAt() const { return m_createdAt; }
        QDateTime lastAccessed() const { return m_lastAccessed; }

        // Setters
        void setId(const QString &id) { m_id = id; }
        void setName(const QString &name) { m_name = name; }
        void setPath(const QString &path) { m_path = path; }
        void setDomain(const QString &domain) { m_domain = domain; }
        void setPhpVersion(const QString &version) { m_phpVersion = version; }
        void setSslEnabled(bool enabled) { m_sslEnabled = enabled; }
        void setIsParked(bool parked) { m_isParked = parked; }
        void setLastAccessed(const QDateTime &dateTime) { m_lastAccessed = dateTime; }

        // URLs
        QString url() const;
        QString secureUrl() const;

        // Validation
        bool isValid() const;
        QStringList validate() const;

        // Serialization
        QJsonObject toJson() const;
        static Site fromJson(const QJsonObject &json);

        // Comparison
        bool operator==(const Site &other) const;
        bool operator!=(const Site &other) const;

    private:
        QString m_id;
        QString m_name;
        QString m_path;
        QString m_domain;
        QString m_phpVersion;
        bool m_sslEnabled;
        bool m_isParked;
        QDateTime m_createdAt;
        QDateTime m_lastAccessed;

        void generateId();
    };
}
#endif
