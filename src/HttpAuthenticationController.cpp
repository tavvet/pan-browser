#include "HttpAuthenticationController.h"

#include "CredentialPromptDialog.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QUrl>

namespace {

constexpr qsizetype maximumRememberedChallenges = 256;
constexpr qsizetype maximumDisplayedRealmLength = 300;

QString uiText(const char *source)
{
    return QCoreApplication::translate("HttpAuthenticationController", source);
}

int effectivePort(const QUrl &url)
{
    if (url.port() >= 0)
        return url.port();
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
        return 443;
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0)
        return 80;
    return -1;
}

QString displayOrigin(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty()) {
        return uiText(QT_TRANSLATE_NOOP(
            "HttpAuthenticationController",
            "Unknown site"
        ));
    }
    QUrl origin;
    origin.setScheme(url.scheme().toLower());
    origin.setHost(url.host().toLower());
    if (url.port() >= 0)
        origin.setPort(url.port());
    return origin.toDisplayString(QUrl::RemovePassword | QUrl::RemoveUserInfo);
}

QString challengeKey(const QUrl &url, const QString &realm)
{
    return QStringLiteral("%1\n%2\n%3\n%4")
        .arg(
            url.scheme().toCaseFolded(),
            url.host().toCaseFolded(),
            QString::number(effectivePort(url)),
            realm
        );
}

} // namespace

namespace HttpAuthenticationPolicy {

bool promptAllowed(
    const QUrl &requestUrl,
    const QUrl &topLevelUrl,
    bool currentTab,
    bool activeWindow
)
{
    if (!currentTab || !activeWindow
        || !requestUrl.isValid() || requestUrl.host().isEmpty()
        || !topLevelUrl.isValid() || topLevelUrl.host().isEmpty()) {
        return false;
    }
    const QString requestScheme = requestUrl.scheme().toLower();
    const QString topLevelScheme = topLevelUrl.scheme().toLower();
    if ((requestScheme != QStringLiteral("http")
         && requestScheme != QStringLiteral("https"))
        || requestScheme != topLevelScheme) {
        return false;
    }
    return requestUrl.host().compare(topLevelUrl.host(), Qt::CaseInsensitive) == 0
        && effectivePort(requestUrl) == effectivePort(topLevelUrl);
}

QString realmForDisplay(const QString &realm)
{
    QString displayed;
    displayed.reserve(qMin(realm.size(), maximumDisplayedRealmLength));
    for (const QChar character : realm.trimmed()) {
        switch (character.category()) {
        case QChar::Other_Control:
        case QChar::Other_Format:
        case QChar::Separator_Line:
        case QChar::Separator_Paragraph:
            displayed.append(QLatin1Char(' '));
            break;
        default:
            displayed.append(character);
            break;
        }
    }
    displayed = displayed.simplified();
    if (displayed.size() <= maximumDisplayedRealmLength)
        return displayed;
    return displayed.left(maximumDisplayedRealmLength - 1) + QChar(0x2026);
}

} // namespace HttpAuthenticationPolicy

HttpAuthenticationController::HttpAuthenticationController(QObject *parent)
    : QObject(parent)
{
}

void HttpAuthenticationController::requestAuthentication(
    QWidget *parent,
    const QUrl &requestUrl,
    QAuthenticator *authenticator
)
{
    if (!authenticator)
        return;
    if (m_promptActive) {
        *authenticator = QAuthenticator();
        return;
    }

    const QString origin = displayOrigin(requestUrl);
    const QString realm = authenticator->realm();
    const QString displayedRealm = HttpAuthenticationPolicy::realmForDisplay(realm);
    const QString key = challengeKey(requestUrl, realm);
    CredentialPromptContent content;
    content.objectName = QStringLiteral("httpAuthenticationDialog");
    content.title = uiText(QT_TRANSLATE_NOOP(
        "HttpAuthenticationController",
        "Website authentication"
    ));
    content.message = uiText(QT_TRANSLATE_NOOP(
        "HttpAuthenticationController",
        "The website requires a username and password."
    ));
    content.details.append(uiText(QT_TRANSLATE_NOOP(
        "HttpAuthenticationController",
        "Site: %1"
    )).arg(origin));
    if (!displayedRealm.isEmpty()) {
        content.details.append(uiText(QT_TRANSLATE_NOOP(
            "HttpAuthenticationController",
            "Realm: %1"
        )).arg(displayedRealm));
    }
    content.suggestedUsername = authenticator->user();
    content.privacyHint = uiText(QT_TRANSLATE_NOOP(
        "HttpAuthenticationController",
        "The password is used for this browser session and is never written to PanBrowser settings."
    ));
    content.retry = m_submittedChallenges.contains(key);
    content.insecureTransport = requestUrl.scheme().compare(
        QStringLiteral("http"),
        Qt::CaseInsensitive
    ) == 0;

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
    if (m_submittedChallenges.size() >= maximumRememberedChallenges
        && !m_submittedChallenges.contains(key)) {
        m_submittedChallenges.clear();
    }
    m_submittedChallenges.insert(key);
}
