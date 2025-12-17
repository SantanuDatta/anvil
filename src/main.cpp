#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QScrollArea>
#include <QTimer>
#include <QDebug>
#include "utils/Logger.h"
#include "core/ConfigManager.h"
#include "core/ServiceManager.h"
#include "models/Site.h"

using namespace Anvil;

class TestWindow : public QMainWindow
{
    Q_OBJECT

public:
    TestWindow(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        qDebug() << "TestWindow constructor started";

        setWindowTitle("Anvil - Laravel Herd for Linux");
        resize(1400, 900);

        QWidget *centralWidget = new QWidget(this);
        QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

        // Left side - Controls (with scroll)
        QScrollArea *scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setMinimumWidth(400);

        QWidget *controlWidget = new QWidget();
        QVBoxLayout *controlLayout = new QVBoxLayout(controlWidget);

        addServiceControls(controlLayout);
        addPhpControls(controlLayout);
        addNginxControls(controlLayout);
        addDatabaseControls(controlLayout);
        addDnsControls(controlLayout);
        addSiteControls(controlLayout);

        QPushButton *btnClear = new QPushButton("Clear Output", this);
        connect(btnClear, &QPushButton::clicked, this, &TestWindow::clearOutput);
        controlLayout->addWidget(btnClear);

        controlLayout->addStretch();

        scrollArea->setWidget(controlWidget);
        mainLayout->addWidget(scrollArea, 1);

        // Right side - Output
        m_output = new QTextEdit(this);
        m_output->setReadOnly(true);
        m_output->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; font-family: monospace; font-size: 10pt; }");
        mainLayout->addWidget(m_output, 2);

        setCentralWidget(centralWidget);

        qDebug() << "TestWindow UI setup complete";

        showWelcome();

        // Auto-initialize after window shows
        QTimer::singleShot(100, this, &TestWindow::autoInitialize);

        qDebug() << "TestWindow constructor finished";
    }

private:
    void addServiceControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🎛️ System", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        QPushButton *btn = new QPushButton("Quick Install Stack", this);
        btn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; }");
        connect(btn, &QPushButton::clicked, this, &TestWindow::quickInstallStack);
        vbox->addWidget(btn);

        btn = new QPushButton("Detect Installations", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::detectInstallations);
        vbox->addWidget(btn);

        btn = new QPushButton("Check Status", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::checkStatus);
        vbox->addWidget(btn);

        btn = new QPushButton("Start All", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::startAllServices);
        vbox->addWidget(btn);

        btn = new QPushButton("Stop All", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::stopAllServices);
        vbox->addWidget(btn);

        layout->addWidget(group);
    }

    void addPhpControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🐘 PHP", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        QPushButton *btn = new QPushButton("Detect PHP Versions", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::detectPhpVersions);
        vbox->addWidget(btn);

        m_phpVersionCombo = new QComboBox(this);
        vbox->addWidget(new QLabel("Switch to:"));
        vbox->addWidget(m_phpVersionCombo);

        btn = new QPushButton("Set Default Version", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::switchPhp);
        vbox->addWidget(btn);

        layout->addWidget(group);
    }

    void addNginxControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🌐 Nginx", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        QPushButton *btn = new QPushButton("Test Config", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::testNginxConfig);
        vbox->addWidget(btn);

        btn = new QPushButton("Reload Config", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::reloadNginx);
        vbox->addWidget(btn);

        layout->addWidget(group);
    }

    void addDatabaseControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🗄️ Database", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        vbox->addWidget(new QLabel("Database Name:"));
        m_dbNameInput = new QLineEdit(this);
        m_dbNameInput->setPlaceholderText("my_app_db");
        vbox->addWidget(m_dbNameInput);

        QPushButton *btn = new QPushButton("Create Database", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::createDatabase);
        vbox->addWidget(btn);

        btn = new QPushButton("List Databases", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::listDatabases);
        vbox->addWidget(btn);

        layout->addWidget(group);
    }

    void addDnsControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🌍 DNS", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        vbox->addWidget(new QLabel("Domain:"));
        m_domainInput = new QLineEdit(this);
        m_domainInput->setPlaceholderText("example.test");
        vbox->addWidget(m_domainInput);

        QPushButton *btn = new QPushButton("Add DNS Entry", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::addDnsEntry);
        vbox->addWidget(btn);

        layout->addWidget(group);
    }

    void addSiteControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🚀 Create Site", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        vbox->addWidget(new QLabel("Site Name:"));
        m_siteNameInput = new QLineEdit(this);
        m_siteNameInput->setPlaceholderText("My Laravel App");
        vbox->addWidget(m_siteNameInput);

        vbox->addWidget(new QLabel("Site Path:"));
        m_sitePathInput = new QLineEdit(this);
        m_sitePathInput->setPlaceholderText("/home/user/projects/app");
        vbox->addWidget(m_sitePathInput);

        vbox->addWidget(new QLabel("Domain:"));
        m_siteDomainInput = new QLineEdit(this);
        m_siteDomainInput->setPlaceholderText("app.test");
        vbox->addWidget(m_siteDomainInput);

        QPushButton *btn = new QPushButton("Create Complete Site", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::createCompleteSite);
        vbox->addWidget(btn);

        layout->addWidget(group);
    }

    void showWelcome()
    {
        appendOutput("╔════════════════════════════════════════════╗");
        appendOutput("║      Anvil - Laravel Herd for Linux       ║");
        appendOutput("╚════════════════════════════════════════════╝");
        appendOutput("");
        appendOutput("🚀 Manages your existing dev stack");
        appendOutput("");
        appendOutput("Auto-detecting installed services...");
        appendOutput("");
    }

private slots:
    void autoInitialize()
    {
        qDebug() << "autoInitialize() started";

        appendOutput(QString("=").repeated(50));
        appendOutput("AUTO-DETECTION");
        appendOutput(QString("=").repeated(50));

        try
        {
            // Initialize config (uses ~/.anvil)
            qDebug() << "Getting ConfigManager instance";
            Core::ConfigManager &config = Core::ConfigManager::instance();

            qDebug() << "Initializing ConfigManager";
            if (!config.initialize())
            {
                appendOutput("✗ Failed to initialize config");
                return;
            }
            appendOutput("✓ Config directory: ~/.anvil");

            // Initialize service manager
            qDebug() << "Getting ServiceManager instance";
            Core::ServiceManager &manager = Core::ServiceManager::instance();

            qDebug() << "Initializing ServiceManager";
            if (!manager.initialize())
            {
                appendOutput("✗ Failed to initialize services");
                return;
            }

            appendOutput("");
            qDebug() << "Calling detectInstallations()";
            detectInstallations();

            qDebug() << "autoInitialize() completed";
        }
        catch (const std::exception &e)
        {
            qDebug() << "Exception in autoInitialize():" << e.what();
            appendOutput(QString("❌ Error: %1").arg(e.what()));
        }
        catch (...)
        {
            qDebug() << "Unknown exception in autoInitialize()";
            appendOutput("❌ Unknown error occurred");
        }
    }

    void quickInstallStack()
    {
        appendOutput("\n" + QString("=").repeated(50));
        appendOutput("QUICK INSTALL - IDEAL LARAVEL STACK");
        appendOutput(QString("=").repeated(50));
        appendOutput("");
        appendOutput("This will install:");
        appendOutput("  • PHP 8.3 (FPM + CLI + Common extensions)");
        appendOutput("  • Nginx (latest stable)");
        appendOutput("  • MySQL 8.0");
        appendOutput("");
        appendOutput("⚠️  Requires sudo privileges");
        appendOutput("⚠️  This may take 5-10 minutes");
        appendOutput("");

        Core::ServiceManager &manager = Core::ServiceManager::instance();

        // Install PHP
        appendOutput("1️⃣  Installing PHP 8.3...");
        Services::PHPService *php = manager.phpService();
        if (php && !php->isInstalled())
        {
            auto result = php->installVersion("8.3");
            if (result.isSuccess())
            {
                appendOutput("✓ PHP 8.3 installed");
            }
            else
            {
                appendOutput(QString("✗ PHP install failed: %1").arg(result.error));
            }
        }
        else
        {
            appendOutput("⊳ PHP already installed");
        }

        // Install Nginx
        appendOutput("\n2️⃣  Installing Nginx...");
        Services::NginxService *nginx = manager.nginxService();
        if (nginx && !nginx->isInstalled())
        {
            auto result = nginx->install();
            if (result.isSuccess())
            {
                appendOutput("✓ Nginx installed");
            }
            else
            {
                appendOutput(QString("✗ Nginx install failed: %1").arg(result.error));
            }
        }
        else
        {
            appendOutput("⊳ Nginx already installed");
        }

        // Install Database
        appendOutput("\n3️⃣  Installing MySQL...");
        Services::DatabaseService *db = manager.databaseService();
        if (db && !db->isInstalled())
        {
            db->setDatabaseType(Services::DatabaseService::stringToDatabaseType("MySQL"));
            auto result = db->install();
            if (result.isSuccess())
            {
                appendOutput("✓ MySQL installed");
            }
            else
            {
                appendOutput(QString("✗ MySQL install failed: %1").arg(result.error));
            }
        }
        else
        {
            appendOutput("⊳ MySQL already installed");
        }

        appendOutput("\n" + QString("=").repeated(50));
        appendOutput("✅ INSTALLATION COMPLETE");
        appendOutput(QString("=").repeated(50));
        appendOutput("");
        appendOutput("Click 'Detect Installations' to verify");
    }

    void detectInstallations()
    {
        qDebug() << "detectInstallations() started";

        appendOutput("\n" + QString("-").repeated(50));
        appendOutput("DETECTING INSTALLATIONS");
        appendOutput(QString("-").repeated(50));

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        bool allInstalled = true;

        // Detect PHP
        qDebug() << "Detecting PHP";
        Services::PHPService *php = manager.phpService();
        if (php && php->isInstalled())
        {
            appendOutput("✓ PHP detected");
            auto version = php->detectVersion();
            if (version.isSuccess())
            {
                appendOutput(QString("  Version: %1").arg(version.data));
            }

            // Detect installed versions
            auto installed = php->listInstalledVersions();
            if (installed.isSuccess() && !installed.data.isEmpty())
            {
                appendOutput(QString("  Found %1 versions:").arg(installed.data.size()));

                m_phpVersionCombo->clear();
                for (const Models::PHPVersion &v : installed.data)
                {
                    appendOutput(QString("    • PHP %1").arg(v.version()));
                    m_phpVersionCombo->addItem(v.version());
                }
            }
        }
        else
        {
            appendOutput("✗ PHP not found");
            appendOutput("  Recommended: PHP 8.3");
            appendOutput("  Install: sudo apt install php8.3-fpm php8.3-cli php8.3-common");
            allInstalled = false;
        }

        // Detect Nginx
        qDebug() << "Detecting Nginx";
        Services::NginxService *nginx = manager.nginxService();
        if (nginx && nginx->isInstalled())
        {
            appendOutput("✓ Nginx detected");
            auto version = nginx->detectVersion();
            if (version.isSuccess())
            {
                appendOutput(QString("  Version: %1").arg(version.data));
            }
        }
        else
        {
            appendOutput("✗ Nginx not found");
            appendOutput("  Install: sudo apt install nginx");
            allInstalled = false;
        }

        // Detect Database
        qDebug() << "Detecting Database";
        Services::DatabaseService *db = manager.databaseService();
        if (db && db->isInstalled())
        {
            appendOutput(QString("✓ %1 detected").arg(db->databaseTypeString()));
            auto version = db->detectVersion();
            if (version.isSuccess())
            {
                appendOutput(QString("  Version: %1").arg(version.data));
            }
        }
        else
        {
            appendOutput("✗ Database not found");
            appendOutput("  Recommended: MySQL 8.0");
            appendOutput("  Install: sudo apt install mysql-server");
            allInstalled = false;
        }

        appendOutput("");

        if (allInstalled)
        {
            appendOutput("✅ All services detected!");
            appendOutput("   Anvil manages configs in: ~/.anvil");
            appendOutput("   System binaries remain in standard locations");
        }
        else
        {
            appendOutput("⚠️  Some services are missing");
            appendOutput("   Install them using the commands above");
            appendOutput("   Then click 'Detect Installations' again");
        }

        qDebug() << "detectInstallations() completed";
    }

    void detectPhpVersions()
    {
        appendOutput("\n📋 DETECTING PHP VERSIONS");

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        Services::PHPService *php = manager.phpService();

        if (!php || !php->isInstalled())
        {
            appendOutput("✗ PHP not installed");
            return;
        }

        auto installed = php->listInstalledVersions();
        if (installed.isSuccess() && !installed.data.isEmpty())
        {
            appendOutput("Found versions:");

            m_phpVersionCombo->clear();
            for (const Models::PHPVersion &v : installed.data)
            {
                QString marker = v.isDefault() ? " (default)" : "";
                appendOutput(QString("  • PHP %1%2").arg(v.version(), marker));
                m_phpVersionCombo->addItem(v.version());
            }
        }
        else
        {
            appendOutput("No additional PHP versions found");
        }
    }

    void switchPhp()
    {
        QString version = m_phpVersionCombo->currentText();
        if (version.isEmpty())
        {
            appendOutput("✗ Please select a version");
            return;
        }

        appendOutput(QString("\n🔄 Setting PHP %1 as default...").arg(version));

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        Services::PHPService *php = manager.phpService();

        if (!php)
            return;

        auto result = php->switchVersion(version);
        if (result.isSuccess())
        {
            appendOutput(QString("✓ Default PHP version: %1").arg(version));
        }
        else
        {
            appendOutput(QString("✗ Failed: %1").arg(result.error));
        }
    }

    void testNginxConfig()
    {
        appendOutput("\n🔍 Testing Nginx configuration...");

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        Services::NginxService *nginx = manager.nginxService();

        if (!nginx)
            return;

        if (!nginx->isInstalled())
        {
            appendOutput("✗ Nginx not installed");
            return;
        }

        auto result = nginx->testConfig();
        if (result.isSuccess())
        {
            appendOutput("✓ Configuration is valid");
        }
        else
        {
            appendOutput("✗ Config test failed:");
            appendOutput(QString("  %1").arg(result.error));
        }
    }

    void reloadNginx()
    {
        appendOutput("\n🔄 Reloading Nginx...");

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        Services::NginxService *nginx = manager.nginxService();

        if (!nginx)
            return;

        auto result = nginx->reload();
        if (result.isSuccess())
        {
            appendOutput("✓ Configuration reloaded");
        }
        else
        {
            appendOutput(QString("✗ Failed: %1").arg(result.error));
        }
    }

    void startAllServices()
    {
        appendOutput("\n▶️  Starting services...");

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        manager.startAll();

        QTimer::singleShot(1000, this, &TestWindow::checkStatus);
    }

    void stopAllServices()
    {
        appendOutput("\n⏹️  Stopping services...");

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        manager.stopAll();

        checkStatus();
    }

    void checkStatus()
    {
        appendOutput("\n" + QString("-").repeated(50));
        appendOutput("SERVICE STATUS");
        appendOutput(QString("-").repeated(50));

        Core::ServiceManager &manager = Core::ServiceManager::instance();

        for (const QString &name : manager.availableServices())
        {
            Models::Service status = manager.getServiceStatus(name);
            QString icon = status.isRunning() ? "🟢" : "🔴";

            appendOutput(QString("%1 %2: %3")
                             .arg(icon)
                             .arg(name.leftJustified(12))
                             .arg(status.statusString()));

            if (!status.version().isEmpty())
            {
                appendOutput(QString("   Version: %1").arg(status.version()));
            }
        }
    }

    void createDatabase()
    {
        QString name = m_dbNameInput->text();
        if (name.isEmpty())
        {
            appendOutput("✗ Enter database name");
            return;
        }

        appendOutput(QString("\n🗄️  Creating database: %1").arg(name));

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        Services::DatabaseService *db = manager.databaseService();

        if (!db)
            return;

        auto result = db->createDatabase(name);
        if (result.isSuccess())
        {
            appendOutput(QString("✓ Database '%1' created").arg(name));
            m_dbNameInput->clear();
        }
        else
        {
            appendOutput(QString("✗ Failed: %1").arg(result.error));
        }
    }

    void listDatabases()
    {
        appendOutput("\n📋 DATABASES");

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        Services::DatabaseService *db = manager.databaseService();

        if (!db)
            return;

        auto result = db->listDatabases();
        if (result.isSuccess())
        {
            if (result.data.isEmpty())
            {
                appendOutput("No databases found");
            }
            else
            {
                for (const auto &dbInfo : result.data)
                {
                    appendOutput(QString("  • %1").arg(dbInfo.name));
                }
            }
        }
        else
        {
            appendOutput(QString("✗ Failed: %1").arg(result.error));
        }
    }

    void addDnsEntry()
    {
        QString domain = m_domainInput->text();
        if (domain.isEmpty())
        {
            appendOutput("✗ Enter domain");
            return;
        }

        appendOutput(QString("\n🌍 Adding DNS: %1 → 127.0.0.1").arg(domain));

        Core::ServiceManager &manager = Core::ServiceManager::instance();
        Services::DnsService *dns = manager.dnsService();

        if (!dns)
            return;

        auto result = dns->addEntry(domain);
        if (result.isSuccess())
        {
            appendOutput("✓ DNS entry added");
            m_domainInput->clear();
        }
        else
        {
            appendOutput(QString("✗ Failed: %1").arg(result.error));
        }
    }

    void createCompleteSite()
    {
        QString name = m_siteNameInput->text();
        QString path = m_sitePathInput->text();
        QString domain = m_siteDomainInput->text();

        if (name.isEmpty() || path.isEmpty() || domain.isEmpty())
        {
            appendOutput("✗ Fill in all fields");
            return;
        }

        appendOutput("\n" + QString("=").repeated(50));
        appendOutput(QString("CREATING SITE: %1").arg(name));
        appendOutput(QString("=").repeated(50));

        Core::ServiceManager &manager = Core::ServiceManager::instance();

        // 1. DNS
        appendOutput("\n1️⃣  Configuring DNS...");
        Services::DnsService *dns = manager.dnsService();
        if (dns)
        {
            auto result = dns->addEntry(domain);
            if (result.isSuccess())
            {
                appendOutput("✓ DNS entry added");
            }
            else
            {
                appendOutput(QString("✗ DNS: %1").arg(result.error));
            }
        }

        // 2. Database
        QString dbName = domain;
        dbName.replace('.', '_');
        appendOutput("\n2️⃣  Creating database...");
        Services::DatabaseService *db = manager.databaseService();
        if (db && db->isInstalled())
        {
            auto result = db->createDatabase(dbName);
            if (result.isSuccess())
            {
                appendOutput(QString("✓ Database '%1' created").arg(dbName));
            }
            else
            {
                appendOutput(QString("⚠ Database: %1").arg(result.error));
            }
        }
        else
        {
            appendOutput("⊳ Database not available (optional)");
        }

        // 3. Nginx
        appendOutput("\n3️⃣  Configuring Nginx...");
        Models::Site site(name, path, domain);
        site.setPhpVersion(m_phpVersionCombo->currentText());

        Services::NginxService *nginx = manager.nginxService();
        if (nginx && nginx->isInstalled())
        {
            auto result = nginx->addSite(site);
            if (result.isSuccess())
            {
                appendOutput("✓ Nginx site configured");
                appendOutput(QString("  Config: ~/.anvil/nginx/sites-available/%1.conf").arg(site.id()));
            }
            else
            {
                appendOutput(QString("✗ Nginx: %1").arg(result.error));
            }
        }

        appendOutput("\n" + QString("=").repeated(50));
        appendOutput(QString("✅ SITE CREATED: %1").arg(name));
        appendOutput(QString("=").repeated(50));
        appendOutput(QString("🌐 URL: http://%1").arg(domain));
        appendOutput(QString("🗄️  Database: %1").arg(dbName));
        appendOutput(QString("📁 Path: %1").arg(path));

        m_siteNameInput->clear();
        m_sitePathInput->clear();
        m_siteDomainInput->clear();
    }

    void clearOutput()
    {
        m_output->clear();
        showWelcome();
        QTimer::singleShot(100, this, &TestWindow::autoInitialize);
    }

private:
    void appendOutput(const QString &text)
    {
        m_output->append(text);
        m_output->ensureCursorVisible();

        // Also print to console for debugging
        qDebug() << text;
    }

    QTextEdit *m_output;
    QComboBox *m_phpVersionCombo;
    QLineEdit *m_domainInput;
    QLineEdit *m_siteNameInput;
    QLineEdit *m_sitePathInput;
    QLineEdit *m_siteDomainInput;
    QLineEdit *m_dbNameInput;
};

int main(int argc, char *argv[])
{
    qDebug() << "main() started";

    QApplication app(argc, argv);

    app.setOrganizationName("Anvil");
    app.setApplicationName("Anvil");

    qDebug() << "Setting up logger";
    Utils::Logger::instance().setConsoleOutput(true);
    Utils::Logger::instance().setLogLevel(Utils::LogLevel::Info);

    LOG_INFO("Anvil - Laravel Herd for Linux");

    qDebug() << "Creating TestWindow";
    TestWindow window;

    qDebug() << "Showing window";
    window.show();

    qDebug() << "Entering event loop";
    int result = app.exec();

    qDebug() << "Event loop exited with code" << result;
    return result;
}

#include "main.moc"
