#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QProcess>
#include "services/BaseService.h"
#include "utils/Process.h"

namespace Anvil
{
    namespace Managers
    {
        /**
         * @brief High-level process orchestration for Anvil
         *
         * Manages long-running service processes, monitors health,
         * handles automatic restarts, and provides process lifecycle
         * management for the entire application.
         */
        class ProcessManager : public QObject
        {
            Q_OBJECT

        public:
            explicit ProcessManager(QObject *parent = nullptr);
            ~ProcessManager();

            // Initialization
            bool initialize();
            bool isInitialized() const { return m_initialized; }

            // ============================================================
            // Service Process Management
            // ============================================================

            /**
             * @brief Start a service process (PHP-FPM, Nginx, etc.)
             * @param serviceName Service identifier (e.g., "php-fpm-8.3")
             * @param command Command to execute
             * @param args Arguments
             * @param workingDir Working directory
             * @return Process ID on success
             */
            Services::ServiceResult<qint64> startServiceProcess(
                const QString &serviceName,
                const QString &command,
                const QStringList &args = QStringList(),
                const QString &workingDir = QString());

            /**
             * @brief Stop a service process gracefully
             * @param serviceName Service identifier
             * @param timeoutMs Timeout in milliseconds
             */
            Services::ServiceResult<bool> stopServiceProcess(
                const QString &serviceName,
                int timeoutMs = 5000);

            /**
             * @brief Force kill a service process
             * @param serviceName Service identifier
             */
            Services::ServiceResult<bool> killServiceProcess(const QString &serviceName);

            /**
             * @brief Restart a service process
             * @param serviceName Service identifier
             */
            Services::ServiceResult<bool> restartServiceProcess(const QString &serviceName);

            /**
             * @brief Check if service is running
             */
            bool isServiceRunning(const QString &serviceName) const;

            /**
             * @brief Get process ID for service
             */
            qint64 getServicePid(const QString &serviceName) const;

            /**
             * @brief Get all managed service processes
             */
            QStringList getManagedServices() const;

            // ============================================================
            // Process Monitoring & Health
            // ============================================================

            /**
             * @brief Enable/disable auto-restart for a service
             * @param serviceName Service identifier
             * @param enabled Enable auto-restart
             * @param maxAttempts Maximum restart attempts
             */
            void setAutoRestart(const QString &serviceName,
                                bool enabled,
                                int maxAttempts = 3);

            /**
             * @brief Get uptime for a service in seconds
             */
            qint64 getServiceUptime(const QString &serviceName) const;

            /**
             * @brief Get restart count for a service
             */
            int getRestartCount(const QString &serviceName) const;

            /**
             * @brief Check if service is healthy (running without crashes)
             */
            bool isServiceHealthy(const QString &serviceName) const;

            /**
             * @brief Get last crash reason
             */
            QString getLastCrashReason(const QString &serviceName) const;

            // ============================================================
            // One-shot Process Execution
            // ============================================================

            /**
             * @brief Execute a command and wait for completion
             * @param command Command to execute
             * @param args Arguments
             * @param timeoutMs Timeout in milliseconds
             */
            Services::ServiceResult<QString> executeCommand(
                const QString &command,
                const QStringList &args = QStringList(),
                int timeoutMs = 30000);

            /**
             * @brief Execute command with root privileges
             */
            Services::ServiceResult<QString> executeAsRoot(
                const QString &command,
                const QStringList &args = QStringList());

            /**
             * @brief Execute shell command
             */
            Services::ServiceResult<QString> executeShell(
                const QString &shellCommand,
                int timeoutMs = 30000);

            // ============================================================
            // Background Tasks
            // ============================================================

            /**
             * @brief Start a background task (non-service process)
             * @param taskName Task identifier
             * @param command Command to execute
             * @param args Arguments
             * @return Task ID
             */
            Services::ServiceResult<qint64> startBackgroundTask(
                const QString &taskName,
                const QString &command,
                const QStringList &args = QStringList());

            /**
             * @brief Stop a background task
             */
            Services::ServiceResult<bool> stopBackgroundTask(const QString &taskName);

            /**
             * @brief Check if background task is running
             */
            bool isBackgroundTaskRunning(const QString &taskName) const;

            /**
             * @brief Get output from background task
             */
            QString getBackgroundTaskOutput(const QString &taskName) const;

            // ============================================================
            // Process Cleanup & Shutdown
            // ============================================================

            /**
             * @brief Stop all managed processes
             */
            void stopAll();

            /**
             * @brief Graceful shutdown of all services
             */
            void shutdown();

            /**
             * @brief Clean up zombie/dead processes
             */
            void cleanupDeadProcesses();

        signals:
            // Service process signals
            void serviceStarted(const QString &serviceName, qint64 pid);
            void serviceStopped(const QString &serviceName);
            void serviceCrashed(const QString &serviceName, const QString &reason);
            void serviceRestarting(const QString &serviceName, int attempt);

            // Background task signals
            void taskStarted(const QString &taskName, qint64 pid);
            void taskFinished(const QString &taskName, int exitCode);
            void taskOutput(const QString &taskName, const QString &output);

            // General signals
            void processError(const QString &processName, const QString &error);

        private slots:
            void onServiceStarted();
            void onServiceStopped();
            void onServiceCrashed(const QString &reason);
            void onServiceRestarting(int attempt);

            void onTaskFinished(qint64 pid, int exitCode);
            void onTaskOutput(qint64 pid, const QString &output);

        private:
            struct ServiceProcessInfo
            {
                Utils::ServiceProcess *process = nullptr;
                QString command;
                QStringList args;
                QString workingDir;
                QDateTime startTime;
                int restartCount = 0;
                bool autoRestart = false;
                int maxRestartAttempts = 3;
                QString lastCrashReason;
            };

            struct BackgroundTaskInfo
            {
                qint64 pid = 0;
                QString outputBuffer;
                QString errorBuffer;
            };

            // Service management
            QMap<QString, ServiceProcessInfo> m_services;

            // Background task management
            QMap<QString, BackgroundTaskInfo> m_backgroundTasks;
            Utils::ProcessManager *m_processManager;

            // Process execution
            Utils::ProcessExecutor *m_executor;

            bool m_initialized;

            // Helper methods
            void setupServiceProcess(const QString &serviceName,
                                     Utils::ServiceProcess *process);
            void cleanupService(const QString &serviceName);
            QString getLogPath(const QString &serviceName) const;

            bool isInitialized() const { return m_initialized; }
        };
    }
}

#endif
