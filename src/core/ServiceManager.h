#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMutex>

#include "models/Service.h"

namespace Anvil
{
    namespace Services
    {
        class PHPService;
        class NginxService;
        class DatabaseService;
        class DnsService;
        class NodeService;
    }

    namespace Core
    {
        class ServiceManager final : public QObject
        {
            Q_OBJECT

        public:
            static ServiceManager *instance();

            bool initialize();
            bool isInitialized() const;

            Services::PHPService *phpService() const;
            Services::NginxService *nginxService() const;
            Services::DatabaseService *databaseService() const;
            Services::DnsService *dnsService() const;
            Services::NodeService *nodeService() const;

            bool startAll();
            bool stopAll();
            bool restartAll();

            bool startService(const QString &name);
            bool stopService(const QString &name);
            bool restartService(const QString &name);

            Models::Service serviceStatus(const QString &name) const;
            QStringList availableServices() const;
            QStringList runningServices() const;

        signals:
            void serviceStarted(const QString &name);
            void serviceStopped(const QString &name);
            void serviceError(const QString &name, const QString &error);

            void allServicesStarted();
            void allServicesStopped();

        private:
            explicit ServiceManager(QObject *parent = nullptr);
            ~ServiceManager() override;

            ServiceManager(const ServiceManager &) = delete;
            ServiceManager &operator=(const ServiceManager &) = delete;

            void createServices();
            void connectSignals();

            static ServiceManager *s_instance;

            Services::PHPService *m_php = nullptr;
            Services::NginxService *m_nginx = nullptr;
            Services::DatabaseService *m_database = nullptr;
            Services::DnsService *m_dns = nullptr;
            Services::NodeService *m_node = nullptr;

            bool m_initialized = false;
            mutable QMutex m_mutex;
        };
    }
}
