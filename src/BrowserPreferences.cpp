#include "BrowserPreferences.h"

#include "PrivateData.h"
#include "UrlSanitization.h"

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

BrowserPreferences BrowserPreferences::load(const QUrl &legacyStartPage, QString *error)
{
    if (error)
        error->clear();
    QSettings settings(QString::fromLatin1(organization), QString::fromLatin1(application));
    BrowserPreferences preferences;

    const QString startPageKey = QStringLiteral("Browser/startPage");
    if (!settings.contains(startPageKey)) {
        const QUrl safeLegacyStartPage =
            UrlSanitization::httpUrlForPersistence(legacyStartPage);
        if (safeLegacyStartPage.isValid())
            preferences.m_startPage = safeLegacyStartPage;
        settings.setValue(startPageKey, preferences.m_startPage.toString());
    } else {
        const QUrl configured(settings.value(startPageKey).toString());
        const QUrl safeConfigured = UrlSanitization::httpUrlForPersistence(configured);
        if (safeConfigured.isValid()) {
            preferences.m_startPage = safeConfigured;
            if (safeConfigured != configured)
                settings.setValue(startPageKey, safeConfigured.toString(QUrl::FullyEncoded));
        }
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
    preferences.m_interfaceLanguage = loadInterfaceLanguage(settings);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        fail(error, QCoreApplication::translate(
            "BrowserPreferences",
            "Cannot read or migrate application settings"
        ));
        return BrowserPreferences();
    }
    if (!PrivateData::restrictFile(settings.fileName(), error))
        return BrowserPreferences();
    return preferences;
}

InterfaceLanguage BrowserPreferences::loadInterfaceLanguage(QString *error)
{
    if (error)
        error->clear();
    QSettings settings(QString::fromLatin1(organization), QString::fromLatin1(application));
    const InterfaceLanguage language = loadInterfaceLanguage(settings);
    if (settings.status() != QSettings::NoError) {
        fail(error, QCoreApplication::translate(
            "BrowserPreferences",
            "Cannot read application settings"
        ));
        return InterfaceLanguage::System;
    }
    if (!PrivateData::restrictFile(settings.fileName(), error))
        return InterfaceLanguage::System;
    return language;
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

    const QUrl safeStartPage = UrlSanitization::httpUrlForPersistence(m_startPage);

    QSettings settings(QString::fromLatin1(organization), QString::fromLatin1(application));
    settings.setValue(
        QStringLiteral("Browser/startPage"),
        safeStartPage.toString(QUrl::FullyEncoded)
    );
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
    return PrivateData::restrictFile(settings.fileName(), error);
}

bool BrowserPreferences::validate(QString *error) const
{
    if (!UrlSanitization::httpUrlForPersistence(m_startPage).isValid())
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
    m_startPage = UrlSanitization::httpUrlForPersistence(startPage);
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
