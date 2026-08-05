#include "BrowserPreferences.h"

#include <QSettings>
#include <QCoreApplication>

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

InterfaceLanguage languageFromSetting(const QString &value)
{
    if (value == QStringLiteral("system"))
        return InterfaceLanguage::System;
    if (value == QStringLiteral("ru"))
        return InterfaceLanguage::Russian;
    return InterfaceLanguage::English;
}

QString languageSetting(InterfaceLanguage language)
{
    switch (language) {
    case InterfaceLanguage::System:
        return QStringLiteral("system");
    case InterfaceLanguage::Russian:
        return QStringLiteral("ru");
    case InterfaceLanguage::English:
        return QStringLiteral("en");
    }
    return QStringLiteral("en");
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
    preferences.m_saveBrowsingHistory = settings.value(
        QStringLiteral("Browser/saveBrowsingHistory"),
        true
    ).toBool();
    preferences.m_interfaceLanguage = loadInterfaceLanguage();
    settings.sync();
    return preferences;
}

InterfaceLanguage BrowserPreferences::loadInterfaceLanguage()
{
    QSettings settings(QString::fromLatin1(organization), QString::fromLatin1(application));
    return loadInterfaceLanguage(settings);
}

InterfaceLanguage BrowserPreferences::loadInterfaceLanguage(const QSettings &settings)
{
    const QString key = QStringLiteral("Browser/language");
    if (!settings.contains(key))
        return InterfaceLanguage::System;
    return languageFromSetting(settings.value(key).toString());
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
    settings.setValue(QStringLiteral("Browser/saveBrowsingHistory"), m_saveBrowsingHistory);
    settings.setValue(QStringLiteral("Browser/language"), languageSetting(m_interfaceLanguage));
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return fail(error, QCoreApplication::translate(
            "BrowserPreferences",
            "Cannot write application settings"
        ));
    return true;
}

bool BrowserPreferences::validate(QString *error) const
{
    if (!isHttpUrl(m_startPage))
        return fail(error, QCoreApplication::translate(
            "BrowserPreferences",
            "Start page must be a valid HTTP or HTTPS URL"
        ));
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

bool BrowserPreferences::saveBrowsingHistory() const
{
    return m_saveBrowsingHistory;
}

void BrowserPreferences::setSaveBrowsingHistory(bool save)
{
    m_saveBrowsingHistory = save;
}

InterfaceLanguage BrowserPreferences::interfaceLanguage() const
{
    return m_interfaceLanguage;
}

void BrowserPreferences::setInterfaceLanguage(InterfaceLanguage language)
{
    m_interfaceLanguage = language;
}
