#include "BrowserProfile.h"

#include "BrowserDataCleanup.h"
#include "PrivateData.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QWebEngineCookieStore>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>

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

class FailClosedNetworkInterceptor final : public QWebEngineUrlRequestInterceptor {
public:
    explicit FailClosedNetworkInterceptor(QObject *parent)
        : QWebEngineUrlRequestInterceptor(parent)
    {
    }

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        if (BrowserProfile::shouldBlockForProxyConfigurationError(info.requestUrl()))
            info.block(true);
    }
};

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

BrowserProfile::BrowserProfile(
    bool persistSessionCookies,
    QObject *parent,
    bool blockNetwork
)
    : QWebEngineProfile(QStringLiteral("PanBrowser"), parent)
{
    const QString storagePath = profilePath();
    const QString cachePath = webEngineCachePath();

    QString directoryError;
    if (!PrivateData::ensureDirectory(storagePath, &directoryError))
        qWarning().noquote() << "[PanBrowser profile]" << directoryError;
    if (!PrivateData::ensureDirectory(cachePath, &directoryError))
        qWarning().noquote() << "[PanBrowser cache]" << directoryError;

    setPersistentStoragePath(storagePath);
    setCachePath(cachePath);
    setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
    setPersistSessionCookies(persistSessionCookies);
    if (blockNetwork)
        setUrlRequestInterceptor(new FailClosedNetworkInterceptor(this));
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

bool BrowserProfile::shouldBlockForProxyConfigurationError(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http")
        || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("ws")
        || scheme == QStringLiteral("wss");
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
