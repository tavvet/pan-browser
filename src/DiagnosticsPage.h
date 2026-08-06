#pragma once

#include "DnsSettings.h"

#include <QWidget>

class BrowserProfile;

class DiagnosticsPage final : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsPage(
        BrowserProfile *profile,
        const DnsSettings &dnsSettings,
        QWidget *parent = nullptr
    );

private:
    [[nodiscard]] QString diagnosticReport() const;

    BrowserProfile *m_profile = nullptr;
    DnsSettings m_dnsSettings;
};
