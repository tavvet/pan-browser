#include "CredentialStore.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QUrl>

namespace {

constexpr qsizetype maximumRealmBytes = 4096;

QString normalizedHost(const QString &host)
{
    const QByteArray ace = QUrl::toAce(host.trimmed());
    return QString::fromLatin1(ace).toLower();
}

int effectivePort(const QUrl &url)
{
    if (url.port() >= 0)
        return url.port();
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
        return 443;
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0)
        return 80;
    return -1;
}

bool realmCanBeStored(const QString &realm)
{
    return realm.toUtf8().size() <= maximumRealmBytes;
}

} // namespace

void CredentialStoreError::clear()
{
    code = CredentialStoreErrorCode::None;
    message.clear();
}

bool CredentialStoreError::shouldReport() const
{
    return code != CredentialStoreErrorCode::None
        && code != CredentialStoreErrorCode::NotFound;
}

bool CredentialTarget::isValid() const
{
    if (host.isEmpty() || port < 1 || port > 65535 || !realmCanBeStored(realm))
        return false;
    if (authentication != CredentialAuthentication::HttpRealmPassword)
        return false;
    if (kind == CredentialKind::HttpServer)
        return scheme == QStringLiteral("https");
    return scheme == QStringLiteral("proxy");
}

QString CredentialTarget::identifier() const
{
    if (!isValid())
        return {};

    QByteArray canonical;
    QDataStream stream(&canonical, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint32(1)
           << quint8(kind == CredentialKind::HttpServer ? 1 : 2)
           << quint8(authentication == CredentialAuthentication::HttpRealmPassword ? 1 : 0)
           << scheme
           << host
           << qint32(port)
           << realm;
    return QString::fromLatin1(
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex()
    );
}

std::optional<CredentialTarget> CredentialTarget::forHttpServer(
    const QUrl &url,
    const QString &realm
)
{
    CredentialTarget target;
    target.kind = CredentialKind::HttpServer;
    target.scheme = url.scheme().toLower();
    target.host = normalizedHost(url.host());
    target.port = effectivePort(url);
    target.realm = realm;
    if (!target.isValid())
        return std::nullopt;
    return target;
}

std::optional<CredentialTarget> CredentialTarget::forHttpProxy(
    const QString &host,
    int port,
    const QString &realm
)
{
    CredentialTarget target;
    target.kind = CredentialKind::HttpProxy;
    target.scheme = QStringLiteral("proxy");
    target.host = normalizedHost(host);
    target.port = port;
    target.realm = realm;
    if (!target.isValid())
        return std::nullopt;
    return target;
}
