#include "PanBrowserTestCommon.h"

class TrustAndCertificateTests final : public QObject {
    Q_OBJECT

private slots:
    void exactDomainIsCaseInsensitive();
    void wildcardMatchesSubdomainsOnly();
    void malformedWildcardsAreRejected();
    void settingsRoundTripAndCreateBackup();
    void overlappingEnabledDomainsAreRejected();
    void runtimeRejectsOverlappingEnabledDomains();
    void customModeRequiresCertificate();
    void disabledDraftMayBeIncomplete();
    void trustRulesPageLoadsRulesAndSelectsFirst();
    void certificateRepositoryRollsBackPendingImport();
    void certificateRepositoryFinalizesOnlyReferencedImports();
    void certificateRepositoryRejectsInvalidFiles();
#if defined(Q_OS_UNIX)
    void certificateRepositoryRetainsFailedCleanupForRetry();
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    void nativeCertificateValidatorTrustsConfiguredAnchor();
    void nativeCertificateValidatorRejectsWrongHostname();
    void nativeCertificateValidatorRejectsWeakKey();
#endif
#if defined(Q_OS_LINUX)
    void linuxCertificateValidatorSupportsSystemPlusCustom();
    void linuxCertificateValidatorBuildsIntermediateChain();
    void linuxCertificateValidatorTrustsIntermediateAnchor();
    void linuxCertificateValidatorMatchesIpSan();
#endif
};

namespace {

const QByteArray validatorRootCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIIDIDCCAgigAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwHzEdMBsGA1UEAwwUUGFu
QnJvd3Nlci1UZXN0LVJvb3QwHhcNMjYwODA4MTgyMjI5WhcNNDYwODAzMTgyMjI5
WjAfMR0wGwYDVQQDDBRQYW5Ccm93c2VyLVRlc3QtUm9vdDCCASIwDQYJKoZIhvcN
AQEBBQADggEPADCCAQoCggEBAL0jaZ3AYLZimS7ZDuiCtbWBlth7VyXg9euXkHAA
iEz9cK0g/G0z6XAtNlhCIMUnkVsvcfZ8giKLwBwepH1G7dn7FsJVstvIgbb1t2zJ
8WBMKEYdSTpwU6R2DJJte6UU2bJT3bjlE94PP0E2g85PioZtNuhbSZg+G2dZC0bv
VVTtui5rcHugxgpMOZsCXyKAZKvYZZE4uW09S+hvhq6OPKU9xGsiv3Ae5XxMUjdj
/dGY9o30KW1nh8yoiBDXGvN2I0vfpB7P2FkdUNzsQPaJrxH9MqZhiKkgaKYutXto
iGoo0KwjFZsQlpQ6D4/BRvvEahymkik9+2yCV5tTsqHBVj8CAwEAAaNmMGQwEgYD
VR0TAQH/BAgwBgEB/wIBATAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFJer2zHH
OGc2bshmlWaYRpUePeikMB8GA1UdIwQYMBaAFJer2zHHOGc2bshmlWaYRpUePeik
MA0GCSqGSIb3DQEBCwUAA4IBAQAbbBkjnCfuBDzh3XEsFtYxyzczGqiaudNqBUsy
7mFITW2uzN+qnKgPd8FS9R5448JhMjzMvIQsbfcqCLUWBnUvdGt7k6sWIVBAzn5b
RIuf4FSY1QfT0Aezm0NyJ6gbWTpOMMyeBftkNlQ++WOot5Fhq/8zRVeyyuwy14P/
YtPZ3d0pI61l8GbdP0TbGLwOSh02XdIkw4BKo5EBMXNVZ92wUmPFrEjvSPdw+MfV
cGLpQnOaC7Xb/D1l9kPPHF+h1i4mB1d1+zP4ZfECdPjS+vczHKQ9jBku6EjhTZkM
9Q8/Lmk94Tzoz0xkx/z1zmkL4qjSmZYc9fa+1oEHPj256h9N
-----END CERTIFICATE-----
)CERT");

const QByteArray validatorLeafCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIIDXDCCAkSgAwIBAgICEAEwDQYJKoZIhvcNAQELBQAwHzEdMBsGA1UEAwwUUGFu
QnJvd3Nlci1UZXN0LVJvb3QwHhcNMjYwODA4MTgyMjQxWhcNNDYwODAzMTgyMjQx
WjAkMSIwIAYDVQQDDBl2YWxpZGF0b3IucGFuYnJvd3Nlci50ZXN0MIIBIjANBgkq
hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAlc/5ebLYZ3j+OVtNgfiH8fAIVD5cQm6W
xVZNCEPZ8lurUgdQSnaNhSlqamq6aWrO10tW+n8TkBXEVttYF8c88gf9QuXFbAUk
mLrsTeQzUfDD910J0cuw9Eh6aBSq2C+AyWSgoJKRBx3SGi3+eGegO32cYmYszneT
Q5M2tIvKTkqnvB1Ru74qRTlk3hEn9hpO7YC559232rtiQCDivQAFvPQiW2QglY3X
gYcgkxiN8BsVWlMO9ZJOYQyt3ZZCS9m3DqiMP+ZNuUlXpo1i1ixgDoRyfj9AtgXE
wXAfh2GxS/W1sgTR23P4+kIBiUeKSjZbsRTPQRzdhelZJLb0VSr8kwIDAQABo4Gc
MIGZMAwGA1UdEwEB/wQCMAAwDgYDVR0PAQH/BAQDAgWgMBMGA1UdJQQMMAoGCCsG
AQUFBwMBMCQGA1UdEQQdMBuCGXZhbGlkYXRvci5wYW5icm93c2VyLnRlc3QwHQYD
VR0OBBYEFPcGMPV3UF27OOKYw+0JI8wlXGhvMB8GA1UdIwQYMBaAFJer2zHHOGc2
bshmlWaYRpUePeikMA0GCSqGSIb3DQEBCwUAA4IBAQAvTl4wKEuHoi1Pgi8T7FRS
UlvRjUZ09OhGGX1vpDKjSzfx36O2Qc1ebqY4h44JTzFpxkv8HUgfOLzOkEX4Eixu
AD4RBD1cLb9LW7xOgebxw5ADSNrY92Kqergz6COad0FsjlO1jVkIwppe8LSM+NgS
W8EyktDWcSFUZ/9bsW8AYflVxfUnKJ+HncQEj32ZryDkWRyNS0CFjU1ZN2tAPDjA
0P38BHI67JWCFO39B4NmUVs6fFz8mMYF9Bo0ChXmUSeBcs8MOkDoXlwTVKnTEZEl
bIxUrk86o9Xo0/42VLFMCNg3iVIFuqLA0ZkjBiqcXq/pkjOUrH53cXZfOWNm2HOT
-----END CERTIFICATE-----
)CERT");

const QByteArray validatorWeakLeafCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIIC2DCCAcCgAwIBAgICEAIwDQYJKoZIhvcNAQELBQAwHzEdMBsGA1UEAwwUUGFu
QnJvd3Nlci1UZXN0LVJvb3QwHhcNMjYwODA4MTgyMjQxWhcNNDYwODAzMTgyMjQx
WjAkMSIwIAYDVQQDDBl2YWxpZGF0b3IucGFuYnJvd3Nlci50ZXN0MIGfMA0GCSqG
SIb3DQEBAQUAA4GNADCBiQKBgQDJfrCIz839fWoS5UH9lwh//aQ67dhYHGIpQK9W
qH6Sbx0O97oB2jUGsc3lVM4I4yP5XvtjyQr3rMHu0z7nuxRewPMA2nBkJ1V/AGye
49vzReOWZYF0szyo3V+0Wv9klmyize/x0/bgDwkEs5Rvjis9uzvsfj3LwWzcEa5l
+JYUawIDAQABo4GcMIGZMAwGA1UdEwEB/wQCMAAwDgYDVR0PAQH/BAQDAgWgMBMG
A1UdJQQMMAoGCCsGAQUFBwMBMCQGA1UdEQQdMBuCGXZhbGlkYXRvci5wYW5icm93
c2VyLnRlc3QwHQYDVR0OBBYEFGncbxJTSP2o3NENkYnUXXuUSb/IMB8GA1UdIwQY
MBaAFJer2zHHOGc2bshmlWaYRpUePeikMA0GCSqGSIb3DQEBCwUAA4IBAQAwHPk2
HkoZFPzly05kg5/qj2jsvYLTiNpLA3prvFFQcDgT3VQV544i04NwmCQBy2BtsSBy
VV4xLvYLJ/ruyRS+pLVqWVGLESa7NDDotAe3esK8XTXKznH6ZWw9QWZ0pCns1KIY
lpVLElQQTvAYtnXCTyadP/1RtELSNg10T1FoiX3m5Q/y80qsOEx8zV+WMkirB0Bz
x6peWdEhoq73r2KuMLb97HNT9Z5MzBls8ZOyjoZ5S4hmc+kS0x+arRZxOCiAQtwp
a2VOeFPRDk0S+8yagi4bniJMm7WREOswZaFtTKBLiFpJ/7HIBHJlSXjxxBKwZ1lF
ugiIoAqrFwFrqY9W
-----END CERTIFICATE-----
)CERT");

#if defined(Q_OS_LINUX)
const QByteArray linuxValidatorRootCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIIDLDCCAhSgAwIBAgICIAEwDQYJKoZIhvcNAQELBQAwJTEjMCEGA1UEAwwaUGFu
QnJvd3NlciBMaW51eCBUZXN0IFJvb3QwHhcNMjYwODA1MTIzMzI0WhcNMzYwODAy
MTIzMzI0WjAlMSMwIQYDVQQDDBpQYW5Ccm93c2VyIExpbnV4IFRlc3QgUm9vdDCC
ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAJ8wStjdor/CZc/eg6c935WB
rq4H2WXnBjzt5kdwkYCU5kW0XVfsjb0AAXP+7U/T5yuR8ovyko+gu4uULc82LFzv
Aavn3RJ5J+1imqYOS5vrH2Vc1M7FLsyxqRJ71L5ADony52z0XTtyalb5EpCVyEDr
4j7SF5riAz17RUwd41yG7M73yHVk1W1KcZOvy7o6UBXUz0Ym1fdIsu6FcLDSYZro
4NdJaOv8Wd3Q4fkfsq3yVtow/N6wvmIwg68xyaSqYOzkZrcdZp5ib383wVeu5e/j
I9xstWdDchXJAuwRs2sovOYUejnu2VRacbyaggDpUCX7szGGMvUszcxRKwtTOusC
AwEAAaNmMGQwHwYDVR0jBBgwFoAUS3gv7xIYfOOg6I/rqF2DyHilX8kwEgYDVR0T
AQH/BAgwBgEB/wIBATAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFEt4L+8SGHzj
oOiP66hdg8h4pV/JMA0GCSqGSIb3DQEBCwUAA4IBAQA8J2+v2WwNS+Ml7Qpvloyl
lGj1uY8UXeKfsyCvxLHxgJGJKMPiv01HPpxZI2ZmXUUD0sfhGjnm80KjV6Q44N/Z
KVogOmdg2GBxO+QFAl8itO4vai2J6xELRpaT0ZdYyKe43Hz7GpJef0xQuvjCiAb/
mpQJ3vo0TJStaJ8y8nd+d2ZOhK+sofy0kQEiD8OX344jwM7v0SAK/D3MRVA2KlbO
xRT1kF+r5Mx6BcMG/k/NQtcbUUILPPcDgXoaAJp1xveYwIg7jrFZ+VKHX3Umf1ST
dvu+oZGnNI+bGHF4AUdnXt5W+Xv4URKNJoQdQ9CkSrgUs0EdwALosGJ5wtRKnkGX
-----END CERTIFICATE-----
)CERT");

const QByteArray linuxValidatorIntermediateCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIIDNDCCAhygAwIBAgICIAIwDQYJKoZIhvcNAQELBQAwJTEjMCEGA1UEAwwaUGFu
QnJvd3NlciBMaW51eCBUZXN0IFJvb3QwHhcNMjYwODA1MTIzNDQyWhcNMzQxMDIy
MTIzNDQyWjAtMSswKQYDVQQDDCJQYW5Ccm93c2VyIExpbnV4IFRlc3QgSW50ZXJt
ZWRpYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA27/grCU2+ozw
NBwtWhnm8jxbvLhZIqcnvPD+ldMgFbWDtWFUFoFLGXngFLtk1k5Y/dHPIMztylws
xvH4b0XzgD6GftAJhgSAeG7WyWDi0L50z2tfBYMWLVzMNAiHrVzEnA+EnoWhGpIx
JKhrddkbgtaY+z0as9wH9byYVU70Fhx918/ts2UeeovuvrCsoT99z/fxPtF5yQPh
EYd2HlsyRTWPcA+KPIzZnPCgapWdfuZMdUxHsiVeveZolKUl6iL1lLtZiy+o+q98
2p7LUScO7rBpbxC/48J3tfI0cbnlveyyJVdNhSYPSVULUClgJBMpJNrVjhfwNnkd
UVDBCo1klwIDAQABo2YwZDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQE
AwIBBjAdBgNVHQ4EFgQUDyYPmkoSffZwRB9xqjB/MdW0l3gwHwYDVR0jBBgwFoAU
S3gv7xIYfOOg6I/rqF2DyHilX8kwDQYJKoZIhvcNAQELBQADggEBADlQAH8rbjht
A1dcptPP3rDQUXC2ZrBoPVtpW/D9VEDbnJ8Uk7q5jxTlWF9jx3xOmK0HlOfeinDb
5Oo/Qg4/FY3u2t+zwxafXY6O/HQHCLrZOBehOoioPuJtuEeVJpOdVa0KbA1mxHyD
FRLSXI8NSh2g57jLVeG+yqNmjPSn70unuloOOaaxLLOKWUlT6qtAOk8/x1mQ+y4o
aMgnTyJJToym6bgMSDoV8ziWBeOxB7KHfZjgmJ11CyXKBsfDFVA8DjM3c4NERSco
tRtxck4Tcf2H1d9JRhPFdjv7DypJgKWDGiO1eKCiEn+sSPtBUQk0gAfSqo4S1kz/
XI98279ho9E=
-----END CERTIFICATE-----
)CERT");

const QByteArray linuxValidatorLeafCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIIDfDCCAmSgAwIBAgICIAMwDQYJKoZIhvcNAQELBQAwLTErMCkGA1UEAwwiUGFu
QnJvd3NlciBMaW51eCBUZXN0IEludGVybWVkaWF0ZTAeFw0yNjA4MDUxMjM5MDda
Fw0zNDEwMjIxMjM5MDdaMCoxKDAmBgNVBAMMH2xpbnV4LXZhbGlkYXRvci5wYW5i
cm93c2VyLnRlc3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDcXzOE
AkP5T7hw12bM5+jjGMlDEl4ofZKBUKw9bYYdxwNqVYNmbub5G0C1nOJFHcQiyQMA
Vc2ltLrEa6IKC40KLJlfxwGCnnLueFDc9oQkyyQA5KeIUxu5StJeiieKxt7hMAzi
o1DNdak52b/BKJC/RtjI+eVs5CdIuCbuFJ6iNnwhliPWjg6CCBimPFC/W3+zMDtQ
HbPdnoDeDU68hc6K74YUc6eRNj1dvtn3AM0ZuAP6AVoWGC1uDQQ6AewTwV6/EvXW
T63ytMRR3PPtgyeemTGr0qY9jUO21HIcCkLcJPA+Xl+ll6+pun+jh8jdOwWnJvkm
dElQw4JLZXluP1mlAgMBAAGjgagwgaUwDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8E
BAMCBaAwEwYDVR0lBAwwCgYIKwYBBQUHAwEwMAYDVR0RBCkwJ4IfbGludXgtdmFs
aWRhdG9yLnBhbmJyb3dzZXIudGVzdIcEwAACKjAdBgNVHQ4EFgQUdBhLlHYhoghw
mLRLK1TrjCStZ4MwHwYDVR0jBBgwFoAUDyYPmkoSffZwRB9xqjB/MdW0l3gwDQYJ
KoZIhvcNAQELBQADggEBAFYWwgTYKplZs6DgYAvydglyeeZx8DNCUOOJsQ2NkFDj
QoZ7EinV405dHBVr3FTcNYRjD8AKj0/mW4ogsc89KhWj7HCC2X2iSUiinRnl2Q/U
nKPRcqok6oOEwylkqzsDV+J3aIFBD71FGIWT5Twy5WJz3OdgJ4mKqVivsxJsjXmy
GU12WV1Rbhb9H/DfDaogjSmDDeNg6M65/piB4MdCK9IFJEckMcxWBeY8bRVv/vkI
4lvwt0wtoxFTtBZ72sfkgulyTCyCUcZXWxwsWvg6Ti33OZegi5vx1YAKfRR6ydmK
/WsSy4nkA81p29BVJ4uTnvPOIuQtD6Yldr6lJBe+PLw=
-----END CERTIFICATE-----
)CERT");
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
QList<QSslCertificate> testCertificates(const QByteArray &pem)
{
    return QSslCertificate::fromData(pem, QSsl::Pem);
}
#endif

} // namespace

void TrustAndCertificateTests::exactDomainIsCaseInsensitive()
{
    const DomainPattern pattern = DomainPattern::parse(QStringLiteral("Example.COM."));
    QVERIFY(pattern.isValid());
    QVERIFY(pattern.matches(QStringLiteral("example.com")));
    QVERIFY(pattern.matches(QStringLiteral("EXAMPLE.COM.")));
    QVERIFY(!pattern.matches(QStringLiteral("www.example.com")));
}

void TrustAndCertificateTests::wildcardMatchesSubdomainsOnly()
{
    const DomainPattern pattern = DomainPattern::parse(QStringLiteral("*.example.com"));
    QVERIFY(pattern.isValid());
    QVERIFY(pattern.matches(QStringLiteral("www.example.com")));
    QVERIFY(pattern.matches(QStringLiteral("api.internal.example.com")));
    QVERIFY(!pattern.matches(QStringLiteral("example.com")));
    QVERIFY(!pattern.matches(QStringLiteral("notexample.com")));
}

void TrustAndCertificateTests::malformedWildcardsAreRejected()
{
    QVERIFY(!DomainPattern::parse(QStringLiteral("*.com")).isValid());
    QVERIFY(!DomainPattern::parse(QStringLiteral("exam*ple.com")).isValid());
    QVERIFY(!DomainPattern::parse(QString()).isValid());
}

void TrustAndCertificateTests::settingsRoundTripAndCreateBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("rules.json"));

    TrustSettings settings;
    settings.setStartPage(QUrl(QStringLiteral(
        "https://alice:secret@start.example/private"
    )));

    TrustRuleSettings rule;
    rule.name = QStringLiteral("Example");
    rule.enabled = true;
    rule.mode = TrustMode::SystemOnly;
    rule.domains = {QStringLiteral("example.com"), QStringLiteral("*.example.com")};
    settings.rules().append(rule);

    QString error;
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    QVERIFY(QFile::exists(path));

    settings.rules()[0].name = QStringLiteral("Renamed");
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    QVERIFY(QFile::exists(path + QStringLiteral(".backup")));

    QFile legacy(path);
    QVERIFY(legacy.open(QIODevice::ReadWrite));
    QByteArray legacyContents = legacy.readAll();
    QVERIFY(legacyContents.contains("https://start.example/private"));
    legacyContents.replace(
        "https://start.example/private",
        "https://alice:secret@start.example/private"
    );
    QVERIFY(legacy.resize(0));
    QVERIFY(legacy.seek(0));
    QCOMPARE(legacy.write(legacyContents), legacyContents.size());
    legacy.close();

    TrustSettings loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(
        loaded.startPage(),
        QUrl(QStringLiteral("https://start.example/private"))
    );
    TrustPolicy runtimePolicy;
    QVERIFY2(runtimePolicy.load(path, &error), qPrintable(error));
    QCOMPARE(
        runtimePolicy.startPage(),
        QUrl(QStringLiteral("https://start.example/private"))
    );
    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QVERIFY(!saved.readAll().contains("secret"));
    saved.close();
    QCOMPARE(loaded.rules().size(), 1);
    QCOMPARE(loaded.rules().at(0).name, QStringLiteral("Renamed"));
    QCOMPARE(loaded.rules().at(0).domains, rule.domains);
    QCOMPARE(loaded.rules().at(0).mode, TrustMode::SystemOnly);
}

void TrustAndCertificateTests::overlappingEnabledDomainsAreRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TrustSettings settings;
    TrustRuleSettings first;
    first.name = QStringLiteral("First");
    first.mode = TrustMode::SystemOnly;
    first.domains = {QStringLiteral("*.Example.COM")};
    settings.rules().append(first);

    TrustRuleSettings second;
    second.name = QStringLiteral("Second");
    second.mode = TrustMode::SystemOnly;
    second.domains = {QStringLiteral("login.example.com.")};
    settings.rules().append(second);

    QString error;
    QVERIFY(!settings.validate(directory.filePath(QStringLiteral("rules.json")), &error));
    QVERIFY(error.contains(QStringLiteral("overlaps rule First")));
}

void TrustAndCertificateTests::runtimeRejectsOverlappingEnabledDomains()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("rules.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray contents = R"({
        "version": 1,
        "rules": [
            {
                "name": "Broad",
                "domains": ["*.example.com"],
                "mode": "system-only",
                "anchors": []
            },
            {
                "name": "Narrow",
                "domains": ["login.example.com"],
                "mode": "system-only",
                "anchors": []
            }
        ]
    })";
    QCOMPARE(file.write(contents), contents.size());
    file.close();

    TrustPolicy policy;
    QString error;
    QVERIFY(!policy.load(path, &error));
    QVERIFY(error.contains(QStringLiteral("overlaps rule Broad")));
    QCOMPARE(policy.ruleCount(), 0);
}

void TrustAndCertificateTests::customModeRequiresCertificate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TrustSettings settings;
    TrustRuleSettings rule;
    rule.name = QStringLiteral("Custom");
    rule.mode = TrustMode::CustomOnly;
    rule.domains = {QStringLiteral("example.com")};
    settings.rules().append(rule);

    QString error;
    QVERIFY(!settings.validate(directory.filePath(QStringLiteral("rules.json")), &error));
    QVERIFY(error.contains(QStringLiteral("requires at least one certificate")));
}

void TrustAndCertificateTests::disabledDraftMayBeIncomplete()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TrustSettings settings;
    TrustRuleSettings rule;
    rule.name = QStringLiteral("Draft");
    rule.enabled = false;
    rule.mode = TrustMode::CustomOnly;
    settings.rules().append(rule);

    QString error;
    QVERIFY2(
        settings.validate(directory.filePath(QStringLiteral("rules.json")), &error),
        qPrintable(error)
    );
}

void TrustAndCertificateTests::trustRulesPageLoadsRulesAndSelectsFirst()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("rules.json"));

    TrustSettings settings;
    TrustRuleSettings firstRule;
    firstRule.name = QStringLiteral("First bank");
    firstRule.enabled = false;
    firstRule.mode = TrustMode::SystemPlusCustom;
    settings.rules().append(firstRule);

    TrustRuleSettings secondRule;
    secondRule.name = QStringLiteral("Second bank");
    secondRule.enabled = false;
    secondRule.mode = TrustMode::SystemOnly;
    settings.rules().append(secondRule);

    QString error;
    QVERIFY2(settings.save(path, &error), qPrintable(error));

    TrustRulesSettingsPage page(path);
    QVERIFY2(page.load(&error), qPrintable(error));

    auto *ruleList = page.findChild<QListWidget *>(QStringLiteral("ruleList"));
    QVERIFY(ruleList);
    QCOMPARE(ruleList->count(), 2);
    QCOMPARE(ruleList->currentRow(), 0);
    QVERIFY(ruleList->item(0)->text().startsWith(QStringLiteral("First bank")));
    QVERIFY2(page.validate(&error), qPrintable(error));
}

void TrustAndCertificateTests::certificateRepositoryRollsBackPendingImport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("root.pem"));
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(validatorRootCertificate), validatorRootCertificate.size());
    source.close();

    QString importedPath;
    {
        TrustCertificateRepository repository(
            directory.filePath(QStringLiteral("rules.json"))
        );
        const TrustCertificateImportResult result = repository.importFiles({sourcePath});
        QVERIFY(result.certificateDirectoryError.isEmpty());
        QVERIFY(result.failures.isEmpty());
        QCOMPARE(result.anchors.size(), 1);

        const TrustCertificateInfo info = repository.inspect(result.anchors.first());
        QVERIFY(info.isReadable());
        importedPath = info.absolutePath;
        QVERIFY(QFile::exists(importedPath));
    }

    QVERIFY(!QFile::exists(importedPath));
}

void TrustAndCertificateTests::certificateRepositoryFinalizesOnlyReferencedImports()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstSourcePath = directory.filePath(QStringLiteral("first.pem"));
    const QString secondSourcePath = directory.filePath(QStringLiteral("second.pem"));
    for (const QString &path : {firstSourcePath, secondSourcePath}) {
        QFile source(path);
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(validatorRootCertificate), validatorRootCertificate.size());
    }

    QString referencedPath;
    QString unreferencedPath;
    {
        TrustCertificateRepository repository(
            directory.filePath(QStringLiteral("rules.json"))
        );
        const TrustCertificateImportResult result = repository.importFiles(
            {firstSourcePath, secondSourcePath}
        );
        QVERIFY(result.failures.isEmpty());
        QCOMPARE(result.anchors.size(), 2);
        referencedPath = repository.inspect(result.anchors.first()).absolutePath;
        unreferencedPath = repository.inspect(result.anchors.last()).absolutePath;

        TrustRuleSettings rule;
        rule.anchors.append(result.anchors.first());
        QVERIFY(repository.finalize({rule}).isEmpty());
    }

    QVERIFY(QFile::exists(referencedPath));
    QVERIFY(!QFile::exists(unreferencedPath));
}

void TrustAndCertificateTests::certificateRepositoryRejectsInvalidFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("not-a-certificate.pem"));
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QVERIFY(source.write("not a certificate") > 0);
    source.close();

    TrustCertificateRepository repository(
        directory.filePath(QStringLiteral("rules.json"))
    );
    const TrustCertificateImportResult result = repository.importFiles({sourcePath});
    QVERIFY(result.anchors.isEmpty());
    QCOMPARE(result.failures.size(), 1);
    QCOMPARE(
        result.failures.first().reason,
        TrustCertificateImportFailureReason::InvalidCertificate
    );
}

#if defined(Q_OS_UNIX)
void TrustAndCertificateTests::certificateRepositoryRetainsFailedCleanupForRetry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstSourcePath = directory.filePath(QStringLiteral("first.pem"));
    const QString secondSourcePath = directory.filePath(QStringLiteral("second.pem"));
    for (const QString &path : {firstSourcePath, secondSourcePath}) {
        QFile source(path);
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(validatorRootCertificate), validatorRootCertificate.size());
    }

    TrustCertificateRepository repository(
        directory.filePath(QStringLiteral("rules.json"))
    );
    const TrustCertificateImportResult result = repository.importFiles(
        {firstSourcePath, secondSourcePath}
    );
    QVERIFY(result.failures.isEmpty());
    QCOMPARE(result.anchors.size(), 2);

    const QString referencedPath = repository.inspect(result.anchors.first()).absolutePath;
    const QString unreferencedPath = repository.inspect(result.anchors.last()).absolutePath;
    const QString certificateDirectory = QFileInfo(unreferencedPath).absolutePath();
    const QFileDevice::Permissions originalPermissions = QFile::permissions(
        certificateDirectory
    );
    QVERIFY(QFile::setPermissions(
        certificateDirectory,
        QFileDevice::ReadOwner | QFileDevice::ExeOwner
    ));

    TrustRuleSettings rule;
    rule.anchors.append(result.anchors.first());
    const QStringList failures = repository.finalize({rule});

    QVERIFY(QFile::setPermissions(certificateDirectory, originalPermissions));
    QCOMPARE(failures, QStringList{unreferencedPath});
    QVERIFY(QFile::exists(referencedPath));
    QVERIFY(QFile::exists(unreferencedPath));

    QVERIFY(repository.rollback().isEmpty());
    QVERIFY(QFile::exists(referencedPath));
    QVERIFY(!QFile::exists(unreferencedPath));
}
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
void TrustAndCertificateTests::nativeCertificateValidatorTrustsConfiguredAnchor()
{
    const QList<QSslCertificate> chain = testCertificates(validatorLeafCertificate);
    const QList<QSslCertificate> anchors = testCertificates(validatorRootCertificate);
    QCOMPARE(chain.size(), 1);
    QCOMPARE(anchors.size(), 1);

    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        chain,
        anchors,
        QStringLiteral("validator.panbrowser.test"),
        true
    );
    QVERIFY2(result.trusted, qPrintable(result.explanation));
}

void TrustAndCertificateTests::nativeCertificateValidatorRejectsWrongHostname()
{
    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        testCertificates(validatorLeafCertificate),
        testCertificates(validatorRootCertificate),
        QStringLiteral("wrong.panbrowser.test"),
        true
    );
    QVERIFY(!result.trusted);
    QVERIFY(!result.explanation.isEmpty());
}

void TrustAndCertificateTests::nativeCertificateValidatorRejectsWeakKey()
{
    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        testCertificates(validatorWeakLeafCertificate),
        testCertificates(validatorRootCertificate),
        QStringLiteral("validator.panbrowser.test"),
        true
    );
    QVERIFY(!result.trusted);
    QVERIFY(!result.explanation.isEmpty());
}
#endif

#if defined(Q_OS_LINUX)
void TrustAndCertificateTests::linuxCertificateValidatorSupportsSystemPlusCustom()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile systemCaFile(directory.filePath(QStringLiteral("system-ca.pem")));
    QVERIFY(systemCaFile.open(QIODevice::WriteOnly));
    QCOMPARE(systemCaFile.write(validatorRootCertificate), validatorRootCertificate.size());
    systemCaFile.close();

    const bool hadSystemCaOverride = qEnvironmentVariableIsSet("SSL_CERT_FILE");
    const QByteArray previousSystemCaOverride = qgetenv("SSL_CERT_FILE");
    QVERIFY(qputenv("SSL_CERT_FILE", systemCaFile.fileName().toUtf8()));
    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        testCertificates(validatorLeafCertificate),
        testCertificates(validatorRootCertificate),
        QStringLiteral("validator.panbrowser.test"),
        false
    );
    const bool environmentRestored = hadSystemCaOverride
        ? qputenv("SSL_CERT_FILE", previousSystemCaOverride)
        : qunsetenv("SSL_CERT_FILE");

    QVERIFY(environmentRestored);
    QVERIFY2(result.trusted, qPrintable(result.explanation));
}

void TrustAndCertificateTests::linuxCertificateValidatorBuildsIntermediateChain()
{
    QList<QSslCertificate> chain = testCertificates(linuxValidatorLeafCertificate);
    chain.append(testCertificates(linuxValidatorIntermediateCertificate));
    QCOMPARE(chain.size(), 2);

    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        chain,
        testCertificates(linuxValidatorRootCertificate),
        QStringLiteral("linux-validator.panbrowser.test"),
        true
    );
    QVERIFY2(result.trusted, qPrintable(result.explanation));
}

void TrustAndCertificateTests::linuxCertificateValidatorTrustsIntermediateAnchor()
{
    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        testCertificates(linuxValidatorLeafCertificate),
        testCertificates(linuxValidatorIntermediateCertificate),
        QStringLiteral("linux-validator.panbrowser.test"),
        true
    );
    QVERIFY2(result.trusted, qPrintable(result.explanation));
}

void TrustAndCertificateTests::linuxCertificateValidatorMatchesIpSan()
{
    QList<QSslCertificate> chain = testCertificates(linuxValidatorLeafCertificate);
    chain.append(testCertificates(linuxValidatorIntermediateCertificate));

    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        chain,
        testCertificates(linuxValidatorRootCertificate),
        QStringLiteral("192.0.2.42"),
        true
    );
    QVERIFY2(result.trusted, qPrintable(result.explanation));
}
#endif

int runTrustAndCertificateTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    TrustAndCertificateTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "TrustAndCertificateTests.moc"
