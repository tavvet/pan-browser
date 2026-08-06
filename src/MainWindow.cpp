#include "MainWindow.h"

#include "AddressLineEdit.h"
#include "AddressSuggestion.h"
#include "BookmarkDialog.h"
#include "BookmarkStore.h"
#include "BookmarksDialog.h"
#include "BrowserPage.h"
#include "BrowserProfile.h"
#include "CertificateTrustValidator.h"
#include "DownloadManager.h"
#include "DownloadsPanel.h"
#include "ExternalNavigationPolicy.h"
#include "FindBar.h"
#include "AddressCompletionPopup.h"
#include "PermissionController.h"
#include "PermissionPrompt.h"
#include "ProxyAuthenticationController.h"
#include "SettingsDialog.h"
#include "WindowPlacement.h"
#include "WebAppStore.h"
#include "WebAppShortcutManager.h"

#include <QAction>
#include <QApplication>
#include <QAuthenticator>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QIcon>
#include <QProgressBar>
#include <QPixmap>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUuid>
#include <QWebEngineCertificateError>
#include <QWebEngineFindTextResult>
#include <QWebEngineHistory>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineView>

#include <utility>

namespace {

int effectivePort(const QUrl &url)
{
    if (url.port() >= 0)
        return url.port();
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
        return 443;
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0)
        return 80;
    return -1;
}

bool isSameWebOrigin(const QUrl &left, const QUrl &right)
{
    return left.isValid()
        && right.isValid()
        && left.scheme().compare(right.scheme(), Qt::CaseInsensitive) == 0
        && left.host().compare(right.host(), Qt::CaseInsensitive) == 0
        && effectivePort(left) == effectivePort(right);
}

} // namespace

MainWindow::MainWindow(StartupPresentation presentation, QWidget *parent)
    : MainWindow(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        WindowRole::Primary,
        WebApp(),
        presentation == StartupPresentation::Browser,
        parent
    )
{
}

MainWindow::MainWindow(
    BrowserProfile *sharedProfile,
    DownloadManager *sharedDownloadManager,
    HistoryStore *sharedHistoryStore,
    BookmarkStore *sharedBookmarkStore,
    WebAppStore *sharedWebAppStore,
    MainWindow *primaryWindow,
    WindowRole role,
    const WebApp &webApp,
    bool initializePrimaryTabs,
    QWidget *parent
)
    : QMainWindow(parent, role == WindowRole::Primary ? Qt::WindowFlags() : Qt::Window)
    , m_sessionStore(QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    ).filePath(QStringLiteral("session.json")))
{
    m_primaryWindow = primaryWindow ? primaryWindow : this;
    m_windowRole = role;
    m_webApp = webApp;
    m_ownsBrowserResources = role == WindowRole::Primary;

    if (m_ownsBrowserResources) {
        m_configurationPath = ensureConfiguration();
        QString bootstrapError;
        m_trustPolicy.load(m_configurationPath, &bootstrapError);
        m_preferences = BrowserPreferences::load(m_trustPolicy.startPage());
        initializeSearchSettings();
        initializeDnsSettings();
        initializeProxySettings();
        QString dataResetError;
        if (!BrowserProfile::applyPendingDataReset(&dataResetError))
            qWarning().noquote() << "[PanBrowser data reset]" << dataResetError;
        m_profile = new BrowserProfile(
            m_preferences.persistSessionCookies(),
            nullptr,
            m_networkBlockedByProxyError
        );
        m_downloadManager = new DownloadManager(
            m_profile,
            this,
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("downloads.json")),
            this
        );
        m_historyStore = new HistoryStore(
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("history.sqlite")),
            this
        );
        if (!m_historyStore->open(&m_historyError))
            qWarning().noquote() << "[PanBrowser history]" << m_historyError;
        m_bookmarkStore = new BookmarkStore(
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("bookmarks.sqlite")),
            this
        );
        if (!m_bookmarkStore->open(&m_bookmarkError))
            qWarning().noquote() << "[PanBrowser bookmarks]" << m_bookmarkError;
        m_webAppStore = new WebAppStore(
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("web-apps.json")),
            this
        );
        QString webAppsError;
        if (!m_webAppStore->load(&webAppsError))
            qWarning().noquote() << "[PanBrowser web apps]" << webAppsError;
        m_proxyAuthenticationController = new ProxyAuthenticationController(
            m_activeProxySettings,
            this
        );
    } else {
        Q_ASSERT(sharedProfile);
        Q_ASSERT(sharedDownloadManager);
        Q_ASSERT(sharedHistoryStore);
        Q_ASSERT(sharedBookmarkStore);
        Q_ASSERT(sharedWebAppStore);
        Q_ASSERT(primaryWindow);
        m_profile = sharedProfile;
        m_downloadManager = sharedDownloadManager;
        m_historyStore = sharedHistoryStore;
        m_bookmarkStore = sharedBookmarkStore;
        m_webAppStore = sharedWebAppStore;
        m_configurationPath = primaryWindow->m_configurationPath;
        m_searchConfigurationPath = primaryWindow->m_searchConfigurationPath;
        m_dnsConfigurationPath = primaryWindow->m_dnsConfigurationPath;
        m_proxyConfigurationPath = primaryWindow->m_proxyConfigurationPath;
        m_trustPolicy = primaryWindow->m_trustPolicy;
        m_preferences = primaryWindow->m_preferences;
        m_searchSettings = primaryWindow->m_searchSettings;
        m_dnsSettings = primaryWindow->m_dnsSettings;
        m_proxySettings = primaryWindow->m_proxySettings;
        m_activeProxySettings = primaryWindow->m_activeProxySettings;
        m_proxyConfigurationError = primaryWindow->m_proxyConfigurationError;
        m_networkBlockedByProxyError = primaryWindow->m_networkBlockedByProxyError;
        m_proxyAuthenticationController = primaryWindow->m_proxyAuthenticationController;
        setAttribute(Qt::WA_DeleteOnClose);
    }

    createInterface();
    if (!m_historyError.isEmpty())
        setTrustStatus(tr("History unavailable: %1").arg(m_historyError), true);
    m_permissionController = new PermissionController(m_permissionPrompt, this);
    if (m_ownsBrowserResources) {
        restoreWindowPlacement();
        reloadRules();
        if (initializePrimaryTabs) {
            m_primaryTabsInitialized = true;
            restoreInitialTabs();
        }
        if (m_networkBlockedByProxyError) {
            QTimer::singleShot(0, this, [this] {
                showProxyConfigurationError();
            });
        }
    } else {
        reloadRulesLocal();
        createTab(role == WindowRole::WebApp ? m_webApp.startUrl : QUrl());
    }
}

MainWindow::~MainWindow()
{
    if (m_ownsBrowserResources) {
        while (!m_popupWindows.isEmpty()) {
            if (MainWindow *popup = m_popupWindows.takeLast())
                delete popup;
        }
    }
    delete m_permissionController;
    m_permissionController = nullptr;
    delete takeCentralWidget();
    m_tabStack = nullptr;
    m_tabStates.clear();
    delete m_downloadsPanel;
    m_downloadsPanel = nullptr;
    if (m_ownsBrowserResources) {
        delete m_downloadManager;
        delete m_profile;
    }
    m_downloadManager = nullptr;
    m_historyStore = nullptr;
    m_bookmarkStore = nullptr;
    m_webAppStore = nullptr;
    m_profile = nullptr;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_ownsBrowserResources) {
        if (m_primaryTabsInitialized) {
            saveSession();
            QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
            settings.beginGroup(QStringLiteral("MainWindow"));
            settings.setValue(QStringLiteral("geometry"), saveGeometry());
            settings.setValue(QStringLiteral("state"), saveState(1));
            settings.endGroup();
            settings.sync();
        }

        const QList<QPointer<MainWindow>> popups = m_popupWindows;
        for (MainWindow *popup : popups) {
            if (popup && popup->m_windowRole == WindowRole::Popup)
                popup->close();
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::createInterface()
{
    QFile themeFile(QStringLiteral(":/assets/theme.qss"));
    if (themeFile.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(QString::fromUtf8(themeFile.readAll()));

    resize(1180, 760);
    setWindowTitle(
        m_windowRole == WindowRole::WebApp ? m_webApp.name : QStringLiteral("PanBrowser")
    );
    QIcon windowIcon(QStringLiteral(":/assets/app-icon.svg"));
    if (m_windowRole == WindowRole::WebApp && !m_webApp.iconPng.isEmpty()) {
        QPixmap pixmap;
        if (pixmap.loadFromData(m_webApp.iconPng, "PNG"))
            windowIcon = QIcon(pixmap);
    }
    setWindowIcon(windowIcon);

    m_tabStack = new QStackedWidget(this);
    m_tabStack->setObjectName(QStringLiteral("browserTabs"));
    setCentralWidget(m_tabStack);

    QToolBar *tabsToolbar = new QToolBar(tr("Tabs"), this);
    tabsToolbar->setObjectName(QStringLiteral("tabsBar"));
    tabsToolbar->setMovable(false);
    tabsToolbar->setFloatable(false);
    tabsToolbar->setIconSize(QSize(17, 17));

    QWidget *tabsContainer = new QWidget(tabsToolbar);
    tabsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QHBoxLayout *tabsLayout = new QHBoxLayout(tabsContainer);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->setSpacing(4);

    m_tabBar = new QTabBar(tabsContainer);
    m_tabBar->setObjectName(QStringLiteral("browserTabBar"));
    m_tabBar->setDocumentMode(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    m_tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    tabsLayout->addWidget(m_tabBar, 0, Qt::AlignBottom);

    QToolButton *newTabButton = new QToolButton(tabsContainer);
    newTabButton->setObjectName(QStringLiteral("newTabButton"));
    newTabButton->setIcon(QIcon(QStringLiteral(":/assets/icons/plus.svg")));
    newTabButton->setToolTip(tr("New Tab (⌘T)"));
    tabsLayout->addWidget(newTabButton, 0, Qt::AlignBottom);
    tabsLayout->addStretch(1);
    tabsToolbar->addWidget(tabsContainer);

    addToolBar(Qt::TopToolBarArea, tabsToolbar);
    addToolBarBreak(Qt::TopToolBarArea);

    QToolBar *toolbar = new QToolBar(tr("Navigation"), this);
    toolbar->setObjectName(QStringLiteral("navigationBar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(19, 19));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_backAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-left.svg")),
        tr("Back")
    );
    m_forwardAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")),
        tr("Forward")
    );
    m_reloadAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")),
        tr("Reload")
    );
    m_backAction->setEnabled(false);
    m_forwardAction->setEnabled(false);
    m_reloadAction->setEnabled(false);

    m_address = new AddressLineEdit(toolbar);
    m_address->setObjectName(QStringLiteral("addressBar"));
    updateAddressPlaceholder();
    m_address->setClearButtonEnabled(true);
    m_securityIndicator = m_address->addAction(
        QIcon(),
        QLineEdit::LeadingPosition
    );
    m_bookmarkAction = m_address->addAction(
        QIcon(QStringLiteral(":/assets/icons/star.svg")),
        QLineEdit::TrailingPosition
    );
    m_bookmarkAction->setEnabled(false);
    m_bookmarkAction->setToolTip(tr("Add Bookmark (⌘D)"));
    m_addressCompletionPopup = new AddressCompletionPopup(m_address, this);
    m_addressSuggestionTimer = new QTimer(this);
    m_addressSuggestionTimer->setSingleShot(true);
    m_addressSuggestionTimer->setInterval(100);
    QAction *addressWidgetAction = toolbar->addWidget(m_address);
    QAction *go = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")),
        tr("Go")
    );
    m_downloadButton = new DownloadButton(toolbar);
    m_downloadButton->setObjectName(QStringLiteral("downloadsButton"));
    m_downloadButton->setIcon(QIcon(QStringLiteral(":/assets/icons/download.svg")));
    m_downloadButton->setToolTip(tr("Downloads"));
    toolbar->addWidget(m_downloadButton);
    addToolBar(Qt::TopToolBarArea, toolbar);

    addToolBarBreak(Qt::TopToolBarArea);
    m_findToolbar = new QToolBar(tr("Find in page"), this);
    m_findToolbar->setObjectName(QStringLiteral("findBar"));
    m_findToolbar->setMovable(false);
    m_findToolbar->setFloatable(false);
    m_findBar = new FindBar(m_findToolbar);
    m_findToolbar->addWidget(m_findBar);
    addToolBar(Qt::TopToolBarArea, m_findToolbar);
    m_findToolbar->hide();

    addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *permissionToolbar = new QToolBar(tr("Permission request"), this);
    permissionToolbar->setObjectName(QStringLiteral("permissionBar"));
    permissionToolbar->setMovable(false);
    permissionToolbar->setFloatable(false);
    m_permissionPrompt = new PermissionPrompt(permissionToolbar);
    permissionToolbar->addWidget(m_permissionPrompt);
    addToolBar(Qt::TopToolBarArea, permissionToolbar);
    permissionToolbar->hide();

    connect(m_findBar, &FindBar::queryChanged, this, [this] {
        findInPage(false);
    });
    connect(m_findBar, &FindBar::navigationRequested, this, &MainWindow::findInPage);
    connect(m_findBar, &FindBar::closeRequested, this, &MainWindow::closeFindBar);
    m_closeFindAction = new QAction(this);
    m_closeFindAction->setShortcut(QKeySequence(Qt::Key_Escape));
    m_closeFindAction->setShortcutContext(Qt::WindowShortcut);
    m_closeFindAction->setEnabled(false);
    addAction(m_closeFindAction);
    connect(m_closeFindAction, &QAction::triggered, this, &MainWindow::closeFindBar);

    m_downloadsPanel = new DownloadsPanel(m_downloadManager, this);
    m_downloadButton->setActiveCount(m_downloadManager->activeCount());
    connect(m_downloadButton, &QToolButton::clicked, this, [this] {
        if (m_downloadsPanel->isVisible())
            m_downloadsPanel->hide();
        else
            m_downloadsPanel->showBelow(m_downloadButton);
    });
    connect(
        m_downloadManager,
        &DownloadManager::activeCountChanged,
        m_downloadButton,
        &DownloadButton::setActiveCount
    );
    connect(m_downloadManager, &DownloadManager::recordAdded, this, [this] {
        if (isActiveWindow() && m_windowRole != WindowRole::WebApp)
            m_downloadsPanel->showBelow(m_downloadButton);
    });

    connect(newTabButton, &QToolButton::clicked, this, [this] {
        createTab(m_preferences.startPage());
    });
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0) {
            m_tabStack->setCurrentIndex(index);
            if (!m_restoringSession)
                activatePendingTab(currentWebView());
        }
        if (m_permissionController)
            m_permissionController->currentViewChanged(currentWebView());
        if (m_externalUrlSource && m_externalUrlSource != currentWebView())
            cancelExternalUrlPrompt();
        updateCurrentTabUi();
        if (m_findToolbar && m_findToolbar->isVisible())
            findInPage(false);
        scheduleSessionSave();
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *webView = m_tabStack->widget(from);
        if (!webView)
            return;
        m_tabStack->removeWidget(webView);
        m_tabStack->insertWidget(to, webView);
        m_tabStack->setCurrentIndex(m_tabBar->currentIndex());
        scheduleSessionSave();
    });
    connect(m_backAction, &QAction::triggered, this, [this] {
        if (QWebEngineView *webView = currentWebView())
            webView->back();
    });
    connect(m_forwardAction, &QAction::triggered, this, [this] {
        if (QWebEngineView *webView = currentWebView())
            webView->forward();
    });
    connect(m_reloadAction, &QAction::triggered, this, [this] {
        if (QWebEngineView *webView = currentWebView())
            webView->reload();
    });
    connect(go, &QAction::triggered, this, &MainWindow::navigateFromAddressBar);
    connect(m_bookmarkAction, &QAction::triggered, this, &MainWindow::editCurrentBookmark);
    connect(m_address, &QLineEdit::returnPressed, this, &MainWindow::navigateFromAddressBar);
    connect(m_address, &QLineEdit::textEdited, this, [this] {
        m_address->clearGhostCompletion();
        const bool historyAvailable = m_preferences.saveBrowsingHistory()
            && m_historyStore
            && m_historyStore->isOpen();
        const bool bookmarksAvailable = m_bookmarkStore && m_bookmarkStore->isOpen();
        if (!historyAvailable && !bookmarksAvailable) {
            m_addressCompletionPopup->hide();
            return;
        }
        m_addressSuggestionTimer->start();
    });
    connect(m_addressSuggestionTimer, &QTimer::timeout, this, &MainWindow::showAddressSuggestions);
    connect(
        m_addressCompletionPopup,
        &AddressCompletionPopup::urlActivated,
        this,
        [this](const QUrl &url) {
            m_address->setText(url.toString());
            navigateFromAddressBar();
        }
    );
    connect(m_bookmarkStore, &BookmarkStore::bookmarksChanged, this, &MainWindow::updateBookmarkAction);

    QMenu *fileMenu = menuBar()->addMenu(tr("PanBrowser"));
    QAction *newTabAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/plus.svg")),
        tr("New Tab")
    );
    newTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(newTabAction, &QAction::triggered, this, [this] {
        createTab(m_preferences.startPage());
    });

    QAction *closeTabAction = fileMenu->addAction(tr("Close Tab"));
    closeTabAction->setShortcut(QKeySequence::Close);
    connect(closeTabAction, &QAction::triggered, this, [this] {
        closeTab(m_tabBar->currentIndex());
    });

    fileMenu->addSeparator();
    QAction *addBookmarkAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/star.svg")),
        tr("Add Bookmark…")
    );
    addBookmarkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(addBookmarkAction, &QAction::triggered, this, &MainWindow::editCurrentBookmark);
    QAction *bookmarksAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/star-filled.svg")),
        tr("Bookmarks…")
    );
    bookmarksAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
    connect(bookmarksAction, &QAction::triggered, this, &MainWindow::openBookmarks);

    m_installWebAppAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/layout-grid.svg")),
        tr("Install Web App…")
    );
    m_installWebAppAction->setEnabled(false);
    connect(
        m_installWebAppAction,
        &QAction::triggered,
        this,
        &MainWindow::installCurrentWebApp
    );
    m_webAppsMenu = fileMenu->addMenu(
        QIcon(QStringLiteral(":/assets/icons/layout-grid.svg")),
        tr("Web Apps")
    );
    connect(m_webAppsMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildWebAppsMenu);

    fileMenu->addSeparator();
    QAction *findAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/search.svg")),
        tr("Find in Page…")
    );
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::openFindBar);
    QAction *findNextAction = fileMenu->addAction(tr("Find Next"));
    findNextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(findNextAction, &QAction::triggered, this, [this] {
        if (!m_findToolbar->isVisible())
            openFindBar();
        else
            findInPage(false);
    });
    QAction *findPreviousAction = fileMenu->addAction(tr("Find Previous"));
    findPreviousAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    connect(findPreviousAction, &QAction::triggered, this, [this] {
        if (!m_findToolbar->isVisible())
            openFindBar();
        else
            findInPage(true);
    });

    fileMenu->addSeparator();
    QAction *settingsAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/settings.svg")),
        tr("Settings…")
    );
    settingsAction->setMenuRole(QAction::NoRole);
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    fileMenu->addSeparator();
    QAction *reloadRulesAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")),
        tr("Reload Trust Rules")
    );
    reloadRulesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    connect(reloadRulesAction, &QAction::triggered, this, &MainWindow::reloadRules);

    QAction *showConfiguration = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/folder-open.svg")),
        tr("Show Configuration Folder")
    );
    connect(showConfiguration, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QFileInfo(m_configurationPath).absolutePath()
        ));
    });

    m_trustStatus = new QLabel(tr("Ready"), this);
    m_trustStatus->setObjectName(QStringLiteral("trustStatus"));
    m_ruleCount = new QLabel(tr("No rules loaded"), this);
    m_ruleCount->setObjectName(QStringLiteral("ruleCount"));
    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("pageProgress"));
    m_progress->setRange(0, 100);
    m_progress->setMaximumWidth(120);
    m_progress->setTextVisible(false);
    m_progress->hide();

    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_trustStatus, 1);
    statusBar()->addPermanentWidget(m_progress);
    statusBar()->addPermanentWidget(m_ruleCount);

    m_sessionSaveTimer = new QTimer(this);
    m_sessionSaveTimer->setSingleShot(true);
    m_sessionSaveTimer->setInterval(750);
    connect(m_sessionSaveTimer, &QTimer::timeout, this, &MainWindow::saveSession);

    if (m_webAppStore) {
        connect(m_webAppStore, &WebAppStore::appsChanged, this, [this] {
            updateInstallWebAppAction();
            if (m_windowRole == WindowRole::WebApp && !m_webAppStore->app(m_webApp.id))
                close();
        });
    }

    if (m_windowRole == WindowRole::WebApp) {
        tabsToolbar->hide();
        addressWidgetAction->setVisible(false);
        go->setVisible(false);
        newTabAction->setVisible(false);
        closeTabAction->setText(tr("Close Window"));
        addBookmarkAction->setVisible(false);
        bookmarksAction->setVisible(false);
        m_installWebAppAction->setVisible(false);
        m_webAppsMenu->menuAction()->setVisible(false);
        settingsAction->setVisible(false);
        reloadRulesAction->setVisible(false);
        showConfiguration->setVisible(false);
        m_ruleCount->hide();
    }

}

MainWindow *MainWindow::createPopupWindow(
    WindowRole role,
    const QRect &requestedGeometry
)
{
    MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
    auto *popup = new MainWindow(
        m_profile,
        m_downloadManager,
        m_historyStore,
        m_bookmarkStore,
        m_webAppStore,
        primary,
        role,
        WebApp(),
        false,
        primary
    );
    primary->m_popupWindows.append(popup);
    connect(popup, &QObject::destroyed, primary, [primary, popup] {
        primary->m_popupWindows.removeAll(popup);
    });
    popup->applyPopupGeometry(requestedGeometry);
    return popup;
}

MainWindow *MainWindow::createWebAppWindow(const WebApp &app)
{
    MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
    for (MainWindow *window : std::as_const(primary->m_popupWindows)) {
        if (window
            && window->m_windowRole == WindowRole::WebApp
            && window->m_webApp.id == app.id) {
            window->show();
            window->raise();
            window->activateWindow();
            return window;
        }
    }

    auto *window = new MainWindow(
        m_profile,
        m_downloadManager,
        m_historyStore,
        m_bookmarkStore,
        m_webAppStore,
        primary,
        WindowRole::WebApp,
        app,
        false,
        nullptr
    );
    primary->m_popupWindows.append(window);
    connect(window, &QObject::destroyed, primary, [primary, window] {
        primary->m_popupWindows.removeAll(window);
    });
    window->applyPopupGeometry(QRect());
    window->show();
    window->raise();
    window->activateWindow();
    return window;
}

void MainWindow::applyPopupGeometry(const QRect &requestedGeometry)
{
    QList<QRect> availableScreens;
    for (const QScreen *screen : QGuiApplication::screens())
        availableScreens.append(screen->availableGeometry());
    const QScreen *fallbackScreen = m_primaryWindow ? m_primaryWindow->screen()
                                                     : QGuiApplication::primaryScreen();
    const QRect fallback = fallbackScreen ? fallbackScreen->availableGeometry() : QRect();
    setGeometry(popupWindowGeometry(
        requestedGeometry,
        m_primaryWindow ? m_primaryWindow->geometry() : QRect(),
        availableScreens,
        fallback
    ));
}

QWebEngineView *MainWindow::createTab(
    const QUrl &url,
    bool activate,
    bool deferred,
    const QString &restoredTitle
)
{
    QWebEngineView *webView = new QWebEngineView(m_tabStack);
    auto *page = new BrowserPage(m_profile, webView);
    if (m_windowRole == WindowRole::WebApp)
        page->setWebApp(m_webApp);
    webView->setPage(page);
    BrowserTabState state;
    if (deferred) {
        state.pendingUrl = url;
        state.suppressNextHistoryVisit = true;
    }
    m_tabStates.insert(webView, state);
    connectBrowserSignals(webView);

    const int stackIndex = m_tabStack->addWidget(webView);
    const QString initialTitle = restoredTitle.isEmpty()
        ? (url.host().isEmpty() ? tr("New Tab") : url.host())
        : restoredTitle;
    const int tabIndex = m_tabBar->addTab(initialTitle);
    Q_ASSERT(stackIndex == tabIndex);
    m_tabBar->setTabToolTip(tabIndex, url.isEmpty() ? initialTitle : url.toString());

    if (activate)
        m_tabBar->setCurrentIndex(tabIndex);
    if (!deferred && url.isValid() && !url.isEmpty())
        webView->setUrl(url);
    if (!m_restoringSession)
        scheduleSessionSave();

    return webView;
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabBar->count())
        return;

    QWebEngineView *webView = qobject_cast<QWebEngineView *>(m_tabStack->widget(index));
    if (!webView)
        return;

    m_permissionController->cancelForView(webView);
    cancelExternalUrlPrompt(webView);
    if (m_findView == webView) {
        ++m_findRequestSerial;
        m_findView.clear();
        if (m_findBar)
            m_findBar->clearResults();
    }
    disconnect(webView, nullptr, this, nullptr);
    disconnect(webView->page(), nullptr, this, nullptr);
    webView->stop();
    m_tabStates.remove(webView);
    m_tabStack->removeWidget(webView);
    m_tabBar->removeTab(index);
    webView->deleteLater();

    if (m_tabBar->count() == 0) {
        if (m_ownsBrowserResources) {
            m_discardSessionOnClose = true;
            m_sessionStore.clear();
        }
        close();
        return;
    }

    m_tabStack->setCurrentIndex(m_tabBar->currentIndex());
    updateCurrentTabUi();
    scheduleSessionSave();
}

QWebEngineView *MainWindow::currentWebView() const
{
    return qobject_cast<QWebEngineView *>(m_tabStack->currentWidget());
}

void MainWindow::activatePendingTab(QWebEngineView *webView)
{
    if (!webView || !m_tabStates.contains(webView))
        return;
    BrowserTabState &state = m_tabStates[webView];
    if (state.pendingUrl.isEmpty())
        return;
    const QUrl url = state.pendingUrl;
    state.pendingUrl.clear();
    webView->setUrl(url);
}

void MainWindow::connectBrowserSignals(QWebEngineView *webView)
{
    connect(
        webView->page(),
        &QWebEnginePage::proxyAuthenticationRequired,
        this,
        [this](
            const QUrl &requestUrl,
            QAuthenticator *authenticator,
            const QString &proxyHost
        ) {
            if (m_proxyAuthenticationController) {
                m_proxyAuthenticationController->requestAuthentication(
                    this,
                    requestUrl,
                    authenticator,
                    proxyHost
                );
            }
        }
    );
    connect(webView, &QWebEngineView::urlChanged, this, [this, webView](const QUrl &url) {
        BrowserTabState &state = m_tabStates[webView];
        state.pendingUrl.clear();
        state.topLevelUrl = url;
        const int index = m_tabStack->indexOf(webView);
        if (index >= 0 && webView->title().isEmpty()) {
            const QString label = url.host().isEmpty() ? tr("New Tab") : url.host();
            m_tabBar->setTabText(index, label);
            m_tabBar->setTabToolTip(index, url.toString());
        }
        if (webView == currentWebView())
            m_address->setText(url.toString());
        if (webView == currentWebView())
            updateBookmarkAction();
        if (m_addressSuggestionTimer)
            m_addressSuggestionTimer->stop();
        if (m_addressCompletionPopup)
            m_addressCompletionPopup->hide();
        updateNavigationActions();
        if (webView == currentWebView())
            updateInstallWebAppAction();
        scheduleSessionSave();
    });
    connect(webView, &QWebEngineView::titleChanged, this, [this, webView](const QString &title) {
        const int index = m_tabStack->indexOf(webView);
        if (index >= 0) {
            const QString label = title.isEmpty()
                ? (webView->url().host().isEmpty() ? tr("New Tab") : webView->url().host())
                : title;
            m_tabBar->setTabText(index, label);
            m_tabBar->setTabToolTip(index, title.isEmpty() ? webView->url().toString() : title);
        }
        if (webView == currentWebView()) {
            if (m_windowRole == WindowRole::WebApp)
                setWindowTitle(m_webApp.name);
            else
                setWindowTitle(title.isEmpty() ? QStringLiteral("PanBrowser") : title + QStringLiteral(" — PanBrowser"));
        }
        if (m_preferences.saveBrowsingHistory() && m_historyStore && m_historyStore->isOpen()) {
            QString error;
            if (!m_historyStore->updateTitle(webView->url(), title, &error))
                qWarning().noquote() << "[PanBrowser history]" << error;
        }
        scheduleSessionSave();
    });
    connect(webView, &QWebEngineView::iconChanged, this, [this, webView](const QIcon &icon) {
        const int index = m_tabStack->indexOf(webView);
        if (index >= 0)
            m_tabBar->setTabIcon(index, icon);
    });
    connect(webView, &QWebEngineView::loadStarted, this, [this, webView] {
        m_permissionController->cancelForView(webView);
        cancelExternalUrlPrompt(webView);
        BrowserTabState &state = m_tabStates[webView];
        state.previousTrustStatus = state.trustStatus;
        state.previousTrustError = state.trustError;
        state.previousAcceptedRule = state.lastAcceptedRule;
        state.externalNavigationDelegated = false;
        state.lastAcceptedRule.clear();
        state.manifestUrl = QUrl();
        state.manifestTitle.clear();
        state.loading = true;
        state.progress = 0;
        if (webView == m_findView) {
            ++m_findRequestSerial;
            if (webView == currentWebView()
                && m_findToolbar
                && m_findToolbar->isVisible()
                && !m_findBar->query().isEmpty()) {
                m_findBar->setSearching();
            }
        }
        setTabTrustStatus(webView, tr("Loading…"));
        if (webView == currentWebView()) {
            m_progress->setValue(0);
            m_progress->show();
        }
        updateNavigationActions();
        if (webView == currentWebView())
            updateInstallWebAppAction();
    });
    connect(webView, &QWebEngineView::loadProgress, this, [this, webView](int progress) {
        m_tabStates[webView].progress = progress;
        if (webView == currentWebView())
            m_progress->setValue(progress);
    });
    connect(webView, &QWebEngineView::loadFinished, this, [this, webView](bool ok) {
        BrowserTabState &state = m_tabStates[webView];
        state.loading = false;
        state.progress = 100;
        if (webView == currentWebView())
            m_progress->hide();
        if (state.externalNavigationDelegated) {
            state.externalNavigationDelegated = false;
            state.lastAcceptedRule = state.previousAcceptedRule;
            setTabTrustStatus(webView, state.previousTrustStatus, state.previousTrustError);
            state.pendingHistoryTransition = HistoryTransition::Other;
            state.suppressNextHistoryVisit = false;
            updateNavigationActions();
            if (webView == currentWebView()
                && m_findToolbar
                && m_findToolbar->isVisible()
                && !m_findBar->query().isEmpty()) {
                findInPage(false);
            }
            return;
        }
        if (ok) {
            if (!state.suppressNextHistoryVisit
                && m_preferences.saveBrowsingHistory()
                && m_historyStore
                && m_historyStore->isOpen()
                && HistoryStore::sanitizedUrl(webView->url()).isValid()) {
                QString historyError;
                if (!m_historyStore->recordVisit(
                        webView->url(),
                        webView->title(),
                        state.pendingHistoryTransition,
                        QDateTime::currentDateTimeUtc(),
                        &historyError
                    )) {
                    qWarning().noquote() << "[PanBrowser history]" << historyError;
                }
            }
            const QString scheme = webView->url().scheme().toLower();
            if (scheme == QStringLiteral("https")) {
                if (state.lastAcceptedRule.isEmpty()) {
                    setTabTrustStatus(webView, tr("Secure · Chromium system trust"));
                }
            } else if (scheme == QStringLiteral("http")) {
                setTabTrustStatus(webView, tr("Not secure · HTTP connection"), true);
            } else {
                setTabTrustStatus(webView, tr("No HTTPS security information"));
            }
            if (m_windowRole != WindowRole::WebApp)
                detectWebAppManifest(webView);
        } else if (state.lastAcceptedRule.isEmpty()) {
            setTabTrustStatus(webView, tr("Page loading failed"), true);
        }
        state.pendingHistoryTransition = HistoryTransition::Other;
        state.suppressNextHistoryVisit = false;
        updateNavigationActions();
        if (webView == currentWebView()
            && m_findToolbar
            && m_findToolbar->isVisible()
            && !m_findBar->query().isEmpty()) {
            findInPage(false);
        }
    });
    connect(
        static_cast<BrowserPage *>(webView->page()),
        &BrowserPage::mainFrameNavigationRequested,
        this,
        [this, webView](const QUrl &url, int navigationType) {
            BrowserTabState &state = m_tabStates[webView];
            state.topLevelUrl = url;
            const auto type = static_cast<QWebEnginePage::NavigationType>(navigationType);
            switch (type) {
            case QWebEnginePage::NavigationTypeLinkClicked:
                state.pendingHistoryTransition = HistoryTransition::Link;
                break;
            case QWebEnginePage::NavigationTypeFormSubmitted:
                state.pendingHistoryTransition = HistoryTransition::Form;
                break;
            case QWebEnginePage::NavigationTypeBackForward:
                state.pendingHistoryTransition = HistoryTransition::BackForward;
                break;
            case QWebEnginePage::NavigationTypeReload:
                state.pendingHistoryTransition = HistoryTransition::Reload;
                break;
            default:
                break;
            }
        }
    );
    connect(
        static_cast<BrowserPage *>(webView->page()),
        &BrowserPage::outOfScopeNavigationRequested,
        this,
        [this](const QUrl &url, int navigationType) {
            if (m_windowRole != WindowRole::WebApp)
                return;
            const auto type = static_cast<QWebEnginePage::NavigationType>(navigationType);
            if (type == QWebEnginePage::NavigationTypeLinkClicked
                || type == QWebEnginePage::NavigationTypeTyped) {
                openUrlInPrimaryWindow(url);
                return;
            }
            if (type == QWebEnginePage::NavigationTypeFormSubmitted) {
                statusBar()->showMessage(
                    tr("Blocked a form submission outside this web app's scope"),
                    6000
                );
                return;
            }
            statusBar()->showMessage(
                tr("Blocked automatic navigation outside this web app's scope"),
                6000
            );
        }
    );
    connect(
        static_cast<BrowserPage *>(webView->page()),
        &BrowserPage::webAppManifestFetched,
        this,
        &MainWindow::handleFetchedWebAppManifest
    );
    connect(
        static_cast<BrowserPage *>(webView->page()),
        &BrowserPage::externalUrlRequested,
        this,
        [this, webView](const QUrl &url) {
            m_tabStates[webView].externalNavigationDelegated = true;
            handleExternalUrlRequest(webView, url);
        }
    );
    connect(
        webView->page(),
        &QWebEnginePage::certificateError,
        this,
        [this, webView](const QWebEngineCertificateError &error) {
            handleCertificateError(webView, error);
        }
    );
    connect(
        webView->page(),
        &QWebEnginePage::permissionRequested,
        this,
        [this, webView](const QWebEnginePermission &permission) {
            m_permissionController->request(webView, permission);
        }
    );
    connect(
        webView->page(),
        &QWebEnginePage::newWindowRequested,
        this,
        [this, webView](QWebEngineNewWindowRequest &request) {
            if (!request.isUserInitiated())
                return;

            const QUrl requestedUrl = request.requestedUrl();
            if (!requestedUrl.scheme().isEmpty()) {
                switch (externalNavigationDisposition(requestedUrl, true)) {
                case ExternalNavigationDisposition::Prompt:
                    handleExternalUrlRequest(webView, requestedUrl);
                    return;
                case ExternalNavigationDisposition::Block:
                    return;
                case ExternalNavigationDisposition::Browse:
                    break;
                }
            }

            if (m_windowRole == WindowRole::WebApp) {
                openWindowRequestInPrimary(request);
                return;
            }

            const bool separateWindow = request.destination()
                    == QWebEngineNewWindowRequest::InNewWindow
                || request.destination() == QWebEngineNewWindowRequest::InNewDialog;
            if (separateWindow) {
                MainWindow *popup = createPopupWindow(
                    WindowRole::Popup,
                    request.requestedGeometry()
                );
                request.openIn(popup->currentWebView()->page());
                popup->show();
                popup->raise();
                popup->activateWindow();
                return;
            }

            const bool activate = request.destination()
                != QWebEngineNewWindowRequest::InNewBackgroundTab;
            QWebEngineView *newView = createTab(QUrl(), activate);
            request.openIn(newView->page());
        }
    );
}

void MainWindow::handleExternalUrlRequest(QWebEngineView *webView, const QUrl &url)
{
    if (!webView || webView != currentWebView() || m_externalUrlDialog)
        return;

    const QUrl sourceUrl = webView->url();
    const QString source = sourceUrl.host().isEmpty()
        ? tr("The current page")
        : QStringLiteral("%1://%2").arg(sourceUrl.scheme(), sourceUrl.host());
    QString target = url.toDisplayString(QUrl::RemovePassword);
    constexpr qsizetype maximumDisplayedUrlLength = 700;
    if (target.size() > maximumDisplayedUrlLength)
        target = target.left(maximumDisplayedUrlLength) + QStringLiteral("…");

    auto *dialog = new QMessageBox(this);
    dialog->setWindowTitle(tr("Open external application?"));
    dialog->setIcon(QMessageBox::Question);
    dialog->setTextFormat(Qt::PlainText);
    dialog->setText(tr("Open this link in another application?"));
    dialog->setInformativeText(
        tr("%1 wants to open:\n\n%2").arg(source, target)
    );
    auto *openButton = dialog->addButton(
        tr("Open application"),
        QMessageBox::AcceptRole
    );
    auto *cancelButton = dialog->addButton(tr("Cancel"), QMessageBox::RejectRole);
    dialog->setDefaultButton(cancelButton);
    dialog->setEscapeButton(cancelButton);
    dialog->setWindowModality(Qt::WindowModal);

    m_externalUrlDialog = dialog;
    m_externalUrlSource = webView;
    const QPointer<QWebEngineView> sourceView(webView);
    connect(dialog, &QMessageBox::finished, this, [
        this,
        dialog,
        openButton,
        sourceView,
        url
    ] {
        const bool accepted = dialog->clickedButton() == openButton;
        m_externalUrlDialog.clear();
        m_externalUrlSource.clear();
        dialog->deleteLater();
        if (!accepted || !sourceView || sourceView != currentWebView())
            return;
        if (!QDesktopServices::openUrl(url)) {
            statusBar()->showMessage(
                tr("No application is available for the %1 link").arg(url.scheme()),
                5000
            );
        }
    });
    dialog->open();
}

void MainWindow::cancelExternalUrlPrompt(QWebEngineView *webView)
{
    if (!m_externalUrlDialog)
        return;
    if (webView && m_externalUrlSource != webView)
        return;
    m_externalUrlDialog->reject();
}

void MainWindow::updateCurrentTabUi()
{
    QWebEngineView *webView = currentWebView();
    if (!webView) {
        m_address->clear();
        setWindowTitle(
            m_windowRole == WindowRole::WebApp ? m_webApp.name : QStringLiteral("PanBrowser")
        );
        m_progress->hide();
        updateNavigationActions();
        updateBookmarkAction();
        updateInstallWebAppAction();
        return;
    }

    m_address->setText(webView->url().toString());
    if (webView->url().isEmpty() && !m_tabStates.value(webView).pendingUrl.isEmpty())
        m_address->setText(m_tabStates.value(webView).pendingUrl.toString());
    const QString title = webView->title();
    if (m_windowRole == WindowRole::WebApp)
        setWindowTitle(m_webApp.name);
    else
        setWindowTitle(title.isEmpty() ? QStringLiteral("PanBrowser") : title + QStringLiteral(" — PanBrowser"));

    const BrowserTabState state = m_tabStates.value(webView);
    setTrustStatus(state.trustStatus, state.trustError);
    m_progress->setValue(state.progress);
    m_progress->setVisible(state.loading);
    updateNavigationActions();
    updateBookmarkAction();
    updateInstallWebAppAction();
}

void MainWindow::updateNavigationActions()
{
    QWebEngineView *webView = currentWebView();
    m_backAction->setEnabled(webView && webView->history()->canGoBack());
    m_forwardAction->setEnabled(webView && webView->history()->canGoForward());
    m_reloadAction->setEnabled(webView != nullptr);
}

void MainWindow::updateBookmarkAction()
{
    if (!m_bookmarkAction)
        return;
    QWebEngineView *webView = currentWebView();
    const QUrl url = webView ? BookmarkStore::normalizedUrl(webView->url()) : QUrl();
    const bool available = m_bookmarkStore && m_bookmarkStore->isOpen() && url.isValid();
    m_bookmarkAction->setEnabled(available);
    if (!available) {
        m_bookmarkAction->setIcon(QIcon(QStringLiteral(":/assets/icons/star.svg")));
        m_bookmarkAction->setToolTip(tr("Only web pages can be bookmarked"));
        return;
    }

    QString error;
    const bool bookmarked = m_bookmarkStore->bookmarkForUrl(url, &error).has_value();
    if (!error.isEmpty())
        qWarning().noquote() << "[PanBrowser bookmarks]" << error;
    m_bookmarkAction->setIcon(QIcon(
        bookmarked ? QStringLiteral(":/assets/icons/star-filled.svg")
                   : QStringLiteral(":/assets/icons/star.svg")
    ));
    m_bookmarkAction->setToolTip(
        bookmarked ? tr("Edit Bookmark (⌘D)")
                   : tr("Add Bookmark (⌘D)")
    );
}

void MainWindow::updateAddressPlaceholder()
{
    if (!m_address)
        return;
    const SearchEngineSettings *engine = m_searchSettings.defaultEngine();
    m_address->setPlaceholderText(
        engine ? tr("Search with %1 or enter an address").arg(engine->name)
               : tr("Enter an address")
    );
}

void MainWindow::navigateFromAddressBar()
{
    if (m_addressSuggestionTimer)
        m_addressSuggestionTimer->stop();
    if (m_addressCompletionPopup)
        m_addressCompletionPopup->hide();
    const ResolvedAddressInput result = resolveAddressInput(m_address->text(), m_searchSettings);
    if (result.kind == AddressInputKind::Error) {
        setTrustStatus(result.error, true);
        return;
    }
    if (QWebEngineView *webView = currentWebView()) {
        m_tabStates[webView].pendingHistoryTransition = HistoryTransition::Typed;
        m_tabStates[webView].suppressNextHistoryVisit = false;
        webView->setUrl(result.url);
    }
}

void MainWindow::showAddressSuggestions()
{
    if (!m_addressCompletionPopup) {
        return;
    }
    const QString input = m_address->text().trimmed();
    if (input.isEmpty()
        || input.startsWith(QLatin1Char('?'))
        || input.startsWith(QLatin1Char('@'))) {
        m_addressCompletionPopup->hide();
        return;
    }
    QList<AddressSuggestion> candidates;
    if (m_bookmarkStore && m_bookmarkStore->isOpen()) {
        QString error;
        const QList<Bookmark> bookmarks = m_bookmarkStore->bookmarks(input, &error);
        if (!error.isEmpty()) {
            qWarning().noquote() << "[PanBrowser bookmarks]" << error;
        } else {
            for (const Bookmark &bookmark : bookmarks) {
                candidates.append({
                    bookmark.url,
                    bookmark.title,
                    bookmark.updatedAt,
                    AddressSuggestionSource::Bookmark,
                    0,
                    0,
                });
            }
        }
    }
    if (m_preferences.saveBrowsingHistory() && m_historyStore && m_historyStore->isOpen()) {
        QString error;
        const QList<HistorySuggestion> history = m_historyStore->suggestions(input, 8, &error);
        if (!error.isEmpty()) {
            qWarning().noquote() << "[PanBrowser history]" << error;
        } else {
            for (const HistorySuggestion &suggestion : history) {
                candidates.append({
                    suggestion.url,
                    suggestion.title,
                    suggestion.lastVisitedAt,
                    AddressSuggestionSource::History,
                    suggestion.typedCount,
                    suggestion.visitCount,
                });
            }
        }
    }
    const QList<AddressSuggestion> suggestions = rankedAddressSuggestions(candidates, input, 8);
    m_addressCompletionPopup->showSuggestions(suggestions);
}

void MainWindow::openFindBar()
{
    if (!m_findToolbar || !m_findBar)
        return;
    if (m_addressCompletionPopup)
        m_addressCompletionPopup->hide();

    QString selectedText;
    if (QWebEngineView *webView = currentWebView()) {
        selectedText = webView->page()->selectedText().trimmed();
        if (selectedText.size() > 200
            || selectedText.contains(QLatin1Char('\n'))
            || selectedText.contains(QLatin1Char('\r'))) {
            selectedText.clear();
        }
    }
    const bool wasVisible = m_findToolbar->isVisible();
    const bool queryWillChange = !selectedText.isEmpty()
        && selectedText != m_findBar->query();
    m_findToolbar->show();
    m_closeFindAction->setEnabled(true);
    m_findBar->focusInput(selectedText);
    if (!wasVisible && !queryWillChange)
        findInPage(false);
}

void MainWindow::closeFindBar()
{
    ++m_findRequestSerial;
    if (m_findView)
        m_findView->page()->findText(QString());
    m_findView.clear();
    if (m_findBar)
        m_findBar->clearResults();
    if (m_findToolbar)
        m_findToolbar->hide();
    if (m_closeFindAction)
        m_closeFindAction->setEnabled(false);
    if (QWebEngineView *webView = currentWebView())
        webView->setFocus(Qt::OtherFocusReason);
}

void MainWindow::findInPage(bool backward)
{
    if (!m_findToolbar || !m_findToolbar->isVisible() || !m_findBar)
        return;
    QWebEngineView *webView = currentWebView();
    if (!webView) {
        m_findBar->clearResults();
        return;
    }

    if (m_findView && m_findView != webView)
        m_findView->page()->findText(QString());
    m_findView = webView;

    const QString query = m_findBar->query();
    const quint64 requestSerial = ++m_findRequestSerial;
    if (query.isEmpty()) {
        webView->page()->findText(QString());
        m_findBar->clearResults();
        return;
    }

    m_findBar->setSearching();
    QWebEnginePage::FindFlags flags;
    if (backward)
        flags.setFlag(QWebEnginePage::FindBackward);
    const QPointer<MainWindow> window(this);
    const QPointer<QWebEngineView> target(webView);
    webView->page()->findText(query, flags, [window, target, requestSerial, query](
        const QWebEngineFindTextResult &result
    ) {
        if (!window
            || !target
            || requestSerial != window->m_findRequestSerial
            || target != window->currentWebView()
            || target != window->m_findView
            || !window->m_findToolbar->isVisible()
            || query != window->m_findBar->query()) {
            return;
        }
        window->m_findBar->setResults(result.activeMatch(), result.numberOfMatches());
    });
}

void MainWindow::editCurrentBookmark()
{
    QWebEngineView *webView = currentWebView();
    if (!webView || !m_bookmarkStore || !m_bookmarkStore->isOpen()) {
        statusBar()->showMessage(
            m_bookmarkError.isEmpty() ? tr("Bookmarks are unavailable")
                                      : m_bookmarkError,
            5000
        );
        return;
    }
    const QUrl url = BookmarkStore::normalizedUrl(webView->url());
    if (!url.isValid()) {
        statusBar()->showMessage(tr("Only web pages can be bookmarked"), 4000);
        return;
    }

    QString error;
    const std::optional<Bookmark> existing = m_bookmarkStore->bookmarkForUrl(url, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Bookmarks unavailable"), error);
        return;
    }
    const QString title = webView->title().trimmed().isEmpty()
        ? url.host()
        : webView->title().trimmed();
    BookmarkDialog dialog(m_bookmarkStore, url, title, existing, this);
    dialog.exec();
}

void MainWindow::openBookmarks()
{
    if (!m_ownsBrowserResources && m_primaryWindow) {
        m_primaryWindow->show();
        m_primaryWindow->raise();
        m_primaryWindow->activateWindow();
        m_primaryWindow->openBookmarks();
        return;
    }

    BookmarksDialog dialog(m_bookmarkStore, this);
    connect(&dialog, &BookmarksDialog::openRequested, this, [this](const QUrl &url, bool newTab) {
        if (newTab || !currentWebView()) {
            createTab(url, true);
            return;
        }
        BrowserTabState &state = m_tabStates[currentWebView()];
        state.pendingHistoryTransition = HistoryTransition::Other;
        state.suppressNextHistoryVisit = false;
        currentWebView()->setUrl(url);
    });
    dialog.exec();
}

void MainWindow::detectWebAppManifest(QWebEngineView *webView)
{
    if (!webView || !m_tabStates.contains(webView))
        return;
    const QUrl documentUrl = webView->url();
    if (documentUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
        m_tabStates[webView].manifestUrl = QUrl();
        if (webView == currentWebView())
            updateInstallWebAppAction();
        return;
    }

    const QPointer<MainWindow> window(this);
    const QPointer<QWebEngineView> target(webView);
    webView->page()->runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const link = document.querySelector('link[rel~="manifest"]');
    if (!link || !link.href)
        return null;
    return { url: link.href, title: document.title || "" };
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [window, target, documentUrl](const QVariant &result) {
            if (!window || !target || !window->m_tabStates.contains(target))
                return;
            BrowserTabState &state = window->m_tabStates[target];
            state.manifestUrl = QUrl();
            state.manifestTitle.clear();
            if (target->url() == documentUrl) {
                const QVariantMap object = result.toMap();
                const QUrl manifestUrl(object.value(QStringLiteral("url")).toString());
                if (manifestUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
                    && manifestUrl.userInfo().isEmpty()
                    && isSameWebOrigin(manifestUrl, documentUrl)) {
                    state.manifestUrl = manifestUrl.adjusted(QUrl::NormalizePathSegments);
                    state.manifestTitle = object.value(QStringLiteral("title")).toString().simplified();
                }
            }
            if (target == window->currentWebView())
                window->updateInstallWebAppAction();
        }
    );
}

void MainWindow::updateInstallWebAppAction()
{
    if (!m_installWebAppAction || m_windowRole == WindowRole::WebApp)
        return;
    QWebEngineView *webView = currentWebView();
    const BrowserTabState state = webView ? m_tabStates.value(webView) : BrowserTabState();
    if (!webView
        || state.manifestUrl.isEmpty()
        || !m_webAppStore
        || !m_webAppStore->isAvailable()) {
        m_installWebAppAction->setText(tr("Install Web App…"));
        m_installWebAppAction->setEnabled(false);
        return;
    }
    const std::optional<WebApp> installed = m_webAppStore->appForManifest(state.manifestUrl);
    if (installed) {
        m_installWebAppAction->setText(tr("Open “%1”").arg(installed->name));
        m_installWebAppAction->setEnabled(true);
        return;
    }
    const QString name = state.manifestTitle.isEmpty()
        ? webView->url().host()
        : state.manifestTitle.left(80);
    m_installWebAppAction->setText(tr("Install “%1”…").arg(name));
    m_installWebAppAction->setEnabled(true);
}

void MainWindow::installCurrentWebApp()
{
    if (!m_webAppStore || m_windowRole == WindowRole::WebApp)
        return;
    QWebEngineView *webView = currentWebView();
    if (!webView || !m_tabStates.contains(webView))
        return;
    const BrowserTabState state = m_tabStates.value(webView);
    if (state.manifestUrl.isEmpty())
        return;

    if (const std::optional<WebApp> installed = m_webAppStore->appForManifest(state.manifestUrl)) {
        openInstalledWebApp(installed->id);
        return;
    }

    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_manifestRequests.insert(requestId, {
        webView,
        state.manifestUrl,
        webView->url(),
        state.manifestTitle,
    });
    m_installWebAppAction->setEnabled(false);
    m_installWebAppAction->setText(tr("Reading web app manifest…"));
    static_cast<BrowserPage *>(webView->page())->fetchWebAppManifest(
        requestId,
        state.manifestUrl,
        WebAppStore::maximumManifestBytes
    );
    QTimer::singleShot(15000, this, [this, requestId] {
        const auto found = m_manifestRequests.find(requestId);
        if (found == m_manifestRequests.end())
            return;
        const QPointer<QWebEngineView> webView = found->webView;
        m_manifestRequests.erase(found);
        if (webView) {
            static_cast<BrowserPage *>(webView->page())->cancelWebAppManifestFetch(requestId);
        }
        updateInstallWebAppAction();
        statusBar()->showMessage(tr("Timed out while reading the web app manifest"), 5000);
    });
}

void MainWindow::handleFetchedWebAppManifest(
    const QString &requestId,
    const QByteArray &contents,
    const QString &fetchError
)
{
    const auto found = m_manifestRequests.find(requestId);
    if (found == m_manifestRequests.end())
        return;
    const PendingManifestRequest request = found.value();
    m_manifestRequests.erase(found);
    updateInstallWebAppAction();
    if (!request.webView
        || !m_tabStates.contains(request.webView)
        || m_tabStates.value(request.webView).manifestUrl != request.manifestUrl) {
        return;
    }
    if (!fetchError.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Cannot install web app"),
            tr("PanBrowser could not read the web app manifest: %1").arg(fetchError)
        );
        return;
    }

    QString error;
    std::optional<WebApp> app = WebAppStore::parseManifest(
        contents,
        request.manifestUrl,
        request.documentUrl,
        request.fallbackTitle,
        &error
    );
    if (!app) {
        QMessageBox::warning(this, tr("Cannot install web app"), error);
        return;
    }

    const QPixmap pixmap = request.webView->icon().pixmap(256, 256);
    if (!pixmap.isNull()) {
        QByteArray iconPng;
        QBuffer buffer(&iconPng);
        if (buffer.open(QIODevice::WriteOnly)
            && pixmap.save(&buffer, "PNG")
            && iconPng.size() <= WebAppStore::maximumIconBytes) {
            app->iconPng = iconPng;
        }
    }

    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("Install web app"));
    dialog.setIcon(QMessageBox::Question);
    if (!pixmap.isNull())
        dialog.setIconPixmap(pixmap.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    dialog.setText(tr("Install “%1”?").arg(app->name));
    dialog.setInformativeText(
        tr("The app will open in its own window and share PanBrowser cookies, site data, permissions, and trust rules.\n\nStart page: %1\nAllowed scope: %2")
            .arg(
                app->startUrl.toDisplayString(QUrl::RemovePassword),
                app->scope.toDisplayString(QUrl::RemovePassword)
            )
    );
    QPushButton *installButton = dialog.addButton(tr("Install"), QMessageBox::AcceptRole);
    dialog.addButton(tr("Cancel"), QMessageBox::RejectRole);
    dialog.setDefaultButton(installButton);
    dialog.exec();
    if (dialog.clickedButton() != installButton)
        return;

    if (!m_webAppStore->install(*app, &error)) {
        QMessageBox::warning(this, tr("Cannot install web app"), error);
        return;
    }
    WebAppShortcutManager shortcutManager;
    if (shortcutManager.isSupported()) {
        QString shortcutError;
        if (!shortcutManager.createOrUpdate(*app, &shortcutError)) {
            QMessageBox::warning(
                this,
                tr("Web app installed without a system shortcut"),
                tr("PanBrowser installed the web app, but could not create its system shortcut: %1")
                    .arg(shortcutError)
            );
        }
    }
    statusBar()->showMessage(tr("“%1” installed").arg(app->name), 4000);
    openInstalledWebApp(app->id);
}

void MainWindow::rebuildWebAppsMenu()
{
    if (!m_webAppsMenu || !m_webAppStore)
        return;
    m_webAppsMenu->clear();
    const QList<WebApp> apps = m_webAppStore->apps();
    for (const WebApp &app : apps) {
        QIcon icon(QStringLiteral(":/assets/app-icon.svg"));
        QPixmap pixmap;
        if (!app.iconPng.isEmpty() && pixmap.loadFromData(app.iconPng, "PNG"))
            icon = QIcon(pixmap);
        QAction *action = m_webAppsMenu->addAction(icon, app.name);
        connect(action, &QAction::triggered, this, [this, id = app.id] {
            openInstalledWebApp(id);
        });
    }
    if (apps.isEmpty()) {
        QAction *empty = m_webAppsMenu->addAction(tr("No web apps installed"));
        empty->setEnabled(false);
    }
    m_webAppsMenu->addSeparator();
    QAction *manage = m_webAppsMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/settings.svg")),
        tr("Manage Web Apps…")
    );
    connect(manage, &QAction::triggered, this, &MainWindow::openWebAppsSettings);
}

void MainWindow::openInstalledWebApp(const QString &id)
{
    (void)launchInstalledWebApp(id);
}

void MainWindow::activatePrimaryWindow()
{
    MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
    if (primary != this) {
        primary->activatePrimaryWindow();
        return;
    }
    if (!m_primaryTabsInitialized) {
        m_primaryTabsInitialized = true;
        restoreInitialTabs();
    }
    qApp->setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.svg")));
    show();
    raise();
    activateWindow();
}

bool MainWindow::launchInstalledWebApp(const QString &id)
{
    if (!m_ownsBrowserResources && m_primaryWindow) {
        return m_primaryWindow->launchInstalledWebApp(id);
    }
    if (!m_webAppStore)
        return false;
    const std::optional<WebApp> app = m_webAppStore->app(id);
    if (!app) {
        statusBar()->showMessage(tr("The web app is no longer installed"), 4000);
        return false;
    }
    if (!m_primaryTabsInitialized && m_popupWindows.isEmpty() && !app->iconPng.isEmpty()) {
        QPixmap iconPixmap;
        if (iconPixmap.loadFromData(app->iconPng, "PNG"))
            qApp->setWindowIcon(QIcon(iconPixmap));
    }
    createWebAppWindow(*app);
    return true;
}

void MainWindow::openUrlInPrimaryWindow(const QUrl &url)
{
    MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
    if (primary != this) {
        primary->openUrlInPrimaryWindow(url);
        return;
    }
    if (!primary->m_primaryTabsInitialized)
        primary->m_primaryTabsInitialized = true;
    qApp->setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.svg")));
    primary->show();
    primary->raise();
    primary->activateWindow();
    primary->createTab(url, true);
}

void MainWindow::openWindowRequestInPrimary(QWebEngineNewWindowRequest &request)
{
    MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
    if (primary != this) {
        primary->openWindowRequestInPrimary(request);
        return;
    }

    primary->activatePrimaryWindow();
    const bool separateWindow = request.destination() == QWebEngineNewWindowRequest::InNewWindow
        || request.destination() == QWebEngineNewWindowRequest::InNewDialog;
    if (separateWindow) {
        MainWindow *popup = primary->createPopupWindow(
            WindowRole::Popup,
            request.requestedGeometry()
        );
        request.openIn(popup->currentWebView()->page());
        popup->show();
        popup->raise();
        popup->activateWindow();
        return;
    }

    const bool activate = request.destination()
        != QWebEngineNewWindowRequest::InNewBackgroundTab;
    QWebEngineView *newView = primary->createTab(QUrl(), activate);
    request.openIn(newView->page());
}

void MainWindow::openSettings()
{
    openSettingsPage(static_cast<int>(SettingsDialog::Page::General));
}

void MainWindow::openWebAppsSettings()
{
    openSettingsPage(static_cast<int>(SettingsDialog::Page::WebApps));
}

void MainWindow::openSettingsPage(int page)
{
    if (!m_ownsBrowserResources && m_primaryWindow) {
        m_primaryWindow->show();
        m_primaryWindow->raise();
        m_primaryWindow->activateWindow();
        m_primaryWindow->openSettingsPage(page);
        return;
    }

    SettingsDialog dialog(
        m_configurationPath,
        m_searchConfigurationPath,
        m_dnsConfigurationPath,
        m_proxyConfigurationPath,
        m_preferences,
        m_searchSettings,
        m_dnsSettings,
        m_proxySettings,
        m_activeProxySettings,
        m_networkBlockedByProxyError,
        m_profile,
        m_historyStore,
        m_webAppStore,
        currentWebView() ? currentWebView()->url() : QUrl(),
        static_cast<SettingsDialog::Page>(page),
        this
    );
    QString webAppToOpen;
    connect(&dialog, &SettingsDialog::webAppOpenRequested, &dialog, [&](const QString &id) {
        webAppToOpen = id;
    });
    QString error;
    if (!dialog.load(&error)) {
        setTrustStatus(tr("Rules error: %1").arg(error), true);
        return;
    }

    if (dialog.exec() == QDialog::Accepted) {
        const bool languageChanged = m_preferences.interfaceLanguage()
            != dialog.preferences().interfaceLanguage();
        const bool proxyRestartRequired = m_networkBlockedByProxyError
            || !hasSameEffectiveProxyConfiguration(
                dialog.proxySettings(),
                m_activeProxySettings
            );
        m_preferences = dialog.preferences();
        m_searchSettings = dialog.searchSettings();
        m_dnsSettings = dialog.dnsSettings();
        m_proxySettings = dialog.proxySettings();
        if (!m_preferences.saveBrowsingHistory() && m_addressCompletionPopup)
            m_addressCompletionPopup->hide();
        updateAddressPlaceholder();
        m_profile->setPersistSessionCookies(m_preferences.persistSessionCookies());
        if (m_preferences.startupMode() == StartupMode::RestoreTabs)
            scheduleSessionSave();
        else
            m_sessionStore.clear();
        for (MainWindow *popup : std::as_const(m_popupWindows)) {
            if (popup) {
                popup->m_preferences = m_preferences;
                popup->m_searchSettings = m_searchSettings;
                popup->m_dnsSettings = m_dnsSettings;
                popup->m_proxySettings = m_proxySettings;
                popup->updateAddressPlaceholder();
                if (!m_preferences.saveBrowsingHistory()
                    && popup->m_addressCompletionPopup) {
                    popup->m_addressCompletionPopup->hide();
                }
            }
        }
        reloadRules();
        if (languageChanged || proxyRestartRequired) {
            QString restartMessage;
            if (languageChanged && proxyRestartRequired) {
                restartMessage = tr(
                    "Restart PanBrowser to apply the new interface language and proxy settings."
                );
            } else if (languageChanged) {
                restartMessage = tr(
                    "Restart PanBrowser to apply the new interface language."
                );
            } else {
                restartMessage = tr("Restart PanBrowser to apply the new proxy settings.");
            }
            QMessageBox::information(
                this,
                tr("Restart required"),
                restartMessage
            );
        }
    }
    if (!webAppToOpen.isEmpty())
        openInstalledWebApp(webAppToOpen);
}

void MainWindow::restoreInitialTabs()
{
    if (m_preferences.startupMode() != StartupMode::RestoreTabs) {
        createTab(m_preferences.startPage());
        return;
    }

    QString error;
    const BrowserSession session = m_sessionStore.load(&error);
    if (!error.isEmpty())
        qWarning().noquote() << "[PanBrowser session]" << error;
    if (session.tabs.isEmpty()) {
        createTab(m_preferences.startPage());
        return;
    }

    m_restoringSession = true;
    for (const SessionTab &tab : session.tabs)
        createTab(tab.url, false, true, tab.title);
    m_tabBar->setCurrentIndex(session.activeIndex);
    m_tabStack->setCurrentIndex(session.activeIndex);
    m_restoringSession = false;
    activatePendingTab(currentWebView());
    updateCurrentTabUi();
}

void MainWindow::scheduleSessionSave()
{
    if (!m_ownsBrowserResources || m_restoringSession || !m_sessionSaveTimer)
        return;
    if (m_preferences.startupMode() == StartupMode::RestoreTabs)
        m_sessionSaveTimer->start();
}

void MainWindow::saveSession()
{
    if (!m_ownsBrowserResources)
        return;
    if (m_sessionSaveTimer)
        m_sessionSaveTimer->stop();
    if (m_discardSessionOnClose
        || m_preferences.startupMode() != StartupMode::RestoreTabs) {
        m_sessionStore.clear();
        return;
    }

    QString error;
    if (!m_sessionStore.save(currentSession(), &error))
        qWarning().noquote() << "[PanBrowser session]" << error;
}

BrowserSession MainWindow::currentSession() const
{
    BrowserSession session;
    session.activeIndex = m_tabBar ? m_tabBar->currentIndex() : 0;
    if (!m_tabStack)
        return session;

    for (int index = 0; index < m_tabStack->count(); ++index) {
        QWebEngineView *webView = qobject_cast<QWebEngineView *>(m_tabStack->widget(index));
        if (!webView)
            continue;
        const BrowserTabState state = m_tabStates.value(webView);
        const QUrl url = state.pendingUrl.isEmpty() ? webView->url() : state.pendingUrl;
        session.tabs.append({url, m_tabBar->tabText(index)});
    }
    return session;
}

void MainWindow::reloadRules()
{
    if (!m_ownsBrowserResources && m_primaryWindow) {
        m_primaryWindow->reloadRules();
        return;
    }

    reloadRulesLocal();
    for (MainWindow *popup : std::as_const(m_popupWindows)) {
        if (popup)
            popup->reloadRulesLocal();
    }
}

void MainWindow::reloadRulesLocal()
{
    if (m_ownsBrowserResources)
        m_configurationPath = ensureConfiguration();
    QString error;
    if (!m_trustPolicy.load(m_configurationPath, &error)) {
        m_ruleCount->setText(tr("Rules unavailable"));
        setTrustStatus(tr("Rules error: %1").arg(error), true);
        return;
    }

    const qsizetype count = m_trustPolicy.ruleCount();
    m_ruleCount->setText(
        tr("%n custom rule(s)", nullptr, count)
    );
    setTrustStatus(tr("Trust rules loaded"));
}

void MainWindow::handleCertificateError(
    QWebEngineView *webView,
    const QWebEngineCertificateError &error
)
{
    QWebEngineCertificateError decision(error);
    const QString host = error.url().host();
    const TrustRule *rule = m_trustPolicy.ruleForHost(host);

    if (!rule) {
        qWarning().noquote() << "[PanBrowser TLS] rejected unconfigured host" << host
                             << error.description();
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            tr("Blocked %1: no matching trust rule").arg(host),
            true
        );
        return;
    }

    if (!error.isOverridable()) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            tr("Blocked %1: non-overridable certificate error").arg(host),
            true
        );
        return;
    }

    if (error.type() != QWebEngineCertificateError::CertificateAuthorityInvalid) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            tr("Blocked %1: only an unknown CA may be overridden").arg(host),
            true
        );
        return;
    }

    if (rule->mode == TrustMode::SystemOnly) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            tr("Blocked %1: system validation failed").arg(host),
            true
        );
        return;
    }

    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        error.certificateChain(),
        rule->anchors,
        host,
        rule->mode == TrustMode::CustomOnly
    );

    if (result.trusted) {
        decision.acceptCertificate();
        qInfo().noquote() << "[PanBrowser TLS] accepted" << host << "using rule" << rule->name;
        BrowserTabState &state = m_tabStates[webView];
        if (isSameWebOrigin(error.url(), state.topLevelUrl)) {
            state.lastAcceptedRule = rule->name;
            setTabTrustStatus(webView,
                tr("Secure · %1 · %2").arg(rule->name, result.explanation)
            );
        }
    } else {
        decision.rejectCertificate();
        qWarning().noquote() << "[PanBrowser TLS] rejected" << host << result.explanation;
        setTabTrustStatus(webView,
            tr("Blocked %1: %2").arg(host, result.explanation),
            true
        );
    }
}

void MainWindow::setTabTrustStatus(QWebEngineView *webView, const QString &text, bool error)
{
    if (!webView || !m_tabStates.contains(webView))
        return;

    BrowserTabState &state = m_tabStates[webView];
    state.trustStatus = text;
    state.trustError = error;
    if (webView == currentWebView())
        setTrustStatus(text, error);
}

void MainWindow::setTrustStatus(const QString &text, bool error)
{
    m_trustStatus->setText(text);
    const bool secure = !error && text.startsWith(tr("Secure"));
    m_trustStatus->setProperty(
        "state",
        error ? QStringLiteral("error")
              : secure ? QStringLiteral("secure") : QStringLiteral("neutral")
    );
    m_securityIndicator->setIcon(QIcon(
        error ? QStringLiteral(":/assets/icons/triangle-alert.svg")
              : secure ? QStringLiteral(":/assets/icons/shield-check.svg") : QString()
    ));
    m_trustStatus->style()->unpolish(m_trustStatus);
    m_trustStatus->style()->polish(m_trustStatus);
    m_trustStatus->update();
}

void MainWindow::restoreWindowPlacement()
{
    QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
    settings.beginGroup(QStringLiteral("MainWindow"));
    const QByteArray geometryData = settings.value(QStringLiteral("geometry")).toByteArray();
    const QByteArray stateData = settings.value(QStringLiteral("state")).toByteArray();
    settings.endGroup();

    if (geometryData.isEmpty())
        return;

    restoreGeometry(geometryData);
    if (!stateData.isEmpty())
        restoreState(stateData, 1);

    QList<QRect> availableScreens;
    for (const QScreen *screen : QGuiApplication::screens())
        availableScreens.append(screen->availableGeometry());

    const QScreen *primaryScreen = QGuiApplication::primaryScreen();
    const QRect fallback = primaryScreen ? primaryScreen->availableGeometry() : QRect();
    const bool specialState = isMaximized() || isFullScreen();
    const QRect restored = specialState ? normalGeometry() : geometry();
    const QRect adjusted = adjustedWindowGeometry(restored, availableScreens, fallback);
    if (adjusted == restored)
        return;

    const Qt::WindowStates savedState = windowState();
    setWindowState(Qt::WindowNoState);
    setGeometry(adjusted);
    setWindowState(savedState);
}

QString MainWindow::ensureConfiguration()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(QDir(directory).filePath(QStringLiteral("Certificates")));
    const QString path = QDir(directory).filePath(QStringLiteral("rules.json"));
    if (QFile::exists(path))
        return path;

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("startPage"), QStringLiteral("https://example.com"));
    root.insert(QStringLiteral("rules"), QJsonArray());

    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
    return path;
}

void MainWindow::initializeSearchSettings()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);
    m_searchConfigurationPath = QDir(directory).filePath(
        QStringLiteral("search-engines.json")
    );
    m_searchSettings = SearchSettings::defaults();

    QString error;
    if (QFile::exists(m_searchConfigurationPath)) {
        SearchSettings loaded;
        if (loaded.load(m_searchConfigurationPath, &error)) {
            m_searchSettings = loaded;
        } else {
            qWarning().noquote() << "[PanBrowser search settings]" << error
                                 << "Using in-memory defaults; the file was not overwritten.";
        }
        return;
    }

    if (!m_searchSettings.save(m_searchConfigurationPath, &error))
        qWarning().noquote() << "[PanBrowser search settings]" << error;
}

void MainWindow::initializeDnsSettings()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);
    m_dnsConfigurationPath = QDir(directory).filePath(QStringLiteral("dns-settings.json"));
    m_dnsSettings = DnsSettings::defaults();

    QString error;
    if (QFile::exists(m_dnsConfigurationPath)) {
        DnsSettings loaded;
        if (loaded.load(m_dnsConfigurationPath, &error)) {
            m_dnsSettings = loaded;
        } else {
            qWarning().noquote() << "[PanBrowser DNS settings]" << error
                                 << "Using in-memory defaults; the file was not overwritten.";
        }
    } else if (!m_dnsSettings.save(m_dnsConfigurationPath, &error)) {
        qWarning().noquote() << "[PanBrowser DNS settings]" << error;
    }

    if (applyDnsSettings(m_dnsSettings, &error))
        return;

    qWarning().noquote() << "[PanBrowser DNS settings]" << error
                         << "Falling back to system DNS without overwriting the file.";
    m_dnsSettings.setMode(DnsResolutionMode::System);
    QString fallbackError;
    if (!applyDnsSettings(m_dnsSettings, &fallbackError))
        qWarning().noquote() << "[PanBrowser DNS settings]" << fallbackError;
}

void MainWindow::initializeProxySettings()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);
    m_proxyConfigurationPath = QDir(directory).filePath(
        QStringLiteral("proxy-settings.json")
    );
    m_proxySettings = ProxySettings::defaults();
    m_activeProxySettings = ProxySettings::defaults();
    m_proxyConfigurationError.clear();
    m_networkBlockedByProxyError = false;

    QString error;
    if (QFile::exists(m_proxyConfigurationPath)) {
        ProxySettings loaded;
        if (loaded.load(m_proxyConfigurationPath, &error)) {
            m_proxySettings = loaded;
        } else {
            qWarning().noquote() << "[PanBrowser proxy settings]" << error
                                 << "Blocking network access; the file was not overwritten.";
            m_proxyConfigurationError = error;
            m_networkBlockedByProxyError = true;
            return;
        }
    } else if (!m_proxySettings.save(m_proxyConfigurationPath, &error)) {
        qWarning().noquote() << "[PanBrowser proxy settings]" << error;
    }

    m_activeProxySettings = m_proxySettings;
    if (applyProxySettings(m_activeProxySettings, &error))
        return;

    qWarning().noquote() << "[PanBrowser proxy settings]" << error
                         << "Blocking network access without overwriting the file.";
    m_proxyConfigurationError = error;
    m_networkBlockedByProxyError = true;
}

void MainWindow::showProxyConfigurationError()
{
    if (!m_networkBlockedByProxyError)
        return;

    show();
    raise();
    activateWindow();
    setTrustStatus(tr("Network blocked because proxy settings are invalid."), true);

    QMessageBox message(this);
    message.setIcon(QMessageBox::Critical);
    message.setTextFormat(Qt::PlainText);
    message.setWindowTitle(tr("Network blocked"));
    message.setText(
        tr("PanBrowser blocked network access because the proxy settings could not be loaded.")
    );
    message.setInformativeText(
        tr("%1\n\nOpen Proxy settings, save a valid configuration, and restart PanBrowser.")
            .arg(m_proxyConfigurationError)
    );
    QPushButton *openSettingsButton = message.addButton(
        tr("Open Proxy settings"),
        QMessageBox::ActionRole
    );
    message.addButton(QMessageBox::Close);
    message.exec();
    if (message.clickedButton() == openSettingsButton) {
        openSettingsPage(static_cast<int>(SettingsDialog::Page::Proxy));
    }
}
