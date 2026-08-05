#include "MainWindow.h"

#include "BrowserPage.h"
#include "BrowserProfile.h"
#include "CertificateTrustValidator.h"
#include "DownloadManager.h"
#include "DownloadsPanel.h"
#include "ExternalNavigationPolicy.h"
#include "HistoryCompletionPopup.h"
#include "PermissionController.h"
#include "PermissionPrompt.h"
#include "SettingsDialog.h"
#include "WindowPlacement.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QIcon>
#include <QProgressBar>
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
#include <QWebEngineCertificateError>
#include <QWebEngineHistory>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <utility>

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(nullptr, nullptr, nullptr, nullptr, WindowRole::Primary, parent)
{
}

MainWindow::MainWindow(
    BrowserProfile *sharedProfile,
    DownloadManager *sharedDownloadManager,
    HistoryStore *sharedHistoryStore,
    MainWindow *primaryWindow,
    WindowRole role,
    QWidget *parent
)
    : QMainWindow(parent, role == WindowRole::Primary ? Qt::WindowFlags() : Qt::Window)
    , m_sessionStore(QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    ).filePath(QStringLiteral("session.json")))
{
    m_primaryWindow = primaryWindow ? primaryWindow : this;
    m_ownsBrowserResources = role == WindowRole::Primary;

    if (m_ownsBrowserResources) {
        m_configurationPath = ensureConfiguration();
        QString bootstrapError;
        m_trustPolicy.load(m_configurationPath, &bootstrapError);
        m_preferences = BrowserPreferences::load(m_trustPolicy.startPage());
        initializeSearchSettings();
        QString dataResetError;
        if (!BrowserProfile::applyPendingDataReset(&dataResetError))
            qWarning().noquote() << "[PanBrowser data reset]" << dataResetError;
        m_profile = new BrowserProfile(m_preferences.persistSessionCookies());
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
    } else {
        Q_ASSERT(sharedProfile);
        Q_ASSERT(sharedDownloadManager);
        Q_ASSERT(sharedHistoryStore);
        Q_ASSERT(primaryWindow);
        m_profile = sharedProfile;
        m_downloadManager = sharedDownloadManager;
        m_historyStore = sharedHistoryStore;
        m_configurationPath = primaryWindow->m_configurationPath;
        m_searchConfigurationPath = primaryWindow->m_searchConfigurationPath;
        m_trustPolicy = primaryWindow->m_trustPolicy;
        m_preferences = primaryWindow->m_preferences;
        m_searchSettings = primaryWindow->m_searchSettings;
        setAttribute(Qt::WA_DeleteOnClose);
    }

    createInterface();
    if (!m_historyError.isEmpty())
        setTrustStatus(QStringLiteral("History unavailable: %1").arg(m_historyError), true);
    m_permissionController = new PermissionController(m_permissionPrompt, this);
    if (m_ownsBrowserResources) {
        restoreWindowPlacement();
        reloadRules();
        restoreInitialTabs();
    } else {
        reloadRulesLocal();
        createTab(QUrl());
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
    m_profile = nullptr;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_ownsBrowserResources) {
        saveSession();
        QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
        settings.beginGroup(QStringLiteral("MainWindow"));
        settings.setValue(QStringLiteral("geometry"), saveGeometry());
        settings.setValue(QStringLiteral("state"), saveState(1));
        settings.endGroup();
        settings.sync();

        const QList<QPointer<MainWindow>> popups = m_popupWindows;
        for (MainWindow *popup : popups) {
            if (popup)
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
    setWindowTitle(QStringLiteral("PanBrowser"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.svg")));

    m_tabStack = new QStackedWidget(this);
    m_tabStack->setObjectName(QStringLiteral("browserTabs"));
    setCentralWidget(m_tabStack);

    QToolBar *tabsToolbar = new QToolBar(QStringLiteral("Tabs"), this);
    tabsToolbar->setObjectName(QStringLiteral("tabsBar"));
    tabsToolbar->setMovable(false);
    tabsToolbar->setFloatable(false);
    tabsToolbar->setIconSize(QSize(17, 17));

    m_tabBar = new QTabBar(tabsToolbar);
    m_tabBar->setObjectName(QStringLiteral("browserTabBar"));
    m_tabBar->setDocumentMode(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    m_tabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tabsToolbar->addWidget(m_tabBar);

    QToolButton *newTabButton = new QToolButton(tabsToolbar);
    newTabButton->setObjectName(QStringLiteral("newTabButton"));
    newTabButton->setIcon(QIcon(QStringLiteral(":/assets/icons/plus.svg")));
    newTabButton->setToolTip(QStringLiteral("New Tab (⌘T)"));
    tabsToolbar->addWidget(newTabButton);

    addToolBar(Qt::TopToolBarArea, tabsToolbar);
    addToolBarBreak(Qt::TopToolBarArea);

    QToolBar *toolbar = new QToolBar(QStringLiteral("Navigation"), this);
    toolbar->setObjectName(QStringLiteral("navigationBar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(19, 19));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_backAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-left.svg")),
        QStringLiteral("Back")
    );
    m_forwardAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")),
        QStringLiteral("Forward")
    );
    m_reloadAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")),
        QStringLiteral("Reload")
    );
    m_backAction->setEnabled(false);
    m_forwardAction->setEnabled(false);
    m_reloadAction->setEnabled(false);

    m_address = new QLineEdit(toolbar);
    m_address->setObjectName(QStringLiteral("addressBar"));
    updateAddressPlaceholder();
    m_address->setClearButtonEnabled(true);
    m_securityIndicator = m_address->addAction(
        QIcon(),
        QLineEdit::LeadingPosition
    );
    m_historyCompletionPopup = new HistoryCompletionPopup(m_address, this);
    m_historySuggestionTimer = new QTimer(this);
    m_historySuggestionTimer->setSingleShot(true);
    m_historySuggestionTimer->setInterval(100);
    toolbar->addWidget(m_address);
    QAction *go = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")),
        QStringLiteral("Go")
    );
    m_downloadButton = new DownloadButton(toolbar);
    m_downloadButton->setObjectName(QStringLiteral("downloadsButton"));
    m_downloadButton->setIcon(QIcon(QStringLiteral(":/assets/icons/download.svg")));
    m_downloadButton->setToolTip(QStringLiteral("Downloads"));
    toolbar->addWidget(m_downloadButton);
    addToolBar(Qt::TopToolBarArea, toolbar);

    addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *permissionToolbar = new QToolBar(QStringLiteral("Permission request"), this);
    permissionToolbar->setObjectName(QStringLiteral("permissionBar"));
    permissionToolbar->setMovable(false);
    permissionToolbar->setFloatable(false);
    m_permissionPrompt = new PermissionPrompt(permissionToolbar);
    permissionToolbar->addWidget(m_permissionPrompt);
    addToolBar(Qt::TopToolBarArea, permissionToolbar);
    permissionToolbar->hide();

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
        if (isActiveWindow())
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
    connect(m_address, &QLineEdit::returnPressed, this, &MainWindow::navigateFromAddressBar);
    connect(m_address, &QLineEdit::textEdited, this, [this] {
        if (!m_preferences.saveBrowsingHistory()
            || !m_historyStore
            || !m_historyStore->isOpen()) {
            m_historyCompletionPopup->hide();
            return;
        }
        m_historySuggestionTimer->start();
    });
    connect(m_historySuggestionTimer, &QTimer::timeout, this, &MainWindow::showHistorySuggestions);
    connect(
        m_historyCompletionPopup,
        &HistoryCompletionPopup::urlActivated,
        this,
        [this](const QUrl &url) {
            m_address->setText(url.toString());
            navigateFromAddressBar();
        }
    );

    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("PanBrowser"));
    QAction *newTabAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/plus.svg")),
        QStringLiteral("New Tab")
    );
    newTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(newTabAction, &QAction::triggered, this, [this] {
        createTab(m_preferences.startPage());
    });

    QAction *closeTabAction = fileMenu->addAction(QStringLiteral("Close Tab"));
    closeTabAction->setShortcut(QKeySequence::Close);
    connect(closeTabAction, &QAction::triggered, this, [this] {
        closeTab(m_tabBar->currentIndex());
    });

    fileMenu->addSeparator();
    QAction *settingsAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/settings.svg")),
        QStringLiteral("Settings…")
    );
    settingsAction->setMenuRole(QAction::PreferencesRole);
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, [this] {
        openSettings(false, false);
    });

    QAction *historyAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/history.svg")),
        QStringLiteral("History…")
    );
    historyAction->setMenuRole(QAction::NoRole);
    historyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    connect(historyAction, &QAction::triggered, this, [this] {
        openSettings(false, true);
    });

    QAction *editRulesAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/shield-check.svg")),
        QStringLiteral("Trust Rules…")
    );
    editRulesAction->setMenuRole(QAction::NoRole);
    connect(editRulesAction, &QAction::triggered, this, [this] {
        openSettings(true, false);
    });

    fileMenu->addSeparator();
    QAction *reloadRulesAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")),
        QStringLiteral("Reload Trust Rules")
    );
    reloadRulesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    connect(reloadRulesAction, &QAction::triggered, this, &MainWindow::reloadRules);

    QAction *showConfiguration = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/folder-open.svg")),
        QStringLiteral("Show Configuration Folder")
    );
    connect(showConfiguration, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QFileInfo(m_configurationPath).absolutePath()
        ));
    });

    m_trustStatus = new QLabel(QStringLiteral("Ready"), this);
    m_trustStatus->setObjectName(QStringLiteral("trustStatus"));
    m_ruleCount = new QLabel(QStringLiteral("No rules loaded"), this);
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
        primary,
        role,
        primary
    );
    primary->m_popupWindows.append(popup);
    connect(popup, &QObject::destroyed, primary, [primary, popup] {
        primary->m_popupWindows.removeAll(popup);
    });
    popup->applyPopupGeometry(requestedGeometry);
    return popup;
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
    webView->setPage(new BrowserPage(m_profile, webView));
    BrowserTabState state;
    if (deferred) {
        state.pendingUrl = url;
        state.suppressNextHistoryVisit = true;
    }
    m_tabStates.insert(webView, state);
    connectBrowserSignals(webView);

    const int stackIndex = m_tabStack->addWidget(webView);
    const QString initialTitle = restoredTitle.isEmpty()
        ? (url.host().isEmpty() ? QStringLiteral("New Tab") : url.host())
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
    connect(webView, &QWebEngineView::urlChanged, this, [this, webView](const QUrl &url) {
        m_tabStates[webView].pendingUrl.clear();
        const int index = m_tabStack->indexOf(webView);
        if (index >= 0 && webView->title().isEmpty()) {
            const QString label = url.host().isEmpty() ? QStringLiteral("New Tab") : url.host();
            m_tabBar->setTabText(index, label);
            m_tabBar->setTabToolTip(index, url.toString());
        }
        if (webView == currentWebView())
            m_address->setText(url.toString());
        if (m_historySuggestionTimer)
            m_historySuggestionTimer->stop();
        if (m_historyCompletionPopup)
            m_historyCompletionPopup->hide();
        updateNavigationActions();
        scheduleSessionSave();
    });
    connect(webView, &QWebEngineView::titleChanged, this, [this, webView](const QString &title) {
        const int index = m_tabStack->indexOf(webView);
        if (index >= 0) {
            const QString label = title.isEmpty()
                ? (webView->url().host().isEmpty() ? QStringLiteral("New Tab") : webView->url().host())
                : title;
            m_tabBar->setTabText(index, label);
            m_tabBar->setTabToolTip(index, title.isEmpty() ? webView->url().toString() : title);
        }
        if (webView == currentWebView())
            setWindowTitle(title.isEmpty() ? QStringLiteral("PanBrowser") : title + QStringLiteral(" — PanBrowser"));
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
        state.loading = true;
        state.progress = 0;
        setTabTrustStatus(webView, QStringLiteral("Loading…"));
        if (webView == currentWebView()) {
            m_progress->setValue(0);
            m_progress->show();
        }
        updateNavigationActions();
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
            if (state.lastAcceptedRule.isEmpty())
                setTabTrustStatus(webView, QStringLiteral("Secure · Chromium system trust"));
        } else if (state.lastAcceptedRule.isEmpty()) {
            setTabTrustStatus(webView, QStringLiteral("Page loading failed"), true);
        }
        state.pendingHistoryTransition = HistoryTransition::Other;
        state.suppressNextHistoryVisit = false;
        updateNavigationActions();
    });
    connect(
        static_cast<BrowserPage *>(webView->page()),
        &BrowserPage::mainFrameNavigationRequested,
        this,
        [this, webView](const QUrl &, int navigationType) {
            BrowserTabState &state = m_tabStates[webView];
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
        ? QStringLiteral("The current page")
        : QStringLiteral("%1://%2").arg(sourceUrl.scheme(), sourceUrl.host());
    QString target = url.toDisplayString(QUrl::RemovePassword);
    constexpr qsizetype maximumDisplayedUrlLength = 700;
    if (target.size() > maximumDisplayedUrlLength)
        target = target.left(maximumDisplayedUrlLength) + QStringLiteral("…");

    auto *dialog = new QMessageBox(this);
    dialog->setWindowTitle(QStringLiteral("Open external application?"));
    dialog->setIcon(QMessageBox::Question);
    dialog->setTextFormat(Qt::PlainText);
    dialog->setText(QStringLiteral("Open this link in another application?"));
    dialog->setInformativeText(
        QStringLiteral("%1 wants to open:\n\n%2").arg(source, target)
    );
    auto *openButton = dialog->addButton(
        QStringLiteral("Open application"),
        QMessageBox::AcceptRole
    );
    auto *cancelButton = dialog->addButton(QStringLiteral("Cancel"), QMessageBox::RejectRole);
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
                QStringLiteral("No application is available for the %1 link").arg(url.scheme()),
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
        setWindowTitle(QStringLiteral("PanBrowser"));
        m_progress->hide();
        updateNavigationActions();
        return;
    }

    m_address->setText(webView->url().toString());
    if (webView->url().isEmpty() && !m_tabStates.value(webView).pendingUrl.isEmpty())
        m_address->setText(m_tabStates.value(webView).pendingUrl.toString());
    const QString title = webView->title();
    setWindowTitle(title.isEmpty() ? QStringLiteral("PanBrowser") : title + QStringLiteral(" — PanBrowser"));

    const BrowserTabState state = m_tabStates.value(webView);
    setTrustStatus(state.trustStatus, state.trustError);
    m_progress->setValue(state.progress);
    m_progress->setVisible(state.loading);
    updateNavigationActions();
}

void MainWindow::updateNavigationActions()
{
    QWebEngineView *webView = currentWebView();
    m_backAction->setEnabled(webView && webView->history()->canGoBack());
    m_forwardAction->setEnabled(webView && webView->history()->canGoForward());
    m_reloadAction->setEnabled(webView != nullptr);
}

void MainWindow::updateAddressPlaceholder()
{
    if (!m_address)
        return;
    const SearchEngineSettings *engine = m_searchSettings.defaultEngine();
    m_address->setPlaceholderText(
        engine ? QStringLiteral("Search with %1 or enter an address").arg(engine->name)
               : QStringLiteral("Enter an address")
    );
}

void MainWindow::navigateFromAddressBar()
{
    if (m_historySuggestionTimer)
        m_historySuggestionTimer->stop();
    if (m_historyCompletionPopup)
        m_historyCompletionPopup->hide();
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

void MainWindow::showHistorySuggestions()
{
    if (!m_historyCompletionPopup
        || !m_preferences.saveBrowsingHistory()
        || !m_historyStore
        || !m_historyStore->isOpen()) {
        return;
    }
    const QString input = m_address->text().trimmed();
    if (input.isEmpty()
        || input.startsWith(QLatin1Char('?'))
        || input.startsWith(QLatin1Char('@'))) {
        m_historyCompletionPopup->hide();
        return;
    }
    QString error;
    const QList<HistorySuggestion> suggestions = m_historyStore->suggestions(input, 8, &error);
    if (!error.isEmpty()) {
        qWarning().noquote() << "[PanBrowser history]" << error;
        m_historyCompletionPopup->hide();
        return;
    }
    m_historyCompletionPopup->showSuggestions(suggestions);
}

void MainWindow::openSettings(bool trustRules, bool history)
{
    if (!m_ownsBrowserResources && m_primaryWindow) {
        m_primaryWindow->show();
        m_primaryWindow->raise();
        m_primaryWindow->activateWindow();
        m_primaryWindow->openSettings(trustRules, history);
        return;
    }

    SettingsDialog dialog(
        m_configurationPath,
        m_searchConfigurationPath,
        m_preferences,
        m_searchSettings,
        m_profile,
        m_historyStore,
        currentWebView() ? currentWebView()->url() : QUrl(),
        trustRules ? SettingsDialog::Page::TrustRules
                   : (history ? SettingsDialog::Page::History
                              : SettingsDialog::Page::General),
        this
    );
    QString error;
    if (!dialog.load(&error)) {
        setTrustStatus(QStringLiteral("Rules error: %1").arg(error), true);
        return;
    }

    if (dialog.exec() == QDialog::Accepted) {
        m_preferences = dialog.preferences();
        m_searchSettings = dialog.searchSettings();
        if (!m_preferences.saveBrowsingHistory() && m_historyCompletionPopup)
            m_historyCompletionPopup->hide();
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
                popup->updateAddressPlaceholder();
                if (!m_preferences.saveBrowsingHistory()
                    && popup->m_historyCompletionPopup) {
                    popup->m_historyCompletionPopup->hide();
                }
            }
        }
        reloadRules();
    }
}

void MainWindow::restoreInitialTabs()
{
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1) {
        createTab(QUrl::fromUserInput(arguments.at(1)));
        return;
    }

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
        m_ruleCount->setText(QStringLiteral("Rules unavailable"));
        setTrustStatus(QStringLiteral("Rules error: %1").arg(error), true);
        return;
    }

    const qsizetype count = m_trustPolicy.ruleCount();
    m_ruleCount->setText(
        QStringLiteral("%1 custom rule%2").arg(count).arg(count == 1 ? QString() : QStringLiteral("s"))
    );
    setTrustStatus(QStringLiteral("Trust rules loaded"));
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
            QStringLiteral("Blocked %1: no matching trust rule").arg(host),
            true
        );
        return;
    }

    if (!error.isOverridable()) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: non-overridable certificate error").arg(host),
            true
        );
        return;
    }

    if (error.type() != QWebEngineCertificateError::CertificateAuthorityInvalid) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: only an unknown CA may be overridden").arg(host),
            true
        );
        return;
    }

    if (rule->mode == TrustMode::SystemOnly) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: system validation failed").arg(host),
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
        m_tabStates[webView].lastAcceptedRule = rule->name;
        qInfo().noquote() << "[PanBrowser TLS] accepted" << host << "using rule" << rule->name;
        setTabTrustStatus(webView,
            QStringLiteral("Secure · %1 · %2").arg(rule->name, result.explanation)
        );
    } else {
        decision.rejectCertificate();
        qWarning().noquote() << "[PanBrowser TLS] rejected" << host << result.explanation;
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: %2").arg(host, result.explanation),
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
    const bool secure = !error && text.startsWith(QStringLiteral("Secure"));
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
