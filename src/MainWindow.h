#pragma once

#include "BrowserPreferences.h"
#include "HistoryStore.h"
#include "SessionStore.h"
#include "SearchSettings.h"
#include "TrustConfiguration.h"

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QString>

class QLabel;
class AddressLineEdit;
class QMessageBox;
class QProgressBar;
class QStackedWidget;
class QTabBar;
class QTimer;
class QAction;
class BrowserProfile;
class DownloadButton;
class DownloadManager;
class DownloadsPanel;
class HistoryCompletionPopup;
class PermissionController;
class PermissionPrompt;
class QCloseEvent;
class QWebEngineCertificateError;
class QWebEngineView;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class WindowRole {
        Primary,
        Popup,
    };

    struct BrowserTabState {
        QString lastAcceptedRule;
        QString trustStatus = QStringLiteral("Ready");
        QString previousAcceptedRule;
        QString previousTrustStatus;
        bool trustError = false;
        bool previousTrustError = false;
        bool externalNavigationDelegated = false;
        bool suppressNextHistoryVisit = false;
        bool loading = false;
        int progress = 0;
        QUrl pendingUrl;
        HistoryTransition pendingHistoryTransition = HistoryTransition::Other;
    };

    MainWindow(
        BrowserProfile *sharedProfile,
        DownloadManager *sharedDownloadManager,
        HistoryStore *sharedHistoryStore,
        MainWindow *primaryWindow,
        WindowRole role,
        QWidget *parent
    );

    void createInterface();
    MainWindow *createPopupWindow(WindowRole role, const QRect &requestedGeometry);
    void applyPopupGeometry(const QRect &requestedGeometry);
    QWebEngineView *createTab(
        const QUrl &url,
        bool activate = true,
        bool deferred = false,
        const QString &restoredTitle = QString()
    );
    void closeTab(int index);
    QWebEngineView *currentWebView() const;
    void activatePendingTab(QWebEngineView *webView);
    void connectBrowserSignals(QWebEngineView *webView);
    void updateCurrentTabUi();
    void updateNavigationActions();
    void updateAddressPlaceholder();
    void navigateFromAddressBar();
    void showHistorySuggestions();
    void openSettings(bool trustRules = false, bool history = false);
    void reloadRules();
    void reloadRulesLocal();
    void restoreInitialTabs();
    void scheduleSessionSave();
    void saveSession();
    BrowserSession currentSession() const;
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

    BrowserProfile *m_profile = nullptr;
    DownloadManager *m_downloadManager = nullptr;
    HistoryStore *m_historyStore = nullptr;
    DownloadsPanel *m_downloadsPanel = nullptr;
    DownloadButton *m_downloadButton = nullptr;
    HistoryCompletionPopup *m_historyCompletionPopup = nullptr;
    PermissionController *m_permissionController = nullptr;
    PermissionPrompt *m_permissionPrompt = nullptr;
    QPointer<QMessageBox> m_externalUrlDialog;
    QPointer<QWebEngineView> m_externalUrlSource;
    QTabBar *m_tabBar = nullptr;
    QStackedWidget *m_tabStack = nullptr;
    AddressLineEdit *m_address = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_securityIndicator = nullptr;
    QLabel *m_trustStatus = nullptr;
    QLabel *m_ruleCount = nullptr;
    QProgressBar *m_progress = nullptr;
    QTimer *m_sessionSaveTimer = nullptr;
    QTimer *m_historySuggestionTimer = nullptr;
    QHash<QWebEngineView *, BrowserTabState> m_tabStates;
    BrowserPreferences m_preferences;
    SearchSettings m_searchSettings;
    SessionStore m_sessionStore;
    TrustPolicy m_trustPolicy;
    QString m_configurationPath;
    QString m_searchConfigurationPath;
    QString m_historyError;
    MainWindow *m_primaryWindow = nullptr;
    QList<QPointer<MainWindow>> m_popupWindows;
    bool m_ownsBrowserResources = true;
    bool m_restoringSession = false;
    bool m_discardSessionOnClose = false;
};
