#include "services/BaseService.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"
#include <QThread>
#include <QDateTime>

namespace Anvil
{
    namespace Services
    {
        BaseService::BaseService(const QString &name, QObject *parent)
            : QObject(parent), m_name(name), m_displayName(name), m_enabled(true), m_autoStart(false), m_status(name), m_process(new Utils::ServiceProcess(name, this)), m_executor(new Utils::ProcessExecutor(this))
        {

            m_status.setStatus(Models::ServiceStatus::Stopped);

            // Connect process signals
            connect(m_process, &Utils::ServiceProcess::started, this, [this]()
                    {
                updateStatus(Models::ServiceStatus::Running);
                emit started(); });

            connect(m_process, &Utils::ServiceProcess::stopped, this, [this]()
                    {
                updateStatus(Models::ServiceStatus::Stopped);
                emit stopped(); });

            connect(m_process, &Utils::ServiceProcess::crashed, this, [this](const QString &reason)
                    {
                setError(reason);
                updateStatus(Models::ServiceStatus::Error); });
        }

        BaseService::~BaseService()
        {
            if (m_process && m_process->isRunning())
            {
                m_process->stop();
            }
        }

        ServiceResult<bool> BaseService::restart()
        {
            LOG_INFO(QString("Restarting service: %1").arg(m_name));

            auto stopResult = stop();
            if (stopResult.isError())
            {
                return stopResult;
            }

            // Brief pause between stop and start
            QThread::msleep(500);

            auto startResult = start();
            if (startResult.isSuccess())
            {
                emit restarted();
            }

            return startResult;
        }

        ServiceResult<bool> BaseService::reload()
        {
            LOG_INFO(QString("Reloading service: %1").arg(m_name));

            // Default implementation: restart
            // Services can override for graceful reload
            return restart();
        }

        Models::Service BaseService::status() const
        {
            return m_status;
        }

        ServiceResult<QString> BaseService::getConfig() const
        {
            QString path = configPath();
            if (path.isEmpty())
            {
                return ServiceResult<QString>::Err("Config path not set");
            }

            auto result = Utils::FileSystem::readFile(path);
            if (result.isError())
            {
                return ServiceResult<QString>::Err(result.error);
            }

            return ServiceResult<QString>::Ok(result.data);
        }

        ServiceResult<bool> BaseService::setConfig(const QString &content)
        {
            QString path = configPath();
            if (path.isEmpty())
            {
                return ServiceResult<bool>::Err("Config path not set");
            }

            // Backup current config
            if (Utils::FileSystem::fileExists(path))
            {
                Utils::FileSystem::createBackup(path);
            }

            auto result = Utils::FileSystem::writeFile(path, content);
            if (result.isError())
            {
                return ServiceResult<bool>::Err(result.error);
            }

            return ServiceResult<bool>::Ok(true);
        }

        QString BaseService::version() const
        {
            return m_version;
        }

        ServiceResult<QString> BaseService::detectVersion()
        {
            // Default implementation - services should override
            return ServiceResult<QString>::Err("Version detection not implemented");
        }

        ServiceResult<bool> BaseService::executeCommand(const QString &command,
                                                        const QStringList &args) const
        {
            Utils::ProcessResult result = m_executor->execute(command, args);

            if (result.isSuccess())
            {
                return ServiceResult<bool>::Ok(true);
            }

            // Provide better error message
            QString errorMsg = result.error.isEmpty()
                                   ? QString("Command failed with exit code %1").arg(result.exitCode)
                                   : result.error;

            return ServiceResult<bool>::Err(errorMsg);
        }

        ServiceResult<bool> BaseService::executeAsRoot(const QString &command,
                                                       const QStringList &args) const
        {
            static const QSet<QString> allowedCommands = {
                "apt", "dnf", "yum", "pacman",
                "systemctl", "mv", "cp", "chown"};

            if (!allowedCommands.contains(command))
            {
                return ServiceResult<bool>::Err(
                    QString("Command not allowed: %1").arg(command));
            }

            // Sanitize arguments (prevent injection)
            for (const QString &arg : args)
            {
                if (arg.contains(';') || arg.contains('|') || arg.contains('&'))
                {
                    return ServiceResult<bool>::Err("Invalid characters in arguments");
                }
            }

            Utils::ProcessResult result = m_executor->executeAsRoot(command, args);

            if (result.isSuccess())
            {
                return ServiceResult<bool>::Ok(true);
            }

            // Provide better error message
            QString errorMsg = result.error.isEmpty()
                                   ? QString("Command failed with exit code %1").arg(result.exitCode)
                                   : result.error;

            return ServiceResult<bool>::Err(errorMsg);
        }

        ServiceResult<QString> BaseService::executeAndCapture(const QString &command,
                                                              const QStringList &args) const
        {
            Utils::ProcessResult result = m_executor->execute(command, args);

            if (result.isSuccess())
            {
                return ServiceResult<QString>::Ok(result.output);
            }

            // Provide better error message with output
            QString errorMsg;
            if (!result.error.isEmpty())
            {
                errorMsg = result.error;
            }
            else if (!result.output.isEmpty())
            {
                errorMsg = result.output;
            }
            else
            {
                errorMsg = QString("Command failed with exit code %1").arg(result.exitCode);
            }

            return ServiceResult<QString>::Err(errorMsg);
        }

        bool BaseService::checkProgramExists(const QString &program) const
        {
            return m_executor->programExists(program);
        }

        QString BaseService::getProgramPath(const QString &program) const
        {
            return m_executor->programPath(program);
        }

        void BaseService::updateStatus(Models::ServiceStatus status)
        {
            m_status.setStatus(status);

            if (status == Models::ServiceStatus::Running)
            {
                m_status.setStartedAt(QDateTime::currentDateTime());
                if (m_process && m_process->isRunning())
                {
                    m_status.setPid(m_process->pid());
                }
                clearError();
            }
            else if (status == Models::ServiceStatus::Stopped)
            {
                m_status.setPid(0);
            }

            m_status.setVersion(m_version);

            LOG_DEBUG(QString("Service %1 status: %2").arg(m_name, m_status.statusString()));
            emit statusChanged(m_status);
        }

        void BaseService::setError(const QString &error)
        {
            m_status.setErrorMessage(error);
            LOG_ERROR(QString("Service %1 error: %2").arg(m_name, error));
            emit errorOccurred(error);
        }

        void BaseService::clearError()
        {
            m_status.setErrorMessage(QString());
        }
    }
}
