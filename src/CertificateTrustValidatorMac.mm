#include "CertificateTrustValidator.h"

#include <Security/Security.h>

namespace {

QString fromCFString(CFStringRef value)
{
    if (!value)
        return {};

    const CFIndex length = CFStringGetLength(value);
    const CFIndex maximumSize = CFStringGetMaximumSizeForEncoding(
        length,
        kCFStringEncodingUTF8
    ) + 1;
    QByteArray buffer(maximumSize, '\0');
    if (!CFStringGetCString(value, buffer.data(), maximumSize, kCFStringEncodingUTF8))
        return QStringLiteral("Unknown trust evaluation error");
    return QString::fromUtf8(buffer.constData());
}

CFStringRef toCFString(const QString &value)
{
    return CFStringCreateWithCharacters(
        kCFAllocatorDefault,
        reinterpret_cast<const UniChar *>(value.utf16()),
        value.size()
    );
}

CFMutableArrayRef certificatesToArray(const QList<QSslCertificate> &certificates)
{
    CFMutableArrayRef result = CFArrayCreateMutable(
        kCFAllocatorDefault,
        certificates.size(),
        &kCFTypeArrayCallBacks
    );

    for (const QSslCertificate &certificate : certificates) {
        const QByteArray der = certificate.toDer();
        CFDataRef data = CFDataCreate(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8 *>(der.constData()),
            der.size()
        );
        SecCertificateRef securityCertificate = SecCertificateCreateWithData(
            kCFAllocatorDefault,
            data
        );
        CFRelease(data);

        if (!securityCertificate) {
            CFRelease(result);
            return nullptr;
        }

        CFArrayAppendValue(result, securityCertificate);
        CFRelease(securityCertificate);
    }

    return result;
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
        return { false, QStringLiteral("The server did not provide a certificate chain") };
    if (anchors.isEmpty())
        return { false, QStringLiteral("The matching rule has no anchor certificates") };

    CFMutableArrayRef chainArray = certificatesToArray(serverChain);
    CFMutableArrayRef anchorArray = certificatesToArray(anchors);
    if (!chainArray || !anchorArray) {
        if (chainArray)
            CFRelease(chainArray);
        if (anchorArray)
            CFRelease(anchorArray);
        return { false, QStringLiteral("Cannot convert the certificate chain") };
    }

    CFStringRef hostName = toCFString(host);
    SecPolicyRef sslPolicy = SecPolicyCreateSSL(true, hostName);
    CFRelease(hostName);

    SecTrustRef trust = nullptr;
    const OSStatus createStatus = SecTrustCreateWithCertificates(
        chainArray,
        sslPolicy,
        &trust
    );
    CFRelease(chainArray);
    CFRelease(sslPolicy);

    if (createStatus != errSecSuccess || !trust) {
        CFRelease(anchorArray);
        return {
            false,
            QStringLiteral("Cannot create trust object (%1)").arg(createStatus)
        };
    }

    const OSStatus anchorsStatus = SecTrustSetAnchorCertificates(trust, anchorArray);
    CFRelease(anchorArray);
    if (anchorsStatus != errSecSuccess) {
        CFRelease(trust);
        return {
            false,
            QStringLiteral("Cannot set trust anchors (%1)").arg(anchorsStatus)
        };
    }

    const OSStatus modeStatus = SecTrustSetAnchorCertificatesOnly(trust, customOnly);
    if (modeStatus != errSecSuccess) {
        CFRelease(trust);
        return {
            false,
            QStringLiteral("Cannot set trust mode (%1)").arg(modeStatus)
        };
    }

    CFErrorRef evaluationError = nullptr;
    const bool trusted = SecTrustEvaluateWithError(trust, &evaluationError);
    QString explanation;
    if (trusted) {
        explanation = customOnly
            ? QStringLiteral("validated with configured CA only")
            : QStringLiteral("validated with system or configured CA");
    } else if (evaluationError) {
        CFStringRef description = CFErrorCopyDescription(evaluationError);
        explanation = fromCFString(description);
        CFRelease(description);
    } else {
        explanation = QStringLiteral("Certificate validation failed");
    }

    if (evaluationError)
        CFRelease(evaluationError);
    CFRelease(trust);
    return { trusted, explanation };
}
