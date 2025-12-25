#include "managers/ProcessManager.h"
#include "core/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"

#include <QDateTime>
#include <QThread>

namespace Anvil
{
    namespace Managers
    {
        ProcessManager::ProcessManager(QObject *parent)
            : QObject(parent), m_processManager(new Utils::ProcessManager(this)), m_executor(new Utils::ProcessExecutor(this)), m_initialized(false)
        {
        }

        ProcessManager::~ProcessManager()
        {
            if (m_initialized)
            {
                shutdown();
            }
        }

        bool ProcessManager::initialize()
        {
            if (m_initialized)
            {
                LOG_WARNING("ProcessManager already initialized");
                return true;
            }

            LOG_INFO("Initializing ProcessManager");

            // Connect process manager signals
            connect(m_processManager, &Utils::ProcessManager::processStarted,
                    this, [this](qint64 pid)
                    {
                        // Find which background task this belongs to
                        for (auto it = m_backgroundTasks.begin();
                             it != m_backgroundTasks.end(); ++it)
                        {
                            if (it->pid == pid)
                            {
                                emit taskStarted(it.key(), pid);
                                break;
                            }
                        } });

            connect(m_processManager, &Utils::ProcessManager::processFinished,
                    this, &ProcessManager::onTaskFinished);

            connect(m_processManager, &Utils::ProcessManager::processOutput,
                    this, &ProcessManager::onTaskOutput);

            m_initialized = true;
            LOG_INFO("ProcessManager initialized successfully");

            return true;
        }

        // ============================================================
        // Service Process Management
        // ============================================================

        Services::ServiceResult<qint64> ProcessManager::startServiceProcess(
            const QString &serviceName,
            const QString &command,
            const QStringList &args,
            const QString &workingDir)
        {
            LOG_INFO(QString("Starting service process: %1").arg(serviceName));

            if (m_services.contains(serviceName))
            {
                if (m_services[serviceName].process->isRunning())
                {
                    return Services::ServiceResult<qint64>::Err(
                        QString("Service %1 is already running").arg(serviceName));
                }

                // Clean up old process
                cleanupService(serviceName);
            }

            // Create new service process
            Utils::ServiceProcess *process = new Utils::ServiceProcess(serviceName, this);

            // Setup logging
            QString logPath = getLogPath(serviceName);
            process->setLogFile(logPath);

            // Setup signal connections
            setupServiceProcess(serviceName, process);

            // Start the process
            bool started = process->start(command, args, workingDir);

            if (!started)
            {
                delete process;
                return Services::ServiceResult<qint64>::Err(
                    QString("Failed to start service %1").arg(serviceName));
            }

            // Store service info
            ServiceProcessInfo info;
            info.process = process;
            info.command = command;
            info.args = args;
            info.workingDir = workingDir;
            info.startTime = QDateTime::currentDateTime();
            info.restartCount = 0;

            m_services[serviceName] = info;

            qint64 pid = process->pid();
            emit serviceStarted(serviceName, pid);

            LOG_INFO(QString("Service %1 started (PID: %2)").arg(serviceName).arg(pid));

            return Services::ServiceResult<qint64>::Ok(pid);
        }

        Services::ServiceResult<bool> ProcessManager::stopServiceProcess(
            const QString &serviceName,
            int timeoutMs)
        {
            LOG_INFO(QString("Stopping service: %1").arg(serviceName));

            if (!m_services.contains(serviceName))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Service %1 not found").arg(serviceName));
            }

            ServiceProcessInfo &info = m_services[serviceName];

            if (!info.process->isRunning())
            {
                cleanupService(serviceName);
                return Services::ServiceResult<bool>::Ok(true);
            }

            bool stopped = info.process->stop(timeoutMs);

            if (stopped)
            {
                emit serviceStopped(serviceName);
                cleanupService(serviceName);
                LOG_INFO(QString("Service %1 stopped").arg(serviceName));
            }
            else
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Failed to stop service %1").arg(serviceName));
            }

            return Services::ServiceResult<bool>::Ok(true);
        }

        Services::ServiceResult<bool> ProcessManager::killServiceProcess(
            const QString &serviceName)
        {
            LOG_WARNING(QString("Force killing service: %1").arg(serviceName));

            if (!m_services.contains(serviceName))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Service %1 not found").arg(serviceName));
            }

            ServiceProcessInfo &info = m_services[serviceName];

            if (info.process->isRunning())
            {
                qint64 pid = info.process->pid();
                m_executor->killProcess(pid, true);
            }

            emit serviceStopped(serviceName);
            cleanupService(serviceName);

            return Services::ServiceResult<bool>::Ok(true);
        }

        Services::ServiceResult<bool> ProcessManager::restartServiceProcess(
            const QString &serviceName)
        {
            LOG_INFO(QString("Restarting service: %1").arg(serviceName));

            if (!m_services.contains(serviceName))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Service %1 not found").arg(serviceName));
            }

            ServiceProcessInfo info = m_services[serviceName];

            // Stop current process
            auto stopResult = stopServiceProcess(serviceName);
            if (stopResult.isError())
            {
                return stopResult;
            }

            // Brief pause
            QThread::msleep(500);

            // Start new process
            auto startResult = startServiceProcess(
                serviceName,
                info.command,
                info.args,
                info.workingDir);

            if (startResult.isSuccess())
            {
                // Restore auto-restart setting
                if (info.autoRestart)
                {
                    setAutoRestart(serviceName, true, info.maxRestartAttempts);
                }
            }

            return Services::ServiceResult<bool>::Ok(startResult.isSuccess());
        }

        bool ProcessManager::isServiceRunning(const QString &serviceName) const
        {
            if (!m_services.contains(serviceName))
            {
                return false;
            }

            return m_services[serviceName].process->isRunning();
        }

        qint64 ProcessManager::getServicePid(const QString &serviceName) const
        {
            if (!m_services.contains(serviceName))
            {
                return 0;
            }

            return m_services[serviceName].process->pid();
        }

        QStringList ProcessManager::getManagedServices() const
        {
            return m_services.keys();
        }

        // ============================================================
        // Process Monitoring & Health
        // ============================================================

        void ProcessManager::setAutoRestart(const QString &serviceName,
                                            bool enabled,
                                            int maxAttempts)
        {
            if (!m_services.contains(serviceName))
            {
                LOG_WARNING(QString("Cannot set auto-restart for unknown service: %1")
                                .arg(serviceName));
                return;
            }

            ServiceProcessInfo &info = m_services[serviceName];
            info.autoRestart = enabled;
            info.maxRestartAttempts = maxAttempts;
            info.process->setAutoRestart(enabled, maxAttempts);

            LOG_INFO(QString("Auto-restart %1 for service %2 (max attempts: %3)")
                         .arg(enabled ? "enabled" : "disabled")
                         .arg(serviceName)
                         .arg(maxAttempts));
        }

        qint64 ProcessManager::getServiceUptime(const QString &serviceName) const
        {
            if (!m_services.contains(serviceName))
            {
                return 0;
            }

            const ServiceProcessInfo &info = m_services[serviceName];

            if (!info.process->isRunning())
            {
                return 0;
            }

            return info.startTime.secsTo(QDateTime::currentDateTime());
        }

        int ProcessManager::getRestartCount(const QString &serviceName) const
        {
            if (!m_services.contains(serviceName))
            {
                return 0;
            }

            return m_services[serviceName].restartCount;
        }

        bool ProcessManager::isServiceHealthy(const QString &serviceName) const
        {
            if (!m_services.contains(serviceName))
            {
                return false;
            }

            const ServiceProcessInfo &info = m_services[serviceName];

            // Service is healthy if:
            // 1. It's running
            // 2. Restart count is below max attempts
            // 3. No recent crashes (last crash reason is empty)

            return info.process->isRunning() &&
                   info.restartCount < info.maxRestartAttempts &&
                   info.lastCrashReason.isEmpty();
        }

        QString ProcessManager::getLastCrashReason(const QString &serviceName) const
        {
            if (!m_services.contains(serviceName))
            {
                return QString();
            }

            return m_services[serviceName].lastCrashReason;
        }

        // ============================================================
        // One-shot Process Execution
        // ============================================================

        Services::ServiceResult<QString> ProcessManager::executeCommand(
            const QString &command,
            const QStringList &args,
            int timeoutMs)
        {
            LOG_DEBUG(QString("Executing: %1 %2").arg(command, args.join(" ")));

            Utils::ProcessResult result = m_executor->execute(command, args, QString(), timeoutMs);

            if (result.isSuccess())
            {
                return Services::ServiceResult<QString>::Ok(result.output);
            }

            return Services::ServiceResult<QString>::Err(result.error);
        }

        Services::ServiceResult<QString> ProcessManager::executeAsRoot(
            const QString &command,
            const QStringList &args)
        {
            LOG_INFO(QString("Executing as root: %1 %2").arg(command, args.join(" ")));

            Utils::ProcessResult result = m_executor->executeAsRoot(command, args);

            if (result.isSuccess())
            {
                return Services::ServiceResult<QString>::Ok(result.output);
            }

            return Services::ServiceResult<QString>::Err(result.error);
        }

        Services::ServiceResult<QString> ProcessManager::executeShell(
            const QString &shellCommand,
            int timeoutMs)
        {
            LOG_DEBUG(QString("Executing shell: %1").arg(shellCommand));

            Utils::ProcessResult result = m_executor->executeShell(shellCommand, QString());

            if (result.isSuccess())
            {
                return Services::ServiceResult<QString>::Ok(result.output);
            }

            return Services::ServiceResult<QString>::Err(result.error);
        }

        // ============================================================
        // Background Tasks
        // ============================================================

        Services::ServiceResult<qint64> ProcessManager::startBackgroundTask(
            const QString &taskName,
            const QString &command,
            const QStringList &args)
        {
            LOG_INFO(QString("Starting background task: %1").arg(taskName));

            if (m_backgroundTasks.contains(taskName))
            {
                if (m_processManager->isRunning(m_backgroundTasks[taskName].pid))
                {
                    return Services::ServiceResult<qint64>::Err(
                        QString("Task %1 is already running").arg(taskName));
                }
            }

            qint64 pid = m_processManager->startProcess(command, args);

            if (pid == -1)
            {
                return Services::ServiceResult<qint64>::Err(
                    QString("Failed to start task %1").arg(taskName));
            }

            BackgroundTaskInfo info;
            info.pid = pid;
            m_backgroundTasks[taskName] = info;

            emit taskStarted(taskName, pid);
            LOG_INFO(QString("Background task %1 started (PID: %2)").arg(taskName).arg(pid));

            return Services::ServiceResult<qint64>::Ok(pid);
        }

        Services::ServiceResult<bool> ProcessManager::stopBackgroundTask(
            const QString &taskName)
        {
            LOG_INFO(QString("Stopping background task: %1").arg(taskName));

            if (!m_backgroundTasks.contains(taskName))
            {
                return Services::ServiceResult<bool>::Err(
                    QString("Task %1 not found").arg(taskName));
            }

            qint64 pid = m_backgroundTasks[taskName].pid;
            bool stopped = m_processManager->stopProcess(pid);

            if (stopped)
            {
                m_backgroundTasks.remove(taskName);
                LOG_INFO(QString("Background task %1 stopped").arg(taskName));
            }

            return Services::ServiceResult<bool>::Ok(stopped);
        }

        bool ProcessManager::isBackgroundTaskRunning(const QString &taskName) const
        {
            if (!m_backgroundTasks.contains(taskName))
            {
                return false;
            }

            qint64 pid = m_backgroundTasks[taskName].pid;
            return m_processManager->isRunning(pid);
        }

        QString ProcessManager::getBackgroundTaskOutput(const QString &taskName) const
        {
            if (!m_backgroundTasks.contains(taskName))
            {
                return QString();
            }

            qint64 pid = m_backgroundTasks[taskName].pid;
            return m_processManager->getOutput(pid);
        }

        // ============================================================
        // Process Cleanup & Shutdown
        // ============================================================

        void ProcessManager::stopAll()
        {
            LOG_INFO("Stopping all managed processes");

            // Stop all services
            QStringList services = m_services.keys();
            for (const QString &serviceName : services)
            {
                stopServiceProcess(serviceName);
            }

            // Stop all background tasks
            QStringList tasks = m_backgroundTasks.keys();
            for (const QString &taskName : tasks)
            {
                stopBackgroundTask(taskName);
            }
        }

        void ProcessManager::shutdown()
        {
            LOG_INFO("ProcessManager shutdown");

            stopAll();

            // Wait a bit for graceful shutdown
            QThread::msleep(500);

            // Clean up any remaining processes
            cleanupDeadProcesses();
        }

        void ProcessManager::cleanupDeadProcesses()
        {
            LOG_DEBUG("Cleaning up dead processes");

            // Clean up dead services
            QStringList servicesToRemove;
            for (auto it = m_services.begin(); it != m_services.end(); ++it)
            {
                if (!it->process->isRunning())
                {
                    servicesToRemove.append(it.key());
                }
            }

            for (const QString &serviceName : servicesToRemove)
            {
                cleanupService(serviceName);
            }

            // Clean up dead background tasks
            QStringList tasksToRemove;
            for (auto it = m_backgroundTasks.begin(); it != m_backgroundTasks.end(); ++it)
            {
                if (!m_processManager->isRunning(it->pid))
                {
                    tasksToRemove.append(it.key());
                }
            }

            for (const QString &taskName : tasksToRemove)
            {
                m_backgroundTasks.remove(taskName);
            }

            if (!servicesToRemove.isEmpty() || !tasksToRemove.isEmpty())
            {
                LOG_INFO(QString("Cleaned up %1 services and %2 tasks")
                             .arg(servicesToRemove.size())
                             .arg(tasksToRemove.size()));
            }
        }

        // ============================================================
        // Private Slots
        // ============================================================

        void ProcessManager::onServiceStarted()
        {
            Utils::ServiceProcess *process = qobject_cast<Utils::ServiceProcess *>(sender());
            if (!process)
                return;

            QString serviceName = process->name();
            LOG_DEBUG(QString("Service started signal received: %1").arg(serviceName));
        }

        void ProcessManager::onServiceStopped()
        {
            Utils::ServiceProcess *process = qobject_cast<Utils::ServiceProcess *>(sender());
            if (!process)
                return;

            QString serviceName = process->name();
            LOG_DEBUG(QString("Service stopped signal received: %1").arg(serviceName));
        }

        void ProcessManager::onServiceCrashed(const QString &reason)
        {
            Utils::ServiceProcess *process = qobject_cast<Utils::ServiceProcess *>(sender());
            if (!process)
                return;

            QString serviceName = process->name();

            if (m_services.contains(serviceName))
            {
                ServiceProcessInfo &info = m_services[serviceName];
                info.lastCrashReason = reason;
                info.restartCount++;
            }

            emit serviceCrashed(serviceName, reason);
            LOG_ERROR(QString("Service crashed: %1 - %2").arg(serviceName, reason));
        }

        void ProcessManager::onServiceRestarting(int attempt)
        {
            Utils::ServiceProcess *process = qobject_cast<Utils::ServiceProcess *>(sender());
            if (!process)
                return;

            QString serviceName = process->name();
            emit serviceRestarting(serviceName, attempt);
            LOG_INFO(QString("Service restarting: %1 (attempt %2)").arg(serviceName).arg(attempt));
        }

        void ProcessManager::onTaskFinished(qint64 pid, int exitCode)
        {
            // Find which task this belongs to
            for (auto it = m_backgroundTasks.begin(); it != m_backgroundTasks.end(); ++it)
            {
                if (it->pid == pid)
                {
                    emit taskFinished(it.key(), exitCode);
                    LOG_INFO(QString("Background task %1 finished (exit: %2)")
                                 .arg(it.key())
                                 .arg(exitCode));

                    // Don't remove yet - allow output retrieval
                    break;
                }
            }
        }

        void ProcessManager::onTaskOutput(qint64 pid, const QString &output)
        {
            // Store output in buffer
            for (auto it = m_backgroundTasks.begin(); it != m_backgroundTasks.end(); ++it)
            {
                if (it->pid == pid)
                {
                    it->outputBuffer += output;
                    emit taskOutput(it.key(), output);
                    break;
                }
            }
        }

        // ============================================================
        // Private Helper Methods
        // ============================================================

        void ProcessManager::setupServiceProcess(const QString &serviceName,
                                                 Utils::ServiceProcess *process)
        {
            connect(process, &Utils::ServiceProcess::started,
                    this, &ProcessManager::onServiceStarted);

            connect(process, &Utils::ServiceProcess::stopped,
                    this, &ProcessManager::onServiceStopped);

            connect(process, &Utils::ServiceProcess::crashed,
                    this, &ProcessManager::onServiceCrashed);

            connect(process, &Utils::ServiceProcess::restarting,
                    this, &ProcessManager::onServiceRestarting);
        }

        void ProcessManager::cleanupService(const QString &serviceName)
        {
            if (!m_services.contains(serviceName))
            {
                return;
            }

            ServiceProcessInfo &info = m_services[serviceName];

            if (info.process)
            {
                info.process->deleteLater();
                info.process = nullptr;
            }

            m_services.remove(serviceName);
            LOG_DEBUG(QString("Service cleaned up: %1").arg(serviceName));
        }

        QString ProcessManager::getLogPath(const QString &serviceName) const
        {
            Core::ConfigManager &config = Core::ConfigManager::instance();
            QString logsPath = config.logsPath();

            return Utils::FileSystem::joinPath(logsPath,
                                               QString("%1.log").arg(serviceName));
        }
    }
}
