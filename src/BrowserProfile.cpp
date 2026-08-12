#include "BrowserProfile.h"

#include "BrowserDataCleanup.h"
#include "CrossDomainRequestInterceptor.h"
#include "PrivateData.h"

#include <QCoreApplication>
#include <QDebug>
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

BrowserProfile::BrowserProfile(
    bool persistSessionCookies,
    QObject *parent,
    bool blockNetwork,
    const CrossDomainSettings &crossDomainSettings,
    const QString &crossDomainSettingsPath
)
    : QWebEngineProfile(QStringLiteral("PanBrowser"), parent)
    , m_crossDomainSettings(crossDomainSettings)
    , m_crossDomainSettingsPath(crossDomainSettingsPath)
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
    m_requestInterceptor = new CrossDomainRequestInterceptor(
        blockNetwork,
        m_crossDomainSettings,
        this
    );
    connect(
        m_requestInterceptor,
        &CrossDomainRequestInterceptor::requestBlocked,
        this,
        &BrowserProfile::crossDomainRequestBlocked
    );
    setUrlRequestInterceptor(m_requestInterceptor);
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

CrossDomainSettings BrowserProfile::crossDomainSettings() const
{
    return m_crossDomainSettings;
}

bool BrowserProfile::setCrossDomainSettings(
    const CrossDomainSettings &settings,
    QString *error
)
{
    if (error)
        error->clear();
    if (!settings.validate(error))
        return false;
    m_crossDomainSettings = settings;
    m_requestInterceptor->setSettings(settings);
    return true;
}

bool BrowserProfile::resolveCrossDomainRequest(
    const QString &sourceSite,
    const QString &targetHost,
    CrossDomainRuleDecision decision,
    bool persist,
    QString *error
)
{
    if (error)
        error->clear();
    if (!persist) {
        m_requestInterceptor->resolveForSession(sourceSite, targetHost, decision);
        return true;
    }
    if (m_crossDomainSettingsPath.isEmpty()) {
        if (error) {
            *error = QCoreApplication::translate(
                "BrowserProfile",
                "Site connection settings path is unavailable"
            );
        }
        return false;
    }

    CrossDomainSettings updated = m_crossDomainSettings;
    updated.setRule(sourceSite, targetHost, decision);
    if (!updated.save(m_crossDomainSettingsPath, error))
        return false;
    m_crossDomainSettings = updated;
    m_requestInterceptor->setSettingsResolvingPendingRequest(
        updated,
        sourceSite,
        targetHost
    );
    return true;
}

void BrowserProfile::dismissCrossDomainRequest(
    const QString &sourceSite,
    const QString &targetHost
)
{
    m_requestInterceptor->dismiss(sourceSite, targetHost);
}

void BrowserProfile::dismissCrossDomainRequestSource(
    const QString &sourceSite,
    const QString &targetHost,
    const QUrl &sourceUrl,
    bool sourceUrlIsOriginOnly
)
{
    m_requestInterceptor->dismissSource(
        sourceSite,
        targetHost,
        sourceUrl,
        sourceUrlIsOriginOnly
    );
}

bool BrowserProfile::isCrossDomainRequestPending(
    const QString &sourceSite,
    const QString &targetHost,
    const QUrl &sourceUrl,
    bool sourceUrlIsOriginOnly
) const
{
    return m_requestInterceptor->isPending(
        sourceSite,
        targetHost,
        sourceUrl,
        sourceUrlIsOriginOnly
    );
}
