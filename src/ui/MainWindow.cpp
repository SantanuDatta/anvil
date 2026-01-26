#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "core/ConfigManager.h"
#include "services/DatabaseService.h"
#include "services/NginxService.h"
#include "services/NodeService.h"
#include "services/PHPService.h"
#include "utils/Logger.h"
#include <QApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QFileDialog>
#include <QDir>

namespace Anvil::UI
{
    MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent),
          m_siteManager(new Managers::SiteManager(this)),
          m_versionManager(new Managers::VersionManager(this)),
          m_updateTimer(new QTimer(this))
    {

        setWindowTitle("Anvil - Laravel Development Environment");
        setMinimumSize(1200, 800);

        QScreen *screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        move((screenGeometry.width() - width()) / 2, (screenGeometry.height() - height()) / 2);

        if (!m_siteManager->initialize())
        {
            showError("Initialization Error", "Failed to initialize SiteManager.");
            LOG_ERROR("Failed to initialize SiteManager");
        }

        if (!m_versionManager->initialize())
        {
            showError("Initialization Error", "Failed to initialize VersionManager.");
            LOG_ERROR("Failed to initialize VersionManager");
        }

        setupUi();
        connectSignals();
        loadInitialData();
        Theme::instance().apply();

        m_updateTimer->setInterval(UPDATE_INTERVAL);
        connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::onServiceStatusUpdate);
        m_updateTimer->start();

        LOG_INFO("MainWindow initialized");
    }

    MainWindow::~MainWindow()
    {
        m_updateTimer->stop();
    }

    void MainWindow::setupUi()
    {
        m_centralWidget = new QWidget(this);
        m_mainLayout = new QHBoxLayout(m_centralWidget);
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setSpacing(0);

        setupNavigationSidebar();
        m_contentStack = new QStackedWidget();
        m_mainLayout->addWidget(m_contentStack);

        setupDashboardPage();
        setupGeneralPage();
        setupSitesPage();
        setupPhpPage();
        setupNodePage();
        setupSettingsPage();

        setCentralWidget(m_centralWidget);
    }

    void MainWindow::setupNavigationSidebar()
    {
        m_sidebar = new QListWidget();
        m_sidebar->setObjectName("sidebar");
        m_sidebar->setFixedWidth(SIDEBAR_WIDTH);
        m_sidebar->setFrameShape(QFrame::NoFrame);
        m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_sidebar->setSpacing(4);

        const QStringList items = {"Dashboard", "General", "Sites", "PHP", "Node", "Settings"};
        for (const QString &item : items)
            m_sidebar->addItem(item);

        m_sidebar->setCurrentRow(0);
        connect(m_sidebar, &QListWidget::currentRowChanged, this, &MainWindow::onNavigationChanged);
        m_mainLayout->addWidget(m_sidebar);
    }

    void MainWindow::setupDashboardPage()
    {
        m_dashboardPage = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(m_dashboardPage);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(24);

        QLabel *title = new QLabel("Dashboard");
        title->setProperty("class", "heading");
        layout->addWidget(title);

        m_servicesCard = createCard();
        m_servicesLayout = qobject_cast<QVBoxLayout *>(m_servicesCard->layout());

        QHBoxLayout *servicesHeader = new QHBoxLayout();
        QLabel *servicesTitle = new QLabel("Active Services");
        servicesTitle->setProperty("class", "subheading");
        servicesHeader->addWidget(servicesTitle);
        servicesHeader->addStretch();

        m_stopAllServicesBtn = createButton("Stop All Services", "destructive");
        servicesHeader->addWidget(m_stopAllServicesBtn);
        m_servicesLayout->addLayout(servicesHeader);

        QWidget *indicatorsContainer = new QWidget();
        QVBoxLayout *indicatorsLayout = new QVBoxLayout(indicatorsContainer);
        indicatorsLayout->setSpacing(6);
        indicatorsLayout->setContentsMargins(0, 12, 0, 0);

        const QStringList services = {"nginx", "php", "mysql", "node"};
        for (const QString &service : services)
        {
            QWidget *indicator = createServiceIndicator(service, false);
            m_serviceIndicators[service] = indicator;
            indicatorsLayout->addWidget(indicator);
        }

        m_servicesLayout->addWidget(indicatorsContainer);
        layout->addWidget(m_servicesCard);

        m_phpVersionCard = createCard();
        QVBoxLayout *phpLayout = qobject_cast<QVBoxLayout *>(m_phpVersionCard->layout());

        QHBoxLayout *phpHeader = new QHBoxLayout();
        m_phpVersionLabel = new QLabel("Global PHP Version");
        m_phpVersionLabel->setProperty("class", "subheading");
        phpHeader->addWidget(m_phpVersionLabel);
        phpHeader->addStretch();

        m_phpVersionCombo = new QComboBox();
        m_phpVersionCombo->setMinimumWidth(150);
        phpHeader->addWidget(m_phpVersionCombo);
        phpLayout->addLayout(phpHeader);

        QLabel *phpDesc = new QLabel("This version will be used for all sites unless overridden");
        phpDesc->setProperty("class", "muted");
        phpLayout->addWidget(phpDesc);

        layout->addWidget(m_phpVersionCard);
        layout->addStretch();
        m_contentStack->addWidget(m_dashboardPage);
    }

    void MainWindow::setupGeneralPage()
    {
        m_generalPage = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(m_generalPage);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(24);

        QHBoxLayout *header = new QHBoxLayout();
        QLabel *title = new QLabel("General Settings");
        title->setProperty("class", "heading");
        header->addWidget(title);
        header->addStretch();

        m_addSiteBtn = createButton("+ Add Path", "primary");
        header->addWidget(m_addPathBtn);
        layout->addLayout(header);

        QFrame *pathsCard = createCard();
        QVBoxLayout *pathsLayout = qobject_cast<QVBoxLayout *>(pathsCard->layout());

        QHBoxLayout *pathsHeader = new QHBoxLayout();
        QLabel *pathsTitle = new QLabel("Paths");
        pathsTitle->setProperty("class", "subheading");
        pathsHeader->addWidget(pathsTitle);
        pathsHeader->addStretch();
        m_addPathBtn = createButton("+ Add Path", "secondary");
        pathsHeader->addWidget(m_addPathBtn);
        pathsLayout->addLayout(pathsHeader);

        Core::ConfigManager &config = Core::ConfigManager::instance();
        m_pathsList = new QListWidget();
        m_pathsList->setSelectionMode(QAbstractItemView::NoSelection);
        pathsLayout->addWidget(m_pathsList);

        layout->addWidget(pathsCard);
        layout->addStretch();
        m_contentStack->addWidget(m_generalPage);
    }

    void MainWindow::setupSitesPage()
    {
        m_sitesPage = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(m_sitesPage);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(24);

        QHBoxLayout *header = new QHBoxLayout();
        QLabel *title = new QLabel("Sites");
        title->setProperty("class", "heading");
        header->addWidget(title);
        header->addStretch();

        m_addSiteBtn = createButton("+ Add Site", "primary");
        header->addWidget(m_addSiteBtn);
        layout->addLayout(header);

        QFrame *sitesCard = createCard();
        QVBoxLayout *sitesLayout = qobject_cast<QVBoxLayout *>(sitesCard->layout());

        m_sitesTable = new QTableWidget(0, 4);
        m_sitesTable->setHorizontalHeaderLabels({"Name", "Domain", "Path", "PHP"});
        m_sitesTable->horizontalHeader()->setStretchLastSection(true);
        m_sitesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_sitesTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_sitesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        sitesLayout->addWidget(m_sitesTable);

        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        m_removeSiteBtn = createButton("Remove Site", "destructive");
        m_removeSiteBtn->setEnabled(false);
        btnLayout->addWidget(m_removeSiteBtn);
        sitesLayout->addLayout(btnLayout);

        connect(m_sitesTable, &QTableWidget::itemSelectionChanged, this, [this]()
                { m_removeSiteBtn->setEnabled(m_sitesTable->currentRow() >= 0); });

        layout->addWidget(sitesCard);
        m_contentStack->addWidget(m_sitesPage);
    }

    void MainWindow::setupPhpPage()
    {
        m_phpPage = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(m_phpPage);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(24);

        QLabel *title = new QLabel("PHP Versions");
        title->setProperty("class", "heading");
        layout->addWidget(title);

        QFrame *currentCard = createCard();
        QVBoxLayout *currentLayout = qobject_cast<QVBoxLayout *>(currentCard->layout());
        QLabel *currentTitle = new QLabel("Current Version");
        currentTitle->setProperty("class", "subheading");
        currentLayout->addWidget(currentTitle);
        m_phpCurrentVersion = new QLabel("Loading...");
        currentLayout->addWidget(m_phpCurrentVersion);
        layout->addWidget(currentCard);

        QFrame *installCard = createCard();
        QVBoxLayout *installLayout = qobject_cast<QVBoxLayout *>(installCard->layout());
        QLabel *installTitle = new QLabel("Install PHP Version");
        installTitle->setProperty("class", "subheading");
        installLayout->addWidget(installTitle);

        QHBoxLayout *installRow = new QHBoxLayout();
        m_phpInstallCombo = new QComboBox();
        m_phpInstallCombo->addItems({"8.4", "8.3", "8.2", "8.1", "8.0", "7.4"});
        m_phpInstallCombo->setCurrentIndex(1);
        installRow->addWidget(m_phpInstallCombo);
        m_installPhpBtn = createButton("Install", "primary");
        installRow->addWidget(m_installPhpBtn);
        installLayout->addLayout(installRow);
        layout->addWidget(installCard);

        QFrame *switchCard = createCard();
        QVBoxLayout *switchLayout = qobject_cast<QVBoxLayout *>(switchCard->layout());
        QLabel *switchTitle = new QLabel("Switch Version");
        switchTitle->setProperty("class", "subheading");
        switchLayout->addWidget(switchTitle);

        QHBoxLayout *switchRow = new QHBoxLayout();
        m_phpSwitchCombo = new QComboBox();
        switchRow->addWidget(m_phpSwitchCombo);
        m_switchPhpBtn = createButton("Switch", "secondary");
        switchRow->addWidget(m_switchPhpBtn);
        switchLayout->addLayout(switchRow);
        layout->addWidget(switchCard);

        layout->addStretch();
        m_contentStack->addWidget(m_phpPage);
    }

    void MainWindow::setupNodePage()
    {
        m_nodePage = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(m_nodePage);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(18);

        QLabel *title = new QLabel("Node.js Versions");
        title->setProperty("class", "heading");
        layout->addWidget(title);

        QFrame *card = createCard();
        QVBoxLayout *cardLayout = qobject_cast<QVBoxLayout *>(card->layout());
        QLabel *placeholder = new QLabel("Node.js management coming soon");
        placeholder->setProperty("class", "muted");
        placeholder->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(placeholder);

        layout->addWidget(card);
        layout->addStretch();
        m_contentStack->addWidget(m_nodePage);
    }

    void MainWindow::setupSettingsPage()
    {
        m_settingsPage = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(m_settingsPage);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(18);

        QLabel *title = new QLabel("Settings");
        title->setProperty("class", "heading");
        layout->addWidget(title);

        QFrame *themeCard = createCard();
        QVBoxLayout *themeLayout = qobject_cast<QVBoxLayout *>(themeCard->layout());
        QLabel *themeLabel = new QLabel("Theme");
        themeLabel->setProperty("class", "subheading");
        themeLayout->addWidget(themeLabel);

        QLabel *themeDesc = new QLabel("Choose light, dark, or auto (system) theme");
        themeDesc->setProperty("class", "muted");
        themeLayout->addWidget(themeDesc);

        m_themeCombo = new QComboBox();
        m_themeCombo->addItems({"Light", "Dark", "Auto"});
        m_themeCombo->setCurrentIndex(static_cast<int>(Theme::instance().mode()));
        themeLayout->addWidget(m_themeCombo);

        layout->addWidget(themeCard);
        layout->addStretch();
        m_contentStack->addWidget(m_settingsPage);
    }

    QFrame *MainWindow::createCard(const QString &title)
    {
        QFrame *card = new QFrame();
        card->setProperty("class", "card");
        card->setFrameShape(QFrame::StyledPanel);

        QVBoxLayout *layout = new QVBoxLayout(card);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(12);

        if (!title.isEmpty())
        {
            QLabel *cardTitle = new QLabel(title);
            cardTitle->setProperty("class", "subheading");
            layout->addWidget(cardTitle);
        }

        return card;
    }

    QWidget *MainWindow::createServiceIndicator(const QString &serviceName, bool isRunning)
    {
        QWidget *indicator = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(indicator);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);

        QLabel *dot = new QLabel("●");
        dot->setObjectName("status_dot_" + serviceName);
        dot->setProperty("class", isRunning ? "status-running" : "status-stopped");
        dot->setFixedWidth(20);
        layout->addWidget(dot);

        QLabel *nameLabel = new QLabel(serviceName);
        nameLabel->setObjectName("status_name_" + serviceName);
        layout->addWidget(nameLabel);
        layout->addStretch();

        QLabel *statusLabel = new QLabel(isRunning ? "Running" : "Stopped");
        statusLabel->setObjectName("status_text_" + serviceName);
        statusLabel->setProperty("class", "muted");
        layout->addWidget(statusLabel);

        return indicator;
    }

    QPushButton *MainWindow::createButton(const QString &text, const QString &styleClass)
    {
        QPushButton *btn = new QPushButton(text);
        btn->setProperty("class", styleClass);
        return btn;
    }

    void MainWindow::connectSignals()
    {
        connect(m_stopAllServicesBtn, &QPushButton::clicked, this, &MainWindow::onStopAllServices);
        connect(m_phpVersionCombo, &QComboBox::currentTextChanged, this, &MainWindow::onPhpVersionChanged);
        connect(m_addSiteBtn, &QPushButton::clicked, this, &MainWindow::onAddSiteClicked);
        connect(m_removeSiteBtn, &QPushButton::clicked, this, &MainWindow::onRemoveSiteClicked);
        connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onThemeModeChanged);
        connect(m_installPhpBtn, &QPushButton::clicked, this, &MainWindow::onInstallPhpVersion);
        connect(m_switchPhpBtn, &QPushButton::clicked, this, &MainWindow::onSwitchPhpVersion);
        connect(m_addPathBtn, &QPushButton::clicked, this, &MainWindow::onAddPathClicked);
        connect(m_siteManager, &Managers::SiteManager::siteCreated, this, &MainWindow::onSiteCreated);
        connect(m_siteManager, &Managers::SiteManager::siteRemoved, this, &MainWindow::onSiteRemoved);
        connect(m_versionManager, &Managers::VersionManager::globalPhpVersionChanged, this, &MainWindow::onGlobalPhpVersionChanged);
        connect(&Theme::instance(), &Theme::themeChanged, this, &MainWindow::onThemeChanged);
    }

    void MainWindow::loadInitialData()
    {
        updateServiceIndicators();
        updatePhpVersionCombo();
        updateSitesTable();
        updateParkedPaths();

        QString currentPhp = m_versionManager->globalPhpVersion();
        m_phpCurrentVersion->setText(QString("PHP %1").arg(currentPhp));
    }

    void MainWindow::updateServiceIndicators()
    {
        auto *manager = Core::ServiceManager::instance();
        if (!manager)
        {
            LOG_ERROR("ServiceManager not available for service status update");
            return;
        }

        for (auto it = m_serviceIndicators.begin(); it != m_serviceIndicators.end(); ++it)
        {
            QString serviceName = it.key();
            QWidget *indicator = it.value();
            bool isRunning = false;

            if (serviceName == "nginx")
            {
                auto *nginx = manager->nginxService();
                isRunning = nginx && nginx->isRunning();
            }
            else if (serviceName == "php")
            {
                auto *php = manager->phpService();
                isRunning = php && php->isRunning();
            }
            else if (serviceName == "mysql")
            {
                auto *db = manager->databaseService();
                isRunning = db && db->isRunning();
            }
            else if (serviceName == "node")
            {
                auto *node = manager->nodeService();
                isRunning = node && node->isRunning();
            }

            QLabel *dot = indicator->findChild<QLabel *>("status_dot_" + serviceName);
            QLabel *text = indicator->findChild<QLabel *>("status_text_" + serviceName);

            if (dot)
            {
                dot->setProperty("class", isRunning ? "status-running" : "status-stopped");
                dot->style()->unpolish(dot);
                dot->style()->polish(dot);
            }

            if (text)
                text->setText(isRunning ? "Running" : "Stopped");
        }
    }

    void MainWindow::updatePhpVersionCombo()
    {
        m_phpVersionCombo->clear();
        m_phpSwitchCombo->clear();

        auto result = m_versionManager->listInstalledPhpVersions();
        if (result.isSuccess() && !result.data.isEmpty())
        {
            QString currentVersion = m_versionManager->globalPhpVersion();

            for (const auto &phpVer : result.data)
            {
                QString display = QString("PHP %1").arg(phpVer.version());
                m_phpVersionCombo->addItem(display, phpVer.version());
                m_phpSwitchCombo->addItem(display, phpVer.version());

                if (phpVer.version() == currentVersion)
                {
                    m_phpVersionCombo->setCurrentIndex(m_phpVersionCombo->count() - 1);
                    m_phpSwitchCombo->setCurrentIndex(m_phpSwitchCombo->count() - 1);
                }
            }
        }
        else
        {
            m_phpVersionCombo->addItem("No PHP versions installed");
            m_phpSwitchCombo->addItem("No PHP versions installed");
        }
    }

    void MainWindow::updateSitesTable()
    {
        m_sitesTable->setRowCount(0);

        auto result = m_siteManager->listSites();
        if (result.isSuccess())
        {
            for (const auto &site : result.data)
            {
                int row = m_sitesTable->rowCount();
                m_sitesTable->insertRow(row);
                m_sitesTable->setItem(row, 0, new QTableWidgetItem(site.name()));
                m_sitesTable->setItem(row, 1, new QTableWidgetItem(site.domain()));
                m_sitesTable->setItem(row, 2, new QTableWidgetItem(site.path()));
                m_sitesTable->setItem(row, 3, new QTableWidgetItem(site.phpVersion()));
            }
        }
    }

    void MainWindow::updateParkedPaths()
    {
        m_pathsList->clear();

        auto result = m_siteManager->listParkedDirectories();
        if (result.isSuccess())
        {
            for (const auto &path : result.data)
            {
                m_pathsList->addItem(path);
            }
        }

        if (m_pathsList->count() == 0)
        {
            auto *item = new QListWidgetItem("No project paths added yet");
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            m_pathsList->addItem(item);
        }
    }

    void MainWindow::showError(const QString &title, const QString &message)
    {
        QMessageBox::critical(this, title, message);
    }

    void MainWindow::showSuccess(const QString &title, const QString &message)
    {
        QMessageBox::information(this, title, message);
    }

    void MainWindow::onNavigationChanged(int index)
    {
        if (index >= 0 && index < m_contentStack->count())
        {
            m_contentStack->setCurrentIndex(index);
            LOG_DEBUG(QString("Navigated to page: %1").arg(index));
        }
    }

    void MainWindow::onStopAllServices()
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Stop All Services",
            "Are you sure you want to stop all services?\nThis will stop Nginx, PHP-FPM, and database services.",
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        auto *manager = Core::ServiceManager::instance();
        if (!manager)
        {
            showError("Service Manager Error", "Service manager is not available.");
            LOG_ERROR("ServiceManager not available when stopping services");
            return;
        }

        if (auto *nginx = manager->nginxService())
            nginx->stop();
        if (auto *php = manager->phpService())
            php->stop();
        if (auto *db = manager->databaseService())
            db->stop();

        LOG_INFO("All services stopped by user");
        QTimer::singleShot(1000, this, &MainWindow::updateServiceIndicators);
    }

    void MainWindow::onPhpVersionChanged(const QString &version)
    {
        Q_UNUSED(version);
        QString versionNum = m_phpVersionCombo->currentData().toString();
        if (versionNum.isEmpty() || versionNum == m_versionManager->globalPhpVersion())
            return;

        LOG_INFO(QString("Switching global PHP version to: %1").arg(versionNum));

        auto result = m_versionManager->switchGlobalPhpVersion(versionNum);
        if (result.isSuccess())
        {
            showSuccess("PHP Version Changed", QString("Global PHP version changed to %1\nPHP-FPM has been restarted.").arg(versionNum));
            m_phpCurrentVersion->setText(QString("PHP %1").arg(versionNum));
            LOG_INFO(QString("PHP version switched to: %1").arg(versionNum));
        }
        else
        {
            showError("Error", QString("Failed to switch PHP version:\n%1").arg(result.error));
            LOG_ERROR(QString("Failed to switch PHP version: %1").arg(result.error));
            updatePhpVersionCombo();
        }
    }

    void MainWindow::onServiceStatusUpdate()
    {
        updateServiceIndicators();
    }

    void MainWindow::onSiteCreated(const QString &siteId)
    {
        LOG_INFO(QString("Site created: %1").arg(siteId));
        updateSitesTable();
    }

    void MainWindow::onSiteRemoved(const QString &siteId)
    {
        LOG_INFO(QString("Site removed: %1").arg(siteId));
        updateSitesTable();
    }

    void MainWindow::onGlobalPhpVersionChanged(const QString &version)
    {
        LOG_INFO(QString("Global PHP version changed to: %1").arg(version));
        updatePhpVersionCombo();
        m_phpCurrentVersion->setText(QString("PHP %1").arg(version));
    }

    void MainWindow::onThemeChanged()
    {
        LOG_DEBUG("Theme changed, reapplying styles");
        Theme::instance().apply();
    }

    void MainWindow::onAddSiteClicked()
    {
        bool ok;
        QString name = QInputDialog::getText(this, "Add Site", "Site name:", QLineEdit::Normal, "", &ok);
        if (!ok || name.isEmpty())
            return;

        QString domain = QInputDialog::getText(this, "Add Site", "Domain:", QLineEdit::Normal, name + ".test", &ok);
        if (!ok || domain.isEmpty())
            return;

        QString path = QInputDialog::getText(this, "Add Site", "Path:", QLineEdit::Normal, "/home/user/" + name, &ok);
        if (!ok || path.isEmpty())
            return;

        Models::Site site(name, path, domain);
        site.setPhpVersion(m_versionManager->globalPhpVersion());

        auto result = m_siteManager->createSite(site);
        if (result.isSuccess())
        {
            showSuccess("Site Created", QString("Site '%1' created successfully").arg(name));
        }
        else
        {
            showError("Error", QString("Failed to create site:\n%1").arg(result.error));
        }
    }

    void MainWindow::onRemoveSiteClicked()
    {
        int row = m_sitesTable->currentRow();
        if (row < 0)
            return;

        QString domain = m_sitesTable->item(row, 1)->text();

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Remove Site",
            QString("Are you sure you want to remove site '%1'?").arg(domain),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        auto sites = m_siteManager->listSites();
        if (sites.isSuccess() && row < sites.data.size())
        {
            QString siteId = sites.data[row].id();
            auto result = m_siteManager->removeSite(siteId);
            if (result.isSuccess())
            {
                showSuccess("Site Removed", "Site removed successfully");
            }
            else
            {
                showError("Error", QString("Failed to remove site:\n%1").arg(result.error));
            }
        }
    }

    void MainWindow::onAddPathClicked()
    {
        QString selectedPath = QFileDialog::getExistingDirectory(
            this, "Select Project Directory", QDir::homePath(), QFileDialog::ShowDirsOnly);
        if (selectedPath.isEmpty())
            return;

        auto result = m_siteManager->parkDirectory(selectedPath);
        if (result.isSuccess())
        {
            showSuccess("Path Added", QString("Project path added: %1").arg(selectedPath));
            updateParkedPaths();
            updateSitesTable();
        }
        else
        {
            showError("Error", QString("Failed to add path:\n%1").arg(result.error));
        }
    }

    void MainWindow::onThemeModeChanged(int index)
    {
        Theme::instance().setMode(static_cast<ThemeMode>(index));
    }

    void MainWindow::onInstallPhpVersion()
    {
        QString version = m_phpInstallCombo->currentText();

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Install PHP",
            QString("Install PHP %1? This may take several minutes.").arg(version),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        LOG_INFO(QString("Installing PHP %1").arg(version));
        auto result = m_versionManager->installPhpVersion(version);

        if (result.isSuccess())
        {
            showSuccess("PHP Installed", QString("PHP %1 installed successfully").arg(version));
            updatePhpVersionCombo();
        }
        else
        {
            showError("Error", QString("Failed to install PHP:\n%1").arg(result.error));
        }
    }
    void MainWindow::onSwitchPhpVersion()
    {
        QString versionNum = m_phpSwitchCombo->currentData().toString();
        if (versionNum.isEmpty() || versionNum == m_versionManager->globalPhpVersion())
            return;
        auto result = m_versionManager->switchGlobalPhpVersion(versionNum);
        if (result.isSuccess())
        {
            showSuccess("PHP Version Switched", QString("Now using PHP %1").arg(versionNum));
            m_phpCurrentVersion->setText(QString("PHP %1").arg(versionNum));
        }
        else
        {
            showError("Error", QString("Failed to switch PHP:\n%1").arg(result.error));
        }
    }
    void MainWindow::closeEvent(QCloseEvent *event)
    {
        if (m_siteManager)
            m_siteManager->save();
        if (m_versionManager)
            m_versionManager->save();
        LOG_INFO("MainWindow closing");
        event->accept();
    }
}
