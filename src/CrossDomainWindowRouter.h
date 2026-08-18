#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class BrowserProfile;
class CrossDomainPromptController;
class MainWindow;
class QWebEngineView;

class CrossDomainWindowRouter final : public QObject {
    Q_OBJECT

public:
    explicit CrossDomainWindowRouter(
        BrowserProfile *profile,
        QObject *parent = nullptr
    );
    ~CrossDomainWindowRouter() override;

    void registerWindow(
        MainWindow *window,
        CrossDomainPromptController *controller
    );
    void unregisterWindow(MainWindow *window);
    void cancelForView(QWebEngineView *webView);
    void clearRoutes();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    void route(
        const QUrl &sourceUrl,
        const QString &sourceSite,
        const QString &targetHost,
        int resourceType,
        bool sourceUrlIsOriginOnly,
        int attempt = 0
    );
};
