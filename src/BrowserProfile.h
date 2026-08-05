#pragma once

#include <QWebEngineProfile>

class BrowserProfile final : public QWebEngineProfile {
public:
    explicit BrowserProfile(bool persistSessionCookies, QObject *parent = nullptr);

    static bool applyPendingDataReset(QString *error = nullptr);
    static bool scheduleDataReset(QString *error = nullptr);
    static bool cancelDataReset(QString *error = nullptr);
    static bool dataResetScheduled();

    void setPersistSessionCookies(bool persist);
    void clearAllCookies();
};
