#pragma once

#include <QString>

enum class ProxyMode {
    System,
    NoProxy,
    Manual,
};

enum class ManualProxyType {
    Http,
    Socks5,
};

class ProxySettings {
public:
    static ProxySettings defaults();

    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;

    [[nodiscard]] ProxyMode mode() const;
    void setMode(ProxyMode mode);
    [[nodiscard]] ManualProxyType manualType() const;
    void setManualType(ManualProxyType type);
    [[nodiscard]] QString host() const;
    void setHost(const QString &host);
    [[nodiscard]] quint16 port() const;
    void setPort(quint16 port);
    [[nodiscard]] QString username() const;
    void setUsername(const QString &username);

    friend bool operator==(const ProxySettings &, const ProxySettings &) = default;

private:
    ProxyMode m_mode = ProxyMode::System;
    ManualProxyType m_manualType = ManualProxyType::Http;
    QString m_host;
    quint16 m_port = 8080;
    QString m_username;
};

[[nodiscard]] bool applyProxySettings(
    const ProxySettings &settings,
    QString *error = nullptr
);
[[nodiscard]] QString proxyModeDisplayName(ProxyMode mode);
[[nodiscard]] QString manualProxyTypeDisplayName(ManualProxyType type);
[[nodiscard]] bool manualProxyAuthenticationSupported(ManualProxyType type);
[[nodiscard]] QString proxyConfigurationDisplayName(const ProxySettings &settings);
[[nodiscard]] bool hasSameEffectiveProxyConfiguration(
    const ProxySettings &left,
    const ProxySettings &right
);
