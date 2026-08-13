#pragma once

#include <QString>

class QWebEnginePage;

class VideoElementBridge final {
public:
    static void install(
        QWebEnginePage *page,
        const QString &token,
        const QString &buttonLabel
    );
};
