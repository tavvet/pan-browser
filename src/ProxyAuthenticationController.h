#pragma once

#include "ProxySettings.h"

#include <QObject>
#include <QSet>

class QAuthenticator;
class QUrl;
class QWidget;

class ProxyAuthenticationController final : public QObject {
public:
    explicit ProxyAuthenticationController(
        const ProxySettings &activeSettings,
        QObject *parent = nullptr
    );

    void requestAuthentication(
        QWidget *parent,
        const QUrl &requestUrl,
        QAuthenticator *authenticator,
        const QString &proxyHost
    );

private:
    ProxySettings m_activeSettings;
    QSet<QString> m_promptedHosts;
    bool m_promptActive = false;
};
