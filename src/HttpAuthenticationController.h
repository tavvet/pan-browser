#pragma once

#include "CredentialStore.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <memory>

class QAuthenticator;
class QUrl;
class QWidget;

namespace HttpAuthenticationPolicy {

[[nodiscard]] bool promptAllowed(
    const QUrl &requestUrl,
    const QUrl &topLevelUrl,
    bool currentTab,
    bool activeWindow
);
[[nodiscard]] QString realmForDisplay(const QString &realm);

} // namespace HttpAuthenticationPolicy

class HttpAuthenticationController final : public QObject {
public:
    explicit HttpAuthenticationController(
        QObject *parent = nullptr,
        CredentialStore *credentialStore = nullptr
    );

    void requestAuthentication(
        QWidget *parent,
        const QUrl &requestUrl,
        QAuthenticator *authenticator
    );

private:
    std::shared_ptr<CredentialStore> m_ownedCredentialStore;
    CredentialStore *m_credentialStore = nullptr;
    QSet<QString> m_submittedChallenges;
    QSet<QString> m_persistedCredentialAttempts;
    QSet<QString> m_suppressedStoredChallenges;
    bool m_promptActive = false;
};
