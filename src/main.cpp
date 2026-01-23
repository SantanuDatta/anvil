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
#include <QResource>
#include <QThread>
#include <QtConcurrent>
#include "utils/Logger.h"
#include "core/ConfigManager.h"
#include "core/ServiceManager.h"
#include "managers/VersionManager.h"
#include "managers/SiteManager.h"
#include "models/Site.h"
#include "models/PHPVersion.h"
#include "services/PHPService.h"
#include "services/NginxService.h"
#include "services/DatabaseService.h"
#include "services/DnsService.h"
#include "ui/MainWindow.h"

using namespace Anvil;

// Worker class for background initialization
class InitWorker : public QObject
{
    Q_OBJECT

public:
    InitWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void doWork()
    {
        qDebug() << "Background initialization started in thread:" << QThread::currentThreadId();

        try
        {
            // Initialize ConfigManager
            emit progressUpdate("Initializing configuration...");
            Core::ConfigManager &config = Core::ConfigManager::instance();

            if (!config.initialize())
            {
                emit error("Failed to initialize config");
                emit finished();
                return;
            }

            emit progressUpdate("✓ Config initialized");

            // Initialize ServiceManager
            emit progressUpdate("Scanning system services...");
            auto *manager = Core::ServiceManager::instance();

            if (!manager || !manager->initialize())
            {
                emit error("Failed to initialize services");
                emit finished();
                return;
            }

            emit progressUpdate("✓ Service manager ready");

            // Detect services (this can be slow)
            emit progressUpdate("Detecting installed services...");
            QThread::msleep(100); // Small delay to let GUI paint

            // Gather service info
            QStringList detectedServices;
            QMap<QString, QString> serviceVersions;
            QList<Models::PHPVersion> phpVersions;

            if (auto *php = manager->phpService())
            {
                if (php->isInstalled())
                {
                    detectedServices << "PHP";
                    auto ver = php->detectVersion();
                    if (ver.isSuccess())
                    {
                        serviceVersions["PHP"] = ver.data;
                    }

                    // Get all installed PHP versions
                    auto installed = php->listInstalledVersions();
                    if (installed.isSuccess())
                    {
                        phpVersions = installed.data;
                    }
                }
            }

            if (auto *nginx = manager->nginxService())
            {
                if (nginx->isInstalled())
                {
                    detectedServices << "Nginx";
                    auto ver = nginx->detectVersion();
                    if (ver.isSuccess())
                    {
                        serviceVersions["Nginx"] = ver.data;
                    }
                }
            }

            if (auto *db = manager->databaseService())
            {
                if (db->isInstalled())
                {
                    detectedServices << db->databaseTypeString();
                    auto ver = db->detectVersion();
                    if (ver.isSuccess())
                    {
                        serviceVersions[db->databaseTypeString()] = ver.data;
                    }
                }
            }

            emit servicesDetected(detectedServices, serviceVersions);
            emit phpVersionsDetected(phpVersions);
            emit progressUpdate("✓ Detection complete");

            qDebug() << "Background initialization completed";
        }
        catch (const std::exception &e)
        {
            emit error(QString("Exception: %1").arg(e.what()));
        }

        emit finished();
    }

signals:
    void progressUpdate(const QString &message);
    void servicesDetected(const QStringList &services, const QMap<QString, QString> &versions);
    void phpVersionsDetected(const QList<Models::PHPVersion> &versions);
    void error(const QString &message);
    void finished();
};

class TestWindow : public QMainWindow
{
    Q_OBJECT

public:
    TestWindow(QWidget *parent = nullptr) : QMainWindow(parent), m_initThread(nullptr), m_worker(nullptr)
    {
        qDebug() << "TestWindow constructor - Main thread:" << QThread::currentThreadId();

        setWindowTitle("Anvil - Laravel Herd for Linux");
        resize(1400, 900);

        QWidget *centralWidget = new QWidget(this);
        QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

        // Left side - Controls
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

        showWelcome();

        // Start background initialization immediately (non-blocking)
        QTimer::singleShot(0, this, &TestWindow::startBackgroundInit);
    }

    ~TestWindow()
    {
        if (m_initThread)
        {
            m_initThread->quit();
            m_initThread->wait(5000);
        }
    }

private:
    void addServiceControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🎛️ Quick Setup", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        QPushButton *btn = new QPushButton("⚡ Install Complete Stack", this);
        btn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 10px; }");
        connect(btn, &QPushButton::clicked, this, &TestWindow::quickInstallStack);
        vbox->addWidget(btn);

        QLabel *label = new QLabel("Installs: PHP 8.3 + Nginx + MySQL");
        label->setStyleSheet("QLabel { color: #666; font-size: 9pt; }");
        vbox->addWidget(label);

        vbox->addWidget(new QLabel("─────────────────"));

        btn = new QPushButton("🔍 Detect Installations", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::detectInstallations);
        vbox->addWidget(btn);

        btn = new QPushButton("📊 Check Status", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::checkStatus);
        vbox->addWidget(btn);

        layout->addWidget(group);
    }

    void addPhpControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🐘 PHP", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        // Install PHP section
        vbox->addWidget(new QLabel("Install PHP Version:"));
        m_phpInstallCombo = new QComboBox(this);
        m_phpInstallCombo->addItem("8.4 (Latest)");
        m_phpInstallCombo->addItem("8.3 (Stable)");
        m_phpInstallCombo->addItem("8.2");
        m_phpInstallCombo->addItem("8.1");
        m_phpInstallCombo->addItem("8.0");
        m_phpInstallCombo->addItem("7.4");
        m_phpInstallCombo->setCurrentIndex(1); // Default to 8.3
        vbox->addWidget(m_phpInstallCombo);

        QPushButton *btnInstall = new QPushButton("📥 Install PHP Version", this);
        btnInstall->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; padding: 8px; }");
        connect(btnInstall, &QPushButton::clicked, this, &TestWindow::installPhpVersion);
        vbox->addWidget(btnInstall);

        vbox->addWidget(new QLabel("─────────────────"));

        // Switch version section
        QPushButton *btn = new QPushButton("🔍 Detect Installed Versions", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::detectPhpVersions);
        vbox->addWidget(btn);

        vbox->addWidget(new QLabel("Switch to Version:"));
        m_phpVersionCombo = new QComboBox(this);
        m_phpVersionCombo->addItem("Click detect first...");
        m_phpVersionCombo->setEnabled(false);
        vbox->addWidget(m_phpVersionCombo);

        btn = new QPushButton("🔄 Set as Default", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::switchPhp);
        vbox->addWidget(btn);

        layout->addWidget(group);
    }

    void addVersionManagerControls(QVBoxLayout *layout)
    {
        QGroupBox *group = new QGroupBox("🔀 Version Manager", this);
        QVBoxLayout *vbox = new QVBoxLayout(group);

        QPushButton *btn = new QPushButton("📋 Show Version Status", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::showVersionStatus);
        vbox->addWidget(btn);

        vbox->addWidget(new QLabel("─────────────────"));

        vbox->addWidget(new QLabel("Per-Site PHP Version:"));

        QHBoxLayout *hbox = new QHBoxLayout();
        m_siteIdInput = new QLineEdit(this);
        m_siteIdInput->setPlaceholderText("Site ID");
        hbox->addWidget(m_siteIdInput);

        m_siteVersionCombo = new QComboBox(this);
        m_siteVersionCombo->addItem("8.3");
        m_siteVersionCombo->addItem("8.4");
        hbox->addWidget(m_siteVersionCombo);

        vbox->addLayout(hbox);

        btn = new QPushButton("🔧 Set Site Version", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::setSiteVersion);
        vbox->addWidget(btn);

        btn = new QPushButton("📊 Show Sites Using Version", this);
        connect(btn, &QPushButton::clicked, this, &TestWindow::showSitesUsingVersion);
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
        appendOutput("║      Anvil PHP Development Environment     ║");
        appendOutput("╚════════════════════════════════════════════╝");
        appendOutput("");
        appendOutput("🚀 Lightweight PHP development environment");
        appendOutput("");
    }

private slots:
    void showVersionStatus()
    {
        appendOutput("\n" + QString("=").repeated(50));
        appendOutput("📋 VERSION MANAGER STATUS");
        appendOutput(QString("=").repeated(50));

        Managers::VersionManager versionManager;
        if (!versionManager.initialize())
        {
            appendOutput("❌ Failed to initialize VersionManager");
            return;
        }

        appendOutput("✅ VersionManager initialized");
        appendOutput("");

        // Global versions
        appendOutput("🌍 Global Versions:");
        appendOutput(QString("  PHP:  %1").arg(versionManager.globalPhpVersion()));
        appendOutput(QString("  Node: %1").arg(versionManager.globalNodeVersion()));
        appendOutput("");

        // Installed PHP versions
        auto phpVersions = versionManager.listInstalledPhpVersions();
        if (phpVersions.isSuccess() && !phpVersions.data.isEmpty())
        {
            appendOutput("📦 Installed PHP Versions:");
            for (const auto &ver : phpVersions.data)
            {
                QString marker = ver.isDefault() ? " ⭐" : "";
                appendOutput(QString("  • PHP %1%2").arg(ver.version(), marker));
            }
            appendOutput("");
        }

        // Installed Node versions
        auto nodeVersions = versionManager.listInstalledNodeVersions();
        if (nodeVersions.isSuccess() && !nodeVersions.data.isEmpty())
        {
            appendOutput("📦 Installed Node Versions:");
            for (const QString &ver : nodeVersions.data)
            {
                appendOutput(QString("  • Node %1").arg(ver));
            }
        }
    }
    void setSiteVersion()
    {
        QString siteId = m_siteIdInput->text();
        QString version = m_siteVersionCombo->currentText();

        if (siteId.isEmpty())
        {
            appendOutput("✗ Please enter a site ID");
            return;
        }

        appendOutput(QString("\n🔧 Setting site %1 to PHP %2").arg(siteId, version));

        Managers::VersionManager versionManager;
        versionManager.initialize();

        auto result = versionManager.setSitePhpVersion(siteId, version);
        if (result.isSuccess())
        {
            appendOutput(QString("✅ Site %1 now uses PHP %2").arg(siteId, version));
            m_siteIdInput->clear();
        }
        else
        {
            appendOutput(QString("❌ Failed: %1").arg(result.error));
        }
    }
    void showSitesUsingVersion()
    {
        QString version = m_siteVersionCombo->currentText();

        appendOutput(QString("\n📊 Sites using PHP %1:").arg(version));

        Managers::VersionManager versionManager;
        versionManager.initialize();

        QStringList sites = versionManager.getSitesUsingPhpVersion(version);

        if (sites.isEmpty())
        {
            appendOutput("  (no sites using this version)");
        }
        else
        {
            for (const QString &siteId : sites)
            {
                appendOutput(QString("  • %1").arg(siteId));
            }
        }
    }

    void startBackgroundInit()
    {
        appendOutput("Initializing (background)...");
        appendOutput("");

        // Create worker thread
        m_initThread = new QThread(this);
        m_worker = new InitWorker();
        m_worker->moveToThread(m_initThread);

        // Connect signals
        connect(m_initThread, &QThread::started, m_worker, &InitWorker::doWork);
        connect(m_worker, &InitWorker::progressUpdate, this, &TestWindow::onInitProgress);
        connect(m_worker, &InitWorker::servicesDetected, this, &TestWindow::onServicesDetected);
        connect(m_worker, &InitWorker::phpVersionsDetected, this, &TestWindow::onPhpVersionsDetected);
        connect(m_worker, &InitWorker::error, this, &TestWindow::onInitError);
        connect(m_worker, &InitWorker::finished, m_initThread, &QThread::quit);
        connect(m_worker, &InitWorker::finished, m_worker, &InitWorker::deleteLater);

        // Start the thread
        m_initThread->start();
    }

    void quickInstallStack()
    {
        appendOutput("\n" + QString("=").repeated(50));
        appendOutput("⚡ QUICK INSTALL - COMPLETE STACK");
        appendOutput(QString("=").repeated(50));
        appendOutput("");
        appendOutput("This will install:");
        appendOutput("  • PHP 8.3 (FPM + CLI + Extensions)");
        appendOutput("  • Nginx web server");
        appendOutput("  • MySQL database");
        appendOutput("");
        appendOutput("⚠️  Requires sudo password");
        appendOutput("⏱️  Takes ~5-10 minutes");
        appendOutput("");
        appendOutput("Starting installation...");

        auto *manager = Core::ServiceManager::instance();

        // 1. Install PHP
        appendOutput("\n📦 Installing PHP 8.3...");
        if (auto *php = manager ? manager->phpService() : nullptr)
        {
            auto result = php->installVersion("8.3");
            if (result.isSuccess())
            {
                appendOutput("✅ PHP 8.3 installed");
            }
            else
            {
                appendOutput(QString("⚠️  %1").arg(result.error));
            }
        }

        // 2. Install Nginx
        appendOutput("\n📦 Installing Nginx...");
        if (auto *nginx = manager ? manager->nginxService() : nullptr)
        {
            auto result = nginx->install();
            if (result.isSuccess())
            {
                appendOutput("✅ Nginx installed");
            }
            else
            {
                appendOutput(QString("⚠️  %1").arg(result.error));
            }
        }

        // 3. Install MySQL
        appendOutput("\n📦 Installing MySQL...");
        if (auto *db = manager ? manager->databaseService() : nullptr)
        {
            db->setDatabaseType(Services::DatabaseService::stringToDatabaseType("MySQL"));
            auto result = db->install();
            if (result.isSuccess())
            {
                appendOutput("✅ MySQL installed");
            }
            else
            {
                appendOutput(QString("⚠️  %1").arg(result.error));
            }
        }

        appendOutput("");
        appendOutput(QString("=").repeated(50));
        appendOutput("✅ INSTALLATION COMPLETE!");
        appendOutput(QString("=").repeated(50));
        appendOutput("");
        appendOutput("Click 'Detect Installations' to verify");

        // Auto-refresh after install
        QTimer::singleShot(1000, this, &TestWindow::detectInstallations);
    }

    void installPhpVersion()
    {
        QString versionText = m_phpInstallCombo->currentText();
        // Extract version number (e.g., "8.3 (Stable)" -> "8.3")
        QString version = versionText.split(' ').first();

        appendOutput("\n" + QString("=").repeated(50));
        appendOutput(QString("📥 INSTALLING PHP %1").arg(version));
        appendOutput(QString("=").repeated(50));
        appendOutput("");
        appendOutput("⚠️  This requires sudo password");
        appendOutput("⏱️  Installation takes 2-5 minutes");
        appendOutput("");

        auto *manager = Core::ServiceManager::instance();
        auto *php = manager ? manager->phpService() : nullptr;

        if (!php)
        {
            appendOutput("❌ PHP service not available");
            return;
        }

        appendOutput(QString("Installing PHP %1 with FPM and common extensions...").arg(version));

        auto result = php->installVersion(version);

        if (result.isSuccess())
        {
            appendOutput("");
            appendOutput(QString("✅ PHP %1 INSTALLED SUCCESSFULLY!").arg(version));
            appendOutput("");
            appendOutput("What was installed:");
            appendOutput(QString("  • php%1-fpm (FastCGI Process Manager)").arg(version));
            appendOutput(QString("  • php%1-cli (Command Line Interface)").arg(version));
            appendOutput(QString("  • php%1-common (Common files)").arg(version));
            appendOutput("");
            appendOutput("Next steps:");
            appendOutput("  1. Click 'Detect Installed Versions'");
            appendOutput("  2. Select your version and click 'Set as Default'");

            // Auto-refresh PHP versions
            QTimer::singleShot(1000, this, &TestWindow::detectPhpVersions);
        }
        else
        {
            appendOutput("");
            appendOutput("❌ INSTALLATION FAILED");
            appendOutput(QString("Error: %1").arg(result.error));
            appendOutput("");
            appendOutput("Troubleshooting:");
            appendOutput("  • Make sure you entered your sudo password");
            appendOutput("  • Check your internet connection");
            appendOutput("  • Run: sudo apt update");
        }
    }

    void onInitProgress(const QString &message)
    {
        appendOutput(message);
    }

    void onPhpVersionsDetected(const QList<Models::PHPVersion> &versions)
    {
        qDebug() << "Received" << versions.size() << "PHP versions";

        m_phpVersionCombo->clear();

        if (versions.isEmpty())
        {
            m_phpVersionCombo->addItem("No PHP versions found");
            m_phpVersionCombo->setEnabled(false);
            return;
        }

        m_phpVersionCombo->setEnabled(true);

        for (const Models::PHPVersion &v : versions)
        {
            QString displayText = QString("PHP %1").arg(v.version());
            if (v.isDefault())
            {
                displayText += " (current)";
            }
            m_phpVersionCombo->addItem(displayText, v.version());

            qDebug() << "Added version:" << v.version();
        }

        // Select the default/current version
        for (int i = 0; i < m_phpVersionCombo->count(); ++i)
        {
            if (m_phpVersionCombo->itemText(i).contains("(current)"))
            {
                m_phpVersionCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    void onServicesDetected(const QStringList &services, const QMap<QString, QString> &versions)
    {
        appendOutput("");
        appendOutput(QString("=").repeated(50));
        appendOutput("DETECTED SERVICES");
        appendOutput(QString("=").repeated(50));

        if (services.isEmpty())
        {
            appendOutput("✗ No services detected");
            appendOutput("");
            appendOutput("Install recommended stack:");
            appendOutput("  • PHP 8.3: sudo apt install php8.3-fpm php8.3-cli");
            appendOutput("  • Nginx: sudo apt install nginx");
            appendOutput("  • MySQL: sudo apt install mysql-server");
        }
        else
        {
            for (const QString &service : services)
            {
                QString version = versions.value(service, "unknown");
                appendOutput(QString("✓ %1 %2").arg(service).arg(version));
            }
            appendOutput("");
            appendOutput("✅ System ready! Use the controls on the left.");
        }
    }

    void onInitError(const QString &message)
    {
        appendOutput(QString("❌ %1").arg(message));
    }

    void detectInstallations()
    {
        appendOutput("\n" + QString("-").repeated(50));
        appendOutput("MANUAL DETECTION");
        appendOutput(QString("-").repeated(50));

        // Quick check (this should be fast)
        auto *manager = Core::ServiceManager::instance();

        int detected = 0;
        if (auto *php = manager ? manager->phpService() : nullptr; php && php->isInstalled())
        {
            appendOutput("✓ PHP installed");
            detected++;
        }
        if (auto *nginx = manager ? manager->nginxService() : nullptr; nginx && nginx->isInstalled())
        {
            appendOutput("✓ Nginx installed");
            detected++;
        }
        if (auto *db = manager ? manager->databaseService() : nullptr; db && db->isInstalled())
        {
            appendOutput(QString("✓ %1 installed").arg(db->databaseTypeString()));
            detected++;
        }

        if (detected == 0)
        {
            appendOutput("✗ No services found");
        }
    }

    void detectPhpVersions()
    {
        appendOutput("\n📋 DETECTING PHP VERSIONS");

        auto *manager = Core::ServiceManager::instance();
        auto *php = manager ? manager->phpService() : nullptr;

        if (!php || !php->isInstalled())
        {
            appendOutput("✗ PHP not installed");
            m_phpVersionCombo->clear();
            m_phpVersionCombo->addItem("No PHP installed");
            m_phpVersionCombo->setEnabled(false);
            return;
        }

        auto installed = php->listInstalledVersions();
        if (installed.isSuccess() && !installed.data.isEmpty())
        {
            m_phpVersionCombo->clear();
            m_phpVersionCombo->setEnabled(true);

            QString currentVersion = php->currentVersion();
            appendOutput(QString("Current: PHP %1").arg(currentVersion));
            appendOutput("");

            for (const auto &v : installed.data)
            {
                QString marker = v.isDefault() ? " (current)" : "";
                appendOutput(QString("  • PHP %1%2").arg(v.version(), marker));

                QString displayText = QString("PHP %1").arg(v.version());
                if (v.isDefault())
                {
                    displayText += " (current)";
                }
                m_phpVersionCombo->addItem(displayText, v.version());
            }

            appendOutput("");
            appendOutput("Use dropdown above to switch versions");
        }
        else
        {
            appendOutput("✗ No PHP versions detected");
            m_phpVersionCombo->clear();
            m_phpVersionCombo->addItem("No versions found");
            m_phpVersionCombo->setEnabled(false);
        }
    }

    void switchPhp()
    {
        // Get the actual version from the item data (not the display text)
        QString version = m_phpVersionCombo->currentData().toString();
        if (version.isEmpty() || version == "No PHP versions found")
        {
            appendOutput("✗ No PHP version selected");
            return;
        }

        appendOutput(QString("\n🔄 Switching to PHP %1...").arg(version));

        auto *manager = Core::ServiceManager::instance();
        auto *php = manager ? manager->phpService() : nullptr;
        if (!php)
            return;

        auto result = php->switchVersion(version);
        if (result.isSuccess())
        {
            appendOutput(QString("✓ PHP %1 activated").arg(version));

            // Refresh the dropdown to update the "(current)" marker
            QTimer::singleShot(500, this, &TestWindow::detectPhpVersions);
        }
        else
        {
            appendOutput(QString("✗ %1").arg(result.error));
        }
    }

    void testNginxConfig()
    {
        appendOutput("\n🔍 Testing Nginx config...");

        auto *manager = Core::ServiceManager::instance();
        auto *nginx = manager ? manager->nginxService() : nullptr;
        if (!nginx || !nginx->isInstalled())
        {
            appendOutput("✗ Nginx not installed");
            return;
        }

        auto result = nginx->testConfig();
        appendOutput(result.isSuccess() ? "✓ Config valid" : QString("✗ %1").arg(result.error));
    }

    void reloadNginx()
    {
        appendOutput("\n🔄 Reloading Nginx...");

        auto *manager = Core::ServiceManager::instance();
        auto *nginx = manager ? manager->nginxService() : nullptr;
        if (!nginx)
            return;

        auto result = nginx->reload();
        appendOutput(result.isSuccess() ? "✓ Reloaded" : QString("✗ %1").arg(result.error));
    }

    void checkStatus()
    {
        appendOutput("\n" + QString("-").repeated(50));
        appendOutput("SERVICE STATUS");
        appendOutput(QString("-").repeated(50));

        auto *manager = Core::ServiceManager::instance();
        if (!manager)
        {
            appendOutput("✗ Service manager not available");
            return;
        }
        for (const QString &name : manager->availableServices())
        {
            auto status = manager->serviceStatus(name);
            QString icon = status.isRunning() ? "🟢" : "🔴";
            appendOutput(QString("%1 %2: %3").arg(icon, name, status.statusString()));
        }
    }

    void createDatabase()
    {
        QString name = m_dbNameInput->text();
        if (name.isEmpty())
            return;

        appendOutput(QString("\n🗄️  Creating: %1").arg(name));

        auto *manager = Core::ServiceManager::instance();
        auto *db = manager ? manager->databaseService() : nullptr;
        if (!db)
            return;

        auto result = db->createDatabase(name);
        if (result.isSuccess())
        {
            appendOutput("✓ Created");
            m_dbNameInput->clear();
        }
        else
        {
            appendOutput(QString("✗ %1").arg(result.error));
        }
    }

    void addDnsEntry()
    {
        QString domain = m_domainInput->text();
        if (domain.isEmpty())
            return;

        appendOutput(QString("\n🌍 Adding: %1 → 127.0.0.1").arg(domain));

        auto *manager = Core::ServiceManager::instance();
        auto *dns = manager ? manager->dnsService() : nullptr;
        if (!dns)
            return;

        auto result = dns->addEntry(domain);
        if (result.isSuccess())
        {
            appendOutput("✓ Added");
            m_domainInput->clear();
        }
        else
        {
            appendOutput(QString("✗ %1").arg(result.error));
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

        appendOutput(QString("\n🚀 Creating: %1").arg(name));

        auto *manager = Core::ServiceManager::instance();
        if (!manager)
        {
            appendOutput("✗ Service manager not available");
            return;
        }

        // DNS
        if (auto *dns = manager->dnsService())
        {
            dns->addEntry(domain);
            appendOutput("✓ DNS configured");
        }

        // Database
        QString dbName = domain;
        dbName.replace('.', '_');
        if (auto *db = manager->databaseService(); db && db->isInstalled())
        {
            db->createDatabase(dbName);
            appendOutput(QString("✓ Database: %1").arg(dbName));
        }

        // Nginx
        Models::Site site(name, path, domain);
        site.setPhpVersion(m_phpVersionCombo->currentText());

        if (auto *nginx = manager->nginxService(); nginx && nginx->isInstalled())
        {
            auto result = nginx->addSite(site);
            if (result.isSuccess())
            {
                appendOutput("✓ Nginx configured");
                appendOutput(QString("🌐 http://%1").arg(domain));
            }
        }

        m_siteNameInput->clear();
        m_sitePathInput->clear();
        m_siteDomainInput->clear();
    }

    void clearOutput()
    {
        m_output->clear();
        showWelcome();
        startBackgroundInit();
    }

private:
    void appendOutput(const QString &text)
    {
        m_output->append(text);
        m_output->ensureCursorVisible();
        QApplication::processEvents(); // Allow GUI to update
    }

    QTextEdit *m_output;
    QComboBox *m_phpInstallCombo; // For installing new versions
    QComboBox *m_phpVersionCombo; // For switching between installed versions
    QLineEdit *m_domainInput;
    QLineEdit *m_siteNameInput;
    QLineEdit *m_sitePathInput;
    QLineEdit *m_siteDomainInput;
    QLineEdit *m_dbNameInput;
    QLineEdit *m_siteIdInput;
    QComboBox *m_siteVersionCombo;

    QThread *m_initThread;
    InitWorker *m_worker;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setOrganizationName("Anvil");
    app.setApplicationName("Anvil");

    Q_INIT_RESOURCE(resources);

    Utils::Logger::instance().setConsoleOutput(true);
    Utils::Logger::instance().setLogLevel(Utils::LogLevel::Info);

    LOG_INFO("Anvil - Laravel Herd for Linux");

    Anvil::UI::MainWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
