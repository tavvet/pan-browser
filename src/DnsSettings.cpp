#include "DnsSettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QWebEngineGlobalSettings>

namespace {

constexpr qsizetype maximumProviderNameLength = 80;
constexpr qsizetype maximumTemplateLength = 2048;
constexpr qsizetype maximumTemplatesPerProvider = 4;
constexpr qsizetype maximumSettingsFileSize = 256 * 1024;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString text(const char *source)
{
    return QCoreApplication::translate("DnsSettings", source);
}

bool restrictFilePermissions(const QString &path, QString *error)
{
#if defined(Q_OS_UNIX)
    if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("DnsSettings", "Cannot restrict permissions for %1"))
                .arg(path)
        );
    }
#else
    Q_UNUSED(path)
    Q_UNUSED(error)
#endif
    return true;
}

QList<DnsProvider> builtInProviders()
{
    return {
        {
            QStringLiteral("builtin-adguard"),
            QStringLiteral("AdGuard DNS"),
            text(QT_TRANSLATE_NOOP("DnsSettings", "Blocks ads and trackers.")),
            {QStringLiteral("https://dns.adguard-dns.com/dns-query{?dns}")},
            true,
        },
        {
            QStringLiteral("builtin-adguard-unfiltered"),
            QStringLiteral("AdGuard DNS Unfiltered"),
            text(QT_TRANSLATE_NOOP("DnsSettings", "Does not apply content filtering.")),
            {QStringLiteral("https://unfiltered.adguard-dns.com/dns-query{?dns}")},
            true,
        },
        {
            QStringLiteral("builtin-adguard-family"),
            QStringLiteral("AdGuard DNS Family"),
            text(QT_TRANSLATE_NOOP("DnsSettings", "Blocks ads, trackers, adult content, and enables Safe Search where possible.")),
            {QStringLiteral("https://family.adguard-dns.com/dns-query{?dns}")},
            true,
        },
        {
            QStringLiteral("builtin-cloudflare"),
            QStringLiteral("Cloudflare"),
            text(QT_TRANSLATE_NOOP("DnsSettings", "Public DNS-over-HTTPS resolver by Cloudflare.")),
            {QStringLiteral("https://cloudflare-dns.com/dns-query{?dns}")},
            true,
        },
        {
            QStringLiteral("builtin-quad9"),
            QStringLiteral("Quad9"),
            text(QT_TRANSLATE_NOOP("DnsSettings", "Security-focused public DNS-over-HTTPS resolver.")),
            {QStringLiteral("https://dns.quad9.net/dns-query{?dns}")},
            true,
        },
        {
            QStringLiteral("builtin-google"),
            QStringLiteral("Google Public DNS"),
            text(QT_TRANSLATE_NOOP("DnsSettings", "Public DNS-over-HTTPS resolver by Google.")),
            {QStringLiteral("https://dns.google/dns-query{?dns}")},
            true,
        },
    };
}

QString modeKey(DnsResolutionMode mode)
{
    switch (mode) {
    case DnsResolutionMode::System:
        return QStringLiteral("system");
    case DnsResolutionMode::SecureWithFallback:
        return QStringLiteral("secure-with-fallback");
    case DnsResolutionMode::SecureOnly:
        return QStringLiteral("secure-only");
    }
    return QString();
}

bool parseMode(const QString &value, DnsResolutionMode *mode)
{
    if (value == QStringLiteral("system")) {
        *mode = DnsResolutionMode::System;
        return true;
    }
    if (value == QStringLiteral("secure-with-fallback")) {
        *mode = DnsResolutionMode::SecureWithFallback;
        return true;
    }
    if (value == QStringLiteral("secure-only")) {
        *mode = DnsResolutionMode::SecureOnly;
        return true;
    }
    return false;
}

bool validateServerTemplate(const QString &serverTemplate, QString *error)
{
    const QString value = serverTemplate.trimmed();
    if (value.isEmpty() || value.size() > maximumTemplateLength)
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS server template must contain between 1 and 2048 characters")));
    if (value.count(QStringLiteral("{?dns}")) > 1)
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS server template may contain {?dns} at most once")));

    QString urlText = value;
    urlText.replace(QStringLiteral("{?dns}"), QString());
    if (urlText.contains(QLatin1Char('{')) || urlText.contains(QLatin1Char('}')))
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS server template contains an unsupported URI variable")));

    const QUrl url(urlText, QUrl::StrictMode);
    if (!url.isValid()
        || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0
        || url.host().isEmpty()) {
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS server template must be a valid HTTPS URL")));
    }
    if (!url.userName().isEmpty() || !url.password().isEmpty())
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS server template must not contain credentials")));
    if (url.hasFragment())
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS server template must not contain a fragment")));
    return true;
}

} // namespace

DnsSettings DnsSettings::defaults()
{
    DnsSettings settings;
    settings.m_providers = builtInProviders();
    return settings;
}

bool DnsSettings::load(const QString &path, QString *error)
{
    if (!restrictFilePermissions(path, error))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Cannot open %1: %2")).arg(path, file.errorString()));
    if (file.size() > maximumSettingsFileSize)
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS settings file is too large")));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Invalid DNS settings JSON: %1")).arg(parseError.errorString()));

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1)
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Unsupported DNS settings version")));
    if (!root.value(QStringLiteral("customProviders")).isArray())
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Custom DNS providers must be an array")));

    DnsSettings loaded = defaults();
    if (!parseMode(root.value(QStringLiteral("mode")).toString(), &loaded.m_mode))
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Unknown DNS resolution mode")));
    loaded.m_selectedProviderId = root.value(QStringLiteral("selectedProvider")).toString().trimmed();

    for (const QJsonValue &value : root.value(QStringLiteral("customProviders")).toArray()) {
        if (!value.isObject())
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Every custom DNS provider must be an object")));
        const QJsonObject object = value.toObject();
        if (!object.value(QStringLiteral("serverTemplates")).isArray())
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS provider serverTemplates must be an array")));

        DnsProvider provider;
        provider.id = object.value(QStringLiteral("id")).toString().trimmed();
        provider.name = object.value(QStringLiteral("name")).toString().trimmed();
        provider.description = object.value(QStringLiteral("description")).toString().trimmed();
        for (const QJsonValue &templateValue : object.value(QStringLiteral("serverTemplates")).toArray()) {
            if (!templateValue.isString())
                return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Every DNS server template must be a string")));
            provider.serverTemplates.append(templateValue.toString().trimmed());
        }
        loaded.m_providers.append(provider);
    }

    if (!loaded.validate(error))
        return false;
    *this = loaded;
    return true;
}

bool DnsSettings::save(const QString &path, QString *error) const
{
    if (!validate(error))
        return false;

    QJsonArray customProviders;
    for (const DnsProvider &provider : m_providers) {
        if (provider.builtIn)
            continue;
        QJsonObject object;
        object.insert(QStringLiteral("id"), provider.id.trimmed());
        object.insert(QStringLiteral("name"), provider.name.trimmed());
        if (!provider.description.trimmed().isEmpty())
            object.insert(QStringLiteral("description"), provider.description.trimmed());
        QJsonArray templates;
        for (const QString &serverTemplate : provider.serverTemplates)
            templates.append(serverTemplate.trimmed());
        object.insert(QStringLiteral("serverTemplates"), templates);
        customProviders.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("mode"), modeKey(m_mode));
    root.insert(QStringLiteral("selectedProvider"), m_selectedProviderId);
    root.insert(QStringLiteral("customProviders"), customProviders);
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (contents.size() > maximumSettingsFileSize)
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS settings file is too large")));

    if (QFile::exists(path)) {
        const QString backupPath = path + QStringLiteral(".backup");
        if (QFile::exists(backupPath) && !QFile::remove(backupPath))
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Cannot replace backup %1")).arg(backupPath));
        if (!QFile::copy(path, backupPath))
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Cannot create backup %1")).arg(backupPath));
        if (!restrictFilePermissions(backupPath, error))
            return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Cannot write %1: %2")).arg(path, file.errorString()));
    if (file.write(contents) != contents.size() || !file.commit())
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Cannot commit %1: %2")).arg(path, file.errorString()));
    return restrictFilePermissions(path, error);
}

bool DnsSettings::validate(QString *error) const
{
    if (m_providers.isEmpty())
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "At least one DNS provider is required")));

    static const QRegularExpression idPattern(QStringLiteral("^[a-z0-9][a-z0-9._-]{0,127}$"));
    QSet<QString> ids;
    QSet<QString> names;
    for (const DnsProvider &provider : m_providers) {
        const QString id = provider.id.trimmed();
        const QString name = provider.name.trimmed();
        if (!idPattern.match(id).hasMatch())
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS provider has an invalid id")));
        if (ids.contains(id))
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Duplicate DNS provider id: %1")).arg(id));
        ids.insert(id);
        if (!provider.builtIn && id.startsWith(QStringLiteral("builtin-")))
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Custom DNS provider id must not use the built-in prefix")));
        if (name.isEmpty() || name.size() > maximumProviderNameLength)
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS provider name must contain between 1 and 80 characters")));
        const QString foldedName = name.toCaseFolded();
        if (names.contains(foldedName))
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Duplicate DNS provider name: %1")).arg(name));
        names.insert(foldedName);
        if (provider.serverTemplates.isEmpty()
            || provider.serverTemplates.size() > maximumTemplatesPerProvider) {
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "DNS provider must contain between 1 and 4 server templates")));
        }
        QSet<QString> templates;
        for (const QString &serverTemplate : provider.serverTemplates) {
            QString templateError;
            if (!validateServerTemplate(serverTemplate, &templateError))
                return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "%1: %2")).arg(name, templateError));
            const QString normalized = serverTemplate.trimmed();
            if (templates.contains(normalized))
                return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "%1 contains a duplicate server template")).arg(name));
            templates.insert(normalized);
        }
    }

    if (!providerById(m_selectedProviderId))
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Choose an available DNS provider")));
    return true;
}

DnsResolutionMode DnsSettings::mode() const
{
    return m_mode;
}

void DnsSettings::setMode(DnsResolutionMode mode)
{
    m_mode = mode;
}

QString DnsSettings::selectedProviderId() const
{
    return m_selectedProviderId;
}

void DnsSettings::setSelectedProviderId(const QString &id)
{
    m_selectedProviderId = id;
}

const QList<DnsProvider> &DnsSettings::providers() const
{
    return m_providers;
}

QList<DnsProvider> &DnsSettings::providers()
{
    return m_providers;
}

const DnsProvider *DnsSettings::providerById(const QString &id) const
{
    for (const DnsProvider &provider : m_providers) {
        if (provider.id == id)
            return &provider;
    }
    return nullptr;
}

bool applyDnsSettings(const DnsSettings &settings, QString *error)
{
    if (!settings.validate(error))
        return false;

    QWebEngineGlobalSettings::DnsMode dnsMode;
    switch (settings.mode()) {
    case DnsResolutionMode::System:
        dnsMode.secureMode = QWebEngineGlobalSettings::SecureDnsMode::SystemOnly;
        break;
    case DnsResolutionMode::SecureWithFallback:
        dnsMode.secureMode = QWebEngineGlobalSettings::SecureDnsMode::SecureWithFallback;
        break;
    case DnsResolutionMode::SecureOnly:
        dnsMode.secureMode = QWebEngineGlobalSettings::SecureDnsMode::SecureOnly;
        break;
    }
    if (settings.mode() != DnsResolutionMode::System) {
        const DnsProvider *provider = settings.providerById(settings.selectedProviderId());
        if (!provider)
            return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "The selected DNS provider is unavailable")));
        dnsMode.serverTemplates = provider->serverTemplates;
    }

    if (!QWebEngineGlobalSettings::setDnsMode(dnsMode))
        return fail(error, text(QT_TRANSLATE_NOOP("DnsSettings", "Qt WebEngine rejected the DNS server templates")));
    return true;
}

QString dnsModeDisplayName(DnsResolutionMode mode)
{
    switch (mode) {
    case DnsResolutionMode::System:
        return text(QT_TRANSLATE_NOOP("DnsSettings", "System DNS"));
    case DnsResolutionMode::SecureWithFallback:
        return text(QT_TRANSLATE_NOOP("DnsSettings", "Secure DNS with system fallback"));
    case DnsResolutionMode::SecureOnly:
        return text(QT_TRANSLATE_NOOP("DnsSettings", "Secure DNS only"));
    }
    return QString();
}
