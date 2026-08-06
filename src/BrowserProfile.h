#pragma once

#include <QWebEngineProfile>

class QUrl;

class BrowserProfile final : public QWebEngineProfile {
public:
    explicit BrowserProfile(
        bool persistSessionCookies,
        QObject *parent = nullptr,
        bool blockNetwork = false
    );

    static bool applyPendingDataReset(QString *error = nullptr);
    static bool scheduleDataReset(QString *error = nullptr);
    static bool cancelDataReset(QString *error = nullptr);
    static bool dataResetScheduled();
    static bool shouldBlockForProxyConfigurationError(const QUrl &url);

    void setPersistSessionCookies(bool persist);
    void clearAllCookies();
};
