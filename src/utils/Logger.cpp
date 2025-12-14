#include "utils/Logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QMutexLocker>
#include <iostream>
namespace Anvil
{
    namespace Utils
    {
        Logger::Logger()
            : m_currentLevel(LogLevel::Info), m_consoleOutput(true)
        {
            // Default log file location
            QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir().mkpath(logDir);
            setLogFile(logDir + "/anvil.log");
        }
        Logger::~Logger()
        {
            if (m_logFile.isOpen())
            {
                m_logStream.flush();
                m_logFile.close();
            }
        }
        Logger &Logger::instance()
        {
            static Logger instance;
            return instance;
        }
        void Logger::setLogFile(const QString &filePath)
        {
            QMutexLocker locker(&m_mutex);
            if (m_logFile.isOpen())
            {
                m_logStream.flush();
                m_logFile.close();
            }
            m_logFile.setFileName(filePath);
            if (m_logFile.open(QIODevice::Append | QIODevice::Text))
            {
                m_logStream.setDevice(&m_logFile);
            }
        }
        void Logger::setLogLevel(LogLevel level)
        {
            m_currentLevel = level;
        }
        void Logger::setConsoleOutput(bool enabled)
        {
            m_consoleOutput = enabled;
        }
        void Logger::debug(const QString &message, const QString &category)
        {
            log(LogLevel::Debug, message, category);
        }
        void Logger::info(const QString &message, const QString &category)
        {
            log(LogLevel::Info, message, category);
        }
        void Logger::warning(const QString &message, const QString &category)
        {
            log(LogLevel::Warning, message, category);
        }
        void Logger::error(const QString &message, const QString &category)
        {
            log(LogLevel::Error, message, category);
        }
        void Logger::critical(const QString &message, const QString &category)
        {
            log(LogLevel::Critical, message, category);
        }
        void Logger::log(LogLevel level, const QString &message, const QString &category)
        {
            if (level < m_currentLevel)
            {
                return;
            }
            QMutexLocker locker(&m_mutex);
            QString formattedMessage = formatMessage(level, message, category);
            // Write to file
            if (m_logFile.isOpen())
            {
                m_logStream << formattedMessage << "\n";
                m_logStream.flush();
            }
            // Write to console
            if (m_consoleOutput)
            {
                if (level >= LogLevel::Error)
                {
                    std::cerr << formattedMessage.toStdString() << std::endl;
                }
                else
                {
                    std::cout << formattedMessage.toStdString() << std::endl;
                }
            }
        }
        QString Logger::levelToString(LogLevel level) const
        {
            switch (level)
            {
            case LogLevel::Debug:
                return "DEBUG";
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Warning:
                return "WARN";
            case LogLevel::Error:
                return "ERROR";
            case LogLevel::Critical:
                return "CRIT";
            default:
                return "UNKNOWN";
            }
        }
        QString Logger::formatMessage(LogLevel level, const QString &message, const QString &category) const
        {
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
            QString levelStr = levelToString(level);
            if (category.isEmpty())
            {
                return QString("[%1] [%2] %3").arg(timestamp, levelStr, message);
            }
            return QString("[%1] [%2] [%3] %4").arg(timestamp, levelStr, category, message);
        }
    }
}
