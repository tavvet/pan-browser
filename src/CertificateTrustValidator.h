#pragma once

#include <QList>
#include <QSslCertificate>
#include <QString>

struct CertificateValidationResult {
    bool trusted = false;
    QString explanation;
};

class CertificateTrustValidator {
public:
    static CertificateValidationResult evaluate(
        const QList<QSslCertificate> &serverChain,
        const QList<QSslCertificate> &anchors,
        const QString &host,
        bool customOnly
    );
};
