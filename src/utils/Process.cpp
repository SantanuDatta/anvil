#include "utils/Process.h"
#include "utils/Logger.h"

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QProcessEnvironment>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QThread>

#include <signal.h>
#include <unistd.h>

namespace Anvil::Utils
{
    ProcessExecutor::ProcessExecutor(QObject *parent)
        : QObject(parent), m_process(new QProcess(this))
    {

        // Setup signal connections
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]()
                {
                const QString output = m_process->readAllStandardOutput();
                if (!output.isEmpty())
                {
                    m_stdoutBuffer.append(output);
                    emit outputReceived(output);
                } });

        connect(m_process, &QProcess::readyReadStandardError, this, [this]()
                {
                const QString error = m_process->readAllStandardError();
                if (!error.isEmpty())
                {
                    m_stderrBuffer.append(error);
                    emit errorReceived(error);
                } });

        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus)
                { emit finished(exitCode); });
    }

    ProcessExecutor::~ProcessExecutor()
    {
        if (m_process->state() != QProcess::NotRunning)
        {
            m_process->terminate();
            m_process->waitForFinished(3000);
            if (m_process->state() != QProcess::NotRunning)
            {
                m_process->kill();
            }
        }
    }

    ProcessResult ProcessExecutor::execute(const QString &program,
                                           const QStringList &arguments,
                                           const QString &workingDir,
                                           int timeoutMs)
    {
        LOG_DEBUG(QString("Executing: %1 %2").arg(program, arguments.join(" ")));

        m_stdoutBuffer.clear();
        m_stderrBuffer.clear();
        setupProcess(workingDir);

        m_process->start(program, arguments);

        if (!m_process->waitForStarted(5000))
        {
            QString error = QString("Failed to start process: %1").arg(m_process->errorString());
            LOG_ERROR(error);
            return ProcessResult::Err(error);
        }

        if (!m_process->waitForFinished(timeoutMs))
        {
            QString error = QString("Process timeout after %1ms").arg(timeoutMs);
            LOG_ERROR(error);
            m_process->kill();
            return ProcessResult::Err(error);
        }

        int exitCode = m_process->exitCode();
        m_stdoutBuffer.append(m_process->readAllStandardOutput());
        m_stderrBuffer.append(m_process->readAllStandardError());
        QString output = m_stdoutBuffer;
        QString errorOutput = m_stderrBuffer;

        if (exitCode != 0)
        {
            LOG_WARNING(QString("Process exited with code %1: %2").arg(exitCode).arg(errorOutput));
            return ProcessResult::Err(errorOutput.isEmpty() ? "Process failed" : errorOutput, exitCode);
        }

        return ProcessResult::Ok(output, exitCode);
    }

    ProcessResult ProcessExecutor::executeAsRoot(const QString &program,
                                                 const QStringList &arguments,
                                                 const QString &workingDir)
    {
        // Use pkexec for GUI password prompt
        QStringList rootArgs;
        rootArgs << program << arguments;

        return execute("pkexec", rootArgs, workingDir);
    }

    ProcessResult ProcessExecutor::executeShell(const QString &command,
                                                const QString &workingDir,
                                                int timeoutMs)
    {
        return execute("/bin/bash", QStringList() << "-c" << command, workingDir, timeoutMs);
    }

    bool ProcessExecutor::programExists(const QString &program)
    {
        QString path = programPath(program);
        return !path.isEmpty();
    }

    QString ProcessExecutor::programPath(const QString &program)
    {
        ProcessResult result = execute("which", QStringList() << program, QString(), 5000);
        if (result.success)
        {
            return result.output.trimmed();
        }
        return QString();
    }

    bool ProcessExecutor::killProcess(qint64 pid, bool force)
    {
        int signal = force ? SIGKILL : SIGTERM;
        return ::kill(static_cast<pid_t>(pid), signal) == 0;
    }

    bool ProcessExecutor::isProcessRunning(qint64 pid)
    {
        // Send signal 0 to check if process exists
        return ::kill(static_cast<pid_t>(pid), 0) == 0;
    }

    void ProcessExecutor::setEnvironment(const QStringList &env)
    {
        m_environment = env;
    }

    void ProcessExecutor::addEnvironmentVariable(const QString &name, const QString &value)
    {
        m_environment << QString("%1=%2").arg(name, value);
    }

    void ProcessExecutor::setupProcess(const QString &workingDir)
    {
        if (!workingDir.isEmpty())
        {
            m_process->setWorkingDirectory(workingDir);
        }

        if (!m_environment.isEmpty())
        {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            for (const QString &envVar : m_environment)
            {
                QStringList parts = envVar.split('=');
                if (parts.size() == 2)
                {
                    env.insert(parts[0], parts[1]);
                }
            }
            m_process->setProcessEnvironment(env);
        }
    }

    QString ProcessExecutor::escapeShellArgument(const QString &arg) const
    {
        QString escaped = arg;
        escaped.replace("'", "'\\''");
        return QString("'%1'").arg(escaped);
    }

    // ============================================================================
    // ProcessManager Implementation
    // ============================================================================

    ProcessManager::ProcessManager(QObject *parent)
        : QObject(parent)
    {
    }

    ProcessManager::~ProcessManager()
    {
        // Clean up all processes
        for (auto it = m_processes.begin(); it != m_processes.end(); ++it)
        {
            if (it->process->state() != QProcess::NotRunning)
            {
                it->process->terminate();
                it->process->waitForFinished(3000);
                if (it->process->state() != QProcess::NotRunning)
                {
                    it->process->kill();
                }
            }
            delete it->process;
        }
        m_processes.clear();
    }

    qint64 ProcessManager::startProcess(const QString &program,
                                        const QStringList &arguments,
                                        const QString &workingDir)
    {
        QProcess *process = new QProcess(this);

        if (!workingDir.isEmpty())
        {
            process->setWorkingDirectory(workingDir);
        }

        // Connect signals
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ProcessManager::onProcessFinished);
        connect(process, &QProcess::errorOccurred, this, &ProcessManager::onProcessError);
        connect(process, &QProcess::readyReadStandardOutput,
                this, &ProcessManager::onReadyReadStandardOutput);
        connect(process, &QProcess::readyReadStandardError,
                this, &ProcessManager::onReadyReadStandardError);

        process->start(program, arguments);

        if (!process->waitForStarted(5000))
        {
            LOG_ERROR(QString("Failed to start process: %1").arg(process->errorString()));
            delete process;
            return -1;
        }

        qint64 pid = process->processId();
        m_processes[pid] = {process, QString(), QString()};

        LOG_INFO(QString("Started process %1: %2").arg(pid).arg(program));
        emit processStarted(pid);

        return pid;
    }

    bool ProcessManager::stopProcess(qint64 pid, int timeoutMs)
    {
        if (!m_processes.contains(pid))
        {
            return false;
        }

        QProcess *process = m_processes[pid].process;

        if (process->state() == QProcess::NotRunning)
        {
            cleanupProcess(pid);
            return true;
        }

        process->terminate();

        if (!process->waitForFinished(timeoutMs))
        {
            LOG_WARNING(QString("Process %1 did not terminate gracefully, killing").arg(pid));
            process->kill();
            process->waitForFinished(1000);
        }

        cleanupProcess(pid);
        return true;
    }

    bool ProcessManager::killProcess(qint64 pid)
    {
        if (!m_processes.contains(pid))
        {
            return false;
        }

        QProcess *process = m_processes[pid].process;
        process->kill();
        process->waitForFinished(1000);

        cleanupProcess(pid);
        return true;
    }

    bool ProcessManager::isRunning(qint64 pid) const
    {
        if (!m_processes.contains(pid))
        {
            return false;
        }

        return m_processes[pid].process->state() != QProcess::NotRunning;
    }

    QString ProcessManager::getOutput(qint64 pid) const
    {
        return m_processes.contains(pid) ? m_processes[pid].outputBuffer : QString();
    }

    QString ProcessManager::getError(qint64 pid) const
    {
        return m_processes.contains(pid) ? m_processes[pid].errorBuffer : QString();
    }

    void ProcessManager::clearBuffers(qint64 pid)
    {
        if (m_processes.contains(pid))
        {
            m_processes[pid].outputBuffer.clear();
            m_processes[pid].errorBuffer.clear();
        }
    }

    QList<qint64> ProcessManager::getManagedProcesses() const
    {
        return m_processes.keys();
    }

    void ProcessManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
    {
        Q_UNUSED(exitStatus)

        QProcess *process = qobject_cast<QProcess *>(sender());
        if (!process)
            return;

        qint64 pid = process->processId();

        LOG_INFO(QString("Process %1 finished with exit code %2").arg(pid).arg(exitCode));
        emit processFinished(pid, exitCode);

        cleanupProcess(pid);
    }

    void ProcessManager::onProcessError(QProcess::ProcessError error)
    {
        Q_UNUSED(error)

        QProcess *process = qobject_cast<QProcess *>(sender());
        if (!process)
            return;

        qint64 pid = process->processId();
        QString errorString = process->errorString();

        LOG_ERROR(QString("Process %1 error: %2").arg(pid).arg(errorString));
        emit processError(pid, errorString);
    }

    void ProcessManager::onReadyReadStandardOutput()
    {
        QProcess *process = qobject_cast<QProcess *>(sender());
        if (!process)
            return;

        qint64 pid = process->processId();
        QString output = process->readAllStandardOutput();

        if (m_processes.contains(pid))
        {
            m_processes[pid].outputBuffer += output;
            emit processOutput(pid, output);
        }
    }

    void ProcessManager::onReadyReadStandardError()
    {
        QProcess *process = qobject_cast<QProcess *>(sender());
        if (!process)
            return;

        qint64 pid = process->processId();
        QString error = process->readAllStandardError();

        if (m_processes.contains(pid))
        {
            m_processes[pid].errorBuffer += error;
        }
    }

    void ProcessManager::cleanupProcess(qint64 pid)
    {
        if (m_processes.contains(pid))
        {
            QProcess *process = m_processes[pid].process;
            m_processes.remove(pid);
            process->deleteLater();
        }
    }

    // ============================================================================
    // ServiceProcess Implementation
    // ============================================================================

    ServiceProcess::ServiceProcess(const QString &name, QObject *parent)
        : QObject(parent), m_name(name), m_process(new QProcess(this)), m_autoRestart(false), m_maxRestartAttempts(3), m_restartAttempts(0), m_logFile(nullptr), m_logStream(nullptr)
    {

        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ServiceProcess::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred, this, &ServiceProcess::onProcessError);
    }

    ServiceProcess::~ServiceProcess()
    {
        stop();

        if (m_logStream)
        {
            delete m_logStream;
        }
        if (m_logFile)
        {
            m_logFile->close();
            delete m_logFile;
        }
    }

    bool ServiceProcess::start(const QString &program,
                               const QStringList &arguments,
                               const QString &workingDir)
    {
        if (isRunning())
        {
            LOG_WARNING(QString("Service %1 is already running").arg(m_name));
            return false;
        }

        m_program = program;
        m_arguments = arguments;
        m_workingDir = workingDir;
        m_restartAttempts = 0;

        if (!m_workingDir.isEmpty())
        {
            m_process->setWorkingDirectory(m_workingDir);
        }

        writeLog(QString("Starting service with command: %1 %2")
                     .arg(m_program, m_arguments.join(" ")));

        m_process->start(m_program, m_arguments);

        if (!m_process->waitForStarted(5000))
        {
            QString error = m_process->errorString();
            writeLog(QString("Failed to start: %1").arg(error));
            LOG_ERROR(QString("Failed to start service %1: %2").arg(m_name, error));
            return false;
        }

        writeLog(QString("Service started with PID: %1").arg(m_process->processId()));
        LOG_INFO(QString("Service %1 started (PID: %2)").arg(m_name).arg(m_process->processId()));
        emit started();

        return true;
    }

    bool ServiceProcess::stop(int timeoutMs)
    {
        if (!isRunning())
        {
            return true;
        }

        writeLog("Stopping service");

        m_process->terminate();

        if (!m_process->waitForFinished(timeoutMs))
        {
            writeLog("Force killing service");
            m_process->kill();
            m_process->waitForFinished(1000);
        }

        writeLog("Service stopped");
        LOG_INFO(QString("Service %1 stopped").arg(m_name));
        emit stopped();

        return true;
    }

    bool ServiceProcess::restart()
    {
        writeLog("Restarting service");

        if (!stop())
        {
            return false;
        }

        QThread::msleep(500); // Brief pause

        return start(m_program, m_arguments, m_workingDir);
    }

    bool ServiceProcess::isRunning() const
    {
        return m_process->state() != QProcess::NotRunning;
    }

    qint64 ServiceProcess::pid() const
    {
        return m_process->processId();
    }

    void ServiceProcess::setAutoRestart(bool enabled, int maxAttempts)
    {
        m_autoRestart = enabled;
        m_maxRestartAttempts = maxAttempts;
    }

    void ServiceProcess::setLogFile(const QString &path)
    {
        if (m_logStream)
        {
            delete m_logStream;
            m_logStream = nullptr;
        }

        if (m_logFile)
        {
            m_logFile->close();
            delete m_logFile;
            m_logFile = nullptr;
        }

        m_logFile = new QFile(path);
        if (m_logFile->open(QIODevice::Append | QIODevice::Text))
        {
            m_logStream = new QTextStream(m_logFile);
        }
    }

    void ServiceProcess::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
    {
        QString reason = (exitStatus == QProcess::CrashExit)
                             ? "crashed"
                             : QString("exited with code %1").arg(exitCode);

        writeLog(QString("Service finished: %1").arg(reason));
        LOG_WARNING(QString("Service %1 %2").arg(m_name, reason));

        emit crashed(reason);

        if (m_autoRestart && m_restartAttempts < m_maxRestartAttempts)
        {
            attemptRestart();
        }
    }

    void ServiceProcess::onProcessError(QProcess::ProcessError error)
    {
        Q_UNUSED(error)

        QString errorString = m_process->errorString();
        writeLog(QString("Service error: %1").arg(errorString));
        LOG_ERROR(QString("Service %1 error: %2").arg(m_name, errorString));
    }

    void ServiceProcess::writeLog(const QString &message)
    {
        if (m_logStream)
        {
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
            *m_logStream << QString("[%1] [%2] %3\n").arg(timestamp, m_name, message);
            m_logStream->flush();
        }
    }

    void ServiceProcess::attemptRestart()
    {
        m_restartAttempts++;

        writeLog(QString("Auto-restart attempt %1/%2").arg(m_restartAttempts).arg(m_maxRestartAttempts));
        LOG_INFO(QString("Auto-restarting service %1 (attempt %2/%3)")
                     .arg(m_name)
                     .arg(m_restartAttempts)
                     .arg(m_maxRestartAttempts));

        emit restarting(m_restartAttempts);

        QThread::msleep(1000 * m_restartAttempts); // Exponential backoff

        start(m_program, m_arguments, m_workingDir);
    }
}
