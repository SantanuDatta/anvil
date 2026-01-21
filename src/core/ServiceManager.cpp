#include "core/ServiceManager.h"
#include "core/ConfigManager.h"

#include "services/PHPService.h"
#include "services/NginxService.h"
#include "services/DatabaseService.h"
#include "services/DnsService.h"
#include "services/NodeService.h"

#include "utils/Logger.h"

#include <QMutexLocker>
#include <QThread>

namespace Anvil::Core
{
    ServiceManager *ServiceManager::s_instance = nullptr;

    ServiceManager *ServiceManager::instance()
    {
        if (!s_instance)
        {
            s_instance = new ServiceManager();
        }
        return s_instance;
    }

    ServiceManager::ServiceManager(QObject *parent)
        : QObject(parent)
    {
    }

    ServiceManager::~ServiceManager()
    {
        LOG_INFO("ServiceManager shutting down");

        stopAll();

        delete m_php;
        delete m_nginx;
        delete m_database;
        delete m_dns;
        delete m_node;

        s_instance = nullptr;
    }

    bool ServiceManager::initialize()
    {
        QMutexLocker lock(&m_mutex);

        if (m_initialized)
        {
            LOG_WARNING("ServiceManager already initialized");
            return true;
        }

        LOG_INFO("Initializing ServiceManager");

        // Ensure config exists
        auto &config = ConfigManager::instance();
        if (!config.isInitialized() && !config.initialize())
        {
            LOG_ERROR("Failed to initialize ConfigManager");
            return false;
        }

        createServices();
        connectSignals();

        m_initialized = true;
        LOG_INFO("ServiceManager initialized");

        return true;
    }

    bool ServiceManager::isInitialized() const
    {
        return m_initialized;
    }

    void ServiceManager::createServices()
    {
        m_php = new Services::PHPService(this);
        m_nginx = new Services::NginxService(this);
        m_database = new Services::DatabaseService(this);
        m_dns = new Services::DnsService(this);
        m_node = new Services::NodeService(this);
    }

    void ServiceManager::connectSignals()
    {
        connect(m_php, &Services::PHPService::started,
                this, [this]()
                { emit serviceStarted("php"); });

        connect(m_php, &Services::PHPService::stopped,
                this, [this]()
                { emit serviceStopped("php"); });

        connect(m_php, &Services::PHPService::errorOccurred,
                this, [this](const QString &err)
                { emit serviceError("php", err); });

        connect(m_nginx, &Services::NginxService::started,
                this, [this]()
                { emit serviceStarted("nginx"); });

        connect(m_nginx, &Services::NginxService::stopped,
                this, [this]()
                { emit serviceStopped("nginx"); });

        connect(m_nginx, &Services::NginxService::errorOccurred,
                this, [this](const QString &err)
                { emit serviceError("nginx", err); });

        connect(m_database, &Services::DatabaseService::started,
                this, [this]()
                { emit serviceStarted("database"); });

        connect(m_database, &Services::DatabaseService::stopped,
                this, [this]()
                { emit serviceStopped("database"); });

        connect(m_database, &Services::DatabaseService::errorOccurred,
                this, [this](const QString &err)
                { emit serviceError("database", err); });

        connect(m_dns, &Services::DnsService::started,
                this, [this]()
                { emit serviceStarted("dns"); });

        connect(m_dns, &Services::DnsService::stopped,
                this, [this]()
                { emit serviceStopped("dns"); });

        connect(m_dns, &Services::DnsService::errorOccurred,
                this, [this](const QString &err)
                { emit serviceError("dns", err); });

        connect(m_node, &Services::NodeService::started,
                this, [this]()
                { emit serviceStarted("node"); });

        connect(m_node, &Services::NodeService::stopped,
                this, [this]()
                { emit serviceStopped("node"); });

        connect(m_node, &Services::NodeService::errorOccurred,
                this, [this](const QString &err)
                { emit serviceError("node", err); });
    }

    // ============================================================
    // Accessors
    // ============================================================

    Services::PHPService *ServiceManager::phpService() const { return m_php; }
    Services::NginxService *ServiceManager::nginxService() const { return m_nginx; }
    Services::DatabaseService *ServiceManager::databaseService() const { return m_database; }
    Services::DnsService *ServiceManager::dnsService() const { return m_dns; }
    Services::NodeService *ServiceManager::nodeService() const { return m_node; }

    // ============================================================
    // Lifecycle
    // ============================================================

    bool ServiceManager::startAll()
    {
        LOG_INFO("Starting all services");

        bool ok = true;

        if (m_dns)
            ok &= m_dns->start().isSuccess();
        if (m_database)
            ok &= m_database->start().isSuccess();
        if (m_php)
            ok &= m_php->start().isSuccess();
        if (m_nginx)
            ok &= m_nginx->start().isSuccess();
        if (m_node)
            ok &= m_node->start().isSuccess();

        if (ok)
            emit allServicesStarted();

        return ok;
    }

    bool ServiceManager::stopAll()
    {
        LOG_INFO("Stopping all services");

        bool ok = true;

        if (m_nginx)
            ok &= m_nginx->stop().isSuccess();
        if (m_php)
            ok &= m_php->stop().isSuccess();
        if (m_database)
            ok &= m_database->stop().isSuccess();
        if (m_dns)
            ok &= m_dns->stop().isSuccess();
        if (m_node)
            ok &= m_node->stop().isSuccess();

        if (ok)
            emit allServicesStopped();

        return ok;
    }

    bool ServiceManager::restartAll()
    {
        stopAll();
        QThread::msleep(500);
        return startAll();
    }

    // ============================================================
    // Individual services
    // ============================================================

    bool ServiceManager::startService(const QString &name)
    {
        if (name == "php")
            return m_php->start().isSuccess();
        if (name == "nginx")
            return m_nginx->start().isSuccess();
        if (name == "database")
            return m_database->start().isSuccess();
        if (name == "dns")
            return m_dns->start().isSuccess();
        if (name == "node")
            return m_node->start().isSuccess();

        LOG_ERROR("Unknown service: " + name);
        return false;
    }

    bool ServiceManager::stopService(const QString &name)
    {
        if (name == "php")
            return m_php->stop().isSuccess();
        if (name == "nginx")
            return m_nginx->stop().isSuccess();
        if (name == "database")
            return m_database->stop().isSuccess();
        if (name == "dns")
            return m_dns->stop().isSuccess();
        if (name == "node")
            return m_node->stop().isSuccess();

        LOG_ERROR("Unknown service: " + name);
        return false;
    }

    bool ServiceManager::restartService(const QString &name)
    {
        stopService(name);
        QThread::msleep(300);
        return startService(name);
    }

    // ============================================================
    // Status
    // ============================================================

    Models::Service ServiceManager::serviceStatus(const QString &name) const
    {
        if (name == "php")
            return m_php->status();
        if (name == "nginx")
            return m_nginx->status();
        if (name == "database")
            return m_database->status();
        if (name == "dns")
            return m_dns->status();
        if (name == "node")
            return m_node->status();

        return Models::Service(name);
    }

    QStringList ServiceManager::availableServices() const
    {
        return {"php", "nginx", "database", "dns", "node"};
    }

    QStringList ServiceManager::runningServices() const
    {
        QStringList running;

        if (m_php && m_php->isRunning())
            running << "php";
        if (m_nginx && m_nginx->isRunning())
            running << "nginx";
        if (m_database && m_database->isRunning())
            running << "database";
        if (m_dns && m_dns->isRunning())
            running << "dns";
        if (m_node && m_node->isRunning())
            running << "node";

        return running;
    }
}
