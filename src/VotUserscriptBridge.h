#pragma once

#include "VotUserscriptPackage.h"

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QWebEngineFrame>

class BrowserPage;
class QAuthenticator;
class QJsonObject;
class QNetworkReply;
class VotUserscriptStore;

namespace VotNetworkPolicy {

[[nodiscard]] bool mayForwardHeaderAcrossRedirect(
    const QByteArray &name,
    const QUrl &source,
    const QUrl &target
);

} // namespace VotNetworkPolicy

class VotUserscriptBridge final : public QObject {
    Q_OBJECT

public:
    explicit VotUserscriptBridge(
        BrowserPage *page,
        const VotUserscript &userscript,
        VotUserscriptStore *store = nullptr,
        QObject *parent = nullptr
    );

    static QString scriptName();
    static QString messagePrefix();
    static qsizetype maximumMessageLength();
    static QString injectedSource(
        const VotUserscript &userscript,
        const QString &token,
        const QJsonObject &storedValues = {}
    );

signals:
    void proxyAuthenticationRequired(
        BrowserPage *page,
        const QUrl &requestUrl,
        QAuthenticator *authenticator,
        const QString &proxyHost
    );

private:
    struct RequestState;

    void handleMessage(const QJsonObject &message);
    void beginRequest(const QSharedPointer<RequestState> &state, const QUrl &url);
    void finishRequest(
        const QSharedPointer<RequestState> &state,
        QNetworkReply *reply
    );
    void abortRequest(const QString &id);
    void installScript();
    void broadcastStorageUpdate(
        const QString &name,
        const QJsonValue &value,
        bool removed
    );
    void deliver(
        const QJsonObject &message,
        const QString &frameToken = QString()
    );
    void deliverToFrame(
        const QWebEngineFrame &frame,
        const QString &frameToken,
        const QString &payload
    );

    QPointer<BrowserPage> m_page;
    VotUserscript m_userscript;
    VotUserscriptStore *m_store = nullptr;
    QNetworkAccessManager m_network;
    QHash<QString, QSharedPointer<RequestState>> m_requests;
    QHash<QString, QWebEngineFrame> m_frames;
};
