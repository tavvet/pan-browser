#include "CredentialStore.h"
#include "CredentialStorePayload.h"

#include <QByteArray>
#include <QDateTime>
#include <QTimeZone>

#include <utility>

#include <Security/Security.h>

namespace {

constexpr auto serviceName = "dev.panbrowser.credentials.v1";
constexpr auto accountPrefix = "credential:";
constexpr CFIndex maximumCredentialPayloadBytes = 128 * 1024;

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

void clearError(CredentialStoreError *error)
{
    if (error)
        error->clear();
}

void setError(
    CredentialStoreError *error,
    CredentialStoreErrorCode code,
    const QString &message
)
{
    if (!error)
        return;
    error->code = code;
    error->message = message;
}

CredentialStoreErrorCode codeForStatus(OSStatus status)
{
    switch (status) {
    case errSecItemNotFound:
        return CredentialStoreErrorCode::NotFound;
    case errSecNotAvailable:
        return CredentialStoreErrorCode::Unavailable;
    case errSecAuthFailed:
    case errSecInteractionNotAllowed:
    case errSecUserCanceled:
        return CredentialStoreErrorCode::AccessDenied;
    case errSecParam:
        return CredentialStoreErrorCode::InvalidTarget;
    case errSecDecode:
        return CredentialStoreErrorCode::CorruptData;
    default:
        return CredentialStoreErrorCode::PlatformError;
    }
}

void setStatusError(CredentialStoreError *error, OSStatus status)
{
    setError(error, codeForStatus(status), statusDescription(status));
}

ScopedCf serviceQuery()
{
    auto *query = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    if (!query)
        return ScopedCf();
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    ScopedCf service(makeString(QString::fromLatin1(serviceName)));
    if (!service.get()) {
        CFRelease(query);
        return ScopedCf();
    }
    CFDictionarySetValue(query, kSecAttrService, service.get());
    return ScopedCf(query);
}

ScopedCf accountQuery(const QString &accountName)
{
    ScopedCf query = serviceQuery();
    if (!query.get())
        return ScopedCf();
    auto *dictionary = static_cast<CFMutableDictionaryRef>(
        const_cast<void *>(query.get())
    );
    ScopedCf account(makeString(accountName));
    if (!account.get())
        return ScopedCf();
    CFDictionarySetValue(dictionary, kSecAttrAccount, account.get());
    return query;
}

ScopedCf baseQuery(const CredentialTarget &target)
{
    return accountQuery(
        QString::fromLatin1(accountPrefix) + target.identifier()
    );
}

class MacCredentialStore final : public CredentialStore {
public:
    [[nodiscard]] bool isAvailable() const override
    {
        return true;
    }

    [[nodiscard]] std::optional<StoredCredential> read(
        const CredentialTarget &target,
        CredentialStoreError *error
    ) override
    {
        clearError(error);
        if (!target.isValid()) {
            setError(
                error,
                CredentialStoreErrorCode::InvalidTarget,
                QStringLiteral("The credential target is invalid")
            );
            return std::nullopt;
        }

        ScopedCf query = baseQuery(target);
        if (!query.get()) {
            setError(
                error,
                CredentialStoreErrorCode::PlatformError,
                QStringLiteral("Could not create a Keychain query")
            );
            return std::nullopt;
        }
        auto *dictionary = static_cast<CFMutableDictionaryRef>(
            const_cast<void *>(query.get())
        );
        CFDictionarySetValue(dictionary, kSecReturnData, kCFBooleanTrue);
        CFDictionarySetValue(dictionary, kSecMatchLimit, kSecMatchLimitOne);
        CFTypeRef result = nullptr;
        const OSStatus status = SecItemCopyMatching(dictionary, &result);
        ScopedCf ownedResult(result);
        if (status == errSecItemNotFound) {
            setStatusError(error, status);
            return std::nullopt;
        }
        if (status != errSecSuccess) {
            setStatusError(error, status);
            return std::nullopt;
        }
        if (!result || CFGetTypeID(result) != CFDataGetTypeID()) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Keychain returned invalid credential data")
            );
            return std::nullopt;
        }
        const auto data = static_cast<CFDataRef>(result);
        if (CFDataGetLength(data) < 0
            || CFDataGetLength(data) > maximumCredentialPayloadBytes) {
            setError(
                error,
                CredentialStoreErrorCode::TooLarge,
                QStringLiteral("Keychain credential data is too large")
            );
            return std::nullopt;
        }
        QByteArray payload(
            reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
            CFDataGetLength(data)
        );
        const auto decoded = decodeCredentialPayload(payload);
        payload.fill('\0');
        if (!decoded
            || (decoded->target
                && decoded->target->identifier() != target.identifier())) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Keychain credential data is invalid")
            );
            return std::nullopt;
        }
        return decoded->credential;
    }

    bool write(
        const CredentialTarget &target,
        const StoredCredential &credential,
        CredentialStoreError *error
    ) override
    {
        clearError(error);
        if (!target.isValid()) {
            setError(
                error,
                CredentialStoreErrorCode::InvalidTarget,
                QStringLiteral("The credential target is invalid")
            );
            return false;
        }

        QByteArray payload = encodeCredentialPayload(target, credential);
        if (payload.isEmpty()) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Could not encode the credential")
            );
            return false;
        }
        if (payload.size() > maximumCredentialPayloadBytes) {
            payload.fill('\0');
            setError(
                error,
                CredentialStoreErrorCode::TooLarge,
                QStringLiteral("Credential data is too large for Keychain storage")
            );
            return false;
        }
        ScopedCf data(CFDataCreate(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8 *>(payload.constData()),
            payload.size()
        ));
        payload.fill('\0');
        if (!data.get()) {
            setError(
                error,
                CredentialStoreErrorCode::PlatformError,
                QStringLiteral("Could not allocate Keychain credential data")
            );
            return false;
        }
        ScopedCf query = baseQuery(target);
        if (!query.get()) {
            setError(
                error,
                CredentialStoreErrorCode::PlatformError,
                QStringLiteral("Could not create a Keychain query")
            );
            return false;
        }
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
        if (!updates.get()) {
            setError(
                error,
                CredentialStoreErrorCode::PlatformError,
                QStringLiteral("Could not create Keychain update data")
            );
            return false;
        }
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
            if (label.get())
                CFDictionarySetValue(queryDictionary, kSecAttrLabel, label.get());
            status = SecItemAdd(queryDictionary, nullptr);
        }
        if (status != errSecSuccess) {
            setStatusError(error, status);
            return false;
        }
        return true;
    }

    bool remove(
        const CredentialTarget &target,
        CredentialStoreError *error
    ) override
    {
        clearError(error);
        if (!target.isValid()) {
            setError(
                error,
                CredentialStoreErrorCode::InvalidTarget,
                QStringLiteral("The credential target is invalid")
            );
            return false;
        }
        ScopedCf query = baseQuery(target);
        if (!query.get()) {
            setError(
                error,
                CredentialStoreErrorCode::PlatformError,
                QStringLiteral("Could not create a Keychain query")
            );
            return false;
        }
        const OSStatus status = SecItemDelete(
            static_cast<CFDictionaryRef>(query.get())
        );
        if (status == errSecSuccess || status == errSecItemNotFound)
            return true;
        setStatusError(error, status);
        return false;
    }

    bool removeAll(CredentialStoreError *error) override
    {
        clearError(error);
        ScopedCf query = serviceQuery();
        if (!query.get()) {
            setError(
                error,
                CredentialStoreErrorCode::PlatformError,
                QStringLiteral("Could not create a Keychain query")
            );
            return false;
        }
        const OSStatus status = SecItemDelete(
            static_cast<CFDictionaryRef>(query.get())
        );
        if (status == errSecSuccess || status == errSecItemNotFound)
            return true;
        setStatusError(error, status);
        return false;
    }

    [[nodiscard]] QList<StoredCredentialSummary> list(
        CredentialStoreError *error
    ) override
    {
        clearError(error);
        QList<StoredCredentialSummary> summaries;
        ScopedCf query = serviceQuery();
        if (!query.get()) {
            setError(
                error,
                CredentialStoreErrorCode::PlatformError,
                QStringLiteral("Could not create a Keychain query")
            );
            return summaries;
        }
        auto *dictionary = static_cast<CFMutableDictionaryRef>(
            const_cast<void *>(query.get())
        );
        CFDictionarySetValue(dictionary, kSecReturnAttributes, kCFBooleanTrue);
        CFDictionarySetValue(dictionary, kSecMatchLimit, kSecMatchLimitAll);

        CFTypeRef result = nullptr;
        const OSStatus status = SecItemCopyMatching(dictionary, &result);
        ScopedCf ownedResult(result);
        if (status == errSecItemNotFound)
            return summaries;
        if (status != errSecSuccess) {
            setStatusError(error, status);
            return summaries;
        }
        if (!result || CFGetTypeID(result) != CFArrayGetTypeID()) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Keychain returned an invalid credential list")
            );
            return summaries;
        }

        const auto items = static_cast<CFArrayRef>(result);
        const CFIndex count = CFArrayGetCount(items);
        bool encounteredCorruptData = false;
        for (CFIndex index = 0; index < count; ++index) {
            const CFTypeRef value = CFArrayGetValueAtIndex(items, index);
            if (!value || CFGetTypeID(value) != CFDictionaryGetTypeID()) {
                encounteredCorruptData = true;
                continue;
            }
            const auto item = static_cast<CFDictionaryRef>(value);
            const CFTypeRef accountValue = CFDictionaryGetValue(
                item,
                kSecAttrAccount
            );
            if (!accountValue || CFGetTypeID(accountValue) != CFStringGetTypeID()) {
                encounteredCorruptData = true;
                continue;
            }
            const auto accountString = static_cast<CFStringRef>(accountValue);
            const CFIndex accountLength = CFStringGetLength(accountString);
            QByteArray accountUtf8(CFStringGetMaximumSizeForEncoding(
                accountLength,
                kCFStringEncodingUTF8
            ) + 1, Qt::Uninitialized);
            if (!CFStringGetCString(
                    accountString,
                    accountUtf8.data(),
                    accountUtf8.size(),
                    kCFStringEncodingUTF8
                )) {
                encounteredCorruptData = true;
                continue;
            }
            const QString account = QString::fromUtf8(accountUtf8.constData());
            if (!account.startsWith(QString::fromLatin1(accountPrefix))) {
                encounteredCorruptData = true;
                continue;
            }

            ScopedCf dataQuery = accountQuery(account);
            if (!dataQuery.get()) {
                if (!error || !error->shouldReport()) {
                    setError(
                        error,
                        CredentialStoreErrorCode::PlatformError,
                        QStringLiteral("Could not create a Keychain query")
                    );
                }
                continue;
            }
            auto *dataDictionary = static_cast<CFMutableDictionaryRef>(
                const_cast<void *>(dataQuery.get())
            );
            CFDictionarySetValue(dataDictionary, kSecReturnData, kCFBooleanTrue);
            CFDictionarySetValue(dataDictionary, kSecMatchLimit, kSecMatchLimitOne);
            CFTypeRef dataResult = nullptr;
            const OSStatus dataStatus = SecItemCopyMatching(
                dataDictionary,
                &dataResult
            );
            ScopedCf ownedData(dataResult);
            if (dataStatus != errSecSuccess) {
                if (!error || !error->shouldReport())
                    setStatusError(error, dataStatus);
                continue;
            }
            if (!dataResult || CFGetTypeID(dataResult) != CFDataGetTypeID()) {
                encounteredCorruptData = true;
                continue;
            }
            const auto data = static_cast<CFDataRef>(dataResult);
            const CFIndex length = CFDataGetLength(data);
            if (length < 0 || length > maximumCredentialPayloadBytes) {
                encounteredCorruptData = true;
                continue;
            }
            QByteArray payload(
                reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
                length
            );
            auto decoded = decodeCredentialPayload(payload);
            payload.fill('\0');
            if (!decoded) {
                encounteredCorruptData = true;
                continue;
            }
            decoded->credential.password.fill(QChar('\0'));
            if (!decoded->target) {
                encounteredCorruptData = true;
                continue;
            }
            const QString expectedAccount = QString::fromLatin1(accountPrefix)
                + decoded->target->identifier();
            if (account != expectedAccount) {
                encounteredCorruptData = true;
                continue;
            }

            StoredCredentialSummary summary;
            summary.target = *decoded->target;
            summary.username = decoded->credential.username;
            const CFTypeRef modificationValue = CFDictionaryGetValue(
                item,
                kSecAttrModificationDate
            );
            if (modificationValue
                && CFGetTypeID(modificationValue) == CFDateGetTypeID()) {
                const auto modificationDate = static_cast<CFDateRef>(
                    modificationValue
                );
                const qint64 unixMilliseconds = static_cast<qint64>(
                    (CFDateGetAbsoluteTime(modificationDate)
                     + kCFAbsoluteTimeIntervalSince1970) * 1000.0
                );
                summary.lastModified = QDateTime::fromMSecsSinceEpoch(
                    unixMilliseconds,
                    QTimeZone::UTC
                );
            }
            summaries.append(std::move(summary));
        }
        if (encounteredCorruptData && (!error || !error->shouldReport())) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Some Keychain credentials contained invalid data")
            );
        }
        return summaries;
    }
};

} // namespace

std::shared_ptr<CredentialStore> createSystemCredentialStore()
{
    return std::make_shared<MacCredentialStore>();
}
