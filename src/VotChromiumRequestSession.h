#pragma once

#include "VotChromiumRequest.h"

#include <QJsonObject>
#include <QObject>
#include <QPointer>

class QAuthenticator;
class QTimer;
class QWebEnginePage;
class QWebEngineProfile;

class VotChromiumRequestSession final : public QObject {
    Q_OBJECT

public:
    explicit VotChromiumRequestSession(
        VotChromiumRequest request,
        QObject *parent = nullptr
    );
    ~VotChromiumRequestSession() override;

    [[nodiscard]] static QString responseMessagePrefix();
    [[nodiscard]] QString id() const;
    [[nodiscard]] bool isStarted() const;

    void start(
        QWebEngineProfile *profile,
        const QString &extensionId,
        const QString &token
    );
    void abort();
    void fail(const QString &error);

signals:
    void responseReady(const QJsonObject &response);
    void proxyAuthenticationRequired(
        BrowserPage *page,
        const QUrl &requestUrl,
        QAuthenticator *authenticator,
        const QString &proxyHost,
        bool *handled
    );

private:
    void startLoadedRequest(QWebEnginePage *page);
    void handleConsoleMessage(const QString &message);
    void finish(QJsonObject response);

    VotChromiumRequest m_request;
    QPointer<QWebEnginePage> m_page;
    QTimer *m_timeoutTimer = nullptr;
    QString m_token;
    bool m_started = false;
    bool m_finished = false;
};
