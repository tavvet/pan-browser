#include "BrowserProfile.h"

#include <QDir>
#include <QStandardPaths>

BrowserProfile::BrowserProfile(QObject *parent)
    : QWebEngineProfile(QStringLiteral("PanBrowser"), parent)
{
    const QString applicationData = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation
    );
    const QString applicationCache = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation
    );
    const QString storagePath = QDir(applicationData).filePath(
        QStringLiteral("WebEngine/Profile")
    );
    const QString cachePath = QDir(applicationCache).filePath(
        QStringLiteral("WebEngine")
    );

    QDir().mkpath(storagePath);
    QDir().mkpath(cachePath);

    setPersistentStoragePath(storagePath);
    setCachePath(cachePath);
    setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    setPersistentCookiesPolicy(QWebEngineProfile::OnlyPersistentCookies);
}
