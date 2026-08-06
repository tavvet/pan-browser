#pragma once

#include <QList>
#include <QString>
#include <QStringList>

enum class DnsResolutionMode {
    System,
    SecureWithFallback,
    SecureOnly,
};

struct DnsProvider {
    QString id;
    QString name;
    QString description;
    QStringList serverTemplates;
    bool builtIn = false;
};

class DnsSettings {
public:
    static DnsSettings defaults();

    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;

    [[nodiscard]] DnsResolutionMode mode() const;
    void setMode(DnsResolutionMode mode);
    [[nodiscard]] QString selectedProviderId() const;
    void setSelectedProviderId(const QString &id);

    [[nodiscard]] const QList<DnsProvider> &providers() const;
    [[nodiscard]] QList<DnsProvider> &providers();
    [[nodiscard]] const DnsProvider *providerById(const QString &id) const;

private:
    QList<DnsProvider> m_providers;
    DnsResolutionMode m_mode = DnsResolutionMode::System;
    QString m_selectedProviderId = QStringLiteral("builtin-adguard");
};

[[nodiscard]] bool applyDnsSettings(const DnsSettings &settings, QString *error = nullptr);
[[nodiscard]] QString dnsModeDisplayName(DnsResolutionMode mode);
