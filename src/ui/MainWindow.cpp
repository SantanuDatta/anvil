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
#include <QToolButton>
#include <QDialog>
#include <QSize>

namespace Anvil::UI
{
    namespace
    {
        const QStringList kServiceNames = {"nginx", "php", "mysql", "node"};
    }

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

        for (const QString &service : kServiceNames)
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

        QLabel *phpDesc = new QLabel("This version will be used for all sites unless overridden. <a href=\"php-tab\" style=\"color: #2563eb; text-decoration: none; font-weight: 600;\">Check PHP versions</a>");
        phpDesc->setProperty("class", "muted");
        phpDesc->setTextFormat(Qt::RichText);
        phpDesc->setTextInteractionFlags(Qt::TextBrowserInteraction);
        phpDesc->setOpenExternalLinks(false);
        connect(phpDesc, &QLabel::linkActivated, this, [this](const QString &)
                {
                    m_sidebar->setCurrentRow(3);
                    m_contentStack->setCurrentWidget(m_phpPage);
                });
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
        m_addPathBtn = createButton("+ Add Path", "primary");
        header->addWidget(m_addPathBtn);
        layout->addLayout(header);

        QFrame *pathsCard = createCard();
        QVBoxLayout *pathsLayout = qobject_cast<QVBoxLayout *>(pathsCard->layout());

        QHBoxLayout *pathsHeader = new QHBoxLayout();
        QLabel *pathsTitle = new QLabel("Paths");
        pathsTitle->setProperty("class", "subheading");
        pathsHeader->addWidget(pathsTitle);
        pathsHeader->addStretch();

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

        m_sitesTable = new QTableWidget(0, 5);
        m_sitesTable->setHorizontalHeaderLabels({"Name", "Domain", "Path", "PHP", "Actions"});
        m_sitesTable->horizontalHeader()->setStretchLastSection(false);
        m_sitesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_sitesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_sitesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        m_sitesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_sitesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        m_sitesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_sitesTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_sitesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        layout->addWidget(m_sitesTable);

        m_sitesPaginationContainer = new QWidget();
        m_sitesPaginationLayout = new QHBoxLayout(m_sitesPaginationContainer);
        m_sitesPaginationLayout->setContentsMargins(0, 0, 0, 0);
        m_sitesPaginationLayout->setSpacing(8);
        m_sitesPaginationLayout->addStretch();

        m_sitesPrevBtn = new QToolButton();
        m_sitesPrevBtn->setIconSize(QSize(16, 16));
        m_sitesPrevBtn->setToolTip("Previous page");
        m_sitesPrevBtn->setAutoRaise(true);
        m_sitesPrevBtn->setCursor(Qt::PointingHandCursor);
        m_sitesPrevBtn->setProperty("class", "secondary");
        m_sitesPaginationLayout->addWidget(m_sitesPrevBtn);

        m_sitesPageButtonsContainer = new QWidget();
        m_sitesPageButtonsLayout = new QHBoxLayout(m_sitesPageButtonsContainer);
        m_sitesPageButtonsLayout->setContentsMargins(0, 0, 0, 0);
        m_sitesPageButtonsLayout->setSpacing(6);
        m_sitesPaginationLayout->addWidget(m_sitesPageButtonsContainer);

        m_sitesNextBtn = new QToolButton();
        m_sitesNextBtn->setIconSize(QSize(16, 16));
        m_sitesNextBtn->setToolTip("Next page");
        m_sitesNextBtn->setAutoRaise(true);
        m_sitesNextBtn->setCursor(Qt::PointingHandCursor);
        m_sitesNextBtn->setProperty("class", "secondary");
        m_sitesPaginationLayout->addWidget(m_sitesNextBtn);

        m_sitesPaginationLayout->addStretch();
        layout->addWidget(m_sitesPaginationContainer);

        updateThemedIcons();

        m_contentStack->addWidget(m_sitesPage);
    }

    void MainWindow::setupPhpPage()
    {
        m_phpPage = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(m_phpPage);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(24);

        QLabel *title = new QLabel("PHP Verisons");
        title->setProperty("class", "heading");
        layout->addWidget(title);

        m_phpVersionsTable = new QTableWidget(0, 3);
        m_phpVersionsTable->setHorizontalHeaderLabels({"Version", "Global", "Action"});
        m_phpVersionsTable->verticalHeader()->setVisible(false);
        m_phpVersionsTable->verticalHeader()->setDefaultSectionSize(42);
        m_phpVersionsTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_phpVersionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_phpVersionsTable->setFocusPolicy(Qt::NoFocus);
        m_phpVersionsTable->setShowGrid(false);
        m_phpVersionsTable->setAlternatingRowColors(true);
        m_phpVersionsTable->setWordWrap(false);
        m_phpVersionsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_phpVersionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_phpVersionsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        m_phpVersionsTable->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_phpVersionsTable->setColumnWidth(2, 240);
        m_phpVersionsTable->setMinimumHeight(360);
        layout->addWidget(m_phpVersionsTable, 1);

        QFrame *uploadCard = createCard();
        QVBoxLayout *uploadLayout = qobject_cast<QVBoxLayout *>(uploadCard->layout());
        QLabel *uploadTitle = new QLabel("Max File Upload Size");
        uploadTitle->setProperty("class", "subheading");
        uploadLayout->addWidget(uploadTitle);

        QLabel *uploadDesc = new QLabel("Configure the maximum file size that PHP will accept as file uploads (in MB).");
        uploadDesc->setProperty("class", "muted");
        uploadDesc->setWordWrap(true);
        uploadLayout->addWidget(uploadDesc);

        QHBoxLayout *uploadRow = new QHBoxLayout();
        m_phpUploadSizeSpin = new QSpinBox();
        m_phpUploadSizeSpin->setRange(1, 4096);
        m_phpUploadSizeSpin->setSuffix(" MB");
        uploadRow->addWidget(m_phpUploadSizeSpin);
        uploadRow->addStretch();
        m_phpUploadSizeSaveBtn = createButton("Save", "primary");
        uploadRow->addWidget(m_phpUploadSizeSaveBtn);
        uploadLayout->addLayout(uploadRow);
        layout->addWidget(uploadCard);

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
        connect(m_sitesPrevBtn, &QToolButton::clicked, this, [this]()
                {
                    if (m_sitesPageIndex > 0)
                    {
                        --m_sitesPageIndex;
                        updateSitesTable();
                    }
                });
        connect(m_sitesNextBtn, &QToolButton::clicked, this, [this]()
                {
                    ++m_sitesPageIndex;
                    updateSitesTable();
                });
        connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onThemeModeChanged);
        connect(m_phpUploadSizeSaveBtn, &QPushButton::clicked, this, &MainWindow::onSavePhpUploadSize);
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

        Core::ConfigManager &config = Core::ConfigManager::instance();
        m_phpUploadSizeSpin->setValue(config.maxUploadSize());

    }

    void MainWindow::updateServiceIndicators()
    {
        auto *manager = Core::ServiceManager::instance();
        if (!manager)
        {
            LOG_ERROR("ServiceManager not available for service status update");
            return;
        }

        bool allRunning = true;
        bool allStopped = true;
        bool hasServices = false;

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

            hasServices = true;
            allRunning = allRunning && isRunning;
            allStopped = allStopped && !isRunning;

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

        if (!hasServices)
        {
            allRunning = false;
            allStopped = true;
        }

        updateServiceActionButton(allRunning, allStopped);
    }

    void MainWindow::updateServiceActionButton(bool allRunning, bool allStopped)
    {
        QString label = "Stop All Services";
        QString styleClass = "destructive";

        if (allStopped)
        {
            label = "Start All Services";
            styleClass = "primary";
        }
        else if (!allRunning)
        {
            label = "Stop All Services";
            styleClass = "destructive";
        }

        m_stopAllServicesBtn->setText(label);
        m_stopAllServicesBtn->setProperty("class", styleClass);
        m_stopAllServicesBtn->style()->unpolish(m_stopAllServicesBtn);
        m_stopAllServicesBtn->style()->polish(m_stopAllServicesBtn);
        m_stopAllServicesBtn->update();
    }

    void MainWindow::updatePhpVersionCombo()
    {
        m_phpVersionCombo->clear();
        m_phpVersionsTable->setRowCount(0);
        m_installedPhpVersions.clear();

        auto result = m_versionManager->listInstalledPhpVersions();
        QString currentVersion = m_versionManager->globalPhpVersion();

        if (result.isSuccess())
        {
            for (const auto &phpVer : result.data)
            {
                const QString shortVersion = phpVer.shortVersion();
                m_installedPhpVersions.insert(shortVersion);
                const QString fullVersion = phpVer.fullVersion();
                const QString display = (fullVersion.isEmpty() || fullVersion == shortVersion)
                                            ? QString("PHP %1").arg(shortVersion)
                                            : QString("PHP %1 (%2)").arg(shortVersion, fullVersion);

                m_phpVersionCombo->addItem(display, shortVersion);

                if (shortVersion == currentVersion)
                {
                    m_phpVersionCombo->setCurrentIndex(m_phpVersionCombo->count() - 1);
                }
            }
        }

        if (m_phpVersionCombo->count() == 0)
        {
            m_phpVersionCombo->addItem("No PHP versions installed");
        }

        QStringList availableVersions;
        auto availableResult = m_versionManager->listAvailablePhpVersions();
        if (availableResult.isSuccess())
        {
            availableVersions = availableResult.data;
        }
        else
        {
            LOG_WARNING(QString("Failed to query available PHP versions: %1").arg(availableResult.error));
        }

        if (availableVersions.isEmpty())
        {
            availableVersions = QStringList(m_installedPhpVersions.begin(), m_installedPhpVersions.end());
        }


        for (int row = 0; row < availableVersions.size(); ++row)
        {
            const QString version = availableVersions.at(row);
            const bool isInstalled = m_installedPhpVersions.contains(version);
            const bool isActiveGlobalVersion = version == currentVersion;

            m_phpVersionsTable->insertRow(row);

            QString displayVersion = version;
            for (const auto &phpVer : result.data)
            {
                if (phpVer.shortVersion() == version && phpVer.fullVersion() != version)
                {
                    displayVersion = QString("%1 (%2)").arg(version, phpVer.fullVersion());
                    break;
                }
            }

            auto *versionItem = new QTableWidgetItem(displayVersion);
            versionItem->setData(Qt::UserRole, version);
            versionItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            m_phpVersionsTable->setItem(row, 0, versionItem);

            auto *globalItem = new QTableWidgetItem(isActiveGlobalVersion ? "Yes" : "-");
            globalItem->setTextAlignment(Qt::AlignCenter);
            m_phpVersionsTable->setItem(row, 1, globalItem);

            QWidget *actionContainer = new QWidget();
            QHBoxLayout *actionLayout = new QHBoxLayout(actionContainer);
            actionLayout->setContentsMargins(0, 0, 0, 0);
            actionLayout->setSpacing(6);
            actionLayout->setAlignment(Qt::AlignCenter);

            auto *syncBtn = new QToolButton(actionContainer);
            syncBtn->setAutoRaise(true);
            syncBtn->setCursor(isInstalled ? Qt::ArrowCursor : Qt::PointingHandCursor);
            syncBtn->setProperty("class", "icon-primary");
            syncBtn->setIcon(QIcon(isInstalled ? ":/icons/arrow-path.svg" : ":/icons/arrow-down-tray.svg"));
            syncBtn->setToolTip(isInstalled ? "Update checks are coming soon." : "Install this PHP version");
            syncBtn->setEnabled(!isInstalled);
            syncBtn->setIconSize(QSize(18, 18));
            connect(syncBtn, &QToolButton::clicked, this, [this, row]()
                    { onPhpVersionTableAction(row); });
            actionLayout->addWidget(syncBtn);

            auto *deleteBtn = new QToolButton(actionContainer);
            deleteBtn->setAutoRaise(true);
            deleteBtn->setProperty("class", "icon-destructive");
            deleteBtn->setIcon(QIcon(":/icons/trash.svg"));
            deleteBtn->setToolTip("Uninstall this PHP version");
            deleteBtn->setEnabled(isInstalled && !isActiveGlobalVersion);
            deleteBtn->setCursor((isInstalled && !isActiveGlobalVersion) ? Qt::PointingHandCursor : Qt::ArrowCursor);
            deleteBtn->setIconSize(QSize(18, 18));
            if (isActiveGlobalVersion)
                deleteBtn->setToolTip("Active global PHP version cannot be uninstalled. Switch versions first.");
            connect(deleteBtn, &QToolButton::clicked, this, [this, row]()
                    { onPhpVersionUninstallAction(row); });
            actionLayout->addWidget(deleteBtn);

            m_phpVersionsTable->setCellWidget(row, 2, actionContainer);
        }

    }

    void MainWindow::updateSitesTable()
    {
        m_sitesTable->setRowCount(0);

        auto result = m_siteManager->listSites();
        if (result.isSuccess())
        {
            m_sitesCache = result.data;
        }
        else
        {
            m_sitesCache.clear();
        }

        int totalSites = m_sitesCache.size();
        int totalPages = totalSites > 0 ? (totalSites + m_sitesPageSize - 1) / m_sitesPageSize : 1;
        if (m_sitesPageIndex >= totalPages)
            m_sitesPageIndex = totalPages - 1;
        if (m_sitesPageIndex < 0)
            m_sitesPageIndex = 0;

        int startIndex = m_sitesPageIndex * m_sitesPageSize;
        int endIndex = qMin(startIndex + m_sitesPageSize, totalSites);

        for (int index = startIndex; index < endIndex; ++index)
        {
            const auto &site = m_sitesCache.at(index);
            int row = m_sitesTable->rowCount();
            m_sitesTable->insertRow(row);

            QTableWidgetItem *nameItem = new QTableWidgetItem(site.name());
            nameItem->setData(Qt::UserRole, site.id());
            m_sitesTable->setItem(row, 0, nameItem);
            m_sitesTable->setItem(row, 1, new QTableWidgetItem(site.domain()));
            m_sitesTable->setItem(row, 2, new QTableWidgetItem(site.path()));
            m_sitesTable->setItem(row, 3, new QTableWidgetItem(site.phpVersion()));

            auto *deleteBtn = new QToolButton();
            deleteBtn->setIcon(QIcon(":/icons/trash.svg"));
            deleteBtn->setToolTip("Delete site");
            deleteBtn->setAutoRaise(true);
            deleteBtn->setCursor(Qt::PointingHandCursor);
            deleteBtn->setProperty("class", "icon-destructive");
            connect(deleteBtn, &QToolButton::clicked, this, [this, row]()
                    {
                        m_sitesTable->setCurrentCell(row, 0);
                        onRemoveSiteClicked();
                    });
            m_sitesTable->setCellWidget(row, 4, deleteBtn);
        }

        bool showPagination = totalSites > 10;
        m_sitesPaginationContainer->setVisible(showPagination);

        if (!showPagination)
        {
            clearSitesPaginationButtons();
            m_sitesPrevBtn->setEnabled(false);
            m_sitesNextBtn->setEnabled(false);
            return;
        }

        rebuildSitesPagination(totalPages);
        m_sitesPrevBtn->setEnabled(m_sitesPageIndex > 0);
        m_sitesNextBtn->setEnabled(m_sitesPageIndex + 1 < totalPages);
    }

    void MainWindow::rebuildSitesPagination(int totalPages)
    {
        clearSitesPaginationButtons();

        if (totalPages <= 0)
            return;

        int windowSize = 3;
        int startPage = qMax(0, m_sitesPageIndex - 1);
        int endPage = qMin(totalPages - 1, startPage + windowSize - 1);

        if (endPage - startPage + 1 < windowSize)
            startPage = qMax(0, endPage - windowSize + 1);

        for (int page = startPage; page <= endPage; ++page)
        {
            QString styleClass = page == m_sitesPageIndex ? "primary" : "secondary";
            auto *pageButton = createButton(QString::number(page + 1), styleClass);
            pageButton->setFixedWidth(32);
            connect(pageButton, &QPushButton::clicked, this, [this, page]()
                    {
                        m_sitesPageIndex = page;
                        updateSitesTable();
                    });
            m_sitesPageButtonsLayout->addWidget(pageButton);
            m_sitesPageButtons.push_back(pageButton);
        }
    }

    void MainWindow::clearSitesPaginationButtons()
    {
        for (auto *button : m_sitesPageButtons)
        {
            m_sitesPageButtonsLayout->removeWidget(button);
            button->deleteLater();
        }
        m_sitesPageButtons.clear();
    }

    void MainWindow::confirmAndRemoveSite(const QString &siteId, const QString &domain)
    {
        if (siteId.isEmpty())
            return;

        QDialog dialog(this);
        dialog.setWindowTitle("Remove Site");
        dialog.setModal(true);

        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(16);

        QLabel *message = new QLabel(QString("Are you sure you want to remove site '%1'?").arg(domain));
        message->setWordWrap(true);
        layout->addWidget(message);

        QHBoxLayout *buttonsLayout = new QHBoxLayout();
        buttonsLayout->addStretch();
        QPushButton *cancelBtn = createButton("Cancel", "secondary");
        QPushButton *deleteBtn = createButton("Delete", "destructive");
        buttonsLayout->addWidget(cancelBtn);
        buttonsLayout->addWidget(deleteBtn);
        layout->addLayout(buttonsLayout);

        connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(deleteBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

        if (dialog.exec() != QDialog::Accepted)
            return;

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

    void MainWindow::updateThemedIcons()
    {
        if (m_sitesPrevBtn)
        {
            m_sitesPrevBtn->setIcon(QIcon(themedIconPath(":/icons/chevron-left-light.svg", ":/icons/chevron-left-dark.svg")));
        }

        if (m_sitesNextBtn)
        {
            m_sitesNextBtn->setIcon(QIcon(themedIconPath(":/icons/chevron-right-light.svg", ":/icons/chevron-right-dark.svg")));
        }

        if (!m_sitesTable)
            return;

        const QString trashIcon = ":/icons/trash.svg";
        for (int row = 0; row < m_sitesTable->rowCount(); ++row)
        {
            QWidget *cellWidget = m_sitesTable->cellWidget(row, 4);
            auto *deleteBtn = qobject_cast<QToolButton *>(cellWidget);
            if (deleteBtn)
            {
                deleteBtn->setIcon(QIcon(trashIcon));
            }
        }
    }

    QString MainWindow::themedIconPath(const QString &lightPath, const QString &darkPath) const
    {
        return Theme::instance().isDark() ? darkPath : lightPath;
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
        auto *manager = Core::ServiceManager::instance();
        if (!manager)
        {
            showError("Service Manager Error", "Service manager is not available.");
            LOG_ERROR("ServiceManager not available when toggling services");
            return;
        }

        bool allRunning = true;
        bool allStopped = true;
        bool hasServices = false;

        auto updateState = [&](bool isRunning)
        {
            hasServices = true;
            allRunning = allRunning && isRunning;
            allStopped = allStopped && !isRunning;
        };

        if (auto *nginx = manager->nginxService())
            updateState(nginx->isRunning());
        if (auto *php = manager->phpService())
            updateState(php->isRunning());
        if (auto *db = manager->databaseService())
            updateState(db->isRunning());
        if (auto *node = manager->nodeService())
            updateState(node->isRunning());

        if (!hasServices)
        {
            showError("Service Manager Error", "No services are available to control.");
            return;
        }

        bool shouldStart = allStopped;
        QString actionTitle = shouldStart ? "Start All Services" : "Stop All Services";
        QString actionMessage = shouldStart
                                    ? "Are you sure you want to start all services?\nThis will start Nginx, PHP-FPM, database, and Node services."
                                    : "Are you sure you want to stop all services?\nThis will stop Nginx, PHP-FPM, database, and Node services.";

        QMessageBox::StandardButton reply = QMessageBox::question(
            this, actionTitle,
            actionMessage,
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        if (shouldStart)
        {
            if (auto *nginx = manager->nginxService())
                nginx->start();
            if (auto *php = manager->phpService())
                php->start();
            if (auto *db = manager->databaseService())
                db->start();
            if (auto *node = manager->nodeService())
                node->start();
            LOG_INFO("All services started by user");
        }
        else
        {
            if (auto *nginx = manager->nginxService())
                nginx->stop();
            if (auto *php = manager->phpService())
                php->stop();
            if (auto *db = manager->databaseService())
                db->stop();
            if (auto *node = manager->nodeService())
                node->stop();
            LOG_INFO("All services stopped by user");
        }

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
    }

    void MainWindow::onThemeChanged()
    {
        LOG_DEBUG("Theme changed, reapplying styles");
        Theme::instance().apply();
        updateThemedIcons();
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

        QTableWidgetItem *idItem = m_sitesTable->item(row, 0);
        if (!idItem)
            return;

        QString siteId = idItem->data(Qt::UserRole).toString();
        QString domain = m_sitesTable->item(row, 1)->text();
        confirmAndRemoveSite(siteId, domain);
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
        int row = m_phpVersionsTable->currentRow();
        if (row < 0)
            row = 0;
        onPhpVersionTableAction(row);
    }

    void MainWindow::onPhpVersionUninstallAction(int row)
    {
        if (row < 0 || row >= m_phpVersionsTable->rowCount())
            return;

        QTableWidgetItem *versionItem = m_phpVersionsTable->item(row, 0);
        if (!versionItem)
            return;

        const QString version = versionItem->data(Qt::UserRole).toString();
        const bool isActiveGlobalVersion = version == m_versionManager->globalPhpVersion();

        if (isActiveGlobalVersion)
        {
            showError(
                "Cannot Uninstall Active PHP Version",
                QString("PHP %1 is currently the active global version. Switch to another version before uninstalling.")
                    .arg(version));
            return;
        }

        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Uninstall PHP",
            QString("Uninstall PHP %1?").arg(version),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        auto result = m_versionManager->uninstallPhpVersion(version);
        if (result.isSuccess())
        {
            showSuccess("PHP Uninstalled", QString("PHP %1 uninstalled successfully").arg(version));
            updatePhpVersionCombo();
        }
        else
        {
            showError("Error", QString("Failed to uninstall PHP %1:\n%2").arg(version, result.error));
        }
    }

    void MainWindow::onPhpVersionTableAction(int row)
    {
        if (row < 0 || row >= m_phpVersionsTable->rowCount())
            return;

        QTableWidgetItem *versionItem = m_phpVersionsTable->item(row, 0);
        if (!versionItem)
            return;

        const QString version = versionItem->data(Qt::UserRole).toString();
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Install PHP",
            QString("Install PHP %1?").arg(version),
            QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;

        auto result = m_versionManager->installPhpVersion(version);

        if (result.isSuccess())
        {
            showSuccess("PHP Installed", QString("PHP %1 installed successfully").arg(version));
            updatePhpVersionCombo();
        }
        else
        {
            showError("Error", QString("Failed to install PHP %1:\n%2").arg(version, result.error));
        }
    }

    void MainWindow::onSavePhpUploadSize()
    {
        Core::ConfigManager &config = Core::ConfigManager::instance();
        config.setMaxUploadSize(m_phpUploadSizeSpin->value());
        showSuccess("PHP Settings Saved", QString("Max upload size set to %1 MB").arg(m_phpUploadSizeSpin->value()));
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
