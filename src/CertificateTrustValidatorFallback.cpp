#include "CertificateTrustValidator.h"

#include <QCoreApplication>

CertificateValidationResult CertificateTrustValidator::evaluate(
    const QList<QSslCertificate> &,
    const QList<QSslCertificate> &,
    const QString &,
    bool
)
{
    return {
        false,
        QCoreApplication::translate(
            "CertificateTrustValidator",
            "Custom certificate validation is not implemented on this platform yet"
        )
    };
}
