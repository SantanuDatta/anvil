#include "models/Service.h"
namespace Anvil
{
    namespace Models
    {
        Service::Service()
            : m_status(ServiceStatus::Unknown), m_pid(0), m_port(0)
        {
        }
        Service::Service(const QString &name)
            : m_name(name), m_status(ServiceStatus::Stopped), m_pid(0), m_port(0)
        {
        }
        QString Service::statusString() const
        {
            return statusToString(m_status);
        }
        QString Service::statusToString(ServiceStatus status)
        {
            switch (status)
            {
            case ServiceStatus::Stopped:
                return "Stopped";
            case ServiceStatus::Starting:
                return "Starting";
            case ServiceStatus::Running:
                return "Running";
            case ServiceStatus::Stopping:
                return "Stopping";
            case ServiceStatus::Error:
                return "Error";
            case ServiceStatus::Unknown:
                return "Unknown";
            default:
                return "Unknown";
            }
        }
        ServiceStatus Service::stringToStatus(const QString &status)
        {
            if (status == "Stopped")
                return ServiceStatus::Stopped;
            if (status == "Starting")
                return ServiceStatus::Starting;
            if (status == "Running")
                return ServiceStatus::Running;
            if (status == "Stopping")
                return ServiceStatus::Stopping;
            if (status == "Error")
                return ServiceStatus::Error;
            return ServiceStatus::Unknown;
        }
        qint64 Service::uptimeSeconds() const
        {
            if (!isRunning() || !m_startedAt.isValid())
            {
                return 0;
            }
            return m_startedAt.secsTo(QDateTime::currentDateTime());
        }
        QString Service::uptimeString() const
        {
            qint64 seconds = uptimeSeconds();
            if (seconds < 60)
            {
                return QString("%1s").arg(seconds);
            }
            qint64 minutes = seconds / 60;
            if (minutes < 60)
            {
                return QString("%1m %2s").arg(minutes).arg(seconds % 60);
            }
            qint64 hours = minutes / 60;
            if (hours < 24)
            {
                return QString("%1h %2m").arg(hours).arg(minutes % 60);
            }
            qint64 days = hours / 24;
            return QString("%1d %2h").arg(days).arg(hours % 24);
        }
        QJsonObject Service::toJson() const
        {
            QJsonObject json;
            json["name"] = m_name;
            json["status"] = statusToString(m_status);
            json["pid"] = m_pid;
            json["errorMessage"] = m_errorMessage;
            json["startedAt"] = m_startedAt.toString(Qt::ISODate);
            json["port"] = m_port;
            json["version"] = m_version;
            return json;
        }
        Service Service::fromJson(const QJsonObject &json)
        {
            Service service;
            service.m_name = json["name"].toString();
            service.m_status = stringToStatus(json["status"].toString());
            service.m_pid = json["pid"].toInteger(0);
            service.m_errorMessage = json["errorMessage"].toString();
            QString startedAtStr = json["startedAt"].toString();
            if (!startedAtStr.isEmpty())
            {
                service.m_startedAt = QDateTime::fromString(startedAtStr, Qt::ISODate);
            }
            service.m_port = json["port"].toInt(0);
            service.m_version = json["version"].toString();
            return service;
        }
    }
}
