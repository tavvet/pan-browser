#pragma once

#include <QWebEngineProfile>

class BrowserProfile final : public QWebEngineProfile {
public:
    explicit BrowserProfile(QObject *parent = nullptr);
};
