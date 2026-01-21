#ifndef DNSSERVICE_H
#define DNSSERVICE_H
#include "services/BaseService.h"
#include <QMap>

namespace Anvil::Services
{
    class DnsService : public BaseService
    {
        Q_OBJECT
    public:
        explicit DnsService(QObject *parent = nullptr);
        ~DnsService() override;
        // BaseService implementation
        ServiceResult<bool> start() override;
        ServiceResult<bool> stop() override;
        bool isInstalled() const override;
        bool isRunning() const override;
        ServiceResult<bool> install() override;
        ServiceResult<bool> uninstall() override;
        ServiceResult<bool> configure() override;
        QString configPath() const override;
        // DNS/Hosts operations
        ServiceResult<bool> addEntry(const QString &domain, const QString &ip = "127.0.0.1");
        ServiceResult<bool> removeEntry(const QString &domain);
        ServiceResult<bool> updateEntry(const QString &domain, const QString &ip);
        bool hasEntry(const QString &domain) const;
        ServiceResult<QString> getIpForDomain(const QString &domain) const;
        // Bulk operations
        ServiceResult<bool> addEntries(const QMap<QString, QString> &entries);
        ServiceResult<bool> removeEntries(const QStringList &domains);
        ServiceResult<QMap<QString, QString>> getAllEntries() const;
        // Validation
        bool isDomainValid(const QString &domain) const;
        bool isIpValid(const QString &ip) const;
        // Backup and restore
        ServiceResult<bool> backup();
        ServiceResult<bool> restore();
        ServiceResult<QStringList> listBackups() const;
        // System DNS
        ServiceResult<QString> resolveToIp(const QString &domain);
        ServiceResult<bool> flushDnsCache();
    signals:
        void entryAdded(const QString &domain, const QString &ip);
        void entryRemoved(const QString &domain);
        void entryUpdated(const QString &domain, const QString &ip);

    private:
        // Hosts file operations
        ServiceResult<QMap<QString, QString>> parseHostsFile() const;
        ServiceResult<bool> writeHostsFile(const QMap<QString, QString> &entries);
        // Managed entries (entries we added)
        QMap<QString, QString> getManagedEntries() const;
        void saveManagedEntries(const QMap<QString, QString> &entries);
        // Helpers
        QString sanitizeDomain(const QString &domain) const;
        QString sanitizeIp(const QString &ip) const;
        bool isOurEntry(const QString &domain) const;
        QString m_hostsFilePath;
        QString m_hostsBackupPath;
        QString m_managedEntriesPath;
        QMap<QString, QString> m_managedEntries;
    };
}
#endif
