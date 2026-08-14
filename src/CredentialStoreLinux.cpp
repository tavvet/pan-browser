#include <libsecret/secret.h>

#include "CredentialStore.h"
#include "CredentialStorePayload.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QThread>
#include <QTimer>
#include <QTimeZone>
#include <QtConcurrentRun>

#include <atomic>
#include <limits>
#include <memory>
#include <utility>

namespace {

constexpr auto applicationAttribute = "application";
constexpr auto applicationAttributeValue = "dev.panbrowser.credentials.v1";
constexpr auto targetAttribute = "target";
constexpr auto credentialContentType = "application/vnd.panbrowser.credential";
constexpr gsize maximumCredentialPayloadBytes = 128 * 1024;
constexpr int nativeOperationTimeoutMilliseconds = 30'000;

template<typename Result>
struct NativeCallResult {
    Result value{};
    GError *error = nullptr;
    bool timedOut = false;
};

template<typename Result, typename Operation>
NativeCallResult<Result> runNativeCall(Operation operation)
{
    auto invoke = [operation = std::move(operation)](GCancellable *cancellable) mutable {
        NativeCallResult<Result> result;
        result.value = operation(cancellable, &result.error);
        return result;
    };

    QCoreApplication *application = QCoreApplication::instance();
    if (!application || QThread::currentThread() != application->thread())
        return invoke(nullptr);

    GCancellable *cancellable = g_cancellable_new();
    QFuture<NativeCallResult<Result>> future = QtConcurrent::run(
        [invoke = std::move(invoke), cancellable]() mutable {
            return invoke(cancellable);
        }
    );
    QFutureWatcher<NativeCallResult<Result>> watcher;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(
        &watcher,
        &QFutureWatcherBase::finished,
        &loop,
        &QEventLoop::quit
    );
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        if (future.isFinished())
            return;
        timedOut = true;
        g_cancellable_cancel(cancellable);
    });
    watcher.setFuture(future);
    if (!future.isFinished()) {
        timeout.start(nativeOperationTimeoutMilliseconds);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    timeout.stop();

    NativeCallResult<Result> result = future.result();
    result.timedOut = timedOut;
    g_object_unref(cancellable);
    return result;
}

const SecretSchema *credentialSchema()
{
    static const SecretSchema *schema = secret_schema_new(
        "dev.panbrowser.Credential",
        SECRET_SCHEMA_NONE,
        applicationAttribute,
        SECRET_SCHEMA_ATTRIBUTE_STRING,
        targetAttribute,
        SECRET_SCHEMA_ATTRIBUTE_STRING,
        nullptr
    );
    return schema;
}

class ScopedSecretValue final {
public:
    explicit ScopedSecretValue(SecretValue *value = nullptr)
        : m_value(value)
    {
    }

    ~ScopedSecretValue()
    {
        if (m_value)
            secret_value_unref(m_value);
    }

    ScopedSecretValue(const ScopedSecretValue &) = delete;
    ScopedSecretValue &operator=(const ScopedSecretValue &) = delete;

    [[nodiscard]] SecretValue *get() const
    {
        return m_value;
    }

private:
    SecretValue *m_value = nullptr;
};

class ScopedItemList final {
public:
    explicit ScopedItemList(GList *items = nullptr)
        : m_items(items)
    {
    }

    ~ScopedItemList()
    {
        g_list_free_full(m_items, g_object_unref);
    }

    ScopedItemList(const ScopedItemList &) = delete;
    ScopedItemList &operator=(const ScopedItemList &) = delete;

    [[nodiscard]] GList *get() const
    {
        return m_items;
    }

private:
    GList *m_items = nullptr;
};

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

CredentialStoreErrorCode codeForNativeError(const GError *error)
{
    if (!error)
        return CredentialStoreErrorCode::PlatformError;

    if (error->domain == SECRET_ERROR) {
        switch (error->code) {
        case SECRET_ERROR_IS_LOCKED:
            return CredentialStoreErrorCode::AccessDenied;
        case SECRET_ERROR_NO_SUCH_OBJECT:
            return CredentialStoreErrorCode::NotFound;
        case SECRET_ERROR_PROTOCOL:
            return CredentialStoreErrorCode::CorruptData;
        default:
            return CredentialStoreErrorCode::PlatformError;
        }
    }
    if (error->domain == G_IO_ERROR) {
        switch (error->code) {
        case G_IO_ERROR_CANCELLED:
        case G_IO_ERROR_PERMISSION_DENIED:
            return CredentialStoreErrorCode::AccessDenied;
        case G_IO_ERROR_CLOSED:
        case G_IO_ERROR_CONNECTION_CLOSED:
        case G_IO_ERROR_HOST_UNREACHABLE:
        case G_IO_ERROR_NOT_CONNECTED:
            return CredentialStoreErrorCode::Unavailable;
        default:
            break;
        }
    }
    if (error->domain == G_DBUS_ERROR) {
        switch (error->code) {
        case G_DBUS_ERROR_ACCESS_DENIED:
        case G_DBUS_ERROR_AUTH_FAILED:
            return CredentialStoreErrorCode::AccessDenied;
        case G_DBUS_ERROR_DISCONNECTED:
        case G_DBUS_ERROR_NAME_HAS_NO_OWNER:
        case G_DBUS_ERROR_NO_REPLY:
        case G_DBUS_ERROR_SERVICE_UNKNOWN:
            return CredentialStoreErrorCode::Unavailable;
        default:
            break;
        }
    }
    return CredentialStoreErrorCode::PlatformError;
}

void consumeNativeError(
    CredentialStoreError *error,
    GError *nativeError,
    const QString &fallbackMessage,
    bool timedOut = false
)
{
    if (timedOut) {
        if (nativeError)
            g_error_free(nativeError);
        setError(
            error,
            CredentialStoreErrorCode::Unavailable,
            QStringLiteral("The Secret Service operation timed out")
        );
        return;
    }
    if (!nativeError) {
        setError(error, CredentialStoreErrorCode::PlatformError, fallbackMessage);
        return;
    }
    const QString message = nativeError->message
        ? QString::fromUtf8(nativeError->message)
        : fallbackMessage;
    setError(error, codeForNativeError(nativeError), message);
    g_error_free(nativeError);
}

QByteArray targetIdentifier(const CredentialTarget &target)
{
    return target.identifier().toLatin1();
}

QString labelForTarget(const CredentialTarget &target)
{
    return target.kind == CredentialKind::HttpProxy
        ? QStringLiteral("PanBrowser proxy credential")
        : QStringLiteral("PanBrowser website credential");
}

std::optional<DecodedCredentialPayload> decodeValue(
    SecretValue *value,
    CredentialStoreError *error
)
{
    if (!value) {
        setError(
            error,
            CredentialStoreErrorCode::CorruptData,
            QStringLiteral("Secret Service returned an empty credential")
        );
        return std::nullopt;
    }

    gsize length = 0;
    const char *data = secret_value_get(value, &length);
    if (length > maximumCredentialPayloadBytes) {
        setError(
            error,
            CredentialStoreErrorCode::TooLarge,
            QStringLiteral("Secret Service credential data is too large")
        );
        return std::nullopt;
    }
    if (length > 0 && !data) {
        setError(
            error,
            CredentialStoreErrorCode::CorruptData,
            QStringLiteral("Secret Service returned invalid credential data")
        );
        return std::nullopt;
    }

    QByteArray payload(data, static_cast<qsizetype>(length));
    auto decoded = decodeCredentialPayload(payload);
    payload.fill('\0');
    if (!decoded || !decoded->target) {
        setError(
            error,
            CredentialStoreErrorCode::CorruptData,
            QStringLiteral("Secret Service credential data is invalid")
        );
        return std::nullopt;
    }
    return decoded;
}

class LinuxCredentialStore final : public CredentialStore {
public:
    [[nodiscard]] bool isAvailable() const override
    {
        if (m_available.load(std::memory_order_acquire))
            return true;

        NativeCallResult<gboolean> call = runNativeCall<gboolean>(
            [](GCancellable *cancellable, GError **nativeError) {
                ScopedItemList items(secret_password_search_sync(
                    credentialSchema(),
                    SECRET_SEARCH_NONE,
                    cancellable,
                    nativeError,
                    applicationAttribute,
                    applicationAttributeValue,
                    nullptr
                ));
                return *nativeError == nullptr;
            }
        );
        if (call.error) {
            g_error_free(call.error);
            return false;
        }
        if (!call.value)
            return false;
        m_available.store(true, std::memory_order_release);
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

        const QByteArray identifier = targetIdentifier(target);
        NativeCallResult<SecretValue *> call = runNativeCall<SecretValue *>(
            [identifier](GCancellable *cancellable, GError **nativeError) {
                return secret_password_lookup_binary_sync(
                    credentialSchema(),
                    cancellable,
                    nativeError,
                    applicationAttribute,
                    applicationAttributeValue,
                    targetAttribute,
                    identifier.constData(),
                    nullptr
                );
            }
        );
        ScopedSecretValue value(call.value);
        if (call.error) {
            m_available.store(false, std::memory_order_release);
            consumeNativeError(
                error,
                call.error,
                QStringLiteral("Could not read the Secret Service credential"),
                call.timedOut
            );
            return std::nullopt;
        }
        if (!value.get()) {
            setError(
                error,
                CredentialStoreErrorCode::NotFound,
                QStringLiteral("The Secret Service credential was not found")
            );
            return std::nullopt;
        }

        auto decoded = decodeValue(value.get(), error);
        if (!decoded || decoded->target->identifier() != target.identifier()) {
            if (decoded) {
                decoded->credential.password.fill(QChar('\0'));
                setError(
                    error,
                    CredentialStoreErrorCode::CorruptData,
                    QStringLiteral("Secret Service credential target does not match")
                );
            }
            return std::nullopt;
        }
        return std::move(decoded->credential);
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
        if (static_cast<gsize>(payload.size()) > maximumCredentialPayloadBytes) {
            payload.fill('\0');
            setError(
                error,
                CredentialStoreErrorCode::TooLarge,
                QStringLiteral("Credential data is too large for Secret Service storage")
            );
            return false;
        }

        ScopedSecretValue value(secret_value_new(
            payload.constData(),
            static_cast<gssize>(payload.size()),
            credentialContentType
        ));
        payload.fill('\0');
        if (!value.get()) {
            setError(
                error,
                CredentialStoreErrorCode::PlatformError,
                QStringLiteral("Could not allocate Secret Service credential data")
            );
            return false;
        }

        const QByteArray identifier = targetIdentifier(target);
        const QByteArray label = labelForTarget(target).toUtf8();
        NativeCallResult<gboolean> call = runNativeCall<gboolean>(
            [identifier, label, secret = value.get()](
                GCancellable *cancellable,
                GError **nativeError
            ) {
                return secret_password_store_binary_sync(
                    credentialSchema(),
                    SECRET_COLLECTION_DEFAULT,
                    label.constData(),
                    secret,
                    cancellable,
                    nativeError,
                    applicationAttribute,
                    applicationAttributeValue,
                    targetAttribute,
                    identifier.constData(),
                    nullptr
                );
            }
        );
        if (!call.value) {
            m_available.store(false, std::memory_order_release);
            consumeNativeError(
                error,
                call.error,
                QStringLiteral("Could not store the Secret Service credential"),
                call.timedOut
            );
            return false;
        }
        if (call.error)
            g_error_free(call.error);
        m_available.store(true, std::memory_order_release);
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

        enum class RemovalResult {
            Removed,
            MatchingItemsRemain,
        };

        const QByteArray identifier = targetIdentifier(target);
        NativeCallResult<RemovalResult> call = runNativeCall<RemovalResult>(
            [identifier](GCancellable *cancellable, GError **nativeError) {
                ScopedItemList items(secret_password_search_sync(
                    credentialSchema(),
                    static_cast<SecretSearchFlags>(
                        SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK
                    ),
                    cancellable,
                    nativeError,
                    applicationAttribute,
                    applicationAttributeValue,
                    targetAttribute,
                    identifier.constData(),
                    nullptr
                ));
                if (*nativeError || !items.get())
                    return RemovalResult::Removed;

                secret_password_clear_sync(
                    credentialSchema(),
                    cancellable,
                    nativeError,
                    applicationAttribute,
                    applicationAttributeValue,
                    targetAttribute,
                    identifier.constData(),
                    nullptr
                );
                if (*nativeError)
                    return RemovalResult::MatchingItemsRemain;

                ScopedItemList remaining(secret_password_search_sync(
                    credentialSchema(),
                    SECRET_SEARCH_ALL,
                    cancellable,
                    nativeError,
                    applicationAttribute,
                    applicationAttributeValue,
                    targetAttribute,
                    identifier.constData(),
                    nullptr
                ));
                return remaining.get()
                    ? RemovalResult::MatchingItemsRemain
                    : RemovalResult::Removed;
            }
        );
        if (call.error) {
            m_available.store(false, std::memory_order_release);
            consumeNativeError(
                error,
                call.error,
                QStringLiteral("Could not remove the Secret Service credential"),
                call.timedOut
            );
            return false;
        }
        if (call.value == RemovalResult::MatchingItemsRemain) {
            setError(
                error,
                CredentialStoreErrorCode::AccessDenied,
                QStringLiteral("Secret Service credential items remain locked")
            );
            return false;
        }
        m_available.store(true, std::memory_order_release);
        return true;
    }

    [[nodiscard]] QList<StoredCredentialSummary> list(
        CredentialStoreError *error
    ) override
    {
        clearError(error);
        struct ListResult {
            QList<StoredCredentialSummary> summaries;
            bool encounteredCorruptData = false;
        };

        NativeCallResult<ListResult> call = runNativeCall<ListResult>(
            [](GCancellable *cancellable, GError **nativeError) {
                ListResult result;
                ScopedItemList items(secret_password_search_sync(
                    credentialSchema(),
                    static_cast<SecretSearchFlags>(
                        SECRET_SEARCH_ALL
                        | SECRET_SEARCH_UNLOCK
                        | SECRET_SEARCH_LOAD_SECRETS
                    ),
                    cancellable,
                    nativeError,
                    applicationAttribute,
                    applicationAttributeValue,
                    nullptr
                ));
                if (*nativeError)
                    return result;

                for (GList *node = items.get(); node; node = node->next) {
                    auto *retrievable = static_cast<SecretRetrievable *>(node->data);
                    ScopedSecretValue value(secret_retrievable_retrieve_secret_sync(
                        retrievable,
                        cancellable,
                        nativeError
                    ));
                    if (*nativeError)
                        return result;

                    CredentialStoreError decodeError;
                    auto decoded = decodeValue(value.get(), &decodeError);
                    if (!decoded) {
                        result.encounteredCorruptData = true;
                        continue;
                    }
                    decoded->credential.password.fill(QChar('\0'));

                    GHashTable *attributes = secret_retrievable_get_attributes(
                        retrievable
                    );
                    const char *storedIdentifier = attributes
                        ? static_cast<const char *>(g_hash_table_lookup(
                            attributes,
                            targetAttribute
                        ))
                        : nullptr;
                    const bool identifierMatches = storedIdentifier
                        && decoded->target->identifier()
                            == QString::fromLatin1(storedIdentifier);
                    if (attributes)
                        g_hash_table_unref(attributes);
                    if (!identifierMatches) {
                        result.encounteredCorruptData = true;
                        continue;
                    }

                    StoredCredentialSummary summary;
                    summary.target = *decoded->target;
                    summary.username = decoded->credential.username;
                    const guint64 modified = secret_retrievable_get_modified(
                        retrievable
                    );
                    if (modified <= static_cast<guint64>(
                            std::numeric_limits<qint64>::max()
                        )) {
                        summary.lastModified = QDateTime::fromSecsSinceEpoch(
                            static_cast<qint64>(modified),
                            QTimeZone::UTC
                        );
                    }
                    result.summaries.append(std::move(summary));
                }
                return result;
            }
        );
        if (call.error) {
            m_available.store(false, std::memory_order_release);
            consumeNativeError(
                error,
                call.error,
                QStringLiteral("Could not list Secret Service credentials"),
                call.timedOut
            );
            return call.value.summaries;
        }
        m_available.store(true, std::memory_order_release);
        if (call.value.encounteredCorruptData) {
            setError(
                error,
                CredentialStoreErrorCode::CorruptData,
                QStringLiteral("Some Secret Service credentials contained invalid data")
            );
        }
        return call.value.summaries;
    }

private:
    mutable std::atomic_bool m_available = false;
};

} // namespace

std::unique_ptr<CredentialStore> createSystemCredentialStore()
{
    return std::make_unique<LinuxCredentialStore>();
}
