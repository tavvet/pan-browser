#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class BrowserPage;

struct VotChromiumRequest final {
    QString id;
    QUrl url;
    QByteArray method;
    QByteArray body;
    QJsonObject headers;
    QString redirectMode;
    int timeoutMilliseconds = 30'000;
    QPointer<BrowserPage> sourcePage;
};
