#pragma once

#include "CredentialStore.h"
#include "ProxySettings.h"

#include <QObject>
#include <QSet>

#include <memory>

class QAuthenticator;
class QUrl;
class QWidget;

class ProxyAuthenticationController final : public QObject {
public:
    explicit ProxyAuthenticationController(
        const ProxySettings &activeSettings,
        QObject *parent = nullptr,
        CredentialStore *credentialStore = nullptr
    );

    void requestAuthentication(
        QWidget *parent,
        const QUrl &requestUrl,
        QAuthenticator *authenticator,
        const QString &proxyHost
    );

private:
    ProxySettings m_activeSettings;
    std::unique_ptr<CredentialStore> m_ownedCredentialStore;
    CredentialStore *m_credentialStore = nullptr;
    QSet<QString> m_promptedHosts;
    QSet<QString> m_persistedCredentialAttempts;
    QSet<QString> m_suppressedStoredChallenges;
    bool m_promptActive = false;
};
