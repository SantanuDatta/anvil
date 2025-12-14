#ifndef PROCESS_H
#define PROCESS_H

#include <QString>
#include <QStringList>
#include <QProcess>
#include <QObject>
#include <QMap>
#include <QList>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QThread>
#include <memory>
#include <functional>

namespace Anvil
{
    namespace Utils
    {
        struct ProcessResult
        {
            bool success;
            int exitCode;
            QString output;
            QString error;

            static ProcessResult Ok(const QString &output, int exitCode = 0)
            {
                return {true, exitCode, output, QString()};
            }

            static ProcessResult Err(const QString &error, int exitCode = -1)
            {
                return {false, exitCode, QString(), error};
            }

            bool isSuccess() const { return success; }
            bool isError() const { return !success; }
        };

        class ProcessExecutor : public QObject
        {
            Q_OBJECT

        public:
            explicit ProcessExecutor(QObject *parent = nullptr);
            ~ProcessExecutor();

            // Synchronous execution
            ProcessResult execute(const QString &program,
                                  const QStringList &arguments = QStringList(),
                                  const QString &workingDir = QString(),
                                  int timeoutMs = 30000);

            // Execute with root privileges
            ProcessResult executeAsRoot(const QString &program,
                                        const QStringList &arguments = QStringList(),
                                        const QString &workingDir = QString());

            // Execute shell command
            ProcessResult executeShell(const QString &command,
                                       const QString &workingDir = QString());

            // Check if program exists
            bool programExists(const QString &program);

            // Get program path
            QString programPath(const QString &program);

            // Kill process by PID
            bool killProcess(qint64 pid, bool force = false);

            // Check if process is running
            bool isProcessRunning(qint64 pid);

            // Set environment variables
            void setEnvironment(const QStringList &env);
            void addEnvironmentVariable(const QString &name, const QString &value);

        signals:
            void outputReceived(const QString &output);
            void errorReceived(const QString &error);
            void finished(int exitCode);

        private:
            QProcess *m_process;
            QStringList m_environment;

            void setupProcess(const QString &workingDir);
            QString escapeShellArgument(const QString &arg) const;
        };

        // Asynchronous process manager for long-running processes
        class ProcessManager : public QObject
        {
            Q_OBJECT

        public:
            explicit ProcessManager(QObject *parent = nullptr);
            ~ProcessManager();

            // Start a managed process
            qint64 startProcess(const QString &program,
                                const QStringList &arguments = QStringList(),
                                const QString &workingDir = QString());

            // Stop a managed process
            bool stopProcess(qint64 pid, int timeoutMs = 5000);

            // Force kill a process
            bool killProcess(qint64 pid);

            // Check if process is running
            bool isRunning(qint64 pid) const;

            // Get process output (buffered)
            QString getOutput(qint64 pid) const;
            QString getError(qint64 pid) const;

            // Clear output buffers
            void clearBuffers(qint64 pid);

            // Get all managed PIDs
            QList<qint64> getManagedProcesses() const;

        signals:
            void processStarted(qint64 pid);
            void processFinished(qint64 pid, int exitCode);
            void processError(qint64 pid, const QString &error);
            void processOutput(qint64 pid, const QString &output);

        private slots:
            void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
            void onProcessError(QProcess::ProcessError error);
            void onReadyReadStandardOutput();
            void onReadyReadStandardError();

        private:
            struct ManagedProcess
            {
                QProcess *process;
                QString outputBuffer;
                QString errorBuffer;
            };

            QMap<qint64, ManagedProcess> m_processes;

            void cleanupProcess(qint64 pid);
        };

        // Helper class for running background services
        class ServiceProcess : public QObject
        {
            Q_OBJECT

        public:
            explicit ServiceProcess(const QString &name, QObject *parent = nullptr);
            ~ServiceProcess();

            bool start(const QString &program,
                       const QStringList &arguments = QStringList(),
                       const QString &workingDir = QString());

            bool stop(int timeoutMs = 5000);
            bool restart();

            bool isRunning() const;
            qint64 pid() const;
            QString name() const { return m_name; }

            void setAutoRestart(bool enabled, int maxAttempts = 3);
            void setLogFile(const QString &path);

        signals:
            void started();
            void stopped();
            void crashed(const QString &reason);
            void restarting(int attempt);

        private slots:
            void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
            void onProcessError(QProcess::ProcessError error);

        private:
            QString m_name;
            QProcess *m_process;
            QString m_program;
            QStringList m_arguments;
            QString m_workingDir;

            bool m_autoRestart;
            int m_maxRestartAttempts;
            int m_restartAttempts;

            QFile *m_logFile;
            QTextStream *m_logStream;

            void writeLog(const QString &message);
            void attemptRestart();
        };
    }
}
#endif
