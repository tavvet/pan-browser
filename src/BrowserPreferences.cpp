#include "BrowserPreferences.h"

#include <QSettings>

namespace {

constexpr auto organization = "PanBrowser";
constexpr auto application = "PanBrowser";

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

bool isHttpUrl(const QUrl &url)
{
    return url.isValid()
        && !url.host().isEmpty()
        && (url.scheme() == QStringLiteral("http")
            || url.scheme() == QStringLiteral("https"));
}

} // namespace

BrowserPreferences BrowserPreferences::load(const QUrl &legacyStartPage)
{
    QSettings settings(QString::fromLatin1(organization), QString::fromLatin1(application));
    BrowserPreferences preferences;

    const QString startPageKey = QStringLiteral("Browser/startPage");
    if (!settings.contains(startPageKey)) {
        if (isHttpUrl(legacyStartPage))
            preferences.m_startPage = legacyStartPage;
        settings.setValue(startPageKey, preferences.m_startPage.toString());
    } else {
        const QUrl configured(settings.value(startPageKey).toString());
        if (isHttpUrl(configured))
            preferences.m_startPage = configured;
    }

    preferences.m_startupMode = settings.value(
        QStringLiteral("Browser/startupMode"),
        QStringLiteral("start-page")
    ).toString() == QStringLiteral("restore-tabs")
        ? StartupMode::RestoreTabs
        : StartupMode::StartPage;
    preferences.m_persistSessionCookies = settings.value(
        QStringLiteral("Browser/persistSessionCookies"),
        false
    ).toBool();
    settings.sync();
    return preferences;
}

bool BrowserPreferences::save(QString *error) const
{
    if (!validate(error))
        return false;

    QSettings settings(QString::fromLatin1(organization), QString::fromLatin1(application));
    settings.setValue(QStringLiteral("Browser/startPage"), m_startPage.toString());
    settings.setValue(
        QStringLiteral("Browser/startupMode"),
        m_startupMode == StartupMode::RestoreTabs
            ? QStringLiteral("restore-tabs")
            : QStringLiteral("start-page")
    );
    settings.setValue(QStringLiteral("Browser/persistSessionCookies"), m_persistSessionCookies);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return fail(error, QStringLiteral("Cannot write application settings"));
    return true;
}

bool BrowserPreferences::validate(QString *error) const
{
    if (!isHttpUrl(m_startPage))
        return fail(error, QStringLiteral("Start page must be a valid HTTP or HTTPS URL"));
    return true;
}

QUrl BrowserPreferences::startPage() const
{
    return m_startPage;
}

void BrowserPreferences::setStartPage(const QUrl &startPage)
{
    m_startPage = startPage;
}

StartupMode BrowserPreferences::startupMode() const
{
    return m_startupMode;
}

void BrowserPreferences::setStartupMode(StartupMode mode)
{
    m_startupMode = mode;
}

bool BrowserPreferences::persistSessionCookies() const
{
    return m_persistSessionCookies;
}

void BrowserPreferences::setPersistSessionCookies(bool persist)
{
    m_persistSessionCookies = persist;
}
