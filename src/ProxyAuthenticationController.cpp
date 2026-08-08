#include "ProxyAuthenticationController.h"

#include "CredentialPromptDialog.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QUrl>

namespace {

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

} // namespace

ProxyAuthenticationController::ProxyAuthenticationController(
    const ProxySettings &activeSettings,
    QObject *parent
)
    : QObject(parent)
    , m_activeSettings(activeSettings)
{
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

    const QString displayedHost = proxyHost.trimmed().isEmpty()
        ? (m_activeSettings.host().isEmpty()
            ? uiText(QT_TRANSLATE_NOOP("ProxyAuthenticationController", "Unknown proxy"))
            : m_activeSettings.host())
        : proxyHost.trimmed();
    const QString key = displayedHost.toCaseFolded();
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
    content.privacyHint = uiText(QT_TRANSLATE_NOOP(
        "ProxyAuthenticationController",
        "The password is used for this browser session and is never written to PanBrowser settings."
    ));
    content.retry = m_promptedHosts.contains(key);

    m_promptActive = true;
    CredentialPromptDialog dialog(content, parent);
    const int result = dialog.exec();
    m_promptActive = false;
    if (result != QDialog::Accepted) {
        *authenticator = QAuthenticator();
        return;
    }

    authenticator->setUser(dialog.username());
    authenticator->setPassword(dialog.password());
    m_promptedHosts.insert(key);
}
