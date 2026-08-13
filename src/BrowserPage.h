#pragma once

#include "WebAppStore.h"

#include <QJsonObject>
#include <QSize>
#include <QWebEnginePage>

#include <optional>

class BrowserPage final : public QWebEnginePage {
    Q_OBJECT

public:
    explicit BrowserPage(QWebEngineProfile *profile, QObject *parent = nullptr);
    [[nodiscard]] QString videoPopoutToken() const;
    [[nodiscard]] QString votNetworkToken() const;
    void setWebApp(const WebApp &app);
    void fetchWebAppManifest(
        const QString &requestId,
        const QUrl &manifestUrl,
        qsizetype maximumBytes
    );
    void cancelWebAppManifestFetch(const QString &requestId);

signals:
    void externalUrlRequested(const QUrl &url);
    void outOfScopeNavigationRequested(const QUrl &url, int navigationType);
    void mainFrameNavigationRequested(const QUrl &url, int navigationType);
    void videoPopoutRequested(const QUrl &frameUrl, const QSize &videoSize);
    void votNetworkMessage(const QJsonObject &message);
    void webAppManifestFetched(
        const QString &requestId,
        const QByteArray &contents,
        const QString &error
    );

protected:
    bool acceptNavigationRequest(
        const QUrl &url,
        NavigationType type,
        bool isMainFrame
    ) override;
    void javaScriptConsoleMessage(
        JavaScriptConsoleMessageLevel level,
        const QString &message,
        int lineNumber,
        const QString &sourceId
    ) override;

private:
    std::optional<WebApp> m_webApp;
    QString m_videoPopoutToken;
    QString m_votNetworkToken;
};
