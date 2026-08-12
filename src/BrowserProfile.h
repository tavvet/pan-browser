#pragma once

#include "CrossDomainSettings.h"

#include <QWebEngineProfile>

class QUrl;
class CrossDomainRequestInterceptor;

class BrowserProfile final : public QWebEngineProfile {
    Q_OBJECT

public:
    explicit BrowserProfile(
        bool persistSessionCookies,
        QObject *parent = nullptr,
        bool blockNetwork = false,
        const CrossDomainSettings &crossDomainSettings = CrossDomainSettings::defaults(),
        const QString &crossDomainSettingsPath = QString()
    );

    static bool applyPendingDataReset(QString *error = nullptr);
    static bool scheduleDataReset(QString *error = nullptr);
    static bool cancelDataReset(QString *error = nullptr);
    static bool dataResetScheduled();
    static bool shouldBlockForProxyConfigurationError(const QUrl &url);

    void setPersistSessionCookies(bool persist);
    void clearAllCookies();
    [[nodiscard]] CrossDomainSettings crossDomainSettings() const;
    bool setCrossDomainSettings(
        const CrossDomainSettings &settings,
        QString *error = nullptr
    );
    bool resolveCrossDomainRequest(
        const QString &sourceSite,
        const QString &targetHost,
        CrossDomainRuleDecision decision,
        bool persist,
        QString *error = nullptr
    );
    void dismissCrossDomainRequest(const QString &sourceSite, const QString &targetHost);
    void dismissCrossDomainRequestSource(
        const QString &sourceSite,
        const QString &targetHost,
        const QUrl &sourceUrl,
        bool sourceUrlIsOriginOnly
    );
    [[nodiscard]] bool isCrossDomainRequestPending(
        const QString &sourceSite,
        const QString &targetHost,
        const QUrl &sourceUrl,
        bool sourceUrlIsOriginOnly
    ) const;

signals:
    void crossDomainRequestBlocked(
        const QUrl &sourceUrl,
        const QString &sourceSite,
        const QString &targetHost,
        int resourceType,
        bool sourceUrlIsOriginOnly
    );

private:
    CrossDomainRequestInterceptor *m_requestInterceptor = nullptr;
    CrossDomainSettings m_crossDomainSettings;
    QString m_crossDomainSettingsPath;
};
