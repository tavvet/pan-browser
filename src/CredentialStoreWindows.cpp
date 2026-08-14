#include "CredentialStore.h"
#include "CredentialStorePayload.h"

#include <QByteArray>
#include <QDateTime>
#include <QTimeZone>

#include <windows.h>
#include <wincred.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace {

constexpr wchar_t targetPrefix[] = L"dev.panbrowser.credentials.v1:";
constexpr quint64 windowsToUnixEpochTicks = 116444736000000000ULL;
constexpr quint64 ticksPerMillisecond = 10000ULL;

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

CredentialStoreErrorCode codeForWindowsError(DWORD errorCode)
{
    switch (errorCode) {
    case ERROR_NOT_FOUND:
        return CredentialStoreErrorCode::NotFound;
    case ERROR_NO_SUCH_LOGON_SESSION:
    case ERROR_NOT_SUPPORTED:
        return CredentialStoreErrorCode::Unavailable;
    case ERROR_ACCESS_DENIED:
    case ERROR_CANCELLED:
        return CredentialStoreErrorCode::AccessDenied;
    case ERROR_BAD_USERNAME:
    case ERROR_INVALID_FLAGS:
    case ERROR_INVALID_PARAMETER:
        return CredentialStoreErrorCode::InvalidTarget;
    case ERROR_INSUFFICIENT_BUFFER:
        return CredentialStoreErrorCode::TooLarge;
    default:
        return CredentialStoreErrorCode::PlatformError;
    }
}

QString windowsErrorDescription(DWORD errorCode)
{
    wchar_t *buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );
    if (!length || !buffer)
        return QStringLiteral("Windows Credential Manager error %1").arg(errorCode);
    const QString message = QString::fromWCharArray(
        buffer,
        static_cast<qsizetype>(length)
    ).trimmed();
    LocalFree(buffer);
    return message.isEmpty()
        ? QStringLiteral("Windows Credential Manager error %1").arg(errorCode)
        : message;
}

void setWindowsError(CredentialStoreError *error, DWORD errorCode)
{
    setError(
        error,
        codeForWindowsError(errorCode),
        windowsErrorDescription(errorCode)
    );
}

void wipeCredential(PCREDENTIALW credential)
{
    if (credential && credential->CredentialBlob
        && credential->CredentialBlobSize > 0) {
        SecureZeroMemory(
            credential->CredentialBlob,
            credential->CredentialBlobSize
        );
    }
}

class ScopedCredential final {
public:
    explicit ScopedCredential(PCREDENTIALW credential)
        : m_credential(credential)
    {
    }

    ~ScopedCredential()
    {
        wipeCredential(m_credential);
        if (m_credential)
            CredFree(m_credential);
    }

    ScopedCredential(const ScopedCredential &) = delete;
    ScopedCredential &operator=(const ScopedCredential &) = delete;

private:
    PCREDENTIALW m_credential = nullptr;
};

class ScopedCredentialArray final {
public:
    ScopedCredentialArray(PCREDENTIALW *credentials, DWORD count)
        : m_credentials(credentials)
        , m_count(count)
    {
    }

    ~ScopedCredentialArray()
    {
        if (!m_credentials)
            return;
        for (DWORD index = 0; index < m_count; ++index)
            wipeCredential(m_credentials[index]);
        CredFree(m_credentials);
    }

    ScopedCredentialArray(const ScopedCredentialArray &) = delete;
    ScopedCredentialArray &operator=(const ScopedCredentialArray &) = delete;

private:
    PCREDENTIALW *m_credentials = nullptr;
    DWORD m_count = 0;
};

std::wstring targetName(const CredentialTarget &target)
{
    return QString::fromWCharArray(targetPrefix).toStdWString()
        + target.identifier().toStdWString();
}

QDateTime dateTimeFromFileTime(const FILETIME &fileTime)
{
    ULARGE_INTEGER value{};
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;
    if (value.QuadPart < windowsToUnixEpochTicks)
        return {};
    const quint64 unixMilliseconds =
        (value.QuadPart - windowsToUnixEpochTicks) / ticksPerMillisecond;
    if (unixMilliseconds > static_cast<quint64>(std::numeric_limits<qint64>::max()))
        return {};
    return QDateTime::fromMSecsSinceEpoch(
        static_cast<qint64>(unixMilliseconds),
        QTimeZone::UTC
    );
}

class WindowsCredentialStore final : public CredentialStore {
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

        const std::wstring name = targetName(target);
        PCREDENTIALW rawCredential = nullptr;
        if (!CredReadW(name.c_str(), CRED_TYPE_GENERIC, 0, &rawCredential)) {
            setWindowsError(error, GetLastError());
            return std::nullopt;
        }
        ScopedCredential ownedCredential(rawCredential);
        if (!rawCredential || rawCredential->Type != CRED_TYPE_GENERIC
            || rawCredential->CredentialBlobSize > CRED_MAX_CREDENTIAL_BLOB_SIZE
            || (rawCredential->CredentialBlobSize > 0
                && !rawCredential->CredentialBlob)) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Windows Credential Manager returned invalid data")
            );
            return std::nullopt;
        }

        QByteArray payload(
            reinterpret_cast<const char *>(rawCredential->CredentialBlob),
            static_cast<qsizetype>(rawCredential->CredentialBlobSize)
        );
        const auto decoded = decodeCredentialPayload(payload);
        SecureZeroMemory(payload.data(), static_cast<SIZE_T>(payload.size()));
        if (!decoded || !decoded->target
            || decoded->target->identifier() != target.identifier()) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Windows credential data is invalid")
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
        if (payload.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
            SecureZeroMemory(payload.data(), static_cast<SIZE_T>(payload.size()));
            setError(
                error,
                CredentialStoreErrorCode::TooLarge,
                QStringLiteral(
                    "Credential data exceeds the Windows Credential Manager limit"
                )
            );
            return false;
        }

        std::wstring name = targetName(target);
        std::wstring username = credential.username.toStdWString();
        const QString label = QStringLiteral("PanBrowser %1 — %2")
            .arg(
                target.kind == CredentialKind::HttpProxy
                    ? QStringLiteral("proxy")
                    : QStringLiteral("website"),
                target.host
            )
            .left(CRED_MAX_STRING_LENGTH);
        std::wstring comment = label.toStdWString();

        CREDENTIALW nativeCredential{};
        nativeCredential.Type = CRED_TYPE_GENERIC;
        nativeCredential.TargetName = name.data();
        nativeCredential.Comment = comment.data();
        nativeCredential.CredentialBlobSize = static_cast<DWORD>(payload.size());
        nativeCredential.CredentialBlob = reinterpret_cast<LPBYTE>(payload.data());
        nativeCredential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        if (username.size() <= CRED_MAX_USERNAME_LENGTH)
            nativeCredential.UserName = username.data();

        const BOOL succeeded = CredWriteW(&nativeCredential, 0);
        const DWORD errorCode = succeeded ? ERROR_SUCCESS : GetLastError();
        SecureZeroMemory(payload.data(), static_cast<SIZE_T>(payload.size()));
        if (!succeeded) {
            setWindowsError(error, errorCode);
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
        const std::wstring name = targetName(target);
        if (CredDeleteW(name.c_str(), CRED_TYPE_GENERIC, 0))
            return true;
        const DWORD errorCode = GetLastError();
        if (errorCode == ERROR_NOT_FOUND)
            return true;
        setWindowsError(error, errorCode);
        return false;
    }

    [[nodiscard]] QList<StoredCredentialSummary> list(
        CredentialStoreError *error
    ) override
    {
        clearError(error);
        QList<StoredCredentialSummary> summaries;
        const std::wstring filter = std::wstring(targetPrefix) + L"*";
        DWORD count = 0;
        PCREDENTIALW *rawCredentials = nullptr;
        if (!CredEnumerateW(filter.c_str(), 0, &count, &rawCredentials)) {
            const DWORD errorCode = GetLastError();
            if (errorCode == ERROR_NOT_FOUND)
                return summaries;
            setWindowsError(error, errorCode);
            return summaries;
        }
        ScopedCredentialArray ownedCredentials(rawCredentials, count);
        bool encounteredCorruptData = false;
        for (DWORD index = 0; index < count; ++index) {
            const PCREDENTIALW nativeCredential = rawCredentials[index];
            if (!nativeCredential || nativeCredential->Type != CRED_TYPE_GENERIC
                || nativeCredential->CredentialBlobSize
                    > CRED_MAX_CREDENTIAL_BLOB_SIZE
                || (nativeCredential->CredentialBlobSize > 0
                    && !nativeCredential->CredentialBlob)) {
                encounteredCorruptData = true;
                continue;
            }
            QByteArray payload(
                reinterpret_cast<const char *>(nativeCredential->CredentialBlob),
                static_cast<qsizetype>(nativeCredential->CredentialBlobSize)
            );
            auto decoded = decodeCredentialPayload(payload);
            SecureZeroMemory(payload.data(), static_cast<SIZE_T>(payload.size()));
            if (!decoded) {
                encounteredCorruptData = true;
                continue;
            }
            decoded->credential.password.fill(QChar('\0'));
            if (!decoded->target) {
                encounteredCorruptData = true;
                continue;
            }
            const QString nativeName = nativeCredential->TargetName
                ? QString::fromWCharArray(nativeCredential->TargetName)
                : QString();
            const QString expectedName = QString::fromStdWString(
                targetName(*decoded->target)
            );
            if (nativeName.compare(expectedName, Qt::CaseInsensitive) != 0) {
                encounteredCorruptData = true;
                continue;
            }
            summaries.append(StoredCredentialSummary{
                *decoded->target,
                decoded->credential.username,
                dateTimeFromFileTime(nativeCredential->LastWritten),
            });
        }
        if (encounteredCorruptData) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Some Windows credentials contained invalid data")
            );
        }
        return summaries;
    }
};

} // namespace

std::unique_ptr<CredentialStore> createSystemCredentialStore()
{
    return std::make_unique<WindowsCredentialStore>();
}
