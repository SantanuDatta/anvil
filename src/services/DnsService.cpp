#include "services/DnsService.h"
#include "core/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"
#include "utils/Network.h"
#include <QRegularExpression>

namespace Anvil
{
    namespace Services
    {
        DnsService::DnsService(QObject *parent)
            : BaseService("dns", parent), m_hostsFilePath("/etc/hosts"), m_hostsBackupPath("/etc/hosts.anvil.backup")
        {

            setDisplayName("DNS Manager");
            setDescription("Manages /etc/hosts entries for local development");

            Core::ConfigManager &config = Core::ConfigManager::instance();
            m_managedEntriesPath = Utils::FileSystem::joinPath(config.dataPath(), "dns-entries.json");

            // Load managed entries
            m_managedEntries = getManagedEntries();
        }

        DnsService::~DnsService()
        {
        }

        ServiceResult<bool> DnsService::start()
        {
            LOG_INFO("Starting DNS service");
            updateStatus(Models::ServiceStatus::Running);
            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> DnsService::stop()
        {
            LOG_INFO("Stopping DNS service");
            updateStatus(Models::ServiceStatus::Stopped);
            return ServiceResult<bool>::Ok(true);
        }

        bool DnsService::isInstalled() const
        {
            return Utils::FileSystem::fileExists(m_hostsFilePath);
        }

        bool DnsService::isRunning() const
        {
            // DNS service is always "running" if hosts file is accessible
            return Utils::FileSystem::isWritable(m_hostsFilePath);
        }

        ServiceResult<bool> DnsService::install()
        {
            // DNS is part of the system, nothing to install
            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> DnsService::uninstall()
        {
            // Remove all managed entries
            return removeEntries(m_managedEntries.keys());
        }

        ServiceResult<bool> DnsService::configure()
        {
            // Backup hosts file
            return backup();
        }

        QString DnsService::configPath() const
        {
            return m_hostsFilePath;
        }

        ServiceResult<bool> DnsService::addEntry(const QString &domain, const QString &ip)
        {
            LOG_INFO(QString("Adding DNS entry: %1 -> %2").arg(domain, ip));

            if (!isDomainValid(domain))
            {
                return ServiceResult<bool>::Err("Invalid domain name");
            }

            if (!isIpValid(ip))
            {
                return ServiceResult<bool>::Err("Invalid IP address");
            }

            // Read current entries
            auto parseResult = parseHostsFile();
            if (parseResult.isError())
            {
                return ServiceResult<bool>::Err(parseResult.error);
            }

            QMap<QString, QString> entries = parseResult.data;

            // Add new entry
            QString cleanDomain = sanitizeDomain(domain);
            QString cleanIp = sanitizeIp(ip);

            entries[cleanDomain] = cleanIp;

            // Write back
            auto writeResult = writeHostsFile(entries);
            if (writeResult.isError())
            {
                return writeResult;
            }

            // Track as managed entry
            m_managedEntries[cleanDomain] = cleanIp;
            saveManagedEntries(m_managedEntries);

            emit entryAdded(cleanDomain, cleanIp);
            LOG_INFO(QString("DNS entry added: %1 -> %2").arg(cleanDomain, cleanIp));

            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> DnsService::removeEntry(const QString &domain)
        {
            LOG_INFO(QString("Removing DNS entry: %1").arg(domain));

            QString cleanDomain = sanitizeDomain(domain);

            if (!hasEntry(cleanDomain))
            {
                return ServiceResult<bool>::Err("Entry does not exist");
            }

            // Read current entries
            auto parseResult = parseHostsFile();
            if (parseResult.isError())
            {
                return ServiceResult<bool>::Err(parseResult.error);
            }

            QMap<QString, QString> entries = parseResult.data;

            // Remove entry
            entries.remove(cleanDomain);

            // Write back
            auto writeResult = writeHostsFile(entries);
            if (writeResult.isError())
            {
                return writeResult;
            }

            // Remove from managed entries
            m_managedEntries.remove(cleanDomain);
            saveManagedEntries(m_managedEntries);

            emit entryRemoved(cleanDomain);
            LOG_INFO(QString("DNS entry removed: %1").arg(cleanDomain));

            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> DnsService::updateEntry(const QString &domain, const QString &ip)
        {
            LOG_INFO(QString("Updating DNS entry: %1 -> %2").arg(domain, ip));

            // Remove and re-add
            auto removeResult = removeEntry(domain);
            if (removeResult.isError())
            {
                return removeResult;
            }

            auto addResult = addEntry(domain, ip);
            if (addResult.isSuccess())
            {
                emit entryUpdated(domain, ip);
            }

            return addResult;
        }

        bool DnsService::hasEntry(const QString &domain) const
        {
            auto result = parseHostsFile();
            if (result.isError())
            {
                return false;
            }

            QString cleanDomain = sanitizeDomain(domain);
            return result.data.contains(cleanDomain);
        }

        ServiceResult<QString> DnsService::getIpForDomain(const QString &domain) const
        {
            auto result = parseHostsFile();
            if (result.isError())
            {
                return ServiceResult<QString>::Err(result.error);
            }

            QString cleanDomain = sanitizeDomain(domain);
            if (!result.data.contains(cleanDomain))
            {
                return ServiceResult<QString>::Err("Domain not found");
            }

            return ServiceResult<QString>::Ok(result.data[cleanDomain]);
        }

        ServiceResult<bool> DnsService::addEntries(const QMap<QString, QString> &entries)
        {
            LOG_INFO(QString("Adding %1 DNS entries").arg(entries.size()));

            for (auto it = entries.begin(); it != entries.end(); ++it)
            {
                auto result = addEntry(it.key(), it.value());
                if (result.isError())
                {
                    LOG_WARNING(QString("Failed to add entry %1: %2").arg(it.key(), result.error));
                }
            }

            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> DnsService::removeEntries(const QStringList &domains)
        {
            LOG_INFO(QString("Removing %1 DNS entries").arg(domains.size()));

            for (const QString &domain : domains)
            {
                auto result = removeEntry(domain);
                if (result.isError())
                {
                    LOG_WARNING(QString("Failed to remove entry %1: %2").arg(domain, result.error));
                }
            }

            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<QMap<QString, QString>> DnsService::getAllEntries() const
        {
            return parseHostsFile();
        }

        bool DnsService::isDomainValid(const QString &domain) const
        {
            // Check for valid domain format
            QRegularExpression regex("^([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)*[a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?$");
            return regex.match(domain).hasMatch();
        }

        bool DnsService::isIpValid(const QString &ip) const
        {
            // Simple IPv4 validation
            QRegularExpression regex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
            return regex.match(ip).hasMatch();
        }

        ServiceResult<bool> DnsService::backup()
        {
            LOG_INFO("Backing up hosts file");

            if (!Utils::FileSystem::fileExists(m_hostsFilePath))
            {
                return ServiceResult<bool>::Err("Hosts file does not exist");
            }

            bool success = Utils::FileSystem::copyFile(m_hostsFilePath, m_hostsBackupPath, true);

            if (success)
            {
                LOG_INFO("Hosts file backed up successfully");
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err("Failed to backup hosts file");
        }

        ServiceResult<bool> DnsService::restore()
        {
            LOG_INFO("Restoring hosts file from backup");

            if (!Utils::FileSystem::fileExists(m_hostsBackupPath))
            {
                return ServiceResult<bool>::Err("Backup file does not exist");
            }

            bool success = Utils::FileSystem::copyFile(m_hostsBackupPath, m_hostsFilePath, true);

            if (success)
            {
                LOG_INFO("Hosts file restored successfully");
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err("Failed to restore hosts file");
        }

        ServiceResult<QStringList> DnsService::listBackups() const
        {
            QStringList backups;

            if (Utils::FileSystem::fileExists(m_hostsBackupPath))
            {
                backups << m_hostsBackupPath;
            }

            return ServiceResult<QStringList>::Ok(backups);
        }

        ServiceResult<QString> DnsService::resolveToIp(const QString &domain)
        {
            QString ip = Utils::DnsManager::resolveDomain(domain);

            if (ip.isEmpty())
            {
                return ServiceResult<QString>::Err("Could not resolve domain");
            }

            return ServiceResult<QString>::Ok(ip);
        }

        ServiceResult<bool> DnsService::flushDnsCache()
        {
            LOG_INFO("Flushing DNS cache");

            // Different commands for different systems
            QStringList commands = {
                "systemd-resolve --flush-caches",
                "resolvectl flush-caches",
                "nscd -i hosts"};

            for (const QString &cmd : commands)
            {
                auto result = executeCommand("/bin/sh", QStringList() << "-c" << cmd);
                if (result.isSuccess())
                {
                    LOG_INFO("DNS cache flushed successfully");
                    return result;
                }
            }

            LOG_WARNING("Could not flush DNS cache");
            return ServiceResult<bool>::Ok(true); // Not critical if it fails
        }

        ServiceResult<QMap<QString, QString>> DnsService::parseHostsFile() const
        {
            auto readResult = Utils::FileSystem::readLines(m_hostsFilePath);
            if (readResult.isError())
            {
                return ServiceResult<QMap<QString, QString>>::Err(readResult.error);
            }

            QMap<QString, QString> entries;
            QRegularExpression commentRegex("^\\s*#");
            QRegularExpression entryRegex("^\\s*(\\S+)\\s+(\\S+)");

            for (const QString &line : readResult.data)
            {
                // Skip comments and empty lines
                if (commentRegex.match(line).hasMatch() || line.trimmed().isEmpty())
                {
                    continue;
                }

                QRegularExpressionMatch match = entryRegex.match(line);
                if (match.hasMatch())
                {
                    QString ip = match.captured(1);
                    QString domain = match.captured(2);
                    entries[domain] = ip;
                }
            }

            return ServiceResult<QMap<QString, QString>>::Ok(entries);
        }

        ServiceResult<bool> DnsService::writeHostsFile(const QMap<QString, QString> &entries)
        {
            QStringList lines;

            // Add header
            lines << "# /etc/hosts file";
            lines << "# Managed by Linux Herd";
            lines << "";
            lines << "# System entries";
            lines << "127.0.0.1\tlocalhost";
            lines << "127.0.1.1\t" + Utils::Network::getHostname();
            lines << "::1\t\tlocalhost ip6-localhost ip6-loopback";
            lines << "ff02::1\t\tip6-allnodes";
            lines << "ff02::2\t\tip6-allrouters";
            lines << "";

            // Add managed entries
            if (!m_managedEntries.isEmpty())
            {
                lines << "# Linux Herd managed entries";
                for (auto it = m_managedEntries.begin(); it != m_managedEntries.end(); ++it)
                {
                    lines << QString("%1\t%2").arg(it.value(), it.key());
                }
                lines << "";
            }

            // Add other entries
            lines << "# Other entries";
            for (auto it = entries.begin(); it != entries.end(); ++it)
            {
                if (!m_managedEntries.contains(it.key()))
                {
                    lines << QString("%1\t%2").arg(it.value(), it.key());
                }
            }

            // Backup current file
            backup();

            // Write new file (requires root)
            QString content = lines.join('\n') + '\n';

            // Create temporary file
            QString tempFile = Utils::FileSystem::createTempFile("hosts-XXXXXX");
            auto tempResult = Utils::FileSystem::writeFile(tempFile, content);

            if (tempResult.isError())
            {
                return ServiceResult<bool>::Err(tempResult.error);
            }

            // Move to /etc/hosts with root privileges
            QStringList args;
            args << tempFile << m_hostsFilePath;
            auto result = executeAsRoot("mv", args);

            // Flush DNS cache
            flushDnsCache();

            return result;
        }

        QMap<QString, QString> DnsService::getManagedEntries() const
        {
            if (!Utils::FileSystem::fileExists(m_managedEntriesPath))
            {
                return QMap<QString, QString>();
            }

            auto readResult = Utils::FileSystem::readFile(m_managedEntriesPath);
            if (readResult.isError())
            {
                return QMap<QString, QString>();
            }

            QJsonDocument doc = QJsonDocument::fromJson(readResult.data.toUtf8());
            if (!doc.isObject())
            {
                return QMap<QString, QString>();
            }

            QMap<QString, QString> entries;
            QJsonObject obj = doc.object();

            for (auto it = obj.begin(); it != obj.end(); ++it)
            {
                entries[it.key()] = it.value().toString();
            }

            return entries;
        }

        void DnsService::saveManagedEntries(const QMap<QString, QString> &entries)
        {
            QJsonObject obj;

            for (auto it = entries.begin(); it != entries.end(); ++it)
            {
                obj[it.key()] = it.value();
            }

            QJsonDocument doc(obj);
            Utils::FileSystem::writeFile(m_managedEntriesPath, doc.toJson(QJsonDocument::Indented));
        }

        QString DnsService::sanitizeDomain(const QString &domain) const
        {
            return domain.toLower().trimmed();
        }

        QString DnsService::sanitizeIp(const QString &ip) const
        {
            return ip.trimmed();
        }

        bool DnsService::isOurEntry(const QString &domain) const
        {
            return m_managedEntries.contains(sanitizeDomain(domain));
        }
    }
}
