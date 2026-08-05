#include "CertificateTrustValidator.h"

#include <windows.h>
#include <wincrypt.h>

#include <limits>
#include <string>

namespace {

constexpr DWORD certificateEncoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

class CertificateContext final {
public:
    explicit CertificateContext(PCCERT_CONTEXT context = nullptr)
        : m_context(context)
    {
    }

    ~CertificateContext()
    {
        if (m_context)
            CertFreeCertificateContext(m_context);
    }

    CertificateContext(const CertificateContext &) = delete;
    CertificateContext &operator=(const CertificateContext &) = delete;

    [[nodiscard]] PCCERT_CONTEXT get() const { return m_context; }

private:
    PCCERT_CONTEXT m_context = nullptr;
};

class CertificateStore final {
public:
    explicit CertificateStore(HCERTSTORE store = nullptr)
        : m_store(store)
    {
    }

    ~CertificateStore()
    {
        if (m_store)
            CertCloseStore(m_store, 0);
    }

    CertificateStore(const CertificateStore &) = delete;
    CertificateStore &operator=(const CertificateStore &) = delete;

    [[nodiscard]] HCERTSTORE get() const { return m_store; }

private:
    HCERTSTORE m_store = nullptr;
};

class ChainEngine final {
public:
    explicit ChainEngine(HCERTCHAINENGINE engine = nullptr)
        : m_engine(engine)
    {
    }

    ~ChainEngine()
    {
        if (m_engine)
            CertFreeCertificateChainEngine(m_engine);
    }

    ChainEngine(const ChainEngine &) = delete;
    ChainEngine &operator=(const ChainEngine &) = delete;

    [[nodiscard]] HCERTCHAINENGINE get() const { return m_engine; }

private:
    HCERTCHAINENGINE m_engine = nullptr;
};

class ChainContext final {
public:
    explicit ChainContext(PCCERT_CHAIN_CONTEXT chain = nullptr)
        : m_chain(chain)
    {
    }

    ~ChainContext()
    {
        if (m_chain)
            CertFreeCertificateChain(m_chain);
    }

    ChainContext(const ChainContext &) = delete;
    ChainContext &operator=(const ChainContext &) = delete;

    [[nodiscard]] PCCERT_CHAIN_CONTEXT get() const { return m_chain; }

private:
    PCCERT_CHAIN_CONTEXT m_chain = nullptr;
};

QString windowsError(DWORD error)
{
    wchar_t buffer[512] = {};
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        buffer,
        static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])),
        nullptr
    );
    const QString code = QStringLiteral("0x%1").arg(
        static_cast<qulonglong>(error),
        8,
        16,
        QLatin1Char('0')
    );
    if (length == 0)
        return code;
    return QStringLiteral("%1 (%2)").arg(QString::fromWCharArray(buffer, length).trimmed(), code);
}

bool canPassToCryptoApi(const QByteArray &der)
{
    return !der.isEmpty()
        && static_cast<quint64>(der.size()) <= std::numeric_limits<DWORD>::max();
}

CertificateContext createContext(const QSslCertificate &certificate)
{
    const QByteArray der = certificate.toDer();
    if (!canPassToCryptoApi(der))
        return CertificateContext();
    return CertificateContext(CertCreateCertificateContext(
        certificateEncoding,
        reinterpret_cast<const BYTE *>(der.constData()),
        static_cast<DWORD>(der.size())
    ));
}

bool addCertificate(HCERTSTORE store, const QSslCertificate &certificate, QString *error)
{
    const QByteArray der = certificate.toDer();
    if (!canPassToCryptoApi(der)) {
        *error = QStringLiteral("Cannot convert a certificate to DER");
        return false;
    }
    if (!CertAddEncodedCertificateToStore(
            store,
            certificateEncoding,
            reinterpret_cast<const BYTE *>(der.constData()),
            static_cast<DWORD>(der.size()),
            CERT_STORE_ADD_USE_EXISTING,
            nullptr)) {
        *error = QStringLiteral("Cannot add a certificate to the temporary store: %1")
                     .arg(windowsError(GetLastError()));
        return false;
    }
    return true;
}

CertificateStore createMemoryStore()
{
    return CertificateStore(CertOpenStore(
        CERT_STORE_PROV_MEMORY,
        certificateEncoding,
        0,
        CERT_STORE_CREATE_NEW_FLAG,
        nullptr
    ));
}

struct NativeValidationResult {
    bool trusted = false;
    QString explanation;
};

NativeValidationResult evaluateChain(
    HCERTCHAINENGINE engine,
    PCCERT_CONTEXT leaf,
    HCERTSTORE intermediates,
    const QString &host
)
{
    LPSTR serverAuthenticationOid = const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH);
    LPSTR strongSignatureOid = const_cast<LPSTR>(szOID_CERT_STRONG_SIGN_OS_1);
    CERT_STRONG_SIGN_PARA strongSignatureParameters = {};
    strongSignatureParameters.cbSize = sizeof(strongSignatureParameters);
    strongSignatureParameters.dwInfoChoice = CERT_STRONG_SIGN_OID_INFO_CHOICE;
    strongSignatureParameters.pszOID = strongSignatureOid;

    CERT_CHAIN_PARA chainParameters = {};
    chainParameters.cbSize = sizeof(chainParameters);
    chainParameters.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
    chainParameters.RequestedUsage.Usage.cUsageIdentifier = 1;
    chainParameters.RequestedUsage.Usage.rgpszUsageIdentifier = &serverAuthenticationOid;
    chainParameters.pStrongSignPara = &strongSignatureParameters;

    PCCERT_CHAIN_CONTEXT rawChain = nullptr;
    const DWORD chainFlags = CERT_CHAIN_CACHE_END_CERT
        | CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT
        | CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY
        | CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL
        | CERT_CHAIN_OPT_IN_WEAK_SIGNATURE;
    if (!CertGetCertificateChain(
            engine,
            leaf,
            nullptr,
            intermediates,
            &chainParameters,
            chainFlags,
            nullptr,
            &rawChain)) {
        return {
            false,
            QStringLiteral("Windows could not build the certificate chain: %1")
                .arg(windowsError(GetLastError()))
        };
    }
    const ChainContext chain(rawChain);

    std::wstring serverName = host.toStdWString();
    SSL_EXTRA_CERT_CHAIN_POLICY_PARA sslParameters = {};
    sslParameters.cbSize = sizeof(sslParameters);
    sslParameters.dwAuthType = AUTHTYPE_SERVER;
    sslParameters.fdwChecks = 0;
    sslParameters.pwszServerName = serverName.data();

    CERT_CHAIN_POLICY_PARA policyParameters = {};
    policyParameters.cbSize = sizeof(policyParameters);
    policyParameters.dwFlags = CERT_CHAIN_POLICY_IGNORE_ALL_REV_UNKNOWN_FLAGS;
    policyParameters.pvExtraPolicyPara = &sslParameters;

    CERT_CHAIN_POLICY_STATUS policyStatus = {};
    policyStatus.cbSize = sizeof(policyStatus);
    if (!CertVerifyCertificateChainPolicy(
            CERT_CHAIN_POLICY_SSL,
            chain.get(),
            &policyParameters,
            &policyStatus)) {
        return {
            false,
            QStringLiteral("Windows could not evaluate the certificate policy: %1")
                .arg(windowsError(GetLastError()))
        };
    }
    if (policyStatus.dwError != ERROR_SUCCESS) {
        return {
            false,
            QStringLiteral("Windows rejected the certificate: %1")
                .arg(windowsError(policyStatus.dwError))
        };
    }
    return { true, {} };
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
    if (host.trimmed().isEmpty() || host.contains(QChar::Null))
        return { false, QStringLiteral("The certificate hostname is invalid") };

    const CertificateContext leaf = createContext(serverChain.first());
    if (!leaf.get()) {
        return {
            false,
            QStringLiteral("Cannot decode the server certificate: %1")
                .arg(windowsError(GetLastError()))
        };
    }

    const CertificateStore intermediates = createMemoryStore();
    if (!intermediates.get()) {
        return {
            false,
            QStringLiteral("Cannot create a temporary certificate store: %1")
                .arg(windowsError(GetLastError()))
        };
    }
    QString storeError;
    for (qsizetype index = 1; index < serverChain.size(); ++index) {
        if (!addCertificate(intermediates.get(), serverChain.at(index), &storeError))
            return { false, storeError };
    }

    if (!customOnly) {
        const NativeValidationResult systemResult = evaluateChain(
            nullptr,
            leaf.get(),
            intermediates.get(),
            host
        );
        if (systemResult.trusted)
            return { true, QStringLiteral("validated with Windows system CA") };
    }

    const CertificateStore anchorStore = createMemoryStore();
    if (!anchorStore.get()) {
        return {
            false,
            QStringLiteral("Cannot create the custom anchor store: %1")
                .arg(windowsError(GetLastError()))
        };
    }
    for (const QSslCertificate &anchor : anchors) {
        if (!addCertificate(anchorStore.get(), anchor, &storeError))
            return { false, storeError };
    }

    HCERTSTORE additionalStores[] = { intermediates.get() };
    CERT_CHAIN_ENGINE_CONFIG engineConfiguration = {};
    engineConfiguration.cbSize = sizeof(engineConfiguration);
    engineConfiguration.cAdditionalStore = 1;
    engineConfiguration.rghAdditionalStore = additionalStores;
    engineConfiguration.hExclusiveRoot = anchorStore.get();
    engineConfiguration.dwExclusiveFlags = CERT_CHAIN_EXCLUSIVE_ENABLE_CA_FLAG;

    HCERTCHAINENGINE rawEngine = nullptr;
    if (!CertCreateCertificateChainEngine(&engineConfiguration, &rawEngine)) {
        return {
            false,
            QStringLiteral("Cannot create the custom Windows trust engine: %1")
                .arg(windowsError(GetLastError()))
        };
    }
    const ChainEngine customEngine(rawEngine);
    const NativeValidationResult customResult = evaluateChain(
        customEngine.get(),
        leaf.get(),
        intermediates.get(),
        host
    );
    if (!customResult.trusted)
        return { false, customResult.explanation };
    return {
        true,
        customOnly ? QStringLiteral("validated with configured CA only")
                   : QStringLiteral("validated with configured CA")
    };
}
