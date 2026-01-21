#ifndef SERVICE_H
#define SERVICE_H
#include <QString>
#include <QDateTime>
#include <QJsonObject>

namespace Anvil::Models
{
    enum class ServiceStatus
    {
        Stopped,
        Starting,
        Running,
        Stopping,
        Error,
        Unknown
    };
    class Service
    {
    public:
        Service();
        explicit Service(const QString &name);
        // Getters
        QString name() const { return m_name; }
        ServiceStatus status() const { return m_status; }
        qint64 pid() const { return m_pid; }
        QString errorMessage() const { return m_errorMessage; }
        QDateTime startedAt() const { return m_startedAt; }
        int port() const { return m_port; }
        QString version() const { return m_version; }
        // Setters
        void setName(const QString &name) { m_name = name; }
        void setStatus(ServiceStatus status) { m_status = status; }
        void setPid(qint64 pid) { m_pid = pid; }
        void setErrorMessage(const QString &error) { m_errorMessage = error; }
        void setStartedAt(const QDateTime &dateTime) { m_startedAt = dateTime; }
        void setPort(int port) { m_port = port; }
        void setVersion(const QString &version) { m_version = version; }
        // Status checks
        bool isRunning() const { return m_status == ServiceStatus::Running; }
        bool isStopped() const { return m_status == ServiceStatus::Stopped; }
        bool isError() const { return m_status == ServiceStatus::Error; }
        bool hasError() const { return !m_errorMessage.isEmpty(); }
        // Status string
        QString statusString() const;
        static QString statusToString(ServiceStatus status);
        static ServiceStatus stringToStatus(const QString &status);
        // Uptime
        qint64 uptimeSeconds() const;
        QString uptimeString() const;
        // Serialization
        QJsonObject toJson() const;
        static Service fromJson(const QJsonObject &json);

    private:
        QString m_name;
        ServiceStatus m_status;
        qint64 m_pid;
        QString m_errorMessage;
        QDateTime m_startedAt;
        int m_port;
        QString m_version;
    };
}
#endif
