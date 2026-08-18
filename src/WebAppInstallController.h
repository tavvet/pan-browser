#pragma once

#include <QObject>

#include <memory>

class QAction;
class BrowserPage;
class QWebEngineView;
class QWidget;
class WebAppStore;

class WebAppInstallController final : public QObject {
    Q_OBJECT

public:
    explicit WebAppInstallController(
        WebAppStore *store,
        QAction *installAction,
        QWidget *dialogParent,
        QObject *parent = nullptr
    );
    ~WebAppInstallController() override;

    void clearManifest(QWebEngineView *webView);
    void forgetView(QWebEngineView *webView);
    void detectManifest(QWebEngineView *webView, BrowserPage *page);
    void currentViewChanged(QWebEngineView *webView);
    void installCurrent(QWebEngineView *webView, BrowserPage *page);
    void handleManifestFetched(
        const QString &requestId,
        const QByteArray &contents,
        const QString &fetchError
    );

signals:
    void openInstalledAppRequested(const QString &id);
    void statusMessageRequested(const QString &message, int timeoutMs);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
