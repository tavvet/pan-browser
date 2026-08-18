#pragma once

#include "VotChromiumRequest.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSslCertificate>
#include <QUrl>

#include <functional>

class BrowserPage;
class CrossDomainRequestInterceptor;
class QAuthenticator;
class QTimer;
class QWebEngineExtensionInfo;
class QWebEngineExtensionManager;
class QWebEnginePage;
class QWebEngineProfile;
class VotChromiumRequestSession;

enum class VotChromiumTransportState {
    Uninitialized,
    Loading,
    Ready,
    Error,
};

class VotChromiumNetworkTransport final : public QObject {
    Q_OBJECT

public:
    explicit VotChromiumNetworkTransport(
        QWebEngineProfile *profile,
        QObject *parent = nullptr
    );
    VotChromiumNetworkTransport(
        QWebEngineProfile *profile,
        const QString &cacheRoot,
        QObject *parent = nullptr
    );
    VotChromiumNetworkTransport(
        QWebEngineProfile *profile,
        const QString &cacheRoot,
        const QList<QSslCertificate> &additionalTrustedCertificates,
        QObject *parent = nullptr
    );
    ~VotChromiumNetworkTransport() override;

    static QString responseMessagePrefix();
    static bool prepareExtensionDirectory(
        const QString &directoryPath,
        QString *error = nullptr
    );

    void ensureReady();
    void sendRequest(const VotChromiumRequest &request);
    void abortRequest(const QString &id);
    bool registerRequestAuthorizer(
        const QString &requestId,
        std::function<bool(const QUrl &, int)> authorizer
    );
    void unregisterRequestAuthorizer(const QString &requestId);

    [[nodiscard]] VotChromiumTransportState state() const;
    [[nodiscard]] QString errorString() const;

signals:
    void stateChanged();
    void responseReady(const QJsonObject &response);
    void proxyAuthenticationRequired(
        BrowserPage *page,
        const QUrl &requestUrl,
        QAuthenticator *authenticator,
        const QString &proxyHost,
        bool *handled
    );

private:
    void beginExtensionLoad();
    bool createTransportProfile(QString *error);
    void enableExtension(
        QWebEngineExtensionManager *manager,
        const QWebEngineExtensionInfo &extension
    );
    void activateExtension(const QString &extensionId);
    void createControlPage(const QString &extensionId);
    void flushPendingRequests();
    void startRequest(const VotChromiumRequest &request);
    void handleSessionResponse(const QJsonObject &response);
    void resetTransportProfile();
    void fail(const QString &error);
    void setState(
        VotChromiumTransportState state,
        const QString &error = QString()
    );

    QPointer<QWebEngineProfile> m_profile;
    QWebEngineProfile *m_transportProfile = nullptr;
    CrossDomainRequestInterceptor *m_requestInterceptor = nullptr;
    QWebEnginePage *m_profileBootstrapPage = nullptr;
    QWebEnginePage *m_controlPage = nullptr;
    QTimer *m_initializationTimer = nullptr;
    QString m_cacheRoot;
    QList<QSslCertificate> m_additionalTrustedCertificates;
    QString m_extensionDirectory;
    QString m_extensionId;
    QString m_token;
    VotChromiumTransportState m_state = VotChromiumTransportState::Uninitialized;
    QString m_error;
    QHash<QString, VotChromiumRequestSession *> m_activeRequests;
};
