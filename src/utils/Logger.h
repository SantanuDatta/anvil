#ifndef LOGGER_H
#define LOGGER_H
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <memory>

namespace Anvil::Utils
{
    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };
    class Logger
    {
    public:
        static Logger &instance();
        void setLogFile(const QString &filePath);
        void setLogLevel(LogLevel level);
        void setConsoleOutput(bool enabled);
        void debug(const QString &message, const QString &category = QString());
        void info(const QString &message, const QString &category = QString());
        void warning(const QString &message, const QString &category = QString());
        void error(const QString &message, const QString &category = QString());
        void critical(const QString &message, const QString &category = QString());
        // Disable copying
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

    private:
        Logger();
        ~Logger();
        void log(LogLevel level, const QString &message, const QString &category);
        QString levelToString(LogLevel level) const;
        QString formatMessage(LogLevel level, const QString &message, const QString &category) const;
        QFile m_logFile;
        QTextStream m_logStream;
        LogLevel m_currentLevel;
        bool m_consoleOutput;
        QMutex m_mutex;
    };
    // Convenience macros
    #define LOG_DEBUG(msg) Anvil::Utils::Logger::instance().debug(msg)
    #define LOG_INFO(msg) Anvil::Utils::Logger::instance().info(msg)
    #define LOG_WARNING(msg) Anvil::Utils::Logger::instance().warning(msg)
    #define LOG_ERROR(msg) Anvil::Utils::Logger::instance().error(msg)
    #define LOG_CRITICAL(msg) Anvil::Utils::Logger::instance().critical(msg)
}
#endif
