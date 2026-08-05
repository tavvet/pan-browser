#pragma once

#include "BrowserPreferences.h"
#include "SessionStore.h"
#include "TrustConfiguration.h"

#include <QHash>
#include <QMainWindow>
#include <QString>

class QLabel;
class QLineEdit;
class QProgressBar;
class QStackedWidget;
class QTabBar;
class QTimer;
class QAction;
class BrowserProfile;
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
    struct BrowserTabState {
        QString lastAcceptedRule;
        QString trustStatus = QStringLiteral("Ready");
        bool trustError = false;
        bool loading = false;
        int progress = 0;
        QUrl pendingUrl;
    };

    void createInterface();
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
    void navigateFromAddressBar();
    void openSettings(bool trustRules = false);
    void reloadRules();
    void restoreInitialTabs();
    void scheduleSessionSave();
    void saveSession();
    BrowserSession currentSession() const;
    void handleCertificateError(
        QWebEngineView *webView,
        const QWebEngineCertificateError &error
    );
    void setTabTrustStatus(QWebEngineView *webView, const QString &text, bool error = false);
    void setTrustStatus(const QString &text, bool error = false);
    void restoreWindowPlacement();
    QString ensureConfiguration();

    BrowserProfile *m_profile = nullptr;
    QTabBar *m_tabBar = nullptr;
    QStackedWidget *m_tabStack = nullptr;
    QLineEdit *m_address = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_securityIndicator = nullptr;
    QLabel *m_trustStatus = nullptr;
    QLabel *m_ruleCount = nullptr;
    QProgressBar *m_progress = nullptr;
    QTimer *m_sessionSaveTimer = nullptr;
    QHash<QWebEngineView *, BrowserTabState> m_tabStates;
    BrowserPreferences m_preferences;
    SessionStore m_sessionStore;
    TrustPolicy m_trustPolicy;
    QString m_configurationPath;
    bool m_restoringSession = false;
    bool m_discardSessionOnClose = false;
};
