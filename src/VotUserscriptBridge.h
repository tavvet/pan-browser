#pragma once

#include "VotUserscriptPackage.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QWebEngineFrame>

class BrowserPage;
class QJsonObject;
class VotChromiumNetworkTransport;
class VotUserscriptStore;

class VotUserscriptBridge final : public QObject {
    Q_OBJECT

public:
    explicit VotUserscriptBridge(
        BrowserPage *page,
        const VotUserscript &userscript,
        VotUserscriptStore *store = nullptr,
        VotChromiumNetworkTransport *transport = nullptr,
        QObject *parent = nullptr
    );
    ~VotUserscriptBridge() override;

    static QString scriptName();
    static QString messagePrefix();
    static qsizetype maximumMessageLength();
    static QString injectedSource(
        const VotUserscript &userscript,
        const QString &token,
        const QJsonObject &storedValues = {}
    );

private:
    struct RequestState;

    void handleMessage(const QJsonObject &message);
    void handleTransportResponse(const QJsonObject &message);
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
    VotChromiumNetworkTransport *m_transport = nullptr;
    QHash<QString, QSharedPointer<RequestState>> m_requests;
    QHash<QString, QWebEngineFrame> m_frames;
};
