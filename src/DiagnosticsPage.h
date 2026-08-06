#pragma once

#include "DnsSettings.h"
#include "ProxySettings.h"

#include <QWidget>

class BrowserProfile;

class DiagnosticsPage final : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsPage(
        BrowserProfile *profile,
        const DnsSettings &dnsSettings,
        const ProxySettings &activeProxySettings,
        const ProxySettings &configuredProxySettings,
        bool networkBlockedByProxyError,
        QWidget *parent = nullptr
    );

private:
    [[nodiscard]] QString diagnosticReport() const;

    BrowserProfile *m_profile = nullptr;
    DnsSettings m_dnsSettings;
    ProxySettings m_activeProxySettings;
    ProxySettings m_configuredProxySettings;
    bool m_networkBlockedByProxyError = false;
};
