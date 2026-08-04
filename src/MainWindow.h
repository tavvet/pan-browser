#pragma once

#include "TrustConfiguration.h"

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QProgressBar;
class QAction;
class QCloseEvent;
class QWebEngineCertificateError;
class QWebEngineView;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createInterface();
    void connectBrowserSignals();
    void navigateFromAddressBar();
    void openTrustRules();
    void reloadRules();
    void handleCertificateError(const QWebEngineCertificateError &error);
    void setTrustStatus(const QString &text, bool error = false);
    void restoreWindowPlacement();
    QString ensureConfiguration();

    QWebEngineView *m_webView = nullptr;
    QLineEdit *m_address = nullptr;
    QAction *m_securityIndicator = nullptr;
    QLabel *m_trustStatus = nullptr;
    QLabel *m_ruleCount = nullptr;
    QProgressBar *m_progress = nullptr;
    TrustPolicy m_trustPolicy;
    QString m_configurationPath;
    QString m_lastAcceptedRule;
};
