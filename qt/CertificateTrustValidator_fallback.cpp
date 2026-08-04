#include "CertificateTrustValidator.h"

CertificateValidationResult CertificateTrustValidator::evaluate(
    const QList<QSslCertificate> &,
    const QList<QSslCertificate> &,
    const QString &,
    bool
)
{
    return {
        false,
        QStringLiteral("Custom certificate validation is not implemented on this platform yet")
    };
}
