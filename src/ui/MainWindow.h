#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QTableWidget>
#include <QLineEdit>
#include <QToolButton>
#include <QVector>
#include <QSpinBox>
#include <QSet>

#include "core/ServiceManager.h"
#include "managers/SiteManager.h"
#include "managers/VersionManager.h"
#include "models/Site.h"

namespace Anvil::UI
{

    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

    protected:
        void closeEvent(QCloseEvent *event) override;

    private slots:
        void onNavigationChanged(int index);
        void onStopAllServices();
        void onPhpVersionChanged(const QString &version);
        void onServiceStatusUpdate();
        void onSiteCreated(const QString &siteId);
        void onSiteRemoved(const QString &siteId);
        void onGlobalPhpVersionChanged(const QString &version);
        void onThemeChanged();
        void onAddSiteClicked();
        void onRemoveSiteClicked();
        void onThemeModeChanged(int index);
        void onInstallPhpVersion();
        void onAddPathClicked();
        void onPhpVersionTableAction(int row);
        void onPhpVersionUninstallAction(int row);
        void onSavePhpUploadSize();

    private:
        void setupUi();
        void setupNavigationSidebar();
        void setupDashboardPage();
        void setupGeneralPage();
        void setupSitesPage();
        void setupPhpPage();
        void setupNodePage();
        void setupSettingsPage();
        void connectSignals();
        void loadInitialData();

        QFrame *createCard(const QString &title = QString());
        QWidget *createServiceIndicator(const QString &serviceName, bool isRunning);
        QPushButton *createButton(const QString &text, const QString &styleClass);
        void updateServiceIndicators();
        void updateServiceActionButton(bool allRunning, bool allStopped);
        void updatePhpVersionCombo();
        void updateSitesTable();
        void rebuildSitesPagination(int totalPages);
        void clearSitesPaginationButtons();
        void updateParkedPaths();
        void updateThemedIcons();
        QString themedIconPath(const QString &lightPath, const QString &darkPath) const;
        void confirmAndRemoveSite(const QString &siteId, const QString &domain);
        void showError(const QString &title, const QString &message);
        void showSuccess(const QString &title, const QString &message);

        QWidget *m_centralWidget = nullptr;
        QHBoxLayout *m_mainLayout = nullptr;
        QListWidget *m_sidebar = nullptr;
        QStackedWidget *m_contentStack = nullptr;

        QWidget *m_dashboardPage = nullptr;
        QWidget *m_generalPage = nullptr;
        QWidget *m_sitesPage = nullptr;
        QWidget *m_phpPage = nullptr;
        QWidget *m_nodePage = nullptr;
        QWidget *m_settingsPage = nullptr;

        QFrame *m_servicesCard = nullptr;
        QVBoxLayout *m_servicesLayout = nullptr;
        QPushButton *m_stopAllServicesBtn = nullptr;
        QFrame *m_phpVersionCard = nullptr;
        QComboBox *m_phpVersionCombo = nullptr;
        QLabel *m_phpVersionLabel = nullptr;
        QMap<QString, QWidget *> m_serviceIndicators;

        QTableWidget *m_sitesTable = nullptr;
        QPushButton *m_addSiteBtn = nullptr;
        QWidget *m_sitesPaginationContainer = nullptr;
        QHBoxLayout *m_sitesPaginationLayout = nullptr;
        QWidget *m_sitesPageButtonsContainer = nullptr;
        QHBoxLayout *m_sitesPageButtonsLayout = nullptr;
        QToolButton *m_sitesPrevBtn = nullptr;
        QToolButton *m_sitesNextBtn = nullptr;
        QVector<QPushButton *> m_sitesPageButtons;
        QListWidget *m_pathsList = nullptr;
        QPushButton *m_addPathBtn = nullptr;

        QTableWidget *m_phpVersionsTable = nullptr;
        QSet<QString> m_installedPhpVersions;
        QSpinBox *m_phpUploadSizeSpin = nullptr;
        QPushButton *m_phpUploadSizeSaveBtn = nullptr;

        QComboBox *m_themeCombo = nullptr;

        Managers::SiteManager *m_siteManager;
        Managers::VersionManager *m_versionManager;
        QTimer *m_updateTimer;
        QList<Models::Site> m_sitesCache;
        int m_sitesPageIndex = 0;
        int m_sitesPageSize = 12;

        static constexpr int SIDEBAR_WIDTH = 250;
        static constexpr int UPDATE_INTERVAL = 5000;
    };
}
#endif
