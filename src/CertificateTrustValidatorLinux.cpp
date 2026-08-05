#include "CertificateTrustValidator.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QHostAddress>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509err.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <limits>
#include <memory>
#include <utility>

namespace {

using CertificatePtr = std::unique_ptr<X509, decltype(&X509_free)>;
using StorePtr = std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)>;
using StoreContextPtr = std::unique_ptr<X509_STORE_CTX, decltype(&X509_STORE_CTX_free)>;

class CertificateStack final {
public:
    CertificateStack()
        : m_stack(sk_X509_new_null())
    {
    }

    ~CertificateStack()
    {
        if (m_stack)
            sk_X509_pop_free(m_stack, X509_free);
    }

    CertificateStack(const CertificateStack &) = delete;
    CertificateStack &operator=(const CertificateStack &) = delete;

    [[nodiscard]] STACK_OF(X509) *get() const { return m_stack; }

    bool append(CertificatePtr certificate)
    {
        if (!m_stack || !certificate || sk_X509_push(m_stack, certificate.get()) == 0)
            return false;
        certificate.release();
        return true;
    }

private:
    STACK_OF(X509) *m_stack = nullptr;
};

QString opensslErrors()
{
    QStringList messages;
    for (unsigned long error = ERR_get_error(); error != 0; error = ERR_get_error()) {
        char buffer[256] = {};
        ERR_error_string_n(error, buffer, sizeof(buffer));
        messages.append(QString::fromLatin1(buffer));
    }
    return messages.isEmpty()
        ? QCoreApplication::translate("CertificateTrustValidator", "unknown OpenSSL error")
                              : messages.join(QStringLiteral("; "));
}

CertificatePtr decodeCertificate(const QSslCertificate &certificate)
{
    const QByteArray der = certificate.toDer();
    if (der.isEmpty()
        || static_cast<quint64>(der.size())
            > static_cast<quint64>(std::numeric_limits<long>::max())) {
        return CertificatePtr(nullptr, X509_free);
    }

    const unsigned char *cursor = reinterpret_cast<const unsigned char *>(der.constData());
    const unsigned char *const end = cursor + der.size();
    X509 *decoded = d2i_X509(nullptr, &cursor, static_cast<long>(der.size()));
    if (!decoded || cursor != end) {
        if (decoded)
            X509_free(decoded);
        return CertificatePtr(nullptr, X509_free);
    }
    return CertificatePtr(decoded, X509_free);
}

bool configurePeerIdentity(X509_VERIFY_PARAM *parameters, QString host, QString *error)
{
    while (host.endsWith(QLatin1Char('.')))
        host.chop(1);
    if (host.isEmpty() || host.contains(QChar::Null)) {
        *error = QCoreApplication::translate("CertificateTrustValidator", "The certificate hostname is invalid");
        return false;
    }

    QHostAddress address;
    if (address.setAddress(host)) {
        const QByteArray asciiAddress = address.toString().toLatin1();
        if (X509_VERIFY_PARAM_set1_ip_asc(parameters, asciiAddress.constData()) != 1) {
            *error = QCoreApplication::translate("CertificateTrustValidator", "OpenSSL cannot configure IP address validation: %1")
                         .arg(opensslErrors());
            return false;
        }
        return true;
    }

    const QByteArray asciiHost = QUrl::toAce(host);
    if (asciiHost.isEmpty() || asciiHost.contains('\0')) {
        *error = QCoreApplication::translate("CertificateTrustValidator", "The certificate hostname is invalid");
        return false;
    }
    X509_VERIFY_PARAM_set_hostflags(parameters, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (X509_VERIFY_PARAM_set1_host(
            parameters,
            asciiHost.constData(),
            static_cast<size_t>(asciiHost.size())) != 1) {
        *error = QCoreApplication::translate("CertificateTrustValidator", "OpenSSL cannot configure hostname validation: %1")
                     .arg(opensslErrors());
        return false;
    }
    return true;
}

} // namespace

CertificateValidationResult CertificateTrustValidator::evaluate(
    const QList<QSslCertificate> &serverChain,
    const QList<QSslCertificate> &anchors,
    const QString &host,
    bool customOnly
)
{
    if (serverChain.isEmpty())
        return { false, QCoreApplication::translate("CertificateTrustValidator", "The server did not provide a certificate chain") };
    if (anchors.isEmpty())
        return { false, QCoreApplication::translate("CertificateTrustValidator", "The matching rule has no anchor certificates") };

    ERR_clear_error();
    StorePtr store(X509_STORE_new(), X509_STORE_free);
    if (!store)
        return { false, QCoreApplication::translate("CertificateTrustValidator", "Cannot create the OpenSSL trust store: %1").arg(opensslErrors()) };

    QString systemPathsError;
    if (!customOnly && X509_STORE_set_default_paths(store.get()) != 1) {
        systemPathsError = opensslErrors();
        ERR_clear_error();
    }

    QSet<QByteArray> configuredCertificates;
    for (const QSslCertificate &anchor : anchors) {
        const QByteArray der = anchor.toDer();
        if (configuredCertificates.contains(der))
            continue;
        configuredCertificates.insert(der);

        CertificatePtr certificate = decodeCertificate(anchor);
        if (!certificate) {
            return {
                false,
                QCoreApplication::translate("CertificateTrustValidator", "Cannot decode a configured CA certificate: %1")
                    .arg(opensslErrors())
            };
        }
        ERR_clear_error();
        if (X509_STORE_add_cert(store.get(), certificate.get()) != 1) {
            const unsigned long error = ERR_peek_last_error();
            if (ERR_GET_LIB(error) == ERR_LIB_X509
                && ERR_GET_REASON(error) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
                ERR_clear_error();
                continue;
            }
            return {
                false,
                QCoreApplication::translate("CertificateTrustValidator", "Cannot add a certificate to the OpenSSL trust store: %1")
                    .arg(opensslErrors())
            };
        }
    }

    CertificatePtr leaf = decodeCertificate(serverChain.first());
    if (!leaf) {
        return {
            false,
            QCoreApplication::translate("CertificateTrustValidator", "Cannot decode the server certificate: %1").arg(opensslErrors())
        };
    }

    CertificateStack intermediates;
    if (!intermediates.get())
        return { false, QCoreApplication::translate("CertificateTrustValidator", "Cannot create the OpenSSL intermediate stack") };
    for (qsizetype index = 1; index < serverChain.size(); ++index) {
        CertificatePtr certificate = decodeCertificate(serverChain.at(index));
        if (!certificate) {
            return {
                false,
                QCoreApplication::translate("CertificateTrustValidator", "Cannot decode a server chain certificate: %1")
                    .arg(opensslErrors())
            };
        }
        if (!intermediates.append(std::move(certificate))) {
            return {
                false,
                QCoreApplication::translate("CertificateTrustValidator", "Cannot add a certificate to the OpenSSL intermediate stack: %1")
                    .arg(opensslErrors())
            };
        }
    }

    StoreContextPtr context(X509_STORE_CTX_new(), X509_STORE_CTX_free);
    if (!context) {
        return {
            false,
            QCoreApplication::translate("CertificateTrustValidator", "Cannot create the OpenSSL verification context: %1")
                .arg(opensslErrors())
        };
    }
    if (X509_STORE_CTX_init(
            context.get(),
            store.get(),
            leaf.get(),
            intermediates.get()) != 1) {
        return {
            false,
            QCoreApplication::translate("CertificateTrustValidator", "Cannot initialize OpenSSL certificate validation: %1")
                .arg(opensslErrors())
        };
    }

    X509_VERIFY_PARAM *parameters = X509_STORE_CTX_get0_param(context.get());
    if (!parameters)
        return { false, QCoreApplication::translate("CertificateTrustValidator", "OpenSSL did not provide verification parameters") };
    if (X509_VERIFY_PARAM_set_purpose(parameters, X509_PURPOSE_SSL_SERVER) != 1) {
        return {
            false,
            QCoreApplication::translate("CertificateTrustValidator", "OpenSSL cannot configure server authentication: %1")
                .arg(opensslErrors())
        };
    }
    X509_VERIFY_PARAM_set_auth_level(parameters, 2);
    // Revocation is deliberately not enabled here: this synchronous recovery
    // path has no freshness-checked CRL or OCSP input, and enabling CRL checks
    // without one would reject every custom chain. MainWindow only calls this
    // backend for Chromium's unknown-CA error; any explicit Chromium revoked
    // error remains rejected. A custom chain whose status Chromium could not
    // determine is therefore accepted with soft-fail revocation semantics.
    if (X509_VERIFY_PARAM_set_flags(
            parameters,
            X509_V_FLAG_PARTIAL_CHAIN
                | X509_V_FLAG_TRUSTED_FIRST
                | X509_V_FLAG_X509_STRICT) != 1) {
        return {
            false,
            QCoreApplication::translate("CertificateTrustValidator", "OpenSSL cannot configure certificate validation: %1")
                .arg(opensslErrors())
        };
    }
    QString identityError;
    if (!configurePeerIdentity(parameters, host, &identityError))
        return { false, identityError };

    ERR_clear_error();
    if (X509_verify_cert(context.get()) != 1) {
        const int verificationError = X509_STORE_CTX_get_error(context.get());
        QString explanation = QCoreApplication::translate(
            "CertificateTrustValidator",
            "OpenSSL rejected the certificate at chain depth %1: %2"
        )
                                  .arg(X509_STORE_CTX_get_error_depth(context.get()))
                                  .arg(QString::fromLatin1(
                                      X509_verify_cert_error_string(verificationError)
                                  ));
        if (!systemPathsError.isEmpty()) {
            explanation += QCoreApplication::translate(
                "CertificateTrustValidator",
                "; system trust paths were unavailable: %1"
            )
                               .arg(systemPathsError);
        }
        return { false, explanation };
    }

    return {
        true,
        customOnly
            ? QCoreApplication::translate(
                "CertificateTrustValidator",
                "validated with configured CA only"
            )
            : QCoreApplication::translate(
                "CertificateTrustValidator",
                "validated with system or configured CA"
            )
    };
}
