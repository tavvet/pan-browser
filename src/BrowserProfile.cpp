#include "BrowserProfile.h"

#include <QCoreApplication>
#include "BrowserDataCleanup.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QWebEngineCookieStore>

namespace {

QString applicationDataPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString applicationCachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString profilePath()
{
    return QDir(applicationDataPath()).filePath(QStringLiteral("WebEngine/Profile"));
}

QString webEngineCachePath()
{
    return QDir(applicationCachePath()).filePath(QStringLiteral("WebEngine"));
}

QSettings browserSettings()
{
    return QSettings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
}

bool settingsSucceeded(const QSettings &settings, QString *error)
{
    if (settings.status() == QSettings::NoError)
        return true;
    if (error)
        *error = QCoreApplication::translate(
            "BrowserProfile",
            "Cannot update the pending data reset setting"
        );
    return false;
}

} // namespace

BrowserProfile::BrowserProfile(bool persistSessionCookies, QObject *parent)
    : QWebEngineProfile(QStringLiteral("PanBrowser"), parent)
{
    const QString storagePath = profilePath();
    const QString cachePath = webEngineCachePath();

    QDir().mkpath(storagePath);
    QDir().mkpath(cachePath);

    setPersistentStoragePath(storagePath);
    setCachePath(cachePath);
    setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
    setPersistSessionCookies(persistSessionCookies);
}

bool BrowserProfile::applyPendingDataReset(QString *error)
{
    if (error)
        error->clear();
    if (!dataResetScheduled())
        return true;

    if (!removeManagedDataDirectory(profilePath(), applicationDataPath(), error))
        return false;
    if (!removeManagedDataDirectory(webEngineCachePath(), applicationCachePath(), error))
        return false;
    return cancelDataReset(error);
}

bool BrowserProfile::scheduleDataReset(QString *error)
{
    if (error)
        error->clear();
    QSettings settings = browserSettings();
    settings.setValue(QStringLiteral("Browser/resetDataOnNextLaunch"), true);
    settings.sync();
    return settingsSucceeded(settings, error);
}

bool BrowserProfile::cancelDataReset(QString *error)
{
    if (error)
        error->clear();
    QSettings settings = browserSettings();
    settings.remove(QStringLiteral("Browser/resetDataOnNextLaunch"));
    settings.sync();
    return settingsSucceeded(settings, error);
}

bool BrowserProfile::dataResetScheduled()
{
    QSettings settings = browserSettings();
    return settings.value(QStringLiteral("Browser/resetDataOnNextLaunch"), false).toBool();
}

void BrowserProfile::setPersistSessionCookies(bool persist)
{
    setPersistentCookiesPolicy(
        persist ? QWebEngineProfile::ForcePersistentCookies
                : QWebEngineProfile::OnlyPersistentCookies
    );
}

void BrowserProfile::clearAllCookies()
{
    cookieStore()->deleteAllCookies();
}
