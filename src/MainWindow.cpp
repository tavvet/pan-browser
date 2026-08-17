#include "MainWindow.h"

#include "AddressLineEdit.h"
#include "AddressSuggestion.h"
#include "BookmarkDialog.h"
#include "BookmarkStore.h"
#include "BookmarksDialog.h"
#include "BrowserPage.h"
#include "BrowserProfile.h"
#include "CrossDomainPrompt.h"
#include "CrossDomainPromptController.h"
#include "BrowserInteraction.h"
#include "BrowserShortcut.h"
#include "BrowserFullScreenController.h"
#include "BrowserTabBar.h"
#include "CertificateTrustValidator.h"
#include "DownloadManager.h"
#include "DownloadsPanel.h"
#include "DetachedVideoWindow.h"
#include "ExternalNavigationPolicy.h"
#include "FindBar.h"
#include "AddressCompletionPopup.h"
#include "HttpAuthenticationController.h"
#include "PageZoom.h"
#include "PermissionController.h"
#include "PermissionPrompt.h"
#include "PrivateData.h"
#include "ProxyAuthenticationController.h"
#include "ReaderModeController.h"
#include "SettingsDialog.h"
#include "SiteDomain.h"
#include "TabNavigation.h"
#include "VideoElementBridge.h"
#include "VotUserscriptManager.h"
#include "WindowPlacement.h"
#include "WindowChrome.h"
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
#include <QKeyEvent>
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
#include <QVarLengthArray>
#include <QWebEngineCertificateError>
#include <QWebEngineFindTextResult>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineHistory>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr qsizetype maximumClosedTabs = 25;

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

QString crossDomainRequestKey(const QString &sourceSite, const QString &targetHost)
{
    return SiteDomain::registrableDomain(sourceSite)
        + QLatin1Char('\n')
        + SiteDomain::normalizeHost(targetHost);
}

template<typename Callback>
bool triggerShortcutAction(
    QAction *action,
    QKeyEvent *event,
    bool enabled,
    Callback &&callback
)
{
    if (!action
        || !BrowserShortcut::matches(*event, action->shortcuts())) {
        return false;
    }
    event->accept();
    if (!event->isAutoRepeat() && enabled)
        std::forward<Callback>(callback)();
    return true;
}

bool hasPageZoomModifier(Qt::KeyboardModifiers modifiers)
{
    return modifiers.testFlag(Qt::ControlModifier)
        && !modifiers.testFlag(Qt::AltModifier)
        && !modifiers.testFlag(Qt::MetaModifier);
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
    m_integratedWindowChrome = role != WindowRole::WebApp
        && WindowChromeController::platformSupportsIntegratedTitleBar();
    if (m_integratedWindowChrome)
        WindowChromeController::applyIntegratedTitleBar(this);

    if (m_ownsBrowserResources) {
        m_configurationPath = ensureConfiguration();
        QString bootstrapError;
        m_trustPolicy.load(m_configurationPath, &bootstrapError);
        m_preferences = BrowserPreferences::load(
            m_trustPolicy.startPage(),
            &m_startupError
        );
        if (!m_startupError.isEmpty())
            return;
        QString readerSettingsError;
        m_readerSettings = ReaderSettings::load(&readerSettingsError);
        if (!readerSettingsError.isEmpty())
            qWarning().noquote() << "[PanBrowser reader settings]" << readerSettingsError;
        initializeSearchSettings();
        initializeUserAgentSettings();
        initializeDnsSettings();
        initializeProxySettings();
        initializeCrossDomainSettings();
        initializeVideoTranslationSettings();
        QString dataResetError;
        if (!BrowserProfile::applyPendingDataReset(&dataResetError))
            qWarning().noquote() << "[PanBrowser data reset]" << dataResetError;
        m_profile = new BrowserProfile(
            m_preferences.persistSessionCookies(),
            nullptr,
            m_networkBlockedByProxyError,
            m_crossDomainSettings,
            m_crossDomainConfigurationPath,
            m_activeUserAgentSettings
        );
        m_votUserscriptManager = new VotUserscriptManager(
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("vot-storage.json")),
            m_profile,
            m_profile
        );
        connect(
            m_votUserscriptManager,
            &VotUserscriptManager::settingsChanged,
            this,
            [this](const VideoTranslationSettings &settings) {
                m_videoTranslationSettings = settings;
            }
        );
        connect(
            m_votUserscriptManager,
            &VotUserscriptManager::stateChanged,
            this,
            [this] {
                if (m_votUserscriptManager
                    && m_votUserscriptManager->state() == VotUserscriptState::Error) {
                    qWarning().noquote()
                        << "[PanBrowser VOT]"
                        << m_votUserscriptManager->statusText();
                }
            }
        );
        m_votUserscriptManager->applySettings(m_videoTranslationSettings);
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
        m_httpAuthenticationController = new HttpAuthenticationController(this);
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
        m_userAgentConfigurationPath = primaryWindow->m_userAgentConfigurationPath;
        m_dnsConfigurationPath = primaryWindow->m_dnsConfigurationPath;
        m_proxyConfigurationPath = primaryWindow->m_proxyConfigurationPath;
        m_crossDomainConfigurationPath = primaryWindow->m_crossDomainConfigurationPath;
        m_videoTranslationConfigurationPath =
            primaryWindow->m_videoTranslationConfigurationPath;
        m_trustPolicy = primaryWindow->m_trustPolicy;
        m_preferences = primaryWindow->m_preferences;
        m_searchSettings = primaryWindow->m_searchSettings;
        m_userAgentSettings = primaryWindow->m_userAgentSettings;
        m_activeUserAgentSettings = primaryWindow->m_activeUserAgentSettings;
        m_dnsSettings = primaryWindow->m_dnsSettings;
        m_proxySettings = primaryWindow->m_proxySettings;
        m_activeProxySettings = primaryWindow->m_activeProxySettings;
        m_crossDomainSettings = primaryWindow->m_crossDomainSettings;
        m_videoTranslationSettings = primaryWindow->m_videoTranslationSettings;
        m_readerSettings = primaryWindow->m_readerSettings;
        m_votUserscriptManager = primaryWindow->m_votUserscriptManager;
        m_proxyConfigurationError = primaryWindow->m_proxyConfigurationError;
        m_userAgentConfigurationError = primaryWindow->m_userAgentConfigurationError;
        m_networkBlockedByProxyError = primaryWindow->m_networkBlockedByProxyError;
        m_proxyAuthenticationController = primaryWindow->m_proxyAuthenticationController;
        m_httpAuthenticationController = primaryWindow->m_httpAuthenticationController;
        setAttribute(Qt::WA_DeleteOnClose);
    }

    createInterface();
    if (m_votUserscriptManager) {
        connect(
            m_votUserscriptManager,
            &VotUserscriptManager::proxyAuthenticationRequired,
            this,
            [this](
                BrowserPage *page,
                const QUrl &requestUrl,
                QAuthenticator *authenticator,
                const QString &proxyHost,
                bool *handled
            ) {
                QWebEngineView *webView = webViewForPage(page);
                if (!webView)
                    return;
                if (handled)
                    *handled = true;
                if (m_proxyAuthenticationController) {
                    m_proxyAuthenticationController->requestAuthentication(
                        interactionParentForTab(webView),
                        requestUrl,
                        authenticator,
                        proxyHost
                    );
                } else if (authenticator) {
                    *authenticator = QAuthenticator();
                }
            }
        );
    }
    m_fullScreenController = std::make_unique<BrowserFullScreenController>(this);
    connect(
        m_fullScreenController.get(),
        &BrowserFullScreenController::nativeExitRequested,
        this,
        &MainWindow::requestBrowserFullScreenExit
    );
    if (!m_historyError.isEmpty())
        setTrustStatus(tr("History unavailable: %1").arg(m_historyError), true);
    m_permissionController = new PermissionController(m_permissionPrompt, this);
    m_crossDomainPromptController = new CrossDomainPromptController(
        m_profile,
        m_crossDomainPrompt,
        this
    );
    connect(
        m_crossDomainPromptController,
        &CrossDomainPromptController::errorOccurred,
        this,
        [this](const QString &error) {
            QMessageBox::warning(
                this,
                tr("Cannot save site connection decision"),
                error
            );
        }
    );
    MainWindow *promptCoordinator = m_primaryWindow ? m_primaryWindow : this;
    connect(
        m_crossDomainPromptController,
        &CrossDomainPromptController::requestFinished,
        promptCoordinator,
        [promptCoordinator, this](
            const QString &sourceSite,
            const QString &targetHost
        ) {
            const QString key = crossDomainRequestKey(sourceSite, targetHost);
            const auto route = promptCoordinator->m_crossDomainPromptRoutes.find(key);
            if (route != promptCoordinator->m_crossDomainPromptRoutes.end()
                && route->window == this) {
                promptCoordinator->m_crossDomainPromptRoutes.erase(route);
            }
        }
    );
    connect(
        m_crossDomainPromptController,
        &CrossDomainPromptController::requestReanchorRequested,
        promptCoordinator,
        [promptCoordinator, this](
            QWebEngineView *webView,
            const QList<CrossDomainAffectedView> &affectedViews,
            const QList<CrossDomainPromptSource> &sources,
            const QString &sourceSite,
            const QString &targetHost,
            int resourceType
        ) {
            const QString key = crossDomainRequestKey(sourceSite, targetHost);
            MainWindow *newPromptWindow = nullptr;
            QList<MainWindow *> windows{promptCoordinator};
            for (MainWindow *window : std::as_const(promptCoordinator->m_popupWindows)) {
                if (window)
                    windows.append(window);
            }
            for (MainWindow *window : std::as_const(windows)) {
                if (window->m_tabStates.contains(webView)) {
                    newPromptWindow = window;
                    break;
                }
            }

            auto route = promptCoordinator->m_crossDomainPromptRoutes.find(key);
            if (!newPromptWindow) {
                if (route != promptCoordinator->m_crossDomainPromptRoutes.end()
                    && route->window == this) {
                    promptCoordinator->m_crossDomainPromptRoutes.erase(route);
                }
                for (const CrossDomainPromptSource &source : sources) {
                    promptCoordinator->m_profile->dismissCrossDomainRequestSource(
                        sourceSite,
                        targetHost,
                        source.url,
                        source.originOnly
                    );
                }
                return;
            }
            if (route == promptCoordinator->m_crossDomainPromptRoutes.end()) {
                promptCoordinator->m_crossDomainPromptRoutes.insert(
                    key,
                    {newPromptWindow, webView}
                );
            } else {
                route->window = newPromptWindow;
                route->anchor = webView;
            }
            newPromptWindow->m_crossDomainPromptController->request(
                webView,
                affectedViews,
                sources,
                sourceSite,
                targetHost,
                resourceType
            );
        }
    );
    if (m_ownsBrowserResources) {
        connect(
            m_profile,
            &BrowserProfile::crossDomainRequestBlocked,
            this,
            [this](
                const QUrl &sourceUrl,
                const QString &sourceSite,
                const QString &targetHost,
                int resourceType,
                bool sourceUrlIsOriginOnly
            ) {
                routeCrossDomainRequest(
                    sourceUrl,
                    sourceSite,
                    targetHost,
                    resourceType,
                    sourceUrlIsOriginOnly
                );
            }
        );
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
        if (!m_crossDomainConfigurationError.isEmpty()) {
            QTimer::singleShot(0, this, [this] {
                showCrossDomainConfigurationError();
            });
        }
    } else {
        reloadRulesLocal();
        createTab(role == WindowRole::WebApp ? m_webApp.startUrl : QUrl());
    }
}

MainWindow::~MainWindow()
{
    if (qApp)
        qApp->removeEventFilter(this);
    m_expectedBrowserFullScreenExits.clear();
    m_expectedDetachedVideoExits.clear();
    if (m_fullScreenController)
        m_fullScreenController->exit();
    restoreAllDetachedVideos();
    const QList<QWebEngineView *> webViews = m_tabStates.keys();
    for (QWebEngineView *webView : webViews)
        cancelCrossDomainPromptsForView(webView);
    if (m_ownsBrowserResources) {
        while (!m_popupWindows.isEmpty()) {
            if (MainWindow *popup = m_popupWindows.takeLast())
                delete popup;
        }
    }
    for (QWebEngineView *webView : webViews)
        closeDeveloperTools(webView);
    delete m_permissionController;
    m_permissionController = nullptr;
    delete m_crossDomainPromptController;
    m_crossDomainPromptController = nullptr;
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
    m_votUserscriptManager = nullptr;
}

QString MainWindow::startupError() const
{
    return m_startupError;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_expectedBrowserFullScreenExits.clear();
    m_expectedDetachedVideoExits.clear();
    if (m_fullScreenController)
        m_fullScreenController->exit();
    restoreAllDetachedVideos();
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

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    const QEvent::Type eventType = event->type();
    if (eventType != QEvent::KeyPress
        && eventType != QEvent::Wheel
        && eventType != QEvent::WindowActivate) {
        return QMainWindow::eventFilter(watched, event);
    }

    QWebEngineView *webView = activeInteractionWebView();
    if (!webView)
        return QMainWindow::eventFilter(watched, event);
    m_lastInteractionWebView = webView;
    if (eventType == QEvent::WindowActivate) {
        m_zoomAngleRemainder = 0;
        m_zoomPixelRemainder = 0;
        return QMainWindow::eventFilter(watched, event);
    }

    if (eventType == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        QWebEnginePage *page = pageForTab(webView);
        if (m_fullScreenController
            && m_fullScreenController->webView() == webView
            && keyEvent->key() == Qt::Key_Escape
            && keyEvent->modifiers() == Qt::NoModifier) {
            requestBrowserFullScreenExit(webView);
            keyEvent->accept();
            return true;
        }
        if (triggerShortcutAction(
                m_newTabAction,
                keyEvent,
                m_newTabAction && m_newTabAction->isEnabled(),
                [this] { openNewTab(); }
            )
            || triggerShortcutAction(
                m_closeTabAction,
                keyEvent,
                m_closeTabAction && m_closeTabAction->isEnabled(),
                [this, webView] { closeActiveBrowserSurface(webView); }
            )
            || triggerShortcutAction(
                m_reopenClosedTabAction,
                keyEvent,
                m_reopenClosedTabAction && m_reopenClosedTabAction->isEnabled(),
                [this] { reopenLastClosedTab(); }
            )
            || triggerShortcutAction(
                m_nextTabAction,
                keyEvent,
                m_nextTabAction && m_nextTabAction->isEnabled(),
                [this] { activateAdjacentTab(1); }
            )
            || triggerShortcutAction(
                m_previousTabAction,
                keyEvent,
                m_previousTabAction && m_previousTabAction->isEnabled(),
                [this] { activateAdjacentTab(-1); }
            )) {
            return true;
        }
        for (int index = 0; index < m_numberedTabActions.size(); ++index) {
            QAction *action = m_numberedTabActions.at(index);
            if (triggerShortcutAction(
                    action,
                    keyEvent,
                    action && action->isEnabled(),
                    [this, index] { activateNumberedTab(index + 1); }
                )) {
                return true;
            }
        }
        const int zoomPercentage = pageZoomPercentage(
            page ? page->zoomFactor() : defaultPageZoomFactor
        );
        const bool canZoomIn = page
            && zoomPercentage < pageZoomPercentage(maximumPageZoomFactor);
        const bool canZoomOut = page
            && zoomPercentage > pageZoomPercentage(minimumPageZoomFactor);
        const bool canResetZoom = page
            && zoomPercentage != pageZoomPercentage(defaultPageZoomFactor);
        if (triggerShortcutAction(m_zoomInAction, keyEvent, canZoomIn, [this, webView] {
                changePageZoomBySteps(webView, 1);
            })
            || triggerShortcutAction(m_zoomOutAction, keyEvent, canZoomOut, [this, webView] {
                changePageZoomBySteps(webView, -1);
            })
            || triggerShortcutAction(m_resetZoomAction, keyEvent, canResetZoom, [this, webView] {
                setPageZoom(webView, defaultPageZoomFactor);
            })) {
            return true;
        }
        if (m_preferences.developerToolsEnabled()
            && triggerShortcutAction(
                m_developerToolsAction,
                keyEvent,
                true,
                [this, webView] {
                    openDeveloperTools(webView);
                }
            )) {
            return true;
        }
        if (triggerShortcutAction(
                m_readerModeAction,
                keyEvent,
                m_readerModeAction && m_readerModeAction->isEnabled(),
                [this, webView] { toggleReaderMode(webView); }
            )) {
            return true;
        }
    }

    if (eventType == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        if (!hasPageZoomModifier(wheelEvent->modifiers())) {
            m_zoomAngleRemainder = 0;
            m_zoomPixelRemainder = 0;
            return QMainWindow::eventFilter(watched, event);
        }
        QWebEngineView *renderingView = renderingViewForTab(webView);
        if (!renderingView || !renderingView->isVisible())
            return QMainWindow::eventFilter(watched, event);

        const QPoint localPosition = renderingView->mapFromGlobal(
            wheelEvent->globalPosition().toPoint()
        );
        if (!renderingView->rect().contains(localPosition))
            return QMainWindow::eventFilter(watched, event);

        if (wheelEvent->phase() == Qt::ScrollBegin) {
            m_zoomAngleRemainder = 0;
            m_zoomPixelRemainder = 0;
        }

        int steps = 0;
        if (!wheelEvent->pixelDelta().isNull()) {
            m_zoomAngleRemainder = 0;
            steps = takePageZoomSteps(
                wheelEvent->pixelDelta().y(),
                40,
                m_zoomPixelRemainder
            );
        } else if (!wheelEvent->angleDelta().isNull()) {
            m_zoomPixelRemainder = 0;
            steps = takePageZoomSteps(
                wheelEvent->angleDelta().y(),
                120,
                m_zoomAngleRemainder
            );
        }

        if (steps != 0)
            changePageZoomBySteps(webView, std::clamp(steps, -4, 4));
        if (wheelEvent->phase() == Qt::ScrollEnd) {
            m_zoomAngleRemainder = 0;
            m_zoomPixelRemainder = 0;
        }
        wheelEvent->accept();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
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
    QIcon windowIcon(QStringLiteral(":/assets/app-icon.png"));
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
    tabsToolbar->setProperty("integratedChrome", m_integratedWindowChrome);

    QWidget *tabsContainer = new QWidget(tabsToolbar);
    tabsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QHBoxLayout *tabsLayout = new QHBoxLayout(tabsContainer);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->setSpacing(4);

    m_tabBar = new BrowserTabBar(tabsContainer);
    m_tabBar->setObjectName(QStringLiteral("browserTabBar"));
    m_tabBar->setDocumentMode(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    m_tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    if (m_windowRole == WindowRole::Primary)
        m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    tabsLayout->addWidget(m_tabBar, 0, Qt::AlignBottom);

    QToolButton *newTabButton = new QToolButton(tabsContainer);
    newTabButton->setObjectName(QStringLiteral("newTabButton"));
    newTabButton->setIcon(QIcon(QStringLiteral(":/assets/icons/plus.svg")));
    const QList<QKeySequence> newTabShortcuts = TabNavigation::newTabShortcuts();
    const QString newTabShortcutText = newTabShortcuts.isEmpty()
        ? QString()
        : newTabShortcuts.constFirst().toString(QKeySequence::NativeText);
    newTabButton->setToolTip(
        newTabShortcutText.isEmpty()
            ? tr("New Tab")
            : tr("New Tab (%1)").arg(newTabShortcutText)
    );
    tabsLayout->addWidget(newTabButton, 0, Qt::AlignBottom);
    tabsLayout->addStretch(1);
    tabsToolbar->addWidget(tabsContainer);

    if (m_integratedWindowChrome) {
        m_windowChromeController = new WindowChromeController(
            this,
            tabsLayout,
            {tabsToolbar, tabsContainer},
            this
        );
    }

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
    m_readerModeAction = new QAction(
        QIcon(QStringLiteral(":/assets/icons/book-open.svg")),
        tr("Reader Mode"),
        this
    );
    m_readerModeAction->setCheckable(true);
    m_readerModeAction->setShortcut(QKeySequence(Qt::Key_F9));
    m_readerModeAction->setShortcutContext(Qt::WindowShortcut);
    m_readerModeAction->setAutoRepeat(false);
    m_readerModeAction->setVisible(false);
    m_address->addAction(m_readerModeAction, QLineEdit::TrailingPosition);
    connect(m_readerModeAction, &QAction::triggered, this, [this] {
        toggleReaderMode(commandTargetWebView());
    });
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

    addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *crossDomainToolbar = new QToolBar(tr("Site connection request"), this);
    crossDomainToolbar->setObjectName(QStringLiteral("crossDomainBar"));
    crossDomainToolbar->setMovable(false);
    crossDomainToolbar->setFloatable(false);
    m_crossDomainPrompt = new CrossDomainPrompt(crossDomainToolbar);
    crossDomainToolbar->addWidget(m_crossDomainPrompt);
    addToolBar(Qt::TopToolBarArea, crossDomainToolbar);
    crossDomainToolbar->hide();

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

    connect(newTabButton, &QToolButton::clicked, this, &MainWindow::openNewTab);
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (m_fullScreenController && m_fullScreenController->isActive()) {
            QWebEngineView *requestedView = index >= 0
                ? qobject_cast<QWebEngineView *>(m_tabStack->widget(index))
                : nullptr;
            QWebEngineView *fullScreenView = m_fullScreenController->webView();
            if (fullScreenView && requestedView != fullScreenView)
                requestBrowserFullScreenExit(fullScreenView);
        }
        m_zoomAngleRemainder = 0;
        m_zoomPixelRemainder = 0;
        if (index >= 0) {
            m_tabStack->setCurrentIndex(index);
            if (!m_restoringSession)
                activatePendingTab(currentWebView());
        }
        if (m_permissionController)
            m_permissionController->currentViewChanged(currentWebView());
        if (m_crossDomainPromptController)
            m_crossDomainPromptController->currentViewChanged(currentWebView());
        if (isActiveWindow())
            m_lastInteractionWebView = currentWebView();
        if (m_externalUrlSource && m_externalUrlSource != currentWebView()) {
            const auto sourceState = m_tabStates.constFind(m_externalUrlSource);
            if (sourceState == m_tabStates.cend() || !sourceState->detachedVideoWindow)
                cancelExternalUrlPrompt();
        }
        updateCurrentTabUi();
        updateTabNavigationActions();
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
    if (m_windowRole == WindowRole::Primary) {
        connect(
            m_tabBar,
            &QWidget::customContextMenuRequested,
            this,
            &MainWindow::showTabContextMenu
        );
    }
    connect(m_backAction, &QAction::triggered, this, [this] {
        if (QWebEnginePage *page = pageForTab(currentWebView()))
            page->triggerAction(QWebEnginePage::Back);
    });
    connect(m_forwardAction, &QAction::triggered, this, [this] {
        if (QWebEnginePage *page = pageForTab(currentWebView()))
            page->triggerAction(QWebEnginePage::Forward);
    });
    connect(m_reloadAction, &QAction::triggered, this, [this] {
        if (QWebEnginePage *page = pageForTab(currentWebView()))
            page->triggerAction(QWebEnginePage::Reload);
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

#if defined(Q_OS_MACOS)
    QMenu *fileMenu = menuBar()->addMenu(tr("Browser"));
#else
    auto *fileMenu = new QMenu(tr("PanBrowser"), this);
#endif
    m_newTabAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/plus.svg")),
        tr("New Tab")
    );
    m_newTabAction->setShortcuts(TabNavigation::newTabShortcuts());
    m_newTabAction->setShortcutContext(Qt::WindowShortcut);
    m_newTabAction->setAutoRepeat(false);
    connect(m_newTabAction, &QAction::triggered, this, &MainWindow::openNewTab);

    m_closeTabAction = fileMenu->addAction(tr("Close Tab"));
    m_closeTabAction->setShortcuts(TabNavigation::closeTabShortcuts());
    m_closeTabAction->setShortcutContext(Qt::WindowShortcut);
    m_closeTabAction->setAutoRepeat(false);
    connect(m_closeTabAction, &QAction::triggered, this, [this] {
        closeActiveBrowserSurface(commandTargetWebView());
    });

    m_reopenClosedTabAction = fileMenu->addAction(tr("Reopen Closed Tab"));
    m_reopenClosedTabAction->setShortcuts(TabNavigation::reopenClosedTabShortcuts());
    m_reopenClosedTabAction->setShortcutContext(Qt::WindowShortcut);
    m_reopenClosedTabAction->setAutoRepeat(false);
    connect(
        m_reopenClosedTabAction,
        &QAction::triggered,
        this,
        &MainWindow::reopenLastClosedTab
    );

    fileMenu->addSeparator();
    m_nextTabAction = fileMenu->addAction(tr("Next Tab"));
    m_nextTabAction->setShortcuts(TabNavigation::nextTabShortcuts());
    m_nextTabAction->setShortcutContext(Qt::WindowShortcut);
    m_nextTabAction->setAutoRepeat(false);
    connect(m_nextTabAction, &QAction::triggered, this, [this] {
        activateAdjacentTab(1);
    });
    m_previousTabAction = fileMenu->addAction(tr("Previous Tab"));
    m_previousTabAction->setShortcuts(TabNavigation::previousTabShortcuts());
    m_previousTabAction->setShortcutContext(Qt::WindowShortcut);
    m_previousTabAction->setAutoRepeat(false);
    connect(m_previousTabAction, &QAction::triggered, this, [this] {
        activateAdjacentTab(-1);
    });

    m_switchToTabMenu = fileMenu->addMenu(tr("Switch to Tab"));
    for (int position = 1; position <= TabNavigation::numberedShortcutCount; ++position) {
        QAction *action = m_switchToTabMenu->addAction(
            position == TabNavigation::numberedShortcutCount
                ? tr("Last Tab")
                : tr("Tab %1").arg(position)
        );
        action->setShortcut(TabNavigation::numberedTabShortcut(position));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        connect(action, &QAction::triggered, this, [this, position] {
            activateNumberedTab(position);
        });
        m_numberedTabActions.append(action);
    }

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
    fileMenu->addAction(m_readerModeAction);
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
    m_zoomMenu = fileMenu->addMenu(tr("Zoom"));
    m_zoomLevelAction = m_zoomMenu->addAction(QStringLiteral("100%"));
    m_zoomLevelAction->setEnabled(false);
    m_zoomMenu->addSeparator();
    m_zoomInAction = m_zoomMenu->addAction(tr("Zoom In"));
    m_zoomInAction->setShortcuts(pageZoomInShortcuts());
    m_zoomInAction->setShortcutContext(Qt::WindowShortcut);
    m_zoomInAction->setAutoRepeat(false);
    connect(m_zoomInAction, &QAction::triggered, this, [this] {
        changePageZoomBySteps(commandTargetWebView(), 1);
    });
    m_zoomOutAction = m_zoomMenu->addAction(tr("Zoom Out"));
    m_zoomOutAction->setShortcuts(pageZoomOutShortcuts());
    m_zoomOutAction->setShortcutContext(Qt::WindowShortcut);
    m_zoomOutAction->setAutoRepeat(false);
    connect(m_zoomOutAction, &QAction::triggered, this, [this] {
        changePageZoomBySteps(commandTargetWebView(), -1);
    });
    m_resetZoomAction = m_zoomMenu->addAction(tr("Actual Size"));
    m_resetZoomAction->setShortcuts(pageZoomResetShortcuts());
    m_resetZoomAction->setShortcutContext(Qt::WindowShortcut);
    m_resetZoomAction->setAutoRepeat(false);
    connect(m_resetZoomAction, &QAction::triggered, this, [this] {
        setPageZoom(commandTargetWebView(), defaultPageZoomFactor);
    });
    connect(m_zoomMenu, &QMenu::aboutToShow, this, &MainWindow::updateZoomActions);

    fileMenu->addSeparator();
    m_developerToolsAction = fileMenu->addAction(tr("Developer Tools"));
#if defined(Q_OS_MACOS)
    m_developerToolsAction->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I),
        QKeySequence(Qt::Key_F12),
    });
#else
    m_developerToolsAction->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I),
        QKeySequence(Qt::Key_F12),
    });
#endif
    m_developerToolsAction->setShortcutContext(Qt::WindowShortcut);
    connect(m_developerToolsAction, &QAction::triggered, this, [this] {
        openDeveloperTools(commandTargetWebView());
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

#if !defined(Q_OS_MACOS)
    auto *mainMenuButton = new QToolButton(toolbar);
    mainMenuButton->setObjectName(QStringLiteral("mainMenuButton"));
    mainMenuButton->setIcon(
        QIcon(QStringLiteral(":/assets/icons/ellipsis-vertical.svg"))
    );
    mainMenuButton->setToolTip(fileMenu->title());
    mainMenuButton->setAccessibleName(fileMenu->title());
    mainMenuButton->setFocusPolicy(Qt::NoFocus);
    mainMenuButton->setPopupMode(QToolButton::InstantPopup);
    mainMenuButton->setMenu(fileMenu);
    toolbar->addWidget(mainMenuButton);
#endif

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
        m_newTabAction->setVisible(false);
        m_closeTabAction->setText(tr("Close Window"));
        m_reopenClosedTabAction->setVisible(false);
        m_nextTabAction->setVisible(false);
        m_previousTabAction->setVisible(false);
        m_switchToTabMenu->menuAction()->setVisible(false);
        addBookmarkAction->setVisible(false);
        bookmarksAction->setVisible(false);
        m_installWebAppAction->setVisible(false);
        m_webAppsMenu->menuAction()->setVisible(false);
        m_readerModeAction->setVisible(false);
        settingsAction->setVisible(false);
        reloadRulesAction->setVisible(false);
        showConfiguration->setVisible(false);
        m_ruleCount->hide();
    }

    updateZoomActions();
    updateTabNavigationActions();
    applyDeveloperToolsPreference();
    qApp->installEventFilter(this);
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
    const QString &restoredTitle,
    bool pinned
)
{
    if (m_windowRole == WindowRole::Primary && m_ownsBrowserResources)
        m_discardSessionOnClose = false;

    QWebEngineView *webView = new QWebEngineView(m_tabStack);
    auto *page = new BrowserPage(m_profile, webView);
    if (m_windowRole == WindowRole::WebApp)
        page->setWebApp(m_webApp);
    VideoElementBridge::install(
        page,
        page->videoPopoutToken(),
        tr("Open Video in Separate Window")
    );
    if (m_votUserscriptManager)
        m_votUserscriptManager->configurePage(page);
    page->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    webView->setPage(page);
    BrowserTabState state;
    state.title = restoredTitle.isEmpty()
        ? (url.host().isEmpty() ? tr("New Tab") : url.host())
        : restoredTitle;
    state.pinned = pinned;
    state.page = page;
    if (m_windowRole != WindowRole::WebApp) {
        ReaderSettings *readerSettings = m_primaryWindow
            ? &m_primaryWindow->m_readerSettings
            : &m_readerSettings;
        state.readerModeController = new ReaderModeController(
            page,
            readerSettings,
            webView
        );
    }
    state.detachedVideoSession = new DetachedVideoSession(webView);
    if (deferred) {
        state.pendingUrl = url;
        state.suppressNextHistoryVisit = true;
    }
    m_tabStates.insert(webView, state);
    if (state.readerModeController) {
        connect(
            state.readerModeController,
            &ReaderModeController::stateChanged,
            this,
            [this, webView] {
                if (webView == currentWebView())
                    updateReaderModeAction();
            }
        );
        connect(
            state.readerModeController,
            &ReaderModeController::errorOccurred,
            this,
            [this, webView](const QString &message) {
                if (webView == currentWebView())
                    statusBar()->showMessage(message, 5000);
                else
                    qWarning().noquote() << "[PanBrowser reader mode]" << message;
            }
        );
        connect(
            state.readerModeController,
            &ReaderModeController::appearanceChanged,
            this,
            [this, source = state.readerModeController.data()] {
                MainWindow *coordinator = m_primaryWindow ? m_primaryWindow : this;
                QList<MainWindow *> windows{coordinator};
                for (MainWindow *window : std::as_const(coordinator->m_popupWindows)) {
                    if (window)
                        windows.append(window);
                }
                for (MainWindow *window : std::as_const(windows)) {
                    for (const BrowserTabState &tab : std::as_const(window->m_tabStates)) {
                        ReaderModeController *controller = tab.readerModeController;
                        if (controller && controller != source)
                            controller->refreshAppearance();
                    }
                }
            }
        );
    }
    connect(
        state.detachedVideoSession,
        &DetachedVideoSession::exitFullScreenRequested,
        this,
        [this, webView] {
            const auto state = m_tabStates.constFind(webView);
            if (state == m_tabStates.cend() || !state->detachedVideoWindow)
                return;
            if (QWebEnginePage *page = state->page) {
                m_expectedDetachedVideoExits.insert(webView);
                page->triggerAction(QWebEnginePage::ExitFullScreen);
            }
        }
    );
    connect(
        state.detachedVideoSession,
        &DetachedVideoSession::restoreRequested,
        this,
        [this, webView] {
            restoreDetachedVideo(webView);
        }
    );
    connectBrowserSignals(webView);
    webView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        webView,
        &QWidget::customContextMenuRequested,
        this,
        [this, webView](const QPoint &position) {
            showWebContextMenu(webView, webView, position);
        }
    );

    const int stackIndex = m_tabStack->addWidget(webView);
    const int tabIndex = m_tabBar->addTab(state.title);
    Q_ASSERT(stackIndex == tabIndex);
    m_tabBar->setTabPinned(tabIndex, pinned);
    updateTabPresentation(webView);
    updateTabNavigationActions();

    if (activate)
        m_tabBar->setCurrentIndex(tabIndex);
    if (!m_restoringSession)
        m_tabBar->animateTabOpening(tabIndex);
    if (!deferred && url.isValid() && !url.isEmpty())
        webView->setUrl(url);
    if (!m_restoringSession)
        scheduleSessionSave();

    return webView;
}

void MainWindow::showTabContextMenu(const QPoint &position)
{
    if (m_windowRole != WindowRole::Primary)
        return;

    const int index = m_tabBar->tabAt(position);
    if (index < 0)
        return;

    QMenu menu(this);
    const bool pinned = m_tabBar->isTabPinned(index);
    QAction *pinAction = menu.addAction(pinned ? tr("Unpin Tab") : tr("Pin Tab"));
    connect(pinAction, &QAction::triggered, this, [this, index, pinned] {
        setTabPinned(index, !pinned);
    });
    menu.addSeparator();
    QAction *closeAction = menu.addAction(tr("Close Tab"));
    connect(closeAction, &QAction::triggered, this, [this, index] {
        closeTab(index);
    });
    menu.exec(m_tabBar->mapToGlobal(position));
}

void MainWindow::setTabPinned(int index, bool pinned)
{
    if (m_windowRole != WindowRole::Primary
        || index < 0
        || index >= m_tabBar->count()) {
        return;
    }

    QWebEngineView *webView = qobject_cast<QWebEngineView *>(m_tabStack->widget(index));
    auto state = m_tabStates.find(webView);
    if (state == m_tabStates.end() || state->pinned == pinned)
        return;

    state->pinned = pinned;
    m_tabBar->setTabPinned(index, pinned);
    updateTabPresentation(webView);

    const int destination = pinned
        ? m_tabBar->pinnedTabCount() - 1
        : m_tabBar->pinnedTabCount();
    if (destination != index)
        m_tabBar->moveTab(index, destination);
    else
        scheduleSessionSave();
}

void MainWindow::updateTabPresentation(QWebEngineView *webView)
{
    const int index = m_tabStack ? m_tabStack->indexOf(webView) : -1;
    auto state = m_tabStates.find(webView);
    if (index < 0 || state == m_tabStates.end())
        return;

    QWebEnginePage *page = pageForTab(webView);
    const QUrl url = state->pendingUrl.isEmpty() ? urlForTab(webView) : state->pendingUrl;
    if (state->title.isEmpty()) {
        state->title = url.host().isEmpty() ? tr("New Tab") : url.host();
    }

    m_tabBar->setTabPinned(index, state->pinned);
    m_tabBar->setTabText(index, state->pinned ? QString() : state->title);
    m_tabBar->setAccessibleTabName(index, state->title);
    const QString urlText = url.toString();
    m_tabBar->setTabToolTip(
        index,
        urlText.isEmpty() || urlText == state->title
            ? state->title
            : state->title + QLatin1Char('\n') + urlText
    );

    QIcon icon = page ? page->icon() : QIcon();
    if (state->pinned && icon.isNull())
        icon = QIcon(QStringLiteral(":/assets/app-icon.png"));
    m_tabBar->setTabIcon(index, icon);
}

bool MainWindow::hasPinnedTabs() const
{
    if (!m_tabBar)
        return false;
    return m_tabBar->pinnedTabCount() > 0;
}

void MainWindow::openNewTab()
{
    if (m_windowRole == WindowRole::WebApp)
        return;
    createTab(m_preferences.startPage());
}

void MainWindow::closeCurrentTab()
{
    if (m_tabBar)
        closeTab(m_tabBar->currentIndex());
}

void MainWindow::closeActiveBrowserSurface(QWebEngineView *webView)
{
    const auto state = m_tabStates.constFind(webView);
    if (state == m_tabStates.cend())
        return;

    QWidget *detachedVideoWindow = state->detachedVideoWindow.data();
    if (browserCloseTarget(QApplication::activeWindow(), detachedVideoWindow)
        == BrowserCloseTarget::DetachedVideo) {
        requestDetachedVideoReturn(webView);
        return;
    }
    closeCurrentTab();
}

void MainWindow::reopenLastClosedTab()
{
    if (m_windowRole != WindowRole::Primary || m_closedTabs.isEmpty())
        return;

    const SessionTab closedTab = m_closedTabs.takeFirst();
    QWebEngineView *webView = createTab(
        closedTab.url,
        true,
        false,
        closedTab.title,
        closedTab.pinned
    );
    if (closedTab.pinned && webView) {
        const int index = m_tabStack->indexOf(webView);
        const int destination = m_tabBar->pinnedTabCount() - 1;
        if (index >= 0 && destination >= 0 && index != destination)
            m_tabBar->moveTab(index, destination);
    }
    updateTabNavigationActions();
}

void MainWindow::activateAdjacentTab(int offset)
{
    if (!m_tabBar)
        return;
    const int index = TabNavigation::adjacentTabIndex(
        m_tabBar->currentIndex(),
        m_tabBar->count(),
        offset
    );
    if (index >= 0)
        m_tabBar->setCurrentIndex(index);
}

void MainWindow::activateNumberedTab(int position)
{
    if (!m_tabBar)
        return;
    const int index = TabNavigation::numberedTabIndex(position, m_tabBar->count());
    if (index >= 0)
        m_tabBar->setCurrentIndex(index);
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabBar->count())
        return;

    QWebEngineView *webView = qobject_cast<QWebEngineView *>(m_tabStack->widget(index));
    if (!webView)
        return;

    if (m_windowRole == WindowRole::Primary) {
        const auto state = m_tabStates.constFind(webView);
        if (state != m_tabStates.cend()) {
            const QUrl url = state->pendingUrl.isEmpty() ? urlForTab(webView)
                                                         : state->pendingUrl;
            const QString title = state->title.isEmpty() ? titleForTab(webView)
                                                         : state->title;
            m_closedTabs.prepend({url, title, state->pinned});
            while (m_closedTabs.size() > maximumClosedTabs)
                m_closedTabs.removeLast();
        }
    }

    m_expectedDetachedVideoExits.remove(webView);
    restoreDetachedVideo(webView);
    m_expectedBrowserFullScreenExits.remove(webView);
    exitBrowserFullScreen(webView);
    m_permissionController->cancelForView(webView);
    cancelCrossDomainPromptsForView(webView);
    cancelExternalUrlPrompt(webView);
    closeDeveloperTools(webView);
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
    updateTabNavigationActions();

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

QWebEnginePage *MainWindow::pageForTab(QWebEngineView *webView) const
{
    if (!webView)
        return nullptr;
    const auto state = m_tabStates.constFind(webView);
    if (state != m_tabStates.cend() && state->page)
        return state->page;
    return webView->page();
}

QWebEngineView *MainWindow::webViewForPage(const BrowserPage *page) const
{
    if (!page)
        return nullptr;
    for (auto iterator = m_tabStates.cbegin(); iterator != m_tabStates.cend(); ++iterator) {
        if (iterator->page == page)
            return iterator.key();
    }
    return nullptr;
}

QUrl MainWindow::urlForTab(QWebEngineView *webView) const
{
    if (QWebEnginePage *page = pageForTab(webView))
        return page->url();
    return QUrl();
}

QString MainWindow::titleForTab(QWebEngineView *webView) const
{
    if (QWebEnginePage *page = pageForTab(webView))
        return page->title();
    return QString();
}

QWebEngineView *MainWindow::activeInteractionWebView() const
{
    QWidget *activeWindow = QApplication::activeWindow();
    QVarLengthArray<BrowserInteractionSurface, 4> detachedSurfaces;
    for (auto state = m_tabStates.cbegin(); state != m_tabStates.cend(); ++state) {
        DetachedVideoWindow *detachedWindow = state->detachedVideoWindow;
        if (detachedWindow)
            detachedSurfaces.append({state.key(), detachedWindow});
    }
    return resolveActiveBrowserView(
        activeWindow,
        this,
        currentWebView(),
        std::span<const BrowserInteractionSurface>(
            detachedSurfaces.constData(),
            static_cast<std::size_t>(detachedSurfaces.size())
        )
    );
}

QWebEngineView *MainWindow::commandTargetWebView() const
{
    QWebEngineView *activeView = activeInteractionWebView();
    if (activeView)
        return activeView;
    if (QApplication::activeModalWidget())
        return nullptr;
    if (QApplication::activeWindow() && !QApplication::activePopupWidget())
        return nullptr;

    QWebEngineView *lastActiveView = m_lastInteractionWebView;
    if (lastActiveView && !m_tabStates.contains(lastActiveView))
        lastActiveView = nullptr;
    return resolveBrowserCommandTarget(
        nullptr,
        lastActiveView,
        currentWebView()
    );
}

QWebEngineView *MainWindow::renderingViewForTab(QWebEngineView *webView) const
{
    const auto state = m_tabStates.constFind(webView);
    if (state != m_tabStates.cend() && state->detachedVideoWindow)
        return state->detachedVideoWindow->webView();
    return webView;
}

QWidget *MainWindow::interactionParentForTab(QWebEngineView *webView)
{
    const auto state = m_tabStates.constFind(webView);
    if (state != m_tabStates.cend()
        && state->detachedVideoWindow
        && activeInteractionWebView() == webView) {
        return state->detachedVideoWindow;
    }
    return this;
}

bool MainWindow::isTabInteractionActive(QWebEngineView *webView) const
{
    return webView && activeInteractionWebView() == webView;
}

void MainWindow::returnDetachedVideoForPermissionPrompt(QWebEngineView *webView)
{
    if (m_fullScreenController && m_fullScreenController->webView() == webView) {
        requestBrowserFullScreenExit(webView);
    }

    const auto state = m_tabStates.constFind(webView);
    if (state != m_tabStates.cend() && state->detachedVideoWindow) {
        if (state->page) {
            m_expectedDetachedVideoExits.insert(webView);
            state->page->triggerAction(QWebEnginePage::ExitFullScreen);
        }
        if (m_tabStates.contains(webView)
            && m_tabStates.value(webView).detachedVideoWindow) {
            const BrowserTabState currentState = m_tabStates.value(webView);
            if (currentState.detachedVideoSession)
                currentState.detachedVideoSession->forceRestore();
            if (m_tabStates.contains(webView)
                && m_tabStates.value(webView).detachedVideoWindow) {
                restoreDetachedVideo(webView);
            }
        }
    }

    const int index = m_tabStack ? m_tabStack->indexOf(webView) : -1;
    if (index >= 0 && m_tabBar->currentIndex() != index)
        m_tabBar->setCurrentIndex(index);
    if (m_permissionController)
        m_permissionController->currentViewChanged(webView);
    show();
    raise();
    activateWindow();
}

QString MainWindow::developerToolsWindowTitle(QWebEngineView *webView) const
{
    QString pageTitle = titleForTab(webView).trimmed();
    if (pageTitle.isEmpty())
        pageTitle = tr("Untitled page");
    return tr("Developer Tools — %1").arg(pageTitle);
}

void MainWindow::showWebContextMenu(
    QWebEngineView *webView,
    QWebEngineView *renderingView,
    const QPoint &position
)
{
    if (!webView || !renderingView || !m_tabStates.contains(webView))
        return;

    QMenu *menu = renderingView->createStandardContextMenu();
    if (!menu)
        return;
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (QAction *defaultInspect = renderingView->pageAction(QWebEnginePage::InspectElement))
        menu->removeAction(defaultInspect);
    while (!menu->actions().isEmpty() && menu->actions().constLast()->isSeparator())
        menu->removeAction(menu->actions().constLast());

    if (m_preferences.developerToolsEnabled()) {
        if (!menu->actions().isEmpty())
            menu->addSeparator();
        QAction *inspect = menu->addAction(tr("Inspect Element"));
        connect(inspect, &QAction::triggered, this, [this, webView] {
            openDeveloperTools(webView, true);
        });
    }

    menu->popup(renderingView->mapToGlobal(position));
}

void MainWindow::openDeveloperTools(QWebEngineView *webView, bool inspectElement)
{
    if (!m_preferences.developerToolsEnabled()
        || !webView
        || !m_tabStates.contains(webView)) {
        return;
    }

    BrowserTabState &state = m_tabStates[webView];
    QWebEngineView *developerToolsView = state.developerToolsView;
    if (!developerToolsView) {
        developerToolsView = new QWebEngineView(m_profile, this);
        developerToolsView->setWindowFlag(Qt::Window, true);
        developerToolsView->setAttribute(Qt::WA_DeleteOnClose);
        developerToolsView->setWindowIcon(windowIcon());
        developerToolsView->resize(1100, 760);
        developerToolsView->setWindowTitle(developerToolsWindowTitle(webView));
        state.developerToolsView = developerToolsView;
        pageForTab(webView)->setDevToolsPage(developerToolsView->page());

        const QPointer<QWebEngineView> inspectedView(webView);
        connect(developerToolsView, &QObject::destroyed, this, [this, inspectedView] {
            if (!inspectedView)
                return;
            const auto stateIt = m_tabStates.find(inspectedView);
            if (stateIt != m_tabStates.end())
                stateIt->developerToolsView.clear();
        });
    }

    developerToolsView->setWindowTitle(developerToolsWindowTitle(webView));
    developerToolsView->show();
    developerToolsView->raise();
    developerToolsView->activateWindow();
    if (inspectElement)
        pageForTab(webView)->triggerAction(QWebEnginePage::InspectElement);
}

void MainWindow::closeDeveloperTools(QWebEngineView *webView)
{
    const auto stateIt = m_tabStates.find(webView);
    if (stateIt == m_tabStates.end() || !stateIt->developerToolsView)
        return;

    QWebEngineView *developerToolsView = stateIt->developerToolsView;
    stateIt->developerToolsView.clear();
    QWebEnginePage *page = pageForTab(webView);
    if (page && page->devToolsPage() == developerToolsView->page())
        page->setDevToolsPage(nullptr);
    developerToolsView->setAttribute(Qt::WA_DeleteOnClose, false);
    delete developerToolsView;
}

void MainWindow::applyDeveloperToolsPreference()
{
    const bool enabled = m_preferences.developerToolsEnabled();
    if (m_developerToolsAction) {
        m_developerToolsAction->setEnabled(enabled);
        m_developerToolsAction->setVisible(enabled);
    }
    if (enabled)
        return;

    const QList<QWebEngineView *> webViews = m_tabStates.keys();
    for (QWebEngineView *webView : webViews)
        closeDeveloperTools(webView);
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
    QWebEnginePage *page = pageForTab(webView);
    Q_ASSERT(page);
    connect(
        page,
        &QWebEnginePage::authenticationRequired,
        this,
        [this, webView](const QUrl &requestUrl, QAuthenticator *authenticator) {
            const auto state = m_tabStates.constFind(webView);
            const bool interactionActive = isTabInteractionActive(webView);
            const bool promptAllowed = state != m_tabStates.cend()
                && HttpAuthenticationPolicy::promptAllowed(
                    requestUrl,
                    state->topLevelUrl,
                    interactionActive,
                    interactionActive
                );
            if (m_httpAuthenticationController && promptAllowed) {
                m_httpAuthenticationController->requestAuthentication(
                    interactionParentForTab(webView),
                    requestUrl,
                    authenticator
                );
            } else if (authenticator) {
                *authenticator = QAuthenticator();
            }
        }
    );
    connect(
        page,
        &QWebEnginePage::proxyAuthenticationRequired,
        this,
        [this, webView](
            const QUrl &requestUrl,
            QAuthenticator *authenticator,
            const QString &proxyHost
        ) {
            if (m_proxyAuthenticationController) {
                m_proxyAuthenticationController->requestAuthentication(
                    interactionParentForTab(webView),
                    requestUrl,
                    authenticator,
                    proxyHost
                );
            }
        }
    );
    connect(page, &QWebEnginePage::urlChanged, this, [this, webView, page](const QUrl &url) {
        BrowserTabState &state = m_tabStates[webView];
        state.pendingUrl.clear();
        state.topLevelUrl = url;
        applyStoredPageZoom(webView);
        if (page->title().isEmpty())
            state.title = url.host().isEmpty() ? tr("New Tab") : url.host();
        updateTabPresentation(webView);
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
    connect(page, &QWebEnginePage::zoomFactorChanged, this, [this, webView](double) {
        if (webView == currentWebView())
            updateZoomActions();
    });
    connect(page, &QWebEnginePage::titleChanged, this, [this, webView, page](const QString &title) {
        BrowserTabState &state = m_tabStates[webView];
        state.title = title.isEmpty()
            ? (page->url().host().isEmpty() ? tr("New Tab") : page->url().host())
            : title;
        updateTabPresentation(webView);
        if (webView == currentWebView()) {
            if (m_windowRole == WindowRole::WebApp)
                setWindowTitle(m_webApp.name);
            else
                setWindowTitle(title.isEmpty() ? QStringLiteral("PanBrowser") : title + QStringLiteral(" — PanBrowser"));
        }
        const auto stateIt = m_tabStates.constFind(webView);
        if (stateIt != m_tabStates.cend() && stateIt->developerToolsView)
            stateIt->developerToolsView->setWindowTitle(developerToolsWindowTitle(webView));
        if (m_preferences.saveBrowsingHistory() && m_historyStore && m_historyStore->isOpen()) {
            QString error;
            if (!m_historyStore->updateTitle(page->url(), title, &error))
                qWarning().noquote() << "[PanBrowser history]" << error;
        }
        scheduleSessionSave();
    });
    connect(page, &QWebEnginePage::iconChanged, this, [this, webView](const QIcon &icon) {
        Q_UNUSED(icon);
        updateTabPresentation(webView);
    });
    connect(page, &QWebEnginePage::loadStarted, this, [this, webView] {
        m_permissionController->cancelForView(webView);
        cancelCrossDomainPromptsForView(webView);
        cancelExternalUrlPrompt(webView);
        if (m_fullScreenController && m_fullScreenController->webView() == webView) {
            requestBrowserFullScreenExit(webView);
        }
        BrowserTabState &state = m_tabStates[webView];
        ++state.videoPopoutRequestSerial;
        state.videoPopoutRequestDeadlineMs = 0;
        state.videoPopoutRequestOrigin = QUrl();
        state.videoPopoutRequestSize = QSize(16, 9);
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
    connect(page, &QWebEnginePage::loadProgress, this, [this, webView](int progress) {
        m_tabStates[webView].progress = progress;
        if (webView == currentWebView())
            m_progress->setValue(progress);
    });
    connect(page, &QWebEnginePage::loadFinished, this, [this, webView, page](bool ok) {
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
                && HistoryStore::sanitizedUrl(page->url()).isValid()) {
                QString historyError;
                if (!m_historyStore->recordVisit(
                        page->url(),
                        page->title(),
                        state.pendingHistoryTransition,
                        QDateTime::currentDateTimeUtc(),
                        &historyError
                    )) {
                    qWarning().noquote() << "[PanBrowser history]" << historyError;
                }
            }
            const QString scheme = page->url().scheme().toLower();
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
        static_cast<BrowserPage *>(page),
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
        static_cast<BrowserPage *>(page),
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
        static_cast<BrowserPage *>(page),
        &BrowserPage::videoPopoutRequested,
        this,
        [this, webView](const QUrl &frameUrl, const QSize &videoSize) {
            auto state = m_tabStates.find(webView);
            if (state == m_tabStates.end()
                || !isTabInteractionActive(webView)
                || state->detachedVideoWindow
                || (m_fullScreenController && m_fullScreenController->isActive())) {
                return;
            }

            const quint64 requestSerial = ++state->videoPopoutRequestSerial;
            state->videoPopoutRequestDeadlineMs =
                QDateTime::currentMSecsSinceEpoch() + 2500;
            state->videoPopoutRequestOrigin = frameUrl;
            state->videoPopoutRequestSize = videoSize;
            const QPointer<MainWindow> window(this);
            const QPointer<QWebEngineView> target(webView);
            QTimer::singleShot(2600, this, [window, target, requestSerial] {
                if (!window || !target)
                    return;
                auto state = window->m_tabStates.find(target);
                if (state == window->m_tabStates.end()
                    || state->videoPopoutRequestSerial != requestSerial
                    || state->videoPopoutRequestDeadlineMs == 0) {
                    return;
                }
                state->videoPopoutRequestDeadlineMs = 0;
                state->videoPopoutRequestOrigin = QUrl();
                state->videoPopoutRequestSize = QSize(16, 9);
                window->statusBar()->showMessage(
                    QCoreApplication::translate(
                        "MainWindow",
                        "The page did not allow this video to open separately."
                    ),
                    4000
                );
            });
        }
    );
    connect(
        static_cast<BrowserPage *>(page),
        &BrowserPage::webAppManifestFetched,
        this,
        &MainWindow::handleFetchedWebAppManifest
    );
    connect(
        static_cast<BrowserPage *>(page),
        &BrowserPage::externalUrlRequested,
        this,
        [this, webView](const QUrl &url) {
            m_tabStates[webView].externalNavigationDelegated = true;
            handleExternalUrlRequest(webView, url);
        }
    );
    connect(
        page,
        &QWebEnginePage::certificateError,
        this,
        [this, webView](const QWebEngineCertificateError &error) {
            handleCertificateError(webView, error);
        }
    );
    connect(
        page,
        &QWebEnginePage::permissionRequested,
        this,
        [this, webView](const QWebEnginePermission &permission) {
            const auto state = m_tabStates.constFind(webView);
            if (state != m_tabStates.cend() && state->detachedVideoWindow)
                returnDetachedVideoForPermissionPrompt(webView);
            m_permissionController->request(webView, permission);
        }
    );
    connect(
        page,
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
    connect(
        page,
        &QWebEnginePage::fullScreenRequested,
        this,
        [this, webView](QWebEngineFullScreenRequest request) {
            handleFullScreenRequest(webView, request);
        }
    );
}

void MainWindow::handleFullScreenRequest(
    QWebEngineView *webView,
    QWebEngineFullScreenRequest request
)
{
    auto state = m_tabStates.find(webView);
    if (state == m_tabStates.end()) {
        request.reject();
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (request.toggleOn()) {
        const bool browserExitPending = m_expectedBrowserFullScreenExits.remove(webView);
        const bool detachedExitPending = m_expectedDetachedVideoExits.remove(webView);
        if (browserExitPending || detachedExitPending) {
            state->videoPopoutRequestDeadlineMs = 0;
            state->videoPopoutRequestOrigin = QUrl();
            state->videoPopoutRequestSize = QSize(16, 9);
            if (browserExitPending)
                exitBrowserFullScreen(webView);
            if (detachedExitPending) {
                if (state->detachedVideoSession)
                    state->detachedVideoSession->forceRestore();
                else
                    restoreDetachedVideo(webView);
            }
            request.reject();
            return;
        }
    }
    const bool videoPopoutRequested = request.toggleOn()
        && state->videoPopoutRequestDeadlineMs >= now
        && isSameWebOrigin(state->videoPopoutRequestOrigin, request.origin());
    const QSize requestedVideoSize = state->videoPopoutRequestSize;
    if (request.toggleOn() && state->videoPopoutRequestDeadlineMs != 0) {
        state->videoPopoutRequestDeadlineMs = 0;
        state->videoPopoutRequestOrigin = QUrl();
        state->videoPopoutRequestSize = QSize(16, 9);
    }
    const bool browserFullScreenActive = m_fullScreenController
        && m_fullScreenController->isActive();
    const bool browserFullScreenOwnsRequest = m_fullScreenController
        && m_fullScreenController->webView() == webView;
    const bool browserFullScreenExitExpected = !request.toggleOn()
        && m_expectedBrowserFullScreenExits.contains(webView);
    const FullScreenRequestDecision decision = decideFullScreenRequest(
        request.toggleOn(),
        isTabInteractionActive(webView),
        videoPopoutRequested,
        (state->detachedVideoSession && state->detachedVideoSession->isDetached())
            || m_expectedDetachedVideoExits.contains(webView),
        browserFullScreenActive,
        browserFullScreenOwnsRequest || browserFullScreenExitExpected,
        request.origin()
    );
    switch (decision.action) {
    case FullScreenRequestAction::Reject:
        request.reject();
        return;
    case FullScreenRequestAction::EnterBrowserFullScreen:
        if (!m_fullScreenController || !m_fullScreenController->enter(webView)) {
            request.reject();
            return;
        }
        request.accept();
        return;
    case FullScreenRequestAction::DetachVideo:
        request.accept();
        detachVideo(webView, decision.origin, requestedVideoSize);
        return;
    case FullScreenRequestAction::RestoreBrowserFullScreen:
        request.accept();
        m_expectedBrowserFullScreenExits.remove(webView);
        exitBrowserFullScreen(webView);
        return;
    case FullScreenRequestAction::RestoreDetachedVideo:
        request.accept();
        m_expectedDetachedVideoExits.remove(webView);
        if (state->detachedVideoSession)
            state->detachedVideoSession->browserExitedFullScreen();
        else
            restoreDetachedVideo(webView);
        return;
    }
}

void MainWindow::exitBrowserFullScreen(QWebEngineView *webView)
{
    if (!m_fullScreenController || m_fullScreenController->webView() != webView)
        return;
    m_fullScreenController->exit();
    if (webView)
        webView->setFocus(Qt::OtherFocusReason);
}

void MainWindow::requestBrowserFullScreenExit(QWebEngineView *webView)
{
    if (!webView
        || !m_fullScreenController
        || m_fullScreenController->webView() != webView
        || m_expectedBrowserFullScreenExits.contains(webView)) {
        return;
    }

    QWebEnginePage *page = pageForTab(webView);
    if (!page) {
        exitBrowserFullScreen(webView);
        return;
    }

    const quint64 requestSerial = ++m_browserFullScreenExitSerial;
    m_expectedBrowserFullScreenExits.insert(webView, requestSerial);
    page->triggerAction(QWebEnginePage::ExitFullScreen);

    const QPointer<MainWindow> window(this);
    const QPointer<QWebEngineView> target(webView);
    QTimer::singleShot(500, this, [window, target, requestSerial] {
        if (!window || !target)
            return;
        const auto expected = window->m_expectedBrowserFullScreenExits.constFind(target);
        if (expected == window->m_expectedBrowserFullScreenExits.cend()
            || *expected != requestSerial) {
            return;
        }
        window->exitBrowserFullScreen(target);
    });
}

void MainWindow::detachVideo(
    QWebEngineView *webView,
    const QUrl &origin,
    const QSize &videoSize
)
{
    if (!webView || !m_tabStates.contains(webView))
        return;
    BrowserTabState &state = m_tabStates[webView];
    if (state.detachedVideoWindow
        || !state.page
        || !state.detachedVideoSession) {
        return;
    }

    const QString originText = fullScreenOriginDisplay(origin);
    if (originText.isEmpty())
        return;
    if (!state.detachedVideoSession->beginDetached())
        return;

    auto *window = new DetachedVideoWindow(
        webView,
        tr("Video — %1").arg(originText),
        videoSize
    );
    auto *placeholder = new DetachedVideoPlaceholder(
        webView,
        tr("Video is playing in a separate window."),
        tr("Return Video")
    );
    state.detachedVideoWindow = window;
    state.detachedVideoPlaceholder = placeholder;

    connect(window, &DetachedVideoWindow::returnRequested, this, [this, webView] {
        requestDetachedVideoReturn(webView);
    });
    connect(placeholder, &DetachedVideoPlaceholder::returnRequested, this, [this, webView] {
        requestDetachedVideoReturn(webView);
    });
    connect(window, &DetachedVideoWindow::contextMenuRequested, this, [this, webView, window](
        const QPoint &position
    ) {
        showWebContextMenu(webView, window->webView(), position);
    });
    window->show();
    window->raise();
    window->activateWindow();
    m_lastInteractionWebView = webView;
    updateCurrentTabUi();
}

void MainWindow::requestDetachedVideoReturn(QWebEngineView *webView)
{
    auto state = m_tabStates.find(webView);
    if (state == m_tabStates.end()
        || !state->detachedVideoWindow
        || !state->detachedVideoSession) {
        return;
    }
    static_cast<void>(state->detachedVideoSession->requestReturn());
}

void MainWindow::restoreDetachedVideo(QWebEngineView *webView)
{
    auto state = m_tabStates.find(webView);
    if (state == m_tabStates.end())
        return;
    if (state->detachedVideoSession)
        state->detachedVideoSession->reset();
    if (!state->detachedVideoWindow)
        return;

    DetachedVideoWindow *window = state->detachedVideoWindow;
    DetachedVideoPlaceholder *placeholder = state->detachedVideoPlaceholder;
    cancelExternalUrlPrompt(webView);
    state->detachedVideoWindow.clear();
    state->detachedVideoPlaceholder.clear();

    window->restorePage();
    if (placeholder) {
        placeholder->hide();
        placeholder->deleteLater();
    }
    window->hide();
    window->deleteLater();

    if (m_lastInteractionWebView == webView)
        m_lastInteractionWebView = currentWebView();

    if (webView == currentWebView()) {
        updateCurrentTabUi();
        webView->setFocus(Qt::OtherFocusReason);
    }
}

void MainWindow::restoreAllDetachedVideos()
{
    const QList<QWebEngineView *> webViews = m_tabStates.keys();
    for (QWebEngineView *webView : webViews) {
        const auto state = m_tabStates.constFind(webView);
        if (state == m_tabStates.cend() || !state->detachedVideoWindow)
            continue;
        if (QWebEnginePage *page = state->page)
            page->triggerAction(QWebEnginePage::ExitFullScreen);
        if (m_tabStates.contains(webView)
            && m_tabStates.value(webView).detachedVideoWindow) {
            restoreDetachedVideo(webView);
        }
    }
}

void MainWindow::handleExternalUrlRequest(QWebEngineView *webView, const QUrl &url)
{
    if (!webView || !isTabInteractionActive(webView) || m_externalUrlDialog)
        return;

    const QUrl sourceUrl = urlForTab(webView);
    const QString source = sourceUrl.host().isEmpty()
        ? tr("The current page")
        : QStringLiteral("%1://%2").arg(sourceUrl.scheme(), sourceUrl.host());
    QString target = url.toDisplayString(QUrl::RemovePassword);
    constexpr qsizetype maximumDisplayedUrlLength = 700;
    if (target.size() > maximumDisplayedUrlLength)
        target = target.left(maximumDisplayedUrlLength) + QStringLiteral("…");

    auto *dialog = new QMessageBox(interactionParentForTab(webView));
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
        if (!accepted || !sourceView || !m_tabStates.contains(sourceView))
            return;
        const BrowserTabState state = m_tabStates.value(sourceView);
        if (sourceView != currentWebView() && !state.detachedVideoWindow)
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
        updateReaderModeAction();
        updateInstallWebAppAction();
        updateZoomActions();
        return;
    }

    const QUrl currentUrl = urlForTab(webView);
    m_address->setText(currentUrl.toString());
    if (currentUrl.isEmpty() && !m_tabStates.value(webView).pendingUrl.isEmpty())
        m_address->setText(m_tabStates.value(webView).pendingUrl.toString());
    const QString title = titleForTab(webView);
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
    updateReaderModeAction();
    updateInstallWebAppAction();
    updateZoomActions();
}

ReaderModeController *MainWindow::readerModeControllerForTab(
    QWebEngineView *webView
) const
{
    const auto state = m_tabStates.constFind(webView);
    return state == m_tabStates.cend() ? nullptr : state->readerModeController.data();
}

void MainWindow::toggleReaderMode(QWebEngineView *webView)
{
    if (!webView || webView != currentWebView() || m_windowRole == WindowRole::WebApp)
        return;

    ReaderModeController *controller = readerModeControllerForTab(webView);
    if (!controller)
        return;
    if (controller->isActive()) {
        controller->deactivate();
        return;
    }

    const auto state = m_tabStates.constFind(webView);
    if (state == m_tabStates.cend()
        || state->detachedVideoWindow
        || (m_fullScreenController && m_fullScreenController->isActive())) {
        return;
    }
    controller->activate();
}

void MainWindow::updateReaderModeAction()
{
    if (!m_readerModeAction)
        return;

    QWebEngineView *webView = currentWebView();
    ReaderModeController *controller = readerModeControllerForTab(webView);
    const bool active = controller && controller->isActive();
    const bool available = controller
        && (active
            || controller->availability() == ReaderModeController::Availability::Available);
    const bool blockedByPresentation = m_fullScreenController
        && m_fullScreenController->isActive();
    const auto state = m_tabStates.constFind(webView);
    const bool blockedByDetachedVideo = state != m_tabStates.cend()
        && state->detachedVideoWindow;

    m_readerModeAction->setVisible(
        m_windowRole != WindowRole::WebApp && available
    );
    m_readerModeAction->setEnabled(
        available
            && !controller->isActivationPending()
            && (!blockedByPresentation || active)
            && (!blockedByDetachedVideo || active)
    );
    m_readerModeAction->setChecked(active);
    m_readerModeAction->setText(active ? tr("Exit Reader Mode") : tr("Reader Mode"));
    m_readerModeAction->setToolTip(
        active ? tr("Exit Reader Mode (F9)") : tr("Reader Mode (F9)")
    );
}

void MainWindow::updateNavigationActions()
{
    QWebEngineView *webView = currentWebView();
    QWebEnginePage *page = pageForTab(webView);
    m_backAction->setEnabled(page && page->history()->canGoBack());
    m_forwardAction->setEnabled(page && page->history()->canGoForward());
    m_reloadAction->setEnabled(page != nullptr);
}

void MainWindow::updateTabNavigationActions()
{
    const int tabCount = m_tabBar ? m_tabBar->count() : 0;
    const bool tabNavigationAvailable = m_windowRole != WindowRole::WebApp;
    if (m_newTabAction)
        m_newTabAction->setEnabled(tabNavigationAvailable);
    if (m_closeTabAction)
        m_closeTabAction->setEnabled(tabCount > 0);
    if (m_reopenClosedTabAction) {
        m_reopenClosedTabAction->setEnabled(
            m_windowRole == WindowRole::Primary && !m_closedTabs.isEmpty()
        );
    }
    if (m_nextTabAction)
        m_nextTabAction->setEnabled(tabNavigationAvailable && tabCount > 1);
    if (m_previousTabAction)
        m_previousTabAction->setEnabled(tabNavigationAvailable && tabCount > 1);
    if (m_switchToTabMenu)
        m_switchToTabMenu->setEnabled(tabNavigationAvailable && tabCount > 0);
    for (int index = 0; index < m_numberedTabActions.size(); ++index) {
        m_numberedTabActions.at(index)->setEnabled(
            tabNavigationAvailable
                && TabNavigation::numberedTabIndex(index + 1, tabCount) >= 0
        );
    }
}

void MainWindow::updateBookmarkAction()
{
    if (!m_bookmarkAction)
        return;
    QWebEngineView *webView = currentWebView();
    const QUrl url = BookmarkStore::normalizedUrl(urlForTab(webView));
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
        if (QWebEnginePage *page = pageForTab(webView))
            page->setUrl(result.url);
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
        selectedText = pageForTab(webView)->selectedText().trimmed();
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
        pageForTab(m_findView)->findText(QString());
    m_findView.clear();
    if (m_findBar)
        m_findBar->clearResults();
    if (m_findToolbar)
        m_findToolbar->hide();
    if (m_closeFindAction)
        m_closeFindAction->setEnabled(false);
    if (QWebEngineView *webView = currentWebView()) {
        const BrowserTabState state = m_tabStates.value(webView);
        if (state.detachedVideoWindow)
            state.detachedVideoWindow->webView()->setFocus(Qt::OtherFocusReason);
        else
            webView->setFocus(Qt::OtherFocusReason);
    }
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
        pageForTab(m_findView)->findText(QString());
    m_findView = webView;

    const QString query = m_findBar->query();
    const quint64 requestSerial = ++m_findRequestSerial;
    QWebEnginePage *page = pageForTab(webView);
    if (query.isEmpty()) {
        page->findText(QString());
        m_findBar->clearResults();
        return;
    }

    m_findBar->setSearching();
    QWebEnginePage::FindFlags flags;
    if (backward)
        flags.setFlag(QWebEnginePage::FindBackward);
    const QPointer<MainWindow> window(this);
    const QPointer<QWebEngineView> target(webView);
    page->findText(query, flags, [window, target, requestSerial, query](
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

void MainWindow::applyStoredPageZoom(QWebEngineView *webView)
{
    QWebEnginePage *page = pageForTab(webView);
    if (!page)
        return;
    QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
    page->setZoomFactor(storedPageZoomFactor(settings, page->url()));
    if (webView == currentWebView())
        updateZoomActions();
}

void MainWindow::changePageZoomBySteps(QWebEngineView *webView, int steps)
{
    QWebEnginePage *page = pageForTab(webView);
    if (!page || steps == 0)
        return;

    const int boundedSteps = std::clamp(steps, -32, 32);
    const bool zoomIn = boundedSteps > 0;
    double factor = page->zoomFactor();
    for (int remaining = std::abs(boundedSteps); remaining > 0; --remaining)
        factor = nextPageZoomFactor(factor, zoomIn);
    setPageZoom(webView, factor);
}

void MainWindow::setPageZoom(QWebEngineView *webView, double factor)
{
    QWebEnginePage *page = pageForTab(webView);
    if (!page)
        return;

    const double normalized = normalizedPageZoomFactor(factor);
    const QString siteKey = pageZoomSiteKey(page->url());
    bool persisted = true;
    if (!siteKey.isEmpty()) {
        QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
        persisted = persistPageZoomFactor(settings, page->url(), normalized);
    }

    if (siteKey.isEmpty()) {
        page->setZoomFactor(normalized);
        updateZoomActions();
    } else {
        MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
        const auto applyToWindow = [&siteKey, normalized](MainWindow *window) {
            for (QWebEngineView *candidate : window->m_tabStates.keys()) {
                QWebEnginePage *candidatePage = window->pageForTab(candidate);
                if (candidatePage && pageZoomSiteKey(candidatePage->url()) == siteKey)
                    candidatePage->setZoomFactor(normalized);
            }
            window->updateZoomActions();
        };
        applyToWindow(primary);
        for (MainWindow *popup : std::as_const(primary->m_popupWindows)) {
            if (popup)
                applyToWindow(popup);
        }
    }

    const int percentage = pageZoomPercentage(normalized);
    QString message = tr("Zoom: %1%").arg(percentage);
    if (!siteKey.isEmpty()) {
        message += persisted ? tr(" · saved for this site")
                             : tr(" · could not save this site setting");
    }
    statusBar()->showMessage(message, 3500);
}

void MainWindow::updateZoomActions()
{
    if (!m_zoomMenu
        || !m_zoomLevelAction
        || !m_zoomInAction
        || !m_zoomOutAction
        || !m_resetZoomAction) {
        return;
    }

    QWebEngineView *webView = commandTargetWebView();
    QWebEnginePage *page = pageForTab(webView);
    const int percentage = pageZoomPercentage(
        page ? page->zoomFactor() : defaultPageZoomFactor
    );
    m_zoomMenu->setEnabled(page != nullptr);
    m_zoomLevelAction->setText(QStringLiteral("%1%").arg(percentage));
    m_zoomInAction->setEnabled(
        page && percentage < pageZoomPercentage(maximumPageZoomFactor)
    );
    m_zoomOutAction->setEnabled(
        page && percentage > pageZoomPercentage(minimumPageZoomFactor)
    );
    m_resetZoomAction->setEnabled(
        page && percentage != pageZoomPercentage(defaultPageZoomFactor)
    );
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
    const QUrl url = BookmarkStore::normalizedUrl(urlForTab(webView));
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
    const QString pageTitle = titleForTab(webView).trimmed();
    const QString title = pageTitle.isEmpty()
        ? url.host()
        : pageTitle;
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
        pageForTab(currentWebView())->setUrl(url);
    });
    dialog.exec();
}

void MainWindow::detectWebAppManifest(QWebEngineView *webView)
{
    if (!webView || !m_tabStates.contains(webView))
        return;
    QWebEnginePage *page = pageForTab(webView);
    const QUrl documentUrl = page->url();
    if (documentUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
        m_tabStates[webView].manifestUrl = QUrl();
        if (webView == currentWebView())
            updateInstallWebAppAction();
        return;
    }

    const QPointer<MainWindow> window(this);
    const QPointer<QWebEngineView> target(webView);
    page->runJavaScript(
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
            if (window->urlForTab(target) == documentUrl) {
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
        ? urlForTab(webView).host()
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
        urlForTab(webView),
        state.manifestTitle,
    });
    m_installWebAppAction->setEnabled(false);
    m_installWebAppAction->setText(tr("Reading web app manifest…"));
    static_cast<BrowserPage *>(pageForTab(webView))->fetchWebAppManifest(
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
            static_cast<BrowserPage *>(pageForTab(webView))->cancelWebAppManifestFetch(requestId);
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

    QWebEnginePage *page = pageForTab(request.webView);
    const QPixmap pixmap = page ? page->icon().pixmap(256, 256) : QPixmap();
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
        QIcon icon(QStringLiteral(":/assets/app-icon.png"));
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
    if (m_tabBar && m_tabBar->count() == 0)
        createTab(m_preferences.startPage());
    presentPrimaryWindow();
}

void MainWindow::presentPrimaryWindow()
{
    qApp->setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.png")));
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
    primary->initializePrimaryTabsForExternalContent();
    primary->presentPrimaryWindow();
    primary->createTab(url, true);
}

void MainWindow::openWindowRequestInPrimary(QWebEngineNewWindowRequest &request)
{
    MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
    if (primary != this) {
        primary->openWindowRequestInPrimary(request);
        return;
    }

    const bool separateWindow = request.destination() == QWebEngineNewWindowRequest::InNewWindow
        || request.destination() == QWebEngineNewWindowRequest::InNewDialog;
    if (separateWindow) {
        primary->activatePrimaryWindow();
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

    primary->initializePrimaryTabsForExternalContent();
    primary->presentPrimaryWindow();
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

    SettingsDialogContext settingsContext;
    settingsContext.trustConfigurationPath = m_configurationPath;
    settingsContext.searchConfigurationPath = m_searchConfigurationPath;
    settingsContext.userAgentConfigurationPath = m_userAgentConfigurationPath;
    settingsContext.dnsConfigurationPath = m_dnsConfigurationPath;
    settingsContext.proxyConfigurationPath = m_proxyConfigurationPath;
    settingsContext.crossDomainConfigurationPath = m_crossDomainConfigurationPath;
    settingsContext.videoTranslationConfigurationPath =
        m_videoTranslationConfigurationPath;
    settingsContext.preferences = m_preferences;
    settingsContext.searchSettings = m_searchSettings;
    settingsContext.userAgentSettings = m_userAgentSettings;
    settingsContext.activeUserAgentSettings = m_activeUserAgentSettings;
    settingsContext.dnsSettings = m_dnsSettings;
    settingsContext.proxySettings = m_proxySettings;
    settingsContext.activeProxySettings = m_activeProxySettings;
    settingsContext.crossDomainSettings = m_profile->crossDomainSettings();
    settingsContext.videoTranslationSettings = m_videoTranslationSettings;
    settingsContext.networkBlockedByProxyError = m_networkBlockedByProxyError;
    settingsContext.userAgentConfigurationError = m_userAgentConfigurationError;
    settingsContext.profile = m_profile;
    settingsContext.historyStore = m_historyStore;
    settingsContext.webAppStore = m_webAppStore;
    settingsContext.votUserscriptManager = m_votUserscriptManager;
    SettingsDialog dialog(
        settingsContext,
        urlForTab(currentWebView()),
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
        const bool userAgentRestartRequired =
            !hasSameEffectiveUserAgentConfiguration(
                dialog.userAgentSettings(),
                m_activeUserAgentSettings
            );
        m_preferences = dialog.preferences();
        m_searchSettings = dialog.searchSettings();
        m_userAgentSettings = dialog.userAgentSettings();
        if (!userAgentRestartRequired)
            m_activeUserAgentSettings = m_userAgentSettings;
        m_userAgentConfigurationError.clear();
        m_dnsSettings = dialog.dnsSettings();
        m_proxySettings = dialog.proxySettings();
        m_crossDomainSettings = dialog.crossDomainSettings();
        m_videoTranslationSettings = dialog.videoTranslationSettings();
        QString crossDomainError;
        if (!m_profile->setCrossDomainSettings(
                m_crossDomainSettings,
                &crossDomainError
            )) {
            QMessageBox::warning(
                this,
                tr("Cannot apply site connection settings"),
                crossDomainError
            );
        }
        m_crossDomainPromptController->reset();
        if (m_votUserscriptManager) {
            m_votUserscriptManager->applySettings(m_videoTranslationSettings);
            for (auto iterator = m_tabStates.cbegin(); iterator != m_tabStates.cend(); ++iterator) {
                m_votUserscriptManager->configurePage(
                    qobject_cast<BrowserPage *>(iterator->page.data())
                );
            }
        }
        applyDeveloperToolsPreference();
        if (!m_preferences.saveBrowsingHistory() && m_addressCompletionPopup)
            m_addressCompletionPopup->hide();
        updateAddressPlaceholder();
        m_profile->setPersistSessionCookies(m_preferences.persistSessionCookies());
        if (m_preferences.startupMode() == StartupMode::RestoreTabs)
            scheduleSessionSave();
        else
            saveSession();
        for (MainWindow *popup : std::as_const(m_popupWindows)) {
            if (popup) {
                popup->m_preferences = m_preferences;
                popup->m_searchSettings = m_searchSettings;
                popup->m_userAgentSettings = m_userAgentSettings;
                popup->m_activeUserAgentSettings = m_activeUserAgentSettings;
                popup->m_userAgentConfigurationError.clear();
                popup->m_dnsSettings = m_dnsSettings;
                popup->m_proxySettings = m_proxySettings;
                popup->m_crossDomainSettings = m_crossDomainSettings;
                popup->m_videoTranslationSettings = m_videoTranslationSettings;
                if (m_votUserscriptManager) {
                    for (auto iterator = popup->m_tabStates.cbegin();
                        iterator != popup->m_tabStates.cend();
                         ++iterator) {
                        m_votUserscriptManager->configurePage(
                            qobject_cast<BrowserPage *>(iterator->page.data())
                        );
                    }
                }
                popup->m_crossDomainPromptController->reset();
                popup->applyDeveloperToolsPreference();
                popup->updateAddressPlaceholder();
                if (!m_preferences.saveBrowsingHistory()
                    && popup->m_addressCompletionPopup) {
                    popup->m_addressCompletionPopup->hide();
                }
            }
        }
        m_crossDomainPromptRoutes.clear();
        reloadRules();
        if (languageChanged || proxyRestartRequired || userAgentRestartRequired) {
            QStringList restartChanges;
            if (languageChanged)
                restartChanges.append(tr("Interface language"));
            if (proxyRestartRequired)
                restartChanges.append(tr("Proxy settings"));
            if (userAgentRestartRequired)
                restartChanges.append(tr("User-Agent profile"));
            const QString restartMessage = tr(
                "Restart PanBrowser to apply these changes:\n• %1"
            ).arg(restartChanges.join(QStringLiteral("\n• ")));
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
    QString error;
    const BrowserSession session = m_sessionStore.load(&error);
    if (!error.isEmpty())
        qWarning().noquote() << "[PanBrowser session]" << error;

    if (m_preferences.startupMode() != StartupMode::RestoreTabs) {
        m_restoringSession = true;
        for (const SessionTab &tab : session.tabs) {
            if (tab.pinned)
                createTab(tab.url, false, true, tab.title, true);
        }
        createTab(m_preferences.startPage());
        m_restoringSession = false;
        updateCurrentTabUi();
        return;
    }

    if (session.tabs.isEmpty()) {
        createTab(m_preferences.startPage());
        return;
    }

    m_restoringSession = true;
    for (const SessionTab &tab : session.tabs)
        createTab(tab.url, false, true, tab.title, tab.pinned);
    m_tabBar->setCurrentIndex(session.activeIndex);
    m_tabStack->setCurrentIndex(session.activeIndex);
    m_restoringSession = false;
    activatePendingTab(currentWebView());
    updateCurrentTabUi();
}

void MainWindow::initializePrimaryTabsForExternalContent()
{
    if (m_primaryTabsInitialized)
        return;

    m_primaryTabsInitialized = true;
    QString error;
    const BrowserSession session = m_sessionStore.load(&error);
    if (!error.isEmpty())
        qWarning().noquote() << "[PanBrowser session]" << error;

    m_restoringSession = true;
    for (const SessionTab &tab : session.tabs) {
        if (tab.pinned)
            createTab(tab.url, false, true, tab.title, true);
    }
    m_restoringSession = false;
}

void MainWindow::scheduleSessionSave()
{
    if (!m_ownsBrowserResources || m_restoringSession || !m_sessionSaveTimer)
        return;
    if (m_preferences.startupMode() == StartupMode::RestoreTabs
        || hasPinnedTabs()
        || QFile::exists(m_sessionStore.path())) {
        m_sessionSaveTimer->start();
    }
}

void MainWindow::saveSession()
{
    if (!m_ownsBrowserResources)
        return;
    if (m_sessionSaveTimer)
        m_sessionSaveTimer->stop();
    if (m_discardSessionOnClose) {
        m_sessionStore.clear();
        return;
    }

    const BrowserSession session = currentSession(
        m_preferences.startupMode() == StartupMode::RestoreTabs
    );
    if (session.tabs.isEmpty()) {
        m_sessionStore.clear();
        return;
    }

    QString error;
    if (!m_sessionStore.save(session, &error))
        qWarning().noquote() << "[PanBrowser session]" << error;
}

BrowserSession MainWindow::currentSession(bool includeRegularTabs) const
{
    BrowserSession session;
    if (!m_tabStack)
        return session;

    const int activeTabIndex = m_tabBar ? m_tabBar->currentIndex() : 0;
    for (int index = 0; index < m_tabStack->count(); ++index) {
        QWebEngineView *webView = qobject_cast<QWebEngineView *>(m_tabStack->widget(index));
        if (!webView)
            continue;
        const BrowserTabState state = m_tabStates.value(webView);
        if (!includeRegularTabs && !state.pinned)
            continue;
        const QUrl url = state.pendingUrl.isEmpty() ? urlForTab(webView) : state.pendingUrl;
        if (index <= activeTabIndex)
            session.activeIndex = static_cast<int>(session.tabs.size());
        session.tabs.append({url, state.title, state.pinned});
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
        tr("%n custom rule(s)", nullptr, static_cast<int>(count))
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
    QString permissionError;
    if (!PrivateData::ensureDirectory(directory, &permissionError)
        || !PrivateData::ensureDirectory(
            QDir(directory).filePath(QStringLiteral("Certificates")),
            &permissionError
        )) {
        qWarning().noquote() << "[PanBrowser configuration]" << permissionError;
    }
    const QString path = QDir(directory).filePath(QStringLiteral("rules.json"));
    if (QFile::exists(path)) {
        if (!PrivateData::restrictFile(path, &permissionError))
            qWarning().noquote() << "[PanBrowser configuration]" << permissionError;
        return path;
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("startPage"), QStringLiteral("https://example.com"));
    root.insert(QStringLiteral("rules"), QJsonArray());

    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        if (file.commit() && !PrivateData::restrictFile(path, &permissionError))
            qWarning().noquote() << "[PanBrowser configuration]" << permissionError;
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

void MainWindow::initializeUserAgentSettings()
{
    const QString directory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation
    );
    QDir().mkpath(directory);
    m_userAgentConfigurationPath = QDir(directory).filePath(
        QStringLiteral("user-agents.json")
    );
    m_userAgentSettings = UserAgentSettings::defaults();
    m_userAgentConfigurationError.clear();

    QString error;
    if (QFile::exists(m_userAgentConfigurationPath)) {
        UserAgentSettings loaded;
        if (loaded.load(m_userAgentConfigurationPath, &error)) {
            m_userAgentSettings = loaded;
        } else {
            m_userAgentConfigurationError = error;
            qWarning().noquote() << "[PanBrowser User-Agent settings]" << error
                                 << "Using Chromium defaults; the file was not overwritten.";
        }
    } else if (!m_userAgentSettings.save(m_userAgentConfigurationPath, &error)) {
        m_userAgentConfigurationError = error;
        qWarning().noquote() << "[PanBrowser User-Agent settings]" << error;
    }
    m_activeUserAgentSettings = m_userAgentSettings;
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

void MainWindow::initializeCrossDomainSettings()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString directoryError;
    if (!PrivateData::ensureDirectory(directory, &directoryError))
        qWarning().noquote() << "[PanBrowser site connections]" << directoryError;
    m_crossDomainConfigurationPath = QDir(directory).filePath(
        QStringLiteral("site-connections.json")
    );
    m_crossDomainSettings = CrossDomainSettings::defaults();

    QString error;
    if (QFile::exists(m_crossDomainConfigurationPath)
        || QFile::exists(m_crossDomainConfigurationPath + QStringLiteral(".backup"))) {
        CrossDomainSettings loaded;
        bool recoveredFromBackup = false;
        if (loaded.loadRecoveringBackup(
                m_crossDomainConfigurationPath,
                &recoveredFromBackup,
                &error
            )) {
            m_crossDomainSettings = loaded;
            if (recoveredFromBackup) {
                m_crossDomainRecoveredFromBackup = true;
                m_crossDomainConfigurationError = error;
                qWarning().noquote() << "[PanBrowser site connections]" << error
                                     << "Loaded the backup configuration.";
            }
        } else {
            m_crossDomainSettings.setEnabled(true);
            m_crossDomainConfigurationError = error;
            qWarning().noquote() << "[PanBrowser site connections]" << error
                                 << "Using fail-closed in-memory settings; the file was not overwritten.";
        }
        return;
    }

    if (!m_crossDomainSettings.save(m_crossDomainConfigurationPath, &error))
        qWarning().noquote() << "[PanBrowser site connections]" << error;
}

void MainWindow::initializeVideoTranslationSettings()
{
    const QString directory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation
    );
    QString directoryError;
    if (!PrivateData::ensureDirectory(directory, &directoryError))
        qWarning().noquote() << "[PanBrowser VOT settings]" << directoryError;
    m_videoTranslationConfigurationPath = QDir(directory).filePath(
        QStringLiteral("video-translation.json")
    );
    m_videoTranslationSettings = VideoTranslationSettings::defaults();

    QString error;
    if (QFile::exists(m_videoTranslationConfigurationPath)) {
        VideoTranslationSettings loaded;
        if (loaded.load(m_videoTranslationConfigurationPath, &error)) {
            m_videoTranslationSettings = loaded;
        } else {
            qWarning().noquote() << "[PanBrowser VOT settings]" << error
                                 << "Disabling VOT in memory; the file was not overwritten.";
        }
        return;
    }

    if (!m_videoTranslationSettings.save(
            m_videoTranslationConfigurationPath,
            &error
        )) {
        qWarning().noquote() << "[PanBrowser VOT settings]" << error;
    }
}

void MainWindow::cancelCrossDomainPromptsForView(QWebEngineView *webView)
{
    if (!webView)
        return;
    MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
    QList<QPointer<MainWindow>> windows;
    windows.append(primary);
    for (MainWindow *window : std::as_const(primary->m_popupWindows)) {
        if (window)
            windows.append(window);
    }
    if (!windows.contains(this))
        windows.append(this);

    for (MainWindow *window : std::as_const(windows)) {
        if (window && window->m_crossDomainPromptController)
            window->m_crossDomainPromptController->cancelForView(webView);
    }
}

void MainWindow::routeCrossDomainRequest(
    const QUrl &sourceUrl,
    const QString &sourceSite,
    const QString &targetHost,
    int resourceType,
    bool sourceUrlIsOriginOnly,
    int attempt
)
{
    MainWindow *primary = m_primaryWindow ? m_primaryWindow : this;
    if (primary != this) {
        primary->routeCrossDomainRequest(
            sourceUrl,
            sourceSite,
            targetHost,
            resourceType,
            sourceUrlIsOriginOnly,
            attempt
        );
        return;
    }

    if (!m_profile->isCrossDomainRequestPending(
            sourceSite,
            targetHost,
            sourceUrl,
            sourceUrlIsOriginOnly
        )) {
        return;
    }

    if (m_profile->crossDomainSettings().evaluate(sourceSite, targetHost)
        != CrossDomainEvaluation::Ask) {
        m_profile->dismissCrossDomainRequest(sourceSite, targetHost);
        return;
    }

    const QString requestKey = crossDomainRequestKey(sourceSite, targetHost);
    auto route = m_crossDomainPromptRoutes.find(requestKey);
    if (route != m_crossDomainPromptRoutes.end()
        && (!route->window
            || !route->anchor
            || !route->window->m_crossDomainPromptController
            || !route->window->m_tabStates.contains(route->anchor))) {
        route = m_crossDomainPromptRoutes.erase(route);
    }
    const bool hasPromptOwner = route != m_crossDomainPromptRoutes.end();

    QList<MainWindow *> windows;
    windows.append(this);
    for (MainWindow *window : std::as_const(m_popupWindows)) {
        if (window)
            windows.append(window);
    }
    if (auto *active = qobject_cast<MainWindow *>(QApplication::activeWindow())) {
        if (windows.removeOne(active))
            windows.prepend(active);
    }

    struct Candidate {
        MainWindow *window = nullptr;
        QWebEngineView *view = nullptr;
    };
    QList<Candidate> candidates;
    QList<QUrl> candidateUrls;
    for (MainWindow *window : std::as_const(windows)) {
        QList<QWebEngineView *> views = window->m_tabStates.keys();
        if (QWebEngineView *current = window->currentWebView()) {
            if (views.removeOne(current))
                views.prepend(current);
        }
        for (QWebEngineView *view : std::as_const(views)) {
            candidates.append({window, view});
            candidateUrls.append(window->urlForTab(view));
        }
    }

    const QList<qsizetype> matchingIndexes = crossDomainPromptCandidateIndexes(
        sourceUrl,
        sourceSite,
        sourceUrlIsOriginOnly,
        candidateUrls
    );
    const Candidate candidate = matchingIndexes.isEmpty()
        ? Candidate()
        : candidates.at(matchingIndexes.constFirst());
    if (candidate.window && candidate.view) {
        QList<CrossDomainAffectedView> affectedViews;
        affectedViews.reserve(matchingIndexes.size());
        for (const qsizetype index : matchingIndexes) {
            const Candidate &matchingCandidate = candidates.at(index);
            affectedViews.append({
                matchingCandidate.view,
                SiteDomain::normalizedPageUrl(
                    matchingCandidate.window->urlForTab(matchingCandidate.view)
                ),
            });
        }
        MainWindow *promptWindow = candidate.window;
        QWebEngineView *promptAnchor = candidate.view;
        if (hasPromptOwner) {
            promptWindow = route->window;
            promptAnchor = route->anchor;
        } else {
            m_crossDomainPromptRoutes.insert(
                requestKey,
                {promptWindow, promptAnchor}
            );
        }
        promptWindow->m_crossDomainPromptController->request(
            promptAnchor,
            affectedViews,
            QList<CrossDomainPromptSource>{
                {sourceUrl, sourceUrlIsOriginOnly},
            },
            sourceSite,
            targetHost,
            resourceType
        );
        return;
    }

    if (attempt < 3) {
        QTimer::singleShot(50 * (attempt + 1), this, [
            this,
            sourceUrl,
            sourceSite,
            targetHost,
            resourceType,
            sourceUrlIsOriginOnly,
            attempt
        ] {
            routeCrossDomainRequest(
                sourceUrl,
                sourceSite,
                targetHost,
                resourceType,
                sourceUrlIsOriginOnly,
                attempt + 1
            );
        });
        return;
    }
    m_profile->dismissCrossDomainRequestSource(
        sourceSite,
        targetHost,
        sourceUrl,
        sourceUrlIsOriginOnly
    );
}

void MainWindow::showCrossDomainConfigurationError()
{
    if (m_crossDomainConfigurationError.isEmpty())
        return;
    const QString message = m_crossDomainRecoveredFromBackup
        ? tr(
            "PanBrowser could not load the primary site connection settings and recovered the previous backup. Review Site Connections and save the settings to repair the primary file."
        )
        : tr(
            "PanBrowser could not load the site connection settings. Unknown third-party connections are being blocked until you save a valid configuration in Site Connections."
        );
    QMessageBox::warning(
        this,
        m_crossDomainRecoveredFromBackup
            ? tr("Site connection settings recovered")
            : tr("Site connection settings unavailable"),
        message + QStringLiteral("\n\n") + m_crossDomainConfigurationError
    );
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
