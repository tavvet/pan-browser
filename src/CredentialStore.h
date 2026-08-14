#pragma once

#include <QDateTime>
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

class CredentialStore {
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
    [[nodiscard]] virtual QList<StoredCredentialSummary> list(
        CredentialStoreError *error = nullptr
    ) = 0;
};

[[nodiscard]] std::unique_ptr<CredentialStore> createSystemCredentialStore();
