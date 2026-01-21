#ifndef BASESERVICE_H
#define BASESERVICE_H

#include <QObject>
#include <QString>
#include <QProcess>
#include "models/Service.h"
#include "utils/Process.h"

namespace Anvil::Services
{
    template <typename T>
    struct ServiceResult
    {
        bool success;
        T data;
        QString error;

        static ServiceResult<T> Ok(const T &data)
        {
            return {true, data, QString()};
        }

        static ServiceResult<T> Err(const QString &error)
        {
            return {false, T(), error};
        }

        bool isSuccess() const { return success; }
        bool isError() const { return !success; }
    };

    // Base class for all services
    class BaseService : public QObject
    {
        Q_OBJECT

    public:
        explicit BaseService(const QString &name, QObject *parent = nullptr);
        virtual ~BaseService();

        // Service identification
        QString name() const { return m_name; }
        QString displayName() const { return m_displayName; }
        QString description() const { return m_description; }

        // Service lifecycle
        virtual ServiceResult<bool> start() = 0;
        virtual ServiceResult<bool> stop() = 0;
        virtual ServiceResult<bool> restart();
        virtual ServiceResult<bool> reload();

        // Service status
        virtual bool isInstalled() const = 0;
        virtual bool isRunning() const = 0;
        virtual Models::Service status() const;

        // Installation
        virtual ServiceResult<bool> install() = 0;
        virtual ServiceResult<bool> uninstall() = 0;

        // Configuration
        virtual ServiceResult<bool> configure() = 0;
        virtual QString configPath() const = 0;
        virtual ServiceResult<QString> getConfig() const;
        virtual ServiceResult<bool> setConfig(const QString &content);

        // Version info
        virtual QString version() const;
        virtual ServiceResult<QString> detectVersion();

        // Enable/disable
        void setEnabled(bool enabled) { m_enabled = enabled; }
        bool isEnabled() const { return m_enabled; }

        // Auto-start
        void setAutoStart(bool autoStart) { m_autoStart = autoStart; }
        bool autoStart() const { return m_autoStart; }

    signals:
        void started();
        void stopped();
        void restarted();
        void statusChanged(const Models::Service &status);
        void errorOccurred(const QString &error);
        void outputReceived(const QString &output);

    protected:
        // Helper methods for derived classes
        ServiceResult<bool> executeCommand(const QString &command,
                                           const QStringList &args = QStringList()) const;
        ServiceResult<bool> executeAsRoot(const QString &command,
                                          const QStringList &args = QStringList()) const;
        ServiceResult<QString> executeAndCapture(const QString &command,
                                                 const QStringList &args = QStringList()) const;

        bool checkProgramExists(const QString &program) const;
        QString getProgramPath(const QString &program) const;

        // Process management
        Utils::ServiceProcess *process() { return m_process; }
        const Utils::ServiceProcess *process() const { return m_process; }

        void setDisplayName(const QString &name) { m_displayName = name; }
        void setDescription(const QString &desc) { m_description = desc; }
        void setVersion(const QString &version) { m_version = version; }

        void updateStatus(Models::ServiceStatus status);
        void setError(const QString &error);
        void clearError();

    private:
        QString m_name;
        QString m_displayName;
        QString m_description;
        QString m_version;
        bool m_enabled;
        bool m_autoStart;

        Models::Service m_status;
        Utils::ServiceProcess *m_process;
        Utils::ProcessExecutor *m_executor;
    };
}
#endif
