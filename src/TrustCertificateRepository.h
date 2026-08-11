#pragma once

#include "TrustSettings.h"

#include <QList>
#include <QSslCertificate>
#include <QString>
#include <QStringList>

struct TrustCertificateInfo {
    QString configuredPath;
    QString absolutePath;
    QList<QSslCertificate> certificates;

    bool isReadable() const { return !certificates.isEmpty(); }
};

enum class TrustCertificateImportFailureReason {
    InvalidCertificate,
    CopyFailed,
};

struct TrustCertificateImportFailure {
    QString sourcePath;
    TrustCertificateImportFailureReason reason =
        TrustCertificateImportFailureReason::InvalidCertificate;
};

struct TrustCertificateImportResult {
    QString certificateDirectoryError;
    QStringList anchors;
    QList<TrustCertificateImportFailure> failures;
};

class TrustCertificateRepository final {
public:
    explicit TrustCertificateRepository(QString configurationPath);
    ~TrustCertificateRepository();
    TrustCertificateRepository(const TrustCertificateRepository &) = delete;
    TrustCertificateRepository &operator=(const TrustCertificateRepository &) = delete;
    TrustCertificateRepository(TrustCertificateRepository &&) = delete;
    TrustCertificateRepository &operator=(TrustCertificateRepository &&) = delete;

    TrustCertificateInfo inspect(const QString &configuredPath) const;
    TrustCertificateImportResult importFiles(const QStringList &sourcePaths);

    [[nodiscard]] QStringList finalize(const QList<TrustRuleSettings> &rules);
    [[nodiscard]] QStringList rollback();

    static QString displayName(const QSslCertificate &certificate);
    static QString fingerprint(const QSslCertificate &certificate);

private:
    QString resolvePath(const QString &configuredPath) const;

    QString m_configurationPath;
    QStringList m_pendingFiles;
};
