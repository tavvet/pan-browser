#include "ProxySettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

namespace {

constexpr qsizetype maximumSettingsFileSize = 64 * 1024;
constexpr qsizetype maximumHostLength = 253;
constexpr qsizetype maximumUsernameLength = 256;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

bool isControlCharacter(QChar character)
{
    return character.category() == QChar::Other_Control;
}

QString text(const char *source)
{
    return QCoreApplication::translate("ProxySettings", source);
}

bool restrictFilePermissions(const QString &path, QString *error)
{
#if defined(Q_OS_UNIX)
    if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("ProxySettings", "Cannot restrict permissions for %1"))
                .arg(path)
        );
    }
#else
    Q_UNUSED(path)
    Q_UNUSED(error)
#endif
    return true;
}

QString modeKey(ProxyMode mode)
{
    switch (mode) {
    case ProxyMode::System:
        return QStringLiteral("system");
    case ProxyMode::NoProxy:
        return QStringLiteral("no-proxy");
    case ProxyMode::Manual:
        return QStringLiteral("manual");
    }
    return {};
}

bool parseMode(const QString &value, ProxyMode *mode)
{
    if (value == QStringLiteral("system")) {
        *mode = ProxyMode::System;
        return true;
    }
    if (value == QStringLiteral("no-proxy")) {
        *mode = ProxyMode::NoProxy;
        return true;
    }
    if (value == QStringLiteral("manual")) {
        *mode = ProxyMode::Manual;
        return true;
    }
    return false;
}

QString manualTypeKey(ManualProxyType type)
{
    switch (type) {
    case ManualProxyType::Http:
        return QStringLiteral("http");
    case ManualProxyType::Socks5:
        return QStringLiteral("socks5");
    }
    return {};
}

bool parseManualType(const QString &value, ManualProxyType *type)
{
    if (value == QStringLiteral("http")) {
        *type = ManualProxyType::Http;
        return true;
    }
    if (value == QStringLiteral("socks5")) {
        *type = ManualProxyType::Socks5;
        return true;
    }
    return false;
}

bool validDomainName(const QString &host)
{
    QByteArray ace = QUrl::toAce(host);
    if (ace.endsWith('.'))
        ace.chop(1);
    if (ace.isEmpty() || ace.size() > maximumHostLength)
        return false;

    static const QRegularExpression labelPattern(
        QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$")
    );
    const QList<QByteArray> labels = ace.split('.');
    for (const QByteArray &label : labels) {
        if (!labelPattern.match(QString::fromLatin1(label)).hasMatch())
            return false;
    }
    return true;
}

bool validProxyHost(const QString &host)
{
    if (host.isEmpty() || host.size() > maximumHostLength)
        return false;
    for (const QChar character : host) {
        if (character.isSpace() || isControlCharacter(character))
            return false;
    }

    QHostAddress address;
    if (address.setAddress(host))
        return true;
    if (host.contains(QLatin1Char(':'))
        || host.contains(QLatin1Char('/'))
        || host.contains(QLatin1Char('\\'))
        || host.contains(QLatin1Char('@'))
        || host.contains(QLatin1Char('?'))
        || host.contains(QLatin1Char('#'))
        || host.contains(QLatin1Char('['))
        || host.contains(QLatin1Char(']'))) {
        return false;
    }
    return validDomainName(host);
}

} // namespace

ProxySettings ProxySettings::defaults()
{
    return {};
}

bool ProxySettings::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("ProxySettings", "Cannot open %1: %2"))
                .arg(path, file.errorString())
        );
    }
    if (file.size() > maximumSettingsFileSize)
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Proxy settings file is too large")));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("ProxySettings", "Invalid proxy settings JSON: %1"))
                .arg(parseError.errorString())
        );
    }

    const QJsonObject root = document.object();
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != 1.0)
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Unsupported proxy settings version")));
    if (!root.value(QStringLiteral("mode")).isString())
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Proxy mode must be a string")));
    if (!root.value(QStringLiteral("manual")).isObject())
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Manual proxy settings must be an object")));

    ProxySettings loaded = defaults();
    if (!parseMode(root.value(QStringLiteral("mode")).toString(), &loaded.m_mode))
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Unknown proxy mode")));

    const QJsonObject manual = root.value(QStringLiteral("manual")).toObject();
    if (!manual.value(QStringLiteral("type")).isString()
        || !parseManualType(
            manual.value(QStringLiteral("type")).toString(),
            &loaded.m_manualType
        )) {
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Unknown manual proxy type")));
    }
    if (!manual.value(QStringLiteral("host")).isString()
        || !manual.value(QStringLiteral("username")).isString()
        || !manual.value(QStringLiteral("port")).isDouble()) {
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Manual proxy fields have invalid types")));
    }
    const double portValue = manual.value(QStringLiteral("port")).toDouble();
    if (portValue < 0.0
        || portValue > 65535.0
        || portValue != static_cast<double>(static_cast<quint16>(portValue))) {
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Proxy port must be between 1 and 65535")));
    }
    loaded.m_host = manual.value(QStringLiteral("host")).toString().trimmed();
    loaded.m_port = static_cast<quint16>(portValue);
    loaded.m_username = manual.value(QStringLiteral("username")).toString();

    if (!loaded.validate(error))
        return false;
    *this = loaded;
    return true;
}

bool ProxySettings::save(const QString &path, QString *error) const
{
    if (!validate(error))
        return false;

    QJsonObject manual;
    manual.insert(QStringLiteral("type"), manualTypeKey(m_manualType));
    manual.insert(QStringLiteral("host"), m_host.trimmed());
    manual.insert(QStringLiteral("port"), static_cast<int>(m_port));
    manual.insert(QStringLiteral("username"), m_username);

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("mode"), modeKey(m_mode));
    root.insert(QStringLiteral("manual"), manual);
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (contents.size() > maximumSettingsFileSize)
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Proxy settings file is too large")));

    if (QFile::exists(path)) {
        const QString backupPath = path + QStringLiteral(".backup");
        if (QFile::exists(backupPath) && !QFile::remove(backupPath)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("ProxySettings", "Cannot replace backup %1"))
                    .arg(backupPath)
            );
        }
        if (!QFile::copy(path, backupPath)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("ProxySettings", "Cannot create backup %1"))
                    .arg(backupPath)
            );
        }
        if (!restrictFilePermissions(backupPath, error))
            return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("ProxySettings", "Cannot write %1: %2"))
                .arg(path, file.errorString())
        );
    }
    if (file.write(contents) != contents.size() || !file.commit()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("ProxySettings", "Cannot commit %1: %2"))
                .arg(path, file.errorString())
        );
    }
    return restrictFilePermissions(path, error);
}

bool ProxySettings::validate(QString *error) const
{
    switch (m_mode) {
    case ProxyMode::System:
    case ProxyMode::NoProxy:
    case ProxyMode::Manual:
        break;
    default:
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Unknown proxy mode")));
    }
    switch (m_manualType) {
    case ManualProxyType::Http:
    case ManualProxyType::Socks5:
        break;
    default:
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Unknown manual proxy type")));
    }

    const QString normalizedHost = m_host.trimmed();
    if (!normalizedHost.isEmpty() && !validProxyHost(normalizedHost)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("ProxySettings", "Enter a valid proxy hostname or IP address without a scheme or path"))
        );
    }
    if (m_mode == ProxyMode::Manual && normalizedHost.isEmpty())
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Proxy host is required")));
    if (m_port == 0 && m_mode == ProxyMode::Manual)
        return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Proxy port must be between 1 and 65535")));

    if (m_username.size() > maximumUsernameLength) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("ProxySettings", "Proxy username must not exceed 256 characters"))
        );
    }
    for (const QChar character : m_username) {
        if (isControlCharacter(character)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("ProxySettings", "Proxy username must not contain control characters"))
            );
        }
    }
    return true;
}

ProxyMode ProxySettings::mode() const
{
    return m_mode;
}

void ProxySettings::setMode(ProxyMode mode)
{
    m_mode = mode;
}

ManualProxyType ProxySettings::manualType() const
{
    return m_manualType;
}

void ProxySettings::setManualType(ManualProxyType type)
{
    m_manualType = type;
}

QString ProxySettings::host() const
{
    return m_host;
}

void ProxySettings::setHost(const QString &host)
{
    m_host = host.trimmed();
}

quint16 ProxySettings::port() const
{
    return m_port;
}

void ProxySettings::setPort(quint16 port)
{
    m_port = port;
}

QString ProxySettings::username() const
{
    return m_username;
}

void ProxySettings::setUsername(const QString &username)
{
    m_username = username;
}

bool applyProxySettings(const ProxySettings &settings, QString *error)
{
    if (!settings.validate(error))
        return false;

    switch (settings.mode()) {
    case ProxyMode::System:
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        return true;
    case ProxyMode::NoProxy:
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        return true;
    case ProxyMode::Manual: {
        const QNetworkProxy::ProxyType type = settings.manualType() == ManualProxyType::Http
            ? QNetworkProxy::HttpProxy
            : QNetworkProxy::Socks5Proxy;
        QNetworkProxy::setApplicationProxy(
            QNetworkProxy(type, settings.host(), settings.port())
        );
        return true;
    }
    }
    return fail(error, text(QT_TRANSLATE_NOOP("ProxySettings", "Unknown proxy mode")));
}

QString proxyModeDisplayName(ProxyMode mode)
{
    switch (mode) {
    case ProxyMode::System:
        return text(QT_TRANSLATE_NOOP("ProxySettings", "System proxy"));
    case ProxyMode::NoProxy:
        return text(QT_TRANSLATE_NOOP("ProxySettings", "No proxy"));
    case ProxyMode::Manual:
        return text(QT_TRANSLATE_NOOP("ProxySettings", "Manual proxy"));
    }
    return {};
}

QString manualProxyTypeDisplayName(ManualProxyType type)
{
    switch (type) {
    case ManualProxyType::Http:
        return text(QT_TRANSLATE_NOOP("ProxySettings", "HTTP proxy"));
    case ManualProxyType::Socks5:
        return text(QT_TRANSLATE_NOOP("ProxySettings", "SOCKS5 proxy"));
    }
    return {};
}

bool manualProxyAuthenticationSupported(ManualProxyType type)
{
    return type == ManualProxyType::Http;
}

QString proxyConfigurationDisplayName(const ProxySettings &settings)
{
    if (settings.mode() != ProxyMode::Manual)
        return proxyModeDisplayName(settings.mode());
    return manualProxyTypeDisplayName(settings.manualType());
}

bool hasSameEffectiveProxyConfiguration(
    const ProxySettings &left,
    const ProxySettings &right
)
{
    if (left.mode() != right.mode())
        return false;
    if (left.mode() != ProxyMode::Manual)
        return true;
    if (left.manualType() != right.manualType()
        || left.host().compare(right.host(), Qt::CaseInsensitive) != 0
        || left.port() != right.port()) {
        return false;
    }
    return !manualProxyAuthenticationSupported(left.manualType())
        || left.username() == right.username();
}
