#include "ProxyAuthenticationController.h"

#include "CredentialPromptDialog.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QMessageBox>
#include <QScopedValueRollback>
#include <QUrl>

namespace {

constexpr qsizetype maximumRememberedChallenges = 256;

QString uiText(const char *source)
{
    return QCoreApplication::translate("ProxyAuthenticationController", source);
}

QString displayOrigin(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty())
        return uiText(QT_TRANSLATE_NOOP("ProxyAuthenticationController", "Unknown site"));
    QUrl origin;
    origin.setScheme(url.scheme());
    origin.setHost(url.host());
    origin.setPort(url.port());
    return origin.toDisplayString(QUrl::RemovePassword | QUrl::RemoveUserInfo);
}

void rememberChallenge(QSet<QString> &challenges, const QString &key)
{
    if (challenges.size() >= maximumRememberedChallenges
        && !challenges.contains(key)) {
        challenges.clear();
    }
    challenges.insert(key);
}

bool realmPasswordPersistenceSupported(const QAuthenticator &authenticator)
{
    return !authenticator.realm().isEmpty()
        || authenticator.options().contains(QStringLiteral("realm"));
}

} // namespace

ProxyAuthenticationController::ProxyAuthenticationController(
    const ProxySettings &activeSettings,
    QObject *parent,
    CredentialStore *credentialStore
)
    : QObject(parent)
    , m_activeSettings(activeSettings)
{
    if (credentialStore) {
        m_credentialStore = credentialStore;
    } else {
        m_ownedCredentialStore = createSystemCredentialStore();
        m_credentialStore = m_ownedCredentialStore.get();
    }
}

void ProxyAuthenticationController::requestAuthentication(
    QWidget *parent,
    const QUrl &requestUrl,
    QAuthenticator *authenticator,
    const QString &proxyHost
)
{
    if (!authenticator)
        return;
    if (m_activeSettings.mode() == ProxyMode::NoProxy
        || (m_activeSettings.mode() == ProxyMode::Manual
            && !manualProxyAuthenticationSupported(m_activeSettings.manualType()))
        || m_promptActive) {
        *authenticator = QAuthenticator();
        return;
    }
    const QScopedValueRollback promptGuard(m_promptActive, true);

    const QString displayedHost = proxyHost.trimmed().isEmpty()
        ? (m_activeSettings.host().isEmpty()
            ? uiText(QT_TRANSLATE_NOOP("ProxyAuthenticationController", "Unknown proxy"))
            : m_activeSettings.host())
        : proxyHost.trimmed();
    const QString key = displayedHost.toCaseFolded();
    const bool manualCredentialsAvailable = m_activeSettings.mode() == ProxyMode::Manual
        && manualProxyAuthenticationSupported(m_activeSettings.manualType())
        && realmPasswordPersistenceSupported(*authenticator)
        && m_credentialStore
        && m_credentialStore->isAvailable();
    const auto credentialTarget = manualCredentialsAvailable
        ? CredentialTarget::forHttpProxy(
            displayedHost,
            m_activeSettings.port(),
            authenticator->realm()
        )
        : std::nullopt;
    const QString challengeIdentifier = credentialTarget
        ? credentialTarget->identifier()
        : key;
    bool savedCredentialRejected = false;
    bool savedCredentialRemoved = false;
    if (credentialTarget) {
        if (m_persistedCredentialAttempts.remove(challengeIdentifier)) {
            rememberChallenge(m_suppressedStoredChallenges, challengeIdentifier);
            savedCredentialRejected = true;
            CredentialStoreError removeError;
            savedCredentialRemoved = m_credentialStore->remove(
                *credentialTarget,
                &removeError
            );
            if (!savedCredentialRemoved && removeError.shouldReport()) {
                qWarning().noquote()
                    << "[PanBrowser credentials] Could not remove a rejected proxy credential:"
                    << removeError.message;
            }
        } else if (!m_suppressedStoredChallenges.contains(challengeIdentifier)
                   && !m_promptedHosts.contains(challengeIdentifier)) {
            CredentialStoreError readError;
            const auto stored = m_credentialStore->read(*credentialTarget, &readError);
            if (stored) {
                authenticator->setUser(stored->username);
                authenticator->setPassword(stored->password);
                rememberChallenge(m_persistedCredentialAttempts, challengeIdentifier);
                return;
            }
            if (readError.shouldReport()) {
                qWarning().noquote()
                    << "[PanBrowser credentials] Could not read a saved proxy credential:"
                    << readError.message;
            }
        }
    }
    QString suggestedUsername = authenticator->user();
    if (suggestedUsername.isEmpty()
        && m_activeSettings.mode() == ProxyMode::Manual
        && manualProxyAuthenticationSupported(m_activeSettings.manualType())) {
        suggestedUsername = m_activeSettings.username();
    }

    CredentialPromptContent content;
    content.objectName = QStringLiteral("proxyAuthenticationDialog");
    content.title = uiText(QT_TRANSLATE_NOOP(
        "ProxyAuthenticationController",
        "Proxy authentication"
    ));
    content.message = uiText(QT_TRANSLATE_NOOP(
        "ProxyAuthenticationController",
        "The proxy “%1” requires a username and password."
    )).arg(displayedHost);
    content.details.append(uiText(QT_TRANSLATE_NOOP(
        "ProxyAuthenticationController",
        "Requesting site: %1"
    )).arg(displayOrigin(requestUrl)));
    content.suggestedUsername = suggestedUsername;
    content.rememberAvailable = credentialTarget.has_value();
    content.rememberInitiallyChecked = savedCredentialRejected;
    content.privacyHint = credentialTarget
        ? uiText(QT_TRANSLATE_NOOP(
            "ProxyAuthenticationController",
            "Saved passwords are stored by the operating system, not in PanBrowser settings."
        ))
        : uiText(QT_TRANSLATE_NOOP(
            "ProxyAuthenticationController",
            "The password is used for this browser session and is never written to PanBrowser settings."
        ));
    content.retry = savedCredentialRejected
        || m_promptedHosts.contains(challengeIdentifier);
    content.savedCredentialRejected = savedCredentialRejected;
    content.savedCredentialRemoved = savedCredentialRemoved;

    CredentialPromptDialog dialog(content, parent);
    const int result = dialog.exec();
    if (result != QDialog::Accepted) {
        *authenticator = QAuthenticator();
        return;
    }

    authenticator->setUser(dialog.username());
    authenticator->setPassword(dialog.password());
    if (dialog.rememberCredential() && credentialTarget && m_credentialStore) {
        CredentialStoreError writeError;
        if (!m_credentialStore->write(
                *credentialTarget,
                StoredCredential{dialog.username(), dialog.password()},
                &writeError
            )) {
            QMessageBox::warning(
                parent,
                uiText(QT_TRANSLATE_NOOP(
                    "ProxyAuthenticationController",
                    "Password not saved"
                )),
                uiText(QT_TRANSLATE_NOOP(
                    "ProxyAuthenticationController",
                    "PanBrowser could not save the password in the system password manager."
                ))
            );
            qWarning().noquote()
                << "[PanBrowser credentials] Could not save a proxy credential:"
                << writeError.message;
        } else {
            m_suppressedStoredChallenges.remove(challengeIdentifier);
            rememberChallenge(
                m_persistedCredentialAttempts,
                challengeIdentifier
            );
        }
    }
    rememberChallenge(m_promptedHosts, challengeIdentifier);
}
