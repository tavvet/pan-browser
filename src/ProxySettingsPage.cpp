#include "ProxySettingsPage.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

ProxySettingsPage::ProxySettingsPage(const ProxySettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_initialSettings(settings)
{
    setObjectName(QStringLiteral("proxySettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("Proxy"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Route browser traffic through the operating-system proxy or a manually configured server."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *modeLabel = new QLabel(tr("PROXY MODE"), this);
    modeLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(modeLabel);
    auto *modeCard = new QFrame(this);
    modeCard->setObjectName(QStringLiteral("settingsCard"));
    auto *modeLayout = new QFormLayout(modeCard);
    modeLayout->setContentsMargins(18, 16, 18, 16);
    modeLayout->setHorizontalSpacing(18);
    modeLayout->setVerticalSpacing(10);
    modeLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_mode = new QComboBox(modeCard);
    m_mode->addItem(tr("System proxy"), static_cast<int>(ProxyMode::System));
    m_mode->addItem(tr("No proxy"), static_cast<int>(ProxyMode::NoProxy));
    m_mode->addItem(tr("Manual proxy"), static_cast<int>(ProxyMode::Manual));
    m_mode->setCurrentIndex(m_mode->findData(static_cast<int>(settings.mode())));
    modeLayout->addRow(tr("Mode"), m_mode);
    m_modeHint = new QLabel(modeCard);
    m_modeHint->setObjectName(QStringLiteral("fieldHint"));
    m_modeHint->setWordWrap(true);
    modeLayout->addRow(m_modeHint);
    layout->addWidget(modeCard);

    auto *manualLabel = new QLabel(tr("MANUAL PROXY"), this);
    manualLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(manualLabel);
    m_manualCard = new QFrame(this);
    m_manualCard->setObjectName(QStringLiteral("settingsCard"));
    auto *manualLayout = new QFormLayout(m_manualCard);
    manualLayout->setContentsMargins(18, 16, 18, 16);
    manualLayout->setHorizontalSpacing(18);
    manualLayout->setVerticalSpacing(10);
    manualLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_type = new QComboBox(m_manualCard);
    m_type->addItem(
        tr("HTTP proxy (HTTP and HTTPS traffic)"),
        static_cast<int>(ManualProxyType::Http)
    );
    m_type->addItem(
        tr("SOCKS5 proxy (without authentication)"),
        static_cast<int>(ManualProxyType::Socks5)
    );
    m_type->setCurrentIndex(m_type->findData(static_cast<int>(settings.manualType())));
    manualLayout->addRow(tr("Type"), m_type);

    m_host = new QLineEdit(settings.host(), m_manualCard);
    m_host->setPlaceholderText(QStringLiteral("proxy.example.com"));
    manualLayout->addRow(tr("Host"), m_host);

    m_port = new QSpinBox(m_manualCard);
    m_port->setRange(1, 65535);
    m_port->setValue(settings.port() == 0 ? 8080 : settings.port());
    manualLayout->addRow(tr("Port"), m_port);

    m_username = new QLineEdit(settings.username(), m_manualCard);
    m_username->setPlaceholderText(tr("Optional"));
    manualLayout->addRow(tr("Username"), m_username);

    m_credentialsHint = new QLabel(
        QString(),
        m_manualCard
    );
    m_credentialsHint->setObjectName(QStringLiteral("fieldHint"));
    m_credentialsHint->setWordWrap(true);
    manualLayout->addRow(m_credentialsHint);
    layout->addWidget(m_manualCard);

    auto *restartHint = new QLabel(
        tr("Proxy changes take effect after restarting PanBrowser. Existing connections are not rerouted."),
        this
    );
    restartHint->setObjectName(QStringLiteral("fieldHint"));
    restartHint->setWordWrap(true);
    layout->addWidget(restartHint);
    auto *scopeHint = new QLabel(
        tr("The proxy applies to PanBrowser tabs, popups, web apps, and downloads. It does not configure other applications and is not a VPN."),
        this
    );
    scopeHint->setObjectName(QStringLiteral("fieldHint"));
    scopeHint->setWordWrap(true);
    layout->addWidget(scopeHint);
    layout->addStretch();

    connect(m_mode, &QComboBox::currentIndexChanged, this, [this](int) {
        updateModeUi();
    });
    connect(m_type, &QComboBox::currentIndexChanged, this, [this](int) {
        updateModeUi();
    });
    updateModeUi();
}

ProxySettings ProxySettingsPage::settings() const
{
    ProxySettings settings = m_initialSettings;
    settings.setMode(static_cast<ProxyMode>(m_mode->currentData().toInt()));
    settings.setManualType(static_cast<ManualProxyType>(m_type->currentData().toInt()));
    settings.setHost(m_host->text());
    settings.setPort(static_cast<quint16>(m_port->value()));
    settings.setUsername(m_username->text());
    return settings;
}

bool ProxySettingsPage::validate(QString *error) const
{
    return settings().validate(error);
}

void ProxySettingsPage::updateModeUi()
{
    const ProxyMode mode = static_cast<ProxyMode>(m_mode->currentData().toInt());
    const ManualProxyType type = static_cast<ManualProxyType>(
        m_type->currentData().toInt()
    );
    m_manualCard->setEnabled(mode == ProxyMode::Manual);
    m_username->setEnabled(
        mode == ProxyMode::Manual && manualProxyAuthenticationSupported(type)
    );
    if (manualProxyAuthenticationSupported(type)) {
        m_credentialsHint->setText(
            tr("If authentication is required, PanBrowser will request the password when the proxy is first used. Passwords are never saved in the settings file.")
        );
    } else {
        m_credentialsHint->setText(
            tr("Chromium does not support SOCKS5 authentication. Use a SOCKS5 proxy that does not require a username or password.")
        );
    }
    switch (mode) {
    case ProxyMode::System:
        m_modeHint->setText(tr("Use the proxy configuration provided by the operating system."));
        break;
    case ProxyMode::NoProxy:
        m_modeHint->setText(tr("Connect directly and ignore the operating-system proxy configuration."));
        break;
    case ProxyMode::Manual:
        m_modeHint->setText(tr("Use one proxy server for browser traffic. An unavailable proxy will not fall back to a direct connection."));
        break;
    }
}
