#pragma once

#include <QDateTime>
#include <QFuture>
#include <QList>
#include <QString>

#include <memory>
#include <optional>

class QUrl;

enum class CredentialKind {
    HttpServer,
    HttpProxy,
};

enum class CredentialAuthentication {
    HttpRealmPassword,
};

struct CredentialTarget {
    CredentialKind kind = CredentialKind::HttpServer;
    CredentialAuthentication authentication =
        CredentialAuthentication::HttpRealmPassword;
    QString scheme;
    QString host;
    int port = -1;
    QString realm;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString identifier() const;

    [[nodiscard]] static std::optional<CredentialTarget> forHttpServer(
        const QUrl &url,
        const QString &realm
    );
    [[nodiscard]] static std::optional<CredentialTarget> forHttpProxy(
        const QString &host,
        int port,
        const QString &realm
    );
};

struct StoredCredential {
    QString username;
    QString password;
};

enum class CredentialStoreErrorCode {
    None,
    NotFound,
    Unavailable,
    AccessDenied,
    InvalidTarget,
    TooLarge,
    CorruptData,
    PlatformError,
};

struct CredentialStoreError {
    CredentialStoreErrorCode code = CredentialStoreErrorCode::None;
    QString message;

    void clear();
    [[nodiscard]] bool shouldReport() const;
};

struct StoredCredentialSummary {
    CredentialTarget target;
    QString username;
    QDateTime lastModified;
};

struct CredentialStoreListResult {
    QList<StoredCredentialSummary> summaries;
    CredentialStoreError error;
};

struct CredentialRemovalFailure {
    CredentialTarget target;
    CredentialStoreError error;
};

struct CredentialStoreRemovalResult {
    QList<CredentialRemovalFailure> failures;
};

struct CredentialStoreOperationResult {
    bool succeeded = false;
    CredentialStoreError error;
};

class CredentialStore : public std::enable_shared_from_this<CredentialStore> {
public:
    virtual ~CredentialStore() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual std::optional<StoredCredential> read(
        const CredentialTarget &target,
        CredentialStoreError *error = nullptr
    ) = 0;
    virtual bool write(
        const CredentialTarget &target,
        const StoredCredential &credential,
        CredentialStoreError *error = nullptr
    ) = 0;
    virtual bool remove(
        const CredentialTarget &target,
        CredentialStoreError *error = nullptr
    ) = 0;
    virtual bool removeAll(CredentialStoreError *error = nullptr) = 0;
    [[nodiscard]] virtual QList<StoredCredentialSummary> list(
        CredentialStoreError *error = nullptr
    ) = 0;

    [[nodiscard]] virtual QFuture<CredentialStoreListResult> listAsync();
    [[nodiscard]] virtual QFuture<CredentialStoreRemovalResult> removeAsync(
        const QList<CredentialTarget> &targets
    );
    [[nodiscard]] virtual QFuture<CredentialStoreOperationResult> removeAllAsync();
};

[[nodiscard]] std::shared_ptr<CredentialStore> createSystemCredentialStore();
