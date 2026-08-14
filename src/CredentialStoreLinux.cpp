#include <libsecret/secret.h>

#include "CredentialStore.h"
#include "CredentialStorePayload.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QPointer>
#include <QPromise>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QTimeZone>

#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
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

template<typename Result>
void releaseNativeValue(Result &)
{
}

template<>
void releaseNativeValue(SecretValue *&value)
{
    if (value) {
        secret_value_unref(value);
        value = nullptr;
    }
}

template<typename Result>
class NativeCallState final {
public:
    explicit NativeCallState(std::shared_ptr<void> operationLifetime)
        : m_cancellable(g_cancellable_new())
        , m_operationLifetime(std::move(operationLifetime))
    {
    }

    ~NativeCallState()
    {
        releaseNativeValue(m_result.value);
        if (m_result.error)
            g_error_free(m_result.error);
        g_object_unref(m_cancellable);
    }

    NativeCallState(const NativeCallState &) = delete;
    NativeCallState &operator=(const NativeCallState &) = delete;

    [[nodiscard]] GCancellable *cancellable() const
    {
        return m_cancellable;
    }

    void complete(NativeCallResult<Result> result)
    {
        std::function<void()> completionHandler;
        {
            const std::lock_guard lock(m_mutex);
            m_result = std::move(result);
            m_finished = true;
            completionHandler = m_completionHandler;
        }
        if (completionHandler)
            completionHandler();
    }

    [[nodiscard]] bool isFinished() const
    {
        const std::lock_guard lock(m_mutex);
        return m_finished;
    }

    void setCompletionHandler(std::function<void()> handler)
    {
        const std::lock_guard lock(m_mutex);
        m_completionHandler = std::move(handler);
    }

    [[nodiscard]] NativeCallResult<Result> takeResult()
    {
        const std::lock_guard lock(m_mutex);
        NativeCallResult<Result> result = std::move(m_result);
        m_result.value = Result{};
        m_result.error = nullptr;
        return result;
    }

    void cancel()
    {
        g_cancellable_cancel(m_cancellable);
    }

private:
    GCancellable *m_cancellable = nullptr;
    mutable std::mutex m_mutex;
    NativeCallResult<Result> m_result;
    std::function<void()> m_completionHandler;
    // Keeps the GLib context pump and any mutation-specific resources alive
    // until the native callback arrives, including after a local timeout.
    std::shared_ptr<void> m_operationLifetime;
    bool m_finished = false;
};

template<typename Result>
using NativeCompletion = std::function<void(Result, GError *)>;

template<typename Result>
struct AsyncCompletionContext {
    NativeCompletion<Result> completion;
};

template<typename Result>
void finishAsyncContext(
    gpointer userData,
    Result value,
    GError *error
)
{
    const std::unique_ptr<AsyncCompletionContext<Result>> context(
        static_cast<AsyncCompletionContext<Result> *>(userData)
    );
    context->completion(std::move(value), error);
}

template<typename Result>
NativeCallResult<Result> timedOutResult()
{
    NativeCallResult<Result> result;
    result.timedOut = true;
    return result;
}

class GlibContextPump final : public QObject {
public:
    GlibContextPump()
        : m_context(g_main_context_new())
    {
        m_timer.setInterval(5);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
            for (int iteration = 0; iteration < 32; ++iteration) {
                if (!g_main_context_iteration(m_context, FALSE))
                    break;
            }
        });
    }

    ~GlibContextPump() override
    {
        g_main_context_unref(m_context);
    }

    [[nodiscard]] GMainContext *context() const
    {
        return m_context;
    }

    [[nodiscard]] std::shared_ptr<void> acquire()
    {
        if (m_activeOperations++ == 0)
            m_timer.start();
        return std::shared_ptr<void>(
            new char,
            [this](void *token) {
                delete static_cast<char *>(token);
                if (--m_activeOperations == 0)
                    m_timer.stop();
            }
        );
    }

private:
    GMainContext *m_context = nullptr;
    QTimer m_timer;
    int m_activeOperations = 0;
};

GlibContextPump *glibContextPump()
{
    // Process-lifetime by design: a cancelled libsecret operation may deliver
    // its completion after QCoreApplication has started shutting down.
    static auto *pump = new GlibContextPump();
    return pump;
}

struct NativeOperationLifetime {
    std::shared_ptr<void> callerLifetime;
    std::shared_ptr<void> contextLifetime;
};

class ThreadDefaultContextScope final {
public:
    explicit ThreadDefaultContextScope(GMainContext *context)
        : m_context(context)
    {
        g_main_context_push_thread_default(m_context);
    }

    ~ThreadDefaultContextScope()
    {
        g_main_context_pop_thread_default(m_context);
    }

    ThreadDefaultContextScope(const ThreadDefaultContextScope &) = delete;
    ThreadDefaultContextScope &operator=(
        const ThreadDefaultContextScope &
    ) = delete;

private:
    GMainContext *m_context = nullptr;
};

template<typename Result, typename Starter>
NativeCallResult<Result> runNativeCall(
    Starter starter,
    std::shared_ptr<void> keepAlive = {},
    QEventLoop::ProcessEventsFlags processEvents = QEventLoop::ExcludeUserInputEvents
)
{
    QCoreApplication *application = QCoreApplication::instance();
    if (!application || application->thread() != QThread::currentThread()) {
        NativeCallResult<Result> result;
        result.error = g_error_new_literal(
            G_IO_ERROR,
            G_IO_ERROR_NOT_SUPPORTED,
            "Secret Service credentials must be accessed from the application thread"
        );
        return result;
    }

    GlibContextPump *contextPump = glibContextPump();
    auto operationLifetime = std::make_shared<NativeOperationLifetime>(
        NativeOperationLifetime{
            std::move(keepAlive),
            contextPump->acquire(),
        }
    );
    auto state = std::make_shared<NativeCallState<Result>>(
        std::move(operationLifetime)
    );
    QEventLoop loop;
    const QPointer<QEventLoop> loopGuard(&loop);
    state->setCompletionHandler([loopGuard] {
        if (loopGuard) {
            QMetaObject::invokeMethod(
                loopGuard,
                &QEventLoop::quit,
                Qt::QueuedConnection
            );
        }
    });

    {
        const ThreadDefaultContextScope contextScope(contextPump->context());
        starter(
            state->cancellable(),
            [state](Result value, GError *error) {
                NativeCallResult<Result> result;
                result.value = std::move(value);
                result.error = error;
                state->complete(std::move(result));
            }
        );
    }

    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        if (state->isFinished()) {
            loop.quit();
            return;
        }
        timedOut = true;
        state->cancel();
        loop.quit();
    });
    if (!state->isFinished()) {
        timeout.start(nativeOperationTimeoutMilliseconds);
        loop.exec(processEvents);
    }
    timeout.stop();

    if (timedOut)
        return timedOutResult<Result>();
    return state->takeResult();
}

class MutationRegistry final
    : public std::enable_shared_from_this<MutationRegistry> {
public:
    [[nodiscard]] bool contains(const QString &identifier) const
    {
        const std::lock_guard lock(m_mutex);
        return m_exclusive || m_identifiers.contains(identifier);
    }

    [[nodiscard]] std::shared_ptr<void> acquire(const QString &identifier)
    {
        {
            const std::lock_guard lock(m_mutex);
            if (m_exclusive || m_identifiers.contains(identifier))
                return {};
            m_identifiers.insert(identifier);
        }

        const std::shared_ptr<MutationRegistry> registry = shared_from_this();
        return std::shared_ptr<void>(
            new char,
            [registry, identifier](void *token) {
                delete static_cast<char *>(token);
                const std::lock_guard lock(registry->m_mutex);
                registry->m_identifiers.remove(identifier);
            }
        );
    }

    [[nodiscard]] std::shared_ptr<void> acquireAll()
    {
        {
            const std::lock_guard lock(m_mutex);
            if (m_exclusive || !m_identifiers.isEmpty())
                return {};
            m_exclusive = true;
        }

        const std::shared_ptr<MutationRegistry> registry = shared_from_this();
        return std::shared_ptr<void>(
            new char,
            [registry](void *token) {
                delete static_cast<char *>(token);
                const std::lock_guard lock(registry->m_mutex);
                registry->m_exclusive = false;
            }
        );
    }

private:
    mutable std::mutex m_mutex;
    QSet<QString> m_identifiers;
    bool m_exclusive = false;
};

std::shared_ptr<MutationRegistry> mutationRegistry()
{
    // A timed-out mutation keeps its lease until its late callback arrives.
    // Rejecting overlapping access prevents stale native work from winning.
    static const auto registry = std::make_shared<MutationRegistry>();
    return registry;
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
        case G_IO_ERROR_NOT_SUPPORTED:
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

void availabilitySearchFinished(
    GObject *,
    GAsyncResult *asyncResult,
    gpointer userData
)
{
    GError *error = nullptr;
    ScopedItemList items(secret_password_search_finish(asyncResult, &error));
    finishAsyncContext<gboolean>(userData, error ? FALSE : TRUE, error);
}

void passwordLookupFinished(
    GObject *,
    GAsyncResult *asyncResult,
    gpointer userData
)
{
    GError *error = nullptr;
    SecretValue *value = secret_password_lookup_binary_finish(
        asyncResult,
        &error
    );
    finishAsyncContext<SecretValue *>(userData, value, error);
}

void passwordStoreFinished(
    GObject *,
    GAsyncResult *asyncResult,
    gpointer userData
)
{
    GError *error = nullptr;
    const gboolean stored = secret_password_store_finish(asyncResult, &error);
    finishAsyncContext<gboolean>(userData, stored, error);
}

enum class RemovalResult {
    Removed,
    MatchingItemsRemain,
};

struct RemovalContext {
    QByteArray identifier;
    bool allTargets = false;
    GCancellable *cancellable = nullptr;
    GMainContext *mainContext = nullptr;
    NativeCompletion<RemovalResult> completion;

    ~RemovalContext()
    {
        g_main_context_unref(mainContext);
        g_object_unref(cancellable);
    }
};

void completeRemoval(
    RemovalContext *context,
    RemovalResult result,
    GError *error
)
{
    NativeCompletion<RemovalResult> completion = std::move(context->completion);
    delete context;
    completion(result, error);
}

void removalVerifyFinished(
    GObject *,
    GAsyncResult *asyncResult,
    gpointer userData
)
{
    auto *context = static_cast<RemovalContext *>(userData);
    GError *error = nullptr;
    ScopedItemList remaining(secret_password_search_finish(asyncResult, &error));
    completeRemoval(
        context,
        remaining.get()
            ? RemovalResult::MatchingItemsRemain
            : RemovalResult::Removed,
        error
    );
}

void removalClearFinished(
    GObject *,
    GAsyncResult *asyncResult,
    gpointer userData
)
{
    auto *context = static_cast<RemovalContext *>(userData);
    GError *error = nullptr;
    secret_password_clear_finish(asyncResult, &error);
    if (error) {
        completeRemoval(context, RemovalResult::MatchingItemsRemain, error);
        return;
    }

    {
        const ThreadDefaultContextScope contextScope(context->mainContext);
        if (context->allTargets) {
            secret_password_search(
                credentialSchema(),
                SECRET_SEARCH_ALL,
                context->cancellable,
                removalVerifyFinished,
                context,
                applicationAttribute,
                applicationAttributeValue,
                nullptr
            );
        } else {
            secret_password_search(
                credentialSchema(),
                SECRET_SEARCH_ALL,
                context->cancellable,
                removalVerifyFinished,
                context,
                applicationAttribute,
                applicationAttributeValue,
                targetAttribute,
                context->identifier.constData(),
                nullptr
            );
        }
    }
}

void removalSearchFinished(
    GObject *,
    GAsyncResult *asyncResult,
    gpointer userData
)
{
    auto *context = static_cast<RemovalContext *>(userData);
    GError *error = nullptr;
    ScopedItemList items(secret_password_search_finish(asyncResult, &error));
    if (error || !items.get()) {
        completeRemoval(context, RemovalResult::Removed, error);
        return;
    }

    {
        const ThreadDefaultContextScope contextScope(context->mainContext);
        if (context->allTargets) {
            secret_password_clear(
                credentialSchema(),
                context->cancellable,
                removalClearFinished,
                context,
                applicationAttribute,
                applicationAttributeValue,
                nullptr
            );
        } else {
            secret_password_clear(
                credentialSchema(),
                context->cancellable,
                removalClearFinished,
                context,
                applicationAttribute,
                applicationAttributeValue,
                targetAttribute,
                context->identifier.constData(),
                nullptr
            );
        }
    }
}

void startRemoval(
    GCancellable *cancellable,
    NativeCompletion<RemovalResult> completion,
    const QByteArray &identifier,
    bool allTargets
)
{
    auto *context = new RemovalContext{
        identifier,
        allTargets,
        G_CANCELLABLE(g_object_ref(cancellable)),
        g_main_context_ref_thread_default(),
        std::move(completion),
    };
    if (allTargets) {
        secret_password_search(
            credentialSchema(),
            static_cast<SecretSearchFlags>(
                SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK
            ),
            cancellable,
            removalSearchFinished,
            context,
            applicationAttribute,
            applicationAttributeValue,
            nullptr
        );
    } else {
        secret_password_search(
            credentialSchema(),
            static_cast<SecretSearchFlags>(
                SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK
            ),
            cancellable,
            removalSearchFinished,
            context,
            applicationAttribute,
            applicationAttributeValue,
            targetAttribute,
            identifier.constData(),
            nullptr
        );
    }
}

struct ListResult {
    QList<StoredCredentialSummary> summaries;
    bool encounteredCorruptData = false;
};

struct ListContext {
    GCancellable *cancellable = nullptr;
    GMainContext *mainContext = nullptr;
    GList *items = nullptr;
    GList *current = nullptr;
    ListResult result;
    NativeCompletion<ListResult> completion;

    ~ListContext()
    {
        g_list_free_full(items, g_object_unref);
        g_main_context_unref(mainContext);
        g_object_unref(cancellable);
    }
};

void completeList(ListContext *context, GError *error)
{
    NativeCompletion<ListResult> completion = std::move(context->completion);
    ListResult result = std::move(context->result);
    delete context;
    completion(std::move(result), error);
}

void retrieveNextListItem(ListContext *context);

void listItemFinished(
    GObject *source,
    GAsyncResult *asyncResult,
    gpointer userData
)
{
    auto *context = static_cast<ListContext *>(userData);
    GError *error = nullptr;
    ScopedSecretValue value(secret_retrievable_retrieve_secret_finish(
        SECRET_RETRIEVABLE(source),
        asyncResult,
        &error
    ));
    if (error) {
        completeList(context, error);
        return;
    }

    auto *retrievable = static_cast<SecretRetrievable *>(
        context->current->data
    );
    CredentialStoreError decodeError;
    auto decoded = decodeValue(value.get(), &decodeError);
    if (!decoded) {
        context->result.encounteredCorruptData = true;
    } else {
        decoded->credential.password.fill(QChar('\0'));
        GHashTable *attributes = secret_retrievable_get_attributes(retrievable);
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
            context->result.encounteredCorruptData = true;
        } else {
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
            context->result.summaries.append(std::move(summary));
        }
    }

    context->current = context->current->next;
    retrieveNextListItem(context);
}

void retrieveNextListItem(ListContext *context)
{
    if (!context->current) {
        completeList(context, nullptr);
        return;
    }

    auto *retrievable = static_cast<SecretRetrievable *>(
        context->current->data
    );
    {
        const ThreadDefaultContextScope contextScope(context->mainContext);
        secret_retrievable_retrieve_secret(
            retrievable,
            context->cancellable,
            listItemFinished,
            context
        );
    }
}

void listSearchFinished(
    GObject *,
    GAsyncResult *asyncResult,
    gpointer userData
)
{
    auto *context = static_cast<ListContext *>(userData);
    GError *error = nullptr;
    context->items = secret_password_search_finish(asyncResult, &error);
    if (error) {
        completeList(context, error);
        return;
    }
    context->current = context->items;
    retrieveNextListItem(context);
}

template<typename Result, typename Work>
QFuture<Result> runOnApplicationThread(Work work)
{
    auto promise = std::make_shared<QPromise<Result>>();
    QFuture<Result> future = promise->future();
    promise->start();
    QCoreApplication *application = QCoreApplication::instance();
    if (!application) {
        promise->addResult(work());
        promise->finish();
        return future;
    }
    QTimer::singleShot(
        0,
        application,
        [promise, work = std::move(work)]() mutable {
            promise->addResult(work());
            promise->finish();
        }
    );
    return future;
}

struct WriteLifetime {
    std::shared_ptr<void> mutationLease;
    std::shared_ptr<SecretValue> value;
};

class LinuxCredentialStore final : public CredentialStore {
public:
    [[nodiscard]] bool isAvailable() const override
    {
        if (m_available.load(std::memory_order_acquire))
            return true;

        NativeCallResult<gboolean> call = runNativeCall<gboolean>(
            [](GCancellable *cancellable, NativeCompletion<gboolean> completion) {
                auto *context = new AsyncCompletionContext<gboolean>{
                    std::move(completion)
                };
                secret_password_search(
                    credentialSchema(),
                    SECRET_SEARCH_NONE,
                    cancellable,
                    availabilitySearchFinished,
                    context,
                    applicationAttribute,
                    applicationAttributeValue,
                    nullptr
                );
            }
        );
        if (call.timedOut || call.error) {
            if (call.error)
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
        if (mutationRegistry()->contains(target.identifier())) {
            setError(
                error,
                CredentialStoreErrorCode::Unavailable,
                QStringLiteral(
                    "A Secret Service credential update is still pending"
                )
            );
            return std::nullopt;
        }

        const QByteArray identifier = targetIdentifier(target);
        NativeCallResult<SecretValue *> call = runNativeCall<SecretValue *>(
            [identifier](
                GCancellable *cancellable,
                NativeCompletion<SecretValue *> completion
            ) {
                auto *context = new AsyncCompletionContext<SecretValue *>{
                    std::move(completion)
                };
                secret_password_lookup(
                    credentialSchema(),
                    cancellable,
                    passwordLookupFinished,
                    context,
                    applicationAttribute,
                    applicationAttributeValue,
                    targetAttribute,
                    identifier.constData(),
                    nullptr
                );
            }
        );
        ScopedSecretValue value(call.value);
        if (call.timedOut || call.error) {
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
        std::shared_ptr<void> mutationLease = mutationRegistry()->acquire(
            target.identifier()
        );
        if (!mutationLease) {
            setError(
                error,
                CredentialStoreErrorCode::Unavailable,
                QStringLiteral(
                    "A previous Secret Service credential update is still pending"
                )
            );
            return false;
        }
        const auto sharedValue = std::shared_ptr<SecretValue>(
            secret_value_ref(value.get()),
            [](SecretValue *secret) {
                secret_value_unref(secret);
            }
        );
        const auto lifetime = std::make_shared<WriteLifetime>(WriteLifetime{
            std::move(mutationLease),
            sharedValue,
        });
        NativeCallResult<gboolean> call = runNativeCall<gboolean>(
            [identifier, label, secret = sharedValue](
                GCancellable *cancellable,
                NativeCompletion<gboolean> completion
            ) {
                auto *context = new AsyncCompletionContext<gboolean>{
                    std::move(completion)
                };
                secret_password_store_binary(
                    credentialSchema(),
                    SECRET_COLLECTION_DEFAULT,
                    label.constData(),
                    secret.get(),
                    cancellable,
                    passwordStoreFinished,
                    context,
                    applicationAttribute,
                    applicationAttributeValue,
                    targetAttribute,
                    identifier.constData(),
                    nullptr
                );
            },
            lifetime
        );
        if (call.timedOut || !call.value) {
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
        return removeImpl(
            target,
            error,
            QEventLoop::ExcludeUserInputEvents
        );
    }

    bool removeAll(CredentialStoreError *error) override
    {
        return removeAllImpl(error, QEventLoop::ExcludeUserInputEvents);
    }

    [[nodiscard]] QList<StoredCredentialSummary> list(
        CredentialStoreError *error
    ) override
    {
        return listImpl(error, QEventLoop::ExcludeUserInputEvents);
    }

    [[nodiscard]] QFuture<CredentialStoreListResult> listAsync() override
    {
        const auto store = std::static_pointer_cast<LinuxCredentialStore>(
            shared_from_this()
        );
        return runOnApplicationThread<CredentialStoreListResult>([store] {
            CredentialStoreListResult result;
            result.summaries = store->listImpl(
                &result.error,
                QEventLoop::AllEvents
            );
            return result;
        });
    }

    [[nodiscard]] QFuture<CredentialStoreRemovalResult> removeAsync(
        const QList<CredentialTarget> &targets
    ) override
    {
        const auto store = std::static_pointer_cast<LinuxCredentialStore>(
            shared_from_this()
        );
        return runOnApplicationThread<CredentialStoreRemovalResult>(
            [store, targets] {
                CredentialStoreRemovalResult result;
                for (qsizetype index = 0; index < targets.size(); ++index) {
                    const CredentialTarget &target = targets.at(index);
                    CredentialStoreError error;
                    bool backendUnavailable = false;
                    if (!store->removeImpl(
                            target,
                            &error,
                            QEventLoop::AllEvents,
                            &backendUnavailable
                        )) {
                        result.failures.append(CredentialRemovalFailure{
                            target,
                            error,
                        });
                        if (!backendUnavailable)
                            continue;

                        const CredentialStoreError unprocessedError{
                            CredentialStoreErrorCode::Unavailable,
                            QStringLiteral(
                                "Removal was not attempted because Secret Service "
                                "became unavailable"
                            ),
                        };
                        for (qsizetype remaining = index + 1;
                             remaining < targets.size();
                             ++remaining) {
                            result.failures.append(CredentialRemovalFailure{
                                targets.at(remaining),
                                unprocessedError,
                            });
                        }
                        break;
                    }
                }
                return result;
            }
        );
    }

    [[nodiscard]] QFuture<CredentialStoreOperationResult> removeAllAsync() override
    {
        const auto store = std::static_pointer_cast<LinuxCredentialStore>(
            shared_from_this()
        );
        return runOnApplicationThread<CredentialStoreOperationResult>([store] {
            CredentialStoreOperationResult result;
            result.succeeded = store->removeAllImpl(
                &result.error,
                QEventLoop::AllEvents
            );
            return result;
        });
    }

private:
    bool removeImpl(
        const CredentialTarget &target,
        CredentialStoreError *error,
        QEventLoop::ProcessEventsFlags processEvents,
        bool *backendUnavailable = nullptr
    )
    {
        if (backendUnavailable)
            *backendUnavailable = false;
        clearError(error);
        if (!target.isValid()) {
            setError(
                error,
                CredentialStoreErrorCode::InvalidTarget,
                QStringLiteral("The credential target is invalid")
            );
            return false;
        }

        const QByteArray identifier = targetIdentifier(target);
        std::shared_ptr<void> mutationLease = mutationRegistry()->acquire(
            target.identifier()
        );
        if (!mutationLease) {
            setError(
                error,
                CredentialStoreErrorCode::Unavailable,
                QStringLiteral(
                    "A previous Secret Service credential update is still pending"
                )
            );
            return false;
        }
        NativeCallResult<RemovalResult> call = runNativeCall<RemovalResult>(
            [identifier](
                GCancellable *cancellable,
                NativeCompletion<RemovalResult> completion
            ) {
                startRemoval(
                    cancellable,
                    std::move(completion),
                    identifier,
                    false
                );
            },
            mutationLease,
            processEvents
        );
        if (call.timedOut || call.error) {
            const bool nativeBackendUnavailable = call.timedOut
                || codeForNativeError(call.error)
                    == CredentialStoreErrorCode::Unavailable;
            m_available.store(false, std::memory_order_release);
            consumeNativeError(
                error,
                call.error,
                QStringLiteral("Could not remove the Secret Service credential"),
                call.timedOut
            );
            if (backendUnavailable)
                *backendUnavailable = nativeBackendUnavailable;
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

    bool removeAllImpl(
        CredentialStoreError *error,
        QEventLoop::ProcessEventsFlags processEvents
    )
    {
        clearError(error);
        std::shared_ptr<void> mutationLease = mutationRegistry()->acquireAll();
        if (!mutationLease) {
            setError(
                error,
                CredentialStoreErrorCode::Unavailable,
                QStringLiteral(
                    "A previous Secret Service credential update is still pending"
                )
            );
            return false;
        }
        NativeCallResult<RemovalResult> call = runNativeCall<RemovalResult>(
            [](GCancellable *cancellable, NativeCompletion<RemovalResult> completion) {
                startRemoval(
                    cancellable,
                    std::move(completion),
                    {},
                    true
                );
            },
            mutationLease,
            processEvents
        );
        if (call.timedOut || call.error) {
            m_available.store(false, std::memory_order_release);
            consumeNativeError(
                error,
                call.error,
                QStringLiteral("Could not remove Secret Service credentials"),
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

    [[nodiscard]] QList<StoredCredentialSummary> listImpl(
        CredentialStoreError *error,
        QEventLoop::ProcessEventsFlags processEvents
    )
    {
        clearError(error);
        NativeCallResult<ListResult> call = runNativeCall<ListResult>(
            [](GCancellable *cancellable, NativeCompletion<ListResult> completion) {
                auto *context = new ListContext{
                    G_CANCELLABLE(g_object_ref(cancellable)),
                    g_main_context_ref_thread_default(),
                    nullptr,
                    nullptr,
                    {},
                    std::move(completion),
                };
                secret_password_search(
                    credentialSchema(),
                    static_cast<SecretSearchFlags>(
                        SECRET_SEARCH_ALL
                        | SECRET_SEARCH_UNLOCK
                    ),
                    cancellable,
                    listSearchFinished,
                    context,
                    applicationAttribute,
                    applicationAttributeValue,
                    nullptr
                );
            },
            {},
            processEvents
        );
        if (call.timedOut || call.error) {
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

    mutable std::atomic_bool m_available = false;
};

} // namespace

std::shared_ptr<CredentialStore> createSystemCredentialStore()
{
    return std::make_shared<LinuxCredentialStore>();
}
