#pragma once

#include <QWebEngineProfile>

class BrowserProfile final : public QWebEngineProfile {
public:
    explicit BrowserProfile(bool persistSessionCookies, QObject *parent = nullptr);

    void setPersistSessionCookies(bool persist);
};
