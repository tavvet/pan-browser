#pragma once

#include "BrowserPreferences.h"
#include "CrossDomainSettings.h"
#include "DnsSettings.h"
#include "HistoryStore.h"
#include "ProxySettings.h"
#include "SessionStore.h"
#include "SearchSettings.h"
#include "TrustConfiguration.h"
#include "VideoTranslationSettings.h"
#include "WebAppStore.h"

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QSet>
#include <QSize>
#include <QString>

#include <memory>

class QLabel;
class AddressLineEdit;
class QMessageBox;
class QProgressBar;
class QStackedWidget;
class QTimer;
class QToolBar;
class QMenu;
class QAction;
class QPoint;
class BookmarkStore;
class BrowserFullScreenController;
class BrowserPage;
class BrowserTabBar;
class BrowserProfile;
class DownloadButton;
class DownloadManager;
class DownloadsPanel;
class DetachedVideoSession;
class DetachedVideoPlaceholder;
class DetachedVideoWindow;
class FindBar;
class HttpAuthenticationController;
class AddressCompletionPopup;
class PermissionController;
class PermissionPrompt;
class CrossDomainPrompt;
class CrossDomainPromptController;
class ProxyAuthenticationController;
class QCloseEvent;
class QWebEngineCertificateError;
class QWebEngineFullScreenRequest;
class QWebEngineNewWindowRequest;
class QWebEnginePage;
class QWebEngineView;
class WindowChromeController;
class VotUserscriptManager;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    enum class StartupPresentation {
        Browser,
        Background,
    };

    explicit MainWindow(
        StartupPresentation presentation = StartupPresentation::Browser,
        QWidget *parent = nullptr
    );
    ~MainWindow() override;

    void activatePrimaryWindow();
    void openUrlInPrimaryWindow(const QUrl &url);
    [[nodiscard]] bool launchInstalledWebApp(const QString &id);
    [[nodiscard]] QString startupError() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class WindowRole {
        Primary,
        Popup,
        WebApp,
    };

    struct BrowserTabState {
        QString title;
        QString lastAcceptedRule;
        QString trustStatus = QStringLiteral("Ready");
        QString previousAcceptedRule;
        QString previousTrustStatus;
        bool trustError = false;
        bool previousTrustError = false;
        bool externalNavigationDelegated = false;
        bool suppressNextHistoryVisit = false;
        bool loading = false;
        bool pinned = false;
        int progress = 0;
        qint64 videoPopoutRequestDeadlineMs = 0;
        quint64 videoPopoutRequestSerial = 0;
        QUrl videoPopoutRequestOrigin;
        QSize videoPopoutRequestSize = QSize(16, 9);
        QUrl topLevelUrl;
        QUrl pendingUrl;
        HistoryTransition pendingHistoryTransition = HistoryTransition::Other;
        QUrl manifestUrl;
        QString manifestTitle;
        QPointer<QWebEnginePage> page;
        QPointer<QWebEngineView> developerToolsView;
        QPointer<DetachedVideoSession> detachedVideoSession;
        QPointer<DetachedVideoWindow> detachedVideoWindow;
        QPointer<DetachedVideoPlaceholder> detachedVideoPlaceholder;
    };

    struct PendingManifestRequest {
        QPointer<QWebEngineView> webView;
        QUrl manifestUrl;
        QUrl documentUrl;
        QString fallbackTitle;
    };

    struct CrossDomainPromptRoute {
        QPointer<MainWindow> window;
        QPointer<QWebEngineView> anchor;
    };

    MainWindow(
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
    );

    void createInterface();
    MainWindow *createPopupWindow(WindowRole role, const QRect &requestedGeometry);
    MainWindow *createWebAppWindow(const WebApp &app);
    void applyPopupGeometry(const QRect &requestedGeometry);
    QWebEngineView *createTab(
        const QUrl &url,
        bool activate = true,
        bool deferred = false,
        const QString &restoredTitle = QString(),
        bool pinned = false
    );
    void closeTab(int index);
    void showTabContextMenu(const QPoint &position);
    void setTabPinned(int index, bool pinned);
    void updateTabPresentation(QWebEngineView *webView);
    [[nodiscard]] bool hasPinnedTabs() const;
    QWebEngineView *currentWebView() const;
    QWebEnginePage *pageForTab(QWebEngineView *webView) const;
    QWebEngineView *webViewForPage(const BrowserPage *page) const;
    QUrl urlForTab(QWebEngineView *webView) const;
    QString titleForTab(QWebEngineView *webView) const;
    QWebEngineView *activeInteractionWebView() const;
    QWebEngineView *commandTargetWebView() const;
    QWebEngineView *renderingViewForTab(QWebEngineView *webView) const;
    QWidget *interactionParentForTab(QWebEngineView *webView);
    bool isTabInteractionActive(QWebEngineView *webView) const;
    void returnDetachedVideoForPermissionPrompt(QWebEngineView *webView);
    void activatePendingTab(QWebEngineView *webView);
    void connectBrowserSignals(QWebEngineView *webView);
    void handleFullScreenRequest(
        QWebEngineView *webView,
        QWebEngineFullScreenRequest request
    );
    void exitBrowserFullScreen(QWebEngineView *webView);
    void requestBrowserFullScreenExit(QWebEngineView *webView);
    void detachVideo(
        QWebEngineView *webView,
        const QUrl &origin,
        const QSize &videoSize
    );
    void requestDetachedVideoReturn(QWebEngineView *webView);
    void restoreDetachedVideo(QWebEngineView *webView);
    void restoreAllDetachedVideos();
    void updateCurrentTabUi();
    void updateNavigationActions();
    void updateBookmarkAction();
    void updateAddressPlaceholder();
    void navigateFromAddressBar();
    void showAddressSuggestions();
    void openFindBar();
    void closeFindBar();
    void findInPage(bool backward = false);
    void applyStoredPageZoom(QWebEngineView *webView);
    void changePageZoomBySteps(QWebEngineView *webView, int steps);
    void setPageZoom(QWebEngineView *webView, double factor);
    void updateZoomActions();
    void showWebContextMenu(
        QWebEngineView *webView,
        QWebEngineView *renderingView,
        const QPoint &position
    );
    void openDeveloperTools(QWebEngineView *webView, bool inspectElement = false);
    void closeDeveloperTools(QWebEngineView *webView);
    void applyDeveloperToolsPreference();
    [[nodiscard]] QString developerToolsWindowTitle(QWebEngineView *webView) const;
    void openSettings();
    void openWebAppsSettings();
    void openSettingsPage(int page);
    void openInstalledWebApp(const QString &id);
    void installCurrentWebApp();
    void detectWebAppManifest(QWebEngineView *webView);
    void updateInstallWebAppAction();
    void rebuildWebAppsMenu();
    void handleFetchedWebAppManifest(
        const QString &requestId,
        const QByteArray &contents,
        const QString &fetchError
    );
    void openWindowRequestInPrimary(QWebEngineNewWindowRequest &request);
    void editCurrentBookmark();
    void openBookmarks();
    void reloadRules();
    void reloadRulesLocal();
    void restoreInitialTabs();
    void scheduleSessionSave();
    void saveSession();
    BrowserSession currentSession(bool includeRegularTabs = true) const;
    void handleCertificateError(
        QWebEngineView *webView,
        const QWebEngineCertificateError &error
    );
    void handleExternalUrlRequest(QWebEngineView *webView, const QUrl &url);
    void cancelExternalUrlPrompt(QWebEngineView *webView = nullptr);
    void setTabTrustStatus(QWebEngineView *webView, const QString &text, bool error = false);
    void setTrustStatus(const QString &text, bool error = false);
    void restoreWindowPlacement();
    QString ensureConfiguration();
    void initializeSearchSettings();
    void initializeDnsSettings();
    void initializeProxySettings();
    void initializeCrossDomainSettings();
    void initializeVideoTranslationSettings();
    void cancelCrossDomainPromptsForView(QWebEngineView *webView);
    void routeCrossDomainRequest(
        const QUrl &sourceUrl,
        const QString &sourceSite,
        const QString &targetHost,
        int resourceType,
        bool sourceUrlIsOriginOnly,
        int attempt = 0
    );
    void showCrossDomainConfigurationError();
    void showProxyConfigurationError();

    BrowserProfile *m_profile = nullptr;
    VotUserscriptManager *m_votUserscriptManager = nullptr;
    DownloadManager *m_downloadManager = nullptr;
    HistoryStore *m_historyStore = nullptr;
    BookmarkStore *m_bookmarkStore = nullptr;
    WebAppStore *m_webAppStore = nullptr;
    DownloadsPanel *m_downloadsPanel = nullptr;
    DownloadButton *m_downloadButton = nullptr;
    FindBar *m_findBar = nullptr;
    QToolBar *m_findToolbar = nullptr;
    AddressCompletionPopup *m_addressCompletionPopup = nullptr;
    PermissionController *m_permissionController = nullptr;
    PermissionPrompt *m_permissionPrompt = nullptr;
    CrossDomainPromptController *m_crossDomainPromptController = nullptr;
    CrossDomainPrompt *m_crossDomainPrompt = nullptr;
    HttpAuthenticationController *m_httpAuthenticationController = nullptr;
    ProxyAuthenticationController *m_proxyAuthenticationController = nullptr;
    QPointer<QMessageBox> m_externalUrlDialog;
    QPointer<QWebEngineView> m_externalUrlSource;
    QPointer<QWebEngineView> m_findView;
    QPointer<QWebEngineView> m_lastInteractionWebView;
    std::unique_ptr<BrowserFullScreenController> m_fullScreenController;
    QHash<QWebEngineView *, quint64> m_expectedBrowserFullScreenExits;
    QSet<QWebEngineView *> m_expectedDetachedVideoExits;
    quint64 m_browserFullScreenExitSerial = 0;
    BrowserTabBar *m_tabBar = nullptr;
    QStackedWidget *m_tabStack = nullptr;
    AddressLineEdit *m_address = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_securityIndicator = nullptr;
    QAction *m_bookmarkAction = nullptr;
    QAction *m_closeFindAction = nullptr;
    QAction *m_installWebAppAction = nullptr;
    QAction *m_developerToolsAction = nullptr;
    QAction *m_zoomLevelAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_resetZoomAction = nullptr;
    QMenu *m_webAppsMenu = nullptr;
    QMenu *m_zoomMenu = nullptr;
    QLabel *m_trustStatus = nullptr;
    QLabel *m_ruleCount = nullptr;
    QProgressBar *m_progress = nullptr;
    QTimer *m_sessionSaveTimer = nullptr;
    QTimer *m_addressSuggestionTimer = nullptr;
    WindowChromeController *m_windowChromeController = nullptr;
    QHash<QWebEngineView *, BrowserTabState> m_tabStates;
    QHash<QString, PendingManifestRequest> m_manifestRequests;
    QHash<QString, CrossDomainPromptRoute> m_crossDomainPromptRoutes;
    BrowserPreferences m_preferences;
    SearchSettings m_searchSettings;
    DnsSettings m_dnsSettings;
    ProxySettings m_proxySettings;
    ProxySettings m_activeProxySettings;
    CrossDomainSettings m_crossDomainSettings;
    VideoTranslationSettings m_videoTranslationSettings;
    SessionStore m_sessionStore;
    TrustPolicy m_trustPolicy;
    QString m_configurationPath;
    QString m_searchConfigurationPath;
    QString m_dnsConfigurationPath;
    QString m_proxyConfigurationPath;
    QString m_crossDomainConfigurationPath;
    QString m_videoTranslationConfigurationPath;
    QString m_proxyConfigurationError;
    QString m_crossDomainConfigurationError;
    QString m_historyError;
    QString m_bookmarkError;
    QString m_startupError;
    MainWindow *m_primaryWindow = nullptr;
    WindowRole m_windowRole = WindowRole::Primary;
    WebApp m_webApp;
    QList<QPointer<MainWindow>> m_popupWindows;
    bool m_ownsBrowserResources = true;
    bool m_primaryTabsInitialized = false;
    bool m_restoringSession = false;
    bool m_discardSessionOnClose = false;
    bool m_networkBlockedByProxyError = false;
    bool m_crossDomainRecoveredFromBackup = false;
    bool m_integratedWindowChrome = false;
    int m_zoomAngleRemainder = 0;
    int m_zoomPixelRemainder = 0;
    quint64 m_findRequestSerial = 0;
};
