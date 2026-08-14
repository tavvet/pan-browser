#include "CredentialStorePayload.h"

#include <QDataStream>
#include <QIODevice>

#include <limits>
#include <utility>

namespace {

constexpr quint32 credentialPayloadMagic = 0x50424352; // PBCR
constexpr quint16 legacyPayloadVersion = 1;
constexpr quint16 currentPayloadVersion = 2;
constexpr qsizetype maximumCredentialPayloadBytes = 128 * 1024;
constexpr quint32 nullStringMarker = std::numeric_limits<quint32>::max();

class PayloadShapeReader final {
public:
    explicit PayloadShapeReader(const QByteArray &payload)
        : m_payload(payload)
    {
    }

    [[nodiscard]] std::optional<quint8> readUint8()
    {
        if (!canRead(1))
            return std::nullopt;
        return static_cast<quint8>(m_payload.at(m_offset++));
    }

    [[nodiscard]] std::optional<quint16> readUint16()
    {
        if (!canRead(2))
            return std::nullopt;
        const auto first = static_cast<quint8>(m_payload.at(m_offset));
        const auto second = static_cast<quint8>(m_payload.at(m_offset + 1));
        m_offset += 2;
        return (static_cast<quint16>(first) << 8) | second;
    }

    [[nodiscard]] std::optional<quint32> readUint32()
    {
        if (!canRead(4))
            return std::nullopt;
        quint32 result = 0;
        for (int index = 0; index < 4; ++index) {
            result = (result << 8)
                | static_cast<quint8>(m_payload.at(m_offset + index));
        }
        m_offset += 4;
        return result;
    }

    [[nodiscard]] bool skipString()
    {
        const auto byteCount = readUint32();
        if (!byteCount)
            return false;
        if (*byteCount == nullStringMarker)
            return true;
        if ((*byteCount % sizeof(char16_t)) != 0
            || *byteCount > static_cast<quint64>(remaining())) {
            return false;
        }
        m_offset += static_cast<qsizetype>(*byteCount);
        return true;
    }

    [[nodiscard]] bool skip(qsizetype byteCount)
    {
        if (!canRead(byteCount))
            return false;
        m_offset += byteCount;
        return true;
    }

    [[nodiscard]] bool atEnd() const
    {
        return m_offset == m_payload.size();
    }

private:
    [[nodiscard]] qsizetype remaining() const
    {
        return m_payload.size() - m_offset;
    }

    [[nodiscard]] bool canRead(qsizetype byteCount) const
    {
        return byteCount >= 0 && byteCount <= remaining();
    }

    const QByteArray &m_payload;
    qsizetype m_offset = 0;
};

bool hasSafePayloadShape(const QByteArray &payload)
{
    if (payload.size() > maximumCredentialPayloadBytes)
        return false;

    PayloadShapeReader reader(payload);
    const auto magic = reader.readUint32();
    const auto version = reader.readUint16();
    if (!magic || !version || *magic != credentialPayloadMagic)
        return false;
    if (*version == legacyPayloadVersion)
        return reader.skipString() && reader.skipString() && reader.atEnd();
    if (*version != currentPayloadVersion)
        return false;

    return reader.readUint8().has_value()
        && reader.readUint8().has_value()
        && reader.skipString()
        && reader.skipString()
        && reader.skip(sizeof(qint32))
        && reader.skipString()
        && reader.skipString()
        && reader.skipString()
        && reader.atEnd();
}

std::optional<DecodedCredentialPayload> rejectPayload(
    DecodedCredentialPayload &decoded
)
{
    decoded.credential.password.fill(QChar('\0'));
    return std::nullopt;
}

} // namespace

QByteArray encodeCredentialPayload(
    const CredentialTarget &target,
    const StoredCredential &credential
)
{
    if (!target.isValid())
        return {};

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << credentialPayloadMagic
           << currentPayloadVersion
           << quint8(target.kind == CredentialKind::HttpServer ? 1 : 2)
           << quint8(target.authentication == CredentialAuthentication::HttpRealmPassword
                         ? 1
                         : 0)
           << target.scheme
           << target.host
           << qint32(target.port)
           << target.realm
           << credential.username
           << credential.password;
    if (stream.status() != QDataStream::Ok) {
        payload.fill('\0');
        return {};
    }
    return payload;
}

std::optional<DecodedCredentialPayload> decodeCredentialPayload(
    const QByteArray &payload
)
{
    if (!hasSafePayloadShape(payload))
        return std::nullopt;

    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    quint16 version = 0;
    stream >> magic >> version;
    if (stream.status() != QDataStream::Ok || magic != credentialPayloadMagic)
        return std::nullopt;

    DecodedCredentialPayload decoded;
    if (version == legacyPayloadVersion) {
        stream >> decoded.credential.username >> decoded.credential.password;
    } else if (version == currentPayloadVersion) {
        quint8 kind = 0;
        quint8 authentication = 0;
        CredentialTarget target;
        qint32 port = -1;
        stream >> kind
               >> authentication
               >> target.scheme
               >> target.host
               >> port
               >> target.realm
               >> decoded.credential.username
               >> decoded.credential.password;
        if (kind == 1)
            target.kind = CredentialKind::HttpServer;
        else if (kind == 2)
            target.kind = CredentialKind::HttpProxy;
        else
            return rejectPayload(decoded);
        if (authentication != 1)
            return rejectPayload(decoded);
        target.authentication = CredentialAuthentication::HttpRealmPassword;
        target.port = port;
        if (!target.isValid())
            return rejectPayload(decoded);
        decoded.target = std::move(target);
    } else {
        return rejectPayload(decoded);
    }

    if (stream.status() != QDataStream::Ok || !stream.atEnd())
        return rejectPayload(decoded);
    return decoded;
}
