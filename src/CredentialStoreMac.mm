#include "CredentialStore.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

#include <Security/Security.h>

namespace {

constexpr auto serviceName = "dev.panbrowser.credentials.v1";
constexpr auto accountPrefix = "credential:";
constexpr CFIndex maximumCredentialPayloadBytes = 128 * 1024;
constexpr quint32 credentialPayloadMagic = 0x50424352; // PBCR

class ScopedCf final {
public:
    explicit ScopedCf(CFTypeRef value = nullptr)
        : m_value(value)
    {
    }

    ~ScopedCf()
    {
        if (m_value)
            CFRelease(m_value);
    }

    ScopedCf(const ScopedCf &) = delete;
    ScopedCf &operator=(const ScopedCf &) = delete;

    ScopedCf(ScopedCf &&other) noexcept
        : m_value(other.m_value)
    {
        other.m_value = nullptr;
    }

    ScopedCf &operator=(ScopedCf &&other) noexcept
    {
        if (this == &other)
            return *this;
        if (m_value)
            CFRelease(m_value);
        m_value = other.m_value;
        other.m_value = nullptr;
        return *this;
    }

    [[nodiscard]] CFTypeRef get() const
    {
        return m_value;
    }

private:
    CFTypeRef m_value = nullptr;
};

CFStringRef makeString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8 *>(utf8.constData()),
        utf8.size(),
        kCFStringEncodingUTF8,
        false
    );
}

QString statusDescription(OSStatus status)
{
    ScopedCf message(SecCopyErrorMessageString(status, nullptr));
    if (!message.get())
        return QStringLiteral("Keychain error %1").arg(status);
    const auto string = static_cast<CFStringRef>(message.get());
    const CFIndex length = CFStringGetLength(string);
    QByteArray utf8(CFStringGetMaximumSizeForEncoding(
        length,
        kCFStringEncodingUTF8
    ) + 1, Qt::Uninitialized);
    if (!CFStringGetCString(string, utf8.data(), utf8.size(), kCFStringEncodingUTF8))
        return QStringLiteral("Keychain error %1").arg(status);
    return QString::fromUtf8(utf8.constData());
}

void setError(QString *error, OSStatus status)
{
    if (error)
        *error = statusDescription(status);
}

ScopedCf baseQuery(const CredentialTarget &target)
{
    auto *query = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    ScopedCf service(makeString(QString::fromLatin1(serviceName)));
    ScopedCf account(makeString(
        QString::fromLatin1(accountPrefix) + target.identifier()
    ));
    CFDictionarySetValue(query, kSecAttrService, service.get());
    CFDictionarySetValue(query, kSecAttrAccount, account.get());
    return ScopedCf(query);
}

QByteArray serialize(const StoredCredential &credential)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << credentialPayloadMagic
           << quint16(1)
           << credential.username
           << credential.password;
    return payload;
}

std::optional<StoredCredential> deserialize(const QByteArray &payload)
{
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    quint16 version = 0;
    StoredCredential credential;
    stream >> magic >> version >> credential.username >> credential.password;
    if (stream.status() != QDataStream::Ok || !stream.atEnd()
        || magic != credentialPayloadMagic || version != 1) {
        return std::nullopt;
    }
    return credential;
}

class MacCredentialStore final : public CredentialStore {
public:
    [[nodiscard]] bool isAvailable() const override
    {
        return true;
    }

    [[nodiscard]] std::optional<StoredCredential> read(
        const CredentialTarget &target,
        QString *error
    ) override
    {
        if (error)
            error->clear();
        if (!target.isValid())
            return std::nullopt;

        ScopedCf query = baseQuery(target);
        auto *dictionary = static_cast<CFMutableDictionaryRef>(
            const_cast<void *>(query.get())
        );
        CFDictionarySetValue(dictionary, kSecReturnData, kCFBooleanTrue);
        CFDictionarySetValue(dictionary, kSecMatchLimit, kSecMatchLimitOne);
        CFTypeRef result = nullptr;
        const OSStatus status = SecItemCopyMatching(dictionary, &result);
        ScopedCf ownedResult(result);
        if (status == errSecItemNotFound)
            return std::nullopt;
        if (status != errSecSuccess) {
            setError(error, status);
            return std::nullopt;
        }
        if (!result || CFGetTypeID(result) != CFDataGetTypeID()) {
            if (error)
                *error = QStringLiteral("Keychain returned invalid credential data");
            return std::nullopt;
        }
        const auto data = static_cast<CFDataRef>(result);
        if (CFDataGetLength(data) < 0
            || CFDataGetLength(data) > maximumCredentialPayloadBytes) {
            if (error)
                *error = QStringLiteral("Keychain credential data is too large");
            return std::nullopt;
        }
        QByteArray payload(
            reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
            CFDataGetLength(data)
        );
        const auto credential = deserialize(payload);
        payload.fill('\0');
        if (!credential && error)
            *error = QStringLiteral("Keychain credential data is invalid");
        return credential;
    }

    bool write(
        const CredentialTarget &target,
        const StoredCredential &credential,
        QString *error
    ) override
    {
        if (error)
            error->clear();
        if (!target.isValid())
            return false;

        QByteArray payload = serialize(credential);
        if (payload.size() > maximumCredentialPayloadBytes) {
            payload.fill('\0');
            if (error)
                *error = QStringLiteral("Credential data is too large for Keychain storage");
            return false;
        }
        ScopedCf data(CFDataCreate(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8 *>(payload.constData()),
            payload.size()
        ));
        payload.fill('\0');
        ScopedCf query = baseQuery(target);
        auto *queryDictionary = static_cast<CFMutableDictionaryRef>(
            const_cast<void *>(query.get())
        );

        const void *updateKeys[] = {kSecValueData};
        const void *updateValues[] = {data.get()};
        ScopedCf updates(CFDictionaryCreate(
            kCFAllocatorDefault,
            updateKeys,
            updateValues,
            1,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks
        ));
        OSStatus status = SecItemUpdate(
            queryDictionary,
            static_cast<CFDictionaryRef>(updates.get())
        );
        if (status == errSecItemNotFound) {
            CFDictionarySetValue(queryDictionary, kSecValueData, data.get());
            CFDictionarySetValue(
                queryDictionary,
                kSecAttrAccessible,
                kSecAttrAccessibleWhenUnlocked
            );
            ScopedCf label(makeString(
                target.kind == CredentialKind::HttpProxy
                    ? QStringLiteral("PanBrowser proxy — %1").arg(target.host)
                    : QStringLiteral("PanBrowser website — %1").arg(target.host)
            ));
            CFDictionarySetValue(queryDictionary, kSecAttrLabel, label.get());
            status = SecItemAdd(queryDictionary, nullptr);
        }
        if (status != errSecSuccess) {
            setError(error, status);
            return false;
        }
        return true;
    }

    bool remove(const CredentialTarget &target, QString *error) override
    {
        if (error)
            error->clear();
        if (!target.isValid())
            return false;
        ScopedCf query = baseQuery(target);
        const OSStatus status = SecItemDelete(
            static_cast<CFDictionaryRef>(query.get())
        );
        if (status == errSecSuccess || status == errSecItemNotFound)
            return true;
        setError(error, status);
        return false;
    }
};

} // namespace

std::unique_ptr<CredentialStore> createSystemCredentialStore()
{
    return std::make_unique<MacCredentialStore>();
}
