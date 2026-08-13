#pragma once

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

class CredentialStore {
public:
    virtual ~CredentialStore() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual std::optional<StoredCredential> read(
        const CredentialTarget &target,
        QString *error = nullptr
    ) = 0;
    virtual bool write(
        const CredentialTarget &target,
        const StoredCredential &credential,
        QString *error = nullptr
    ) = 0;
    virtual bool remove(
        const CredentialTarget &target,
        QString *error = nullptr
    ) = 0;
};

[[nodiscard]] std::unique_ptr<CredentialStore> createSystemCredentialStore();
