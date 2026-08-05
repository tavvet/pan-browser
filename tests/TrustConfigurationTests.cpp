#include "AddressLineEdit.h"
#include "AddressSuggestion.h"
#include "BookmarkStore.h"
#include "BrowserPreferences.h"
#include "BrowserDataCleanup.h"
#include "CertificateTrustValidator.h"
#include "DownloadHistoryStore.h"
#include "ExternalNavigationPolicy.h"
#include "FindBar.h"
#include "HistoryStore.h"
#include "Localization.h"
#include "PermissionPolicy.h"
#include "SessionStore.h"
#include "SearchSettings.h"
#include "TrustConfiguration.h"
#include "TrustSettings.h"
#include "WindowPlacement.h"

#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QTranslator>

class TrustConfigurationTests final : public QObject {
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
    void visibleWindowPlacementIsPreserved();
    void disconnectedScreenFallsBackToPrimary();
    void inaccessibleTitleAreaIsRecentered();
    void oversizedWindowFitsSmallerResolution();
    void browserPreferencesValidateStartPage();
    void interfaceLanguagePreferenceRoundTrips();
    void interfaceLanguageSettingsParsing();
    void systemInterfaceLanguageUsesFirstSupportedLanguage();
    void unsupportedSystemInterfaceLanguageFallsBackToEnglish();
    void embeddedTranslationCatalogsLoad();
    void bookmarksRoundTripNormalizeAndSearch();
    void bookmarksCanBeEditedAndRemoved();
    void bookmarkEditRejectsDuplicateAddress();
    void corruptBookmarksArePreservedAndDisabled();
    void sessionRoundTripFiltersInvalidUrls();
    void invalidSessionFileFailsClosed();
    void managedDataCleanupStaysInsideRoot();
    void downloadHistoryRoundTripAndLimit();
    void sensitivePermissionsRequireSecureOrigin();
    void unsupportedPermissionsAreDenied();
    void webSchemesStayInsideBrowser();
    void externalSchemesRequireMainFrameConfirmation();
    void dangerousLocalSchemesAreBlocked();
    void popupGeometryIsVisibleAndUsable();
    void searchSettingsRoundTripAndCreateBackup();
    void searchSettingsRejectInvalidTemplatesAndDuplicates();
    void addressInputDistinguishesUrlsAndSearches();
    void addressInputSupportsSearchKeywords();
    void searchTermsAreEncodedExactlyOnce();
    void historySanitizesAndStoresSuccessfulWebVisits();
    void historySuggestionsPreferRelevanceThenRecency();
    void historySuggestionsConsiderOlderExactMatches();
    void historyCanDeleteIndividualVisitsAndClearAll();
    void corruptHistoryIsPreservedAndDisabled();
    void ghostCompletionAcceptsOnlyAddressPrefixes();
    void addressSuggestionsPreferRelevanceThenBookmarks();
    void findBarSupportsKeyboardNavigationAndCounts();
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

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
namespace {

const QByteArray validatorRootCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIIC5zCCAc+gAwIBAgIJALlhRYB2pp4EMA0GCSqGSIb3DQEBCwUAMB8xHTAbBgNV
BAMMFFBhbkJyb3dzZXItVGVzdC1Sb290MB4XDTI2MDgwNTEwNTc1M1oXDTQ2MDcz
MTEwNTc1M1owHzEdMBsGA1UEAwwUUGFuQnJvd3Nlci1UZXN0LVJvb3QwggEiMA0G
CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDtlJTlqxR72qb989fKSFrC3AE38U9R
3RghQLKNGpLD9NBQnG5Y3JGqkpRsjinOgkdoXG0c0lkhWWYk11iJObCwjSpwU7J4
Rv/DXLB+D0r8mHGWLMogU1nu76Qsc2XSztW7OUgq77CUJFp6ck3pcIyxlI/WwSzn
h0i0C4mXCGcLaRgJR/gBbTUz64t4oBWSN+mZ5pkGqYGDQzSnIpzP4WeO4uIjcsB5
HRrVRtHc0NSfdnxU2sVucbvxsYmaD7ER0jW3k2y9rOUj20/yyscYFDg0ZsZDazSk
ZeOFFzFa3OBsO+EY2GMRFjcSFMP7wfw6uDS8LldydxNk2+Wcjprw2HKVAgMBAAGj
JjAkMBIGA1UdEwEB/wQIMAYBAf8CAQAwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3
DQEBCwUAA4IBAQBij4DvRdDZN9z+JMO7I5PUlhT1rnYQRrhmJh3Iy9qOhIuiTyX5
2XktVEdqdO2XaaQOPsUFutSfuDch/2AHo9Ku7W2UdbByb75i46SFlND5qJfVbxLe
j9sTE+nu0XeDn4dgceGXUgF+6Dnm3wzIhzIKOhxOv4KnEoj1ECJCMYVK8RIS6wBs
fz9DznJnQeO0Lvpl/SdS1eM7Sw9yAWMb2m5iu4BctOub82wEn2TpsvkJavadUAvo
gRHLzHtXvQm19EHsje4f+Hxoq7KfRD6lB9YttkBcFH6Ra1EoZ0+bYsxSCPkcD7bf
vl6zW/rfqixwoVC+A9jBhWR4L75f9Iky2/kQ
-----END CERTIFICATE-----
)CERT");

const QByteArray validatorLeafCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIIDITCCAgmgAwIBAgIJAPaaGMM/NUXkMA0GCSqGSIb3DQEBCwUAMB8xHTAbBgNV
BAMMFFBhbkJyb3dzZXItVGVzdC1Sb290MB4XDTI2MDgwNTEwNTc1M1oXDTQ2MDcz
MTEwNTc1M1owJDEiMCAGA1UEAwwZdmFsaWRhdG9yLnBhbmJyb3dzZXIudGVzdDCC
ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKF1bpqXC2kPpZHO40KCMs0Z
lemBL1TiXlI3EMATpbe0ElrfH52euxFQBqd03N/qzInNkQzo+E0J0suNiqqABHxO
ApcL3chr9CBNWTIHIraJdDN5rT/wunjaKmqOTd1z+MNSVppuDd+uyz6OOnpWy55U
qaqZLCQ5rrljys68Leh3GeYsYUZqgI6BBvurnwAya4v4I7AIfCou/BfdisC68Ztx
UkaKE6tP5rLQUAIad6ZZ+XOQs0dJMsO0Twg6q4bk3cJNF8MxLyzLbTu36deYnqHN
NUdsBJftSwWptsMy7L1mTXG8ltHWAlLUBnRO4qwrLn4O6sq7eMU9fKqFWSyUsncC
AwEAAaNbMFkwDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMCBaAwEwYDVR0lBAww
CgYIKwYBBQUHAwEwJAYDVR0RBB0wG4IZdmFsaWRhdG9yLnBhbmJyb3dzZXIudGVz
dDANBgkqhkiG9w0BAQsFAAOCAQEANrgnwy8lg/hoctsb3KQhimJvK6mk3sBZD55r
os8IQxKpXE82ThB9ErovZxUQXQEFSBUrpm+fzKgnw6yWExgUEupQ1GWOKlQVcA/E
XO7d8K5/LK7iugQq6Fg04uGaKYNk0U4DIf2MgE2MzVjwbhAgWmGpWWkDA2a1Uool
3IURhK8ckIicbLlYhWySiHrasNnomHAAgfgGDVKlaniLzScxDPTDFyKm6aiu2EOM
T9Ytdqvku0TuB5SG72H1+FAPK3Hh2rrMiQ9F5LDUrQiupevL5LhUnpgPPVnN/g0z
9Mmb1t5xhRozbtGwVDirlE4ii9KGLO5RwBYI4HuYnvC2EyqtmQ==
-----END CERTIFICATE-----
)CERT");

const QByteArray validatorWeakLeafCertificate = QByteArrayLiteral(R"CERT(
-----BEGIN CERTIFICATE-----
MIICnTCCAYWgAwIBAgIJAPaaGMM/NUXlMA0GCSqGSIb3DQEBCwUAMB8xHTAbBgNV
BAMMFFBhbkJyb3dzZXItVGVzdC1Sb290MB4XDTI2MDgwNTExMTg0MFoXDTQ2MDcz
MTExMTg0MFowJDEiMCAGA1UEAwwZdmFsaWRhdG9yLnBhbmJyb3dzZXIudGVzdDCB
nzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA6T6A60CuAoGBxGS0zj5PjnnRf0am
Cc3rVHDf1fy1QiX34AN5kYhjYrsu1Bbdyfx8PFqIVTdmxAbeSMI5LI286J/g6rAQ
S7iBmwWnYtTWHtMHFUm7uQRzrKbwiN60wEu+3RvJIChxm+V+IKpqp5j8IO/DcuhO
/z/mgS0KjpaUXrkCAwEAAaNbMFkwDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMC
BaAwEwYDVR0lBAwwCgYIKwYBBQUHAwEwJAYDVR0RBB0wG4IZdmFsaWRhdG9yLnBh
bmJyb3dzZXIudGVzdDANBgkqhkiG9w0BAQsFAAOCAQEApBZWvURYeYXnsnkXufwl
09BqDi0VE0XaCJRViwaFk4bUatmDOAY6Ppq6jAptzwKmJUxabjG1qEKPJEbgjbH0
MwWFs7TKcf4Ijur3/+yBI7N1VeRSZZfnjszUYtPaNspcGKeU9l9wAUdz4OeXEsdm
eJkkeBuqtQrqsgvZFRXWSxDIbwQ8axsf5O/qGVO7/k7Sj8wtlC0YqHGnoCZCziOb
jjSi4ubOJE86RRh+mbXbGeV+WPfKS6UeI+CLAqIgFycJBKXcxTr50eZIN2+jcCk2
Dn48apJHhbEOUgUhjTB5MSNfZYy0xl4ihVs5vHBYGpVFURm/G5nfsfaQciv8iAoe
/w==
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

QList<QSslCertificate> testCertificates(const QByteArray &pem)
{
    return QSslCertificate::fromData(pem, QSsl::Pem);
}

} // namespace
#endif

void TrustConfigurationTests::exactDomainIsCaseInsensitive()
{
    const DomainPattern pattern = DomainPattern::parse(QStringLiteral("Example.COM."));
    QVERIFY(pattern.isValid());
    QVERIFY(pattern.matches(QStringLiteral("example.com")));
    QVERIFY(pattern.matches(QStringLiteral("EXAMPLE.COM.")));
    QVERIFY(!pattern.matches(QStringLiteral("www.example.com")));
}

void TrustConfigurationTests::wildcardMatchesSubdomainsOnly()
{
    const DomainPattern pattern = DomainPattern::parse(QStringLiteral("*.example.com"));
    QVERIFY(pattern.isValid());
    QVERIFY(pattern.matches(QStringLiteral("www.example.com")));
    QVERIFY(pattern.matches(QStringLiteral("api.internal.example.com")));
    QVERIFY(!pattern.matches(QStringLiteral("example.com")));
    QVERIFY(!pattern.matches(QStringLiteral("notexample.com")));
}

void TrustConfigurationTests::malformedWildcardsAreRejected()
{
    QVERIFY(!DomainPattern::parse(QStringLiteral("*.com")).isValid());
    QVERIFY(!DomainPattern::parse(QStringLiteral("exam*ple.com")).isValid());
    QVERIFY(!DomainPattern::parse(QString()).isValid());
}

void TrustConfigurationTests::settingsRoundTripAndCreateBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("rules.json"));

    TrustSettings settings;
    settings.setStartPage(QUrl(QStringLiteral("https://start.example")));

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

    TrustSettings loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(loaded.startPage(), QUrl(QStringLiteral("https://start.example")));
    QCOMPARE(loaded.rules().size(), 1);
    QCOMPARE(loaded.rules().at(0).name, QStringLiteral("Renamed"));
    QCOMPARE(loaded.rules().at(0).domains, rule.domains);
    QCOMPARE(loaded.rules().at(0).mode, TrustMode::SystemOnly);
}

void TrustConfigurationTests::overlappingEnabledDomainsAreRejected()
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

void TrustConfigurationTests::runtimeRejectsOverlappingEnabledDomains()
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

void TrustConfigurationTests::customModeRequiresCertificate()
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

void TrustConfigurationTests::disabledDraftMayBeIncomplete()
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

void TrustConfigurationTests::visibleWindowPlacementIsPreserved()
{
    const QRect screen(0, 0, 1440, 900);
    const QRect window(120, 80, 1180, 760);
    QCOMPARE(adjustedWindowGeometry(window, {screen}, screen), window);
}

void TrustConfigurationTests::disconnectedScreenFallsBackToPrimary()
{
    const QRect primaryScreen(0, 0, 1440, 900);
    const QRect windowOnRemovedScreen(1800, 80, 1000, 700);
    const QRect restored = adjustedWindowGeometry(
        windowOnRemovedScreen,
        {primaryScreen},
        primaryScreen
    );

    QVERIFY(primaryScreen.contains(restored));
    QCOMPARE(restored.size(), windowOnRemovedScreen.size());
    QCOMPARE(restored.center(), primaryScreen.center());
}

void TrustConfigurationTests::inaccessibleTitleAreaIsRecentered()
{
    const QRect screen(0, 0, 1440, 900);
    const QRect windowWithBodyOnly(-900, -500, 1000, 700);
    const QRect restored = adjustedWindowGeometry(windowWithBodyOnly, {screen}, screen);

    QVERIFY(restored != windowWithBodyOnly);
    QVERIFY(screen.contains(restored));
    QCOMPARE(restored.center(), screen.center());
}

void TrustConfigurationTests::oversizedWindowFitsSmallerResolution()
{
    const QRect smallerScreen(0, 0, 1280, 720);
    const QRect largeWindow(80, 40, 2560, 1400);
    const QRect restored = adjustedWindowGeometry(largeWindow, {smallerScreen}, smallerScreen);

    QCOMPARE(restored, smallerScreen);
}

void TrustConfigurationTests::browserPreferencesValidateStartPage()
{
    BrowserPreferences preferences;
    QString error;
    preferences.setStartPage(QUrl(QStringLiteral("file:///tmp/private")));
    QVERIFY(!preferences.validate(&error));
    QVERIFY(error.contains(QStringLiteral("HTTP or HTTPS")));

    preferences.setStartPage(QUrl(QStringLiteral("https://example.com/start")));
    QVERIFY2(preferences.validate(&error), qPrintable(error));
}

void TrustConfigurationTests::interfaceLanguagePreferenceRoundTrips()
{
    BrowserPreferences preferences;
    QCOMPARE(preferences.interfaceLanguage(), InterfaceLanguage::System);
    preferences.setInterfaceLanguage(InterfaceLanguage::Russian);
    QCOMPARE(preferences.interfaceLanguage(), InterfaceLanguage::Russian);

    QCOMPARE(
        LocalizationManager::resolveLanguage(InterfaceLanguage::English, {QStringLiteral("ru-RU")}),
        QStringLiteral("en")
    );
    QCOMPARE(
        LocalizationManager::resolveLanguage(InterfaceLanguage::Russian, {QStringLiteral("en-US")}),
        QStringLiteral("ru")
    );
}

void TrustConfigurationTests::interfaceLanguageSettingsParsing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("preferences.ini")), QSettings::IniFormat);

    QCOMPARE(
        BrowserPreferences::loadInterfaceLanguage(settings),
        InterfaceLanguage::System
    );
    settings.setValue(QStringLiteral("Browser/language"), QStringLiteral("ru"));
    QCOMPARE(
        BrowserPreferences::loadInterfaceLanguage(settings),
        InterfaceLanguage::Russian
    );
    settings.setValue(QStringLiteral("Browser/language"), QStringLiteral("en"));
    QCOMPARE(
        BrowserPreferences::loadInterfaceLanguage(settings),
        InterfaceLanguage::English
    );
    settings.setValue(QStringLiteral("Browser/language"), QStringLiteral("unsupported"));
    QCOMPARE(
        BrowserPreferences::loadInterfaceLanguage(settings),
        InterfaceLanguage::English
    );
}

void TrustConfigurationTests::systemInterfaceLanguageUsesFirstSupportedLanguage()
{
    QCOMPARE(
        LocalizationManager::resolveLanguage(
            InterfaceLanguage::System,
            {QStringLiteral("fi-FI"), QStringLiteral("ru-RU"), QStringLiteral("en-US")}
        ),
        QStringLiteral("ru")
    );
    QCOMPARE(
        LocalizationManager::resolveLanguage(
            InterfaceLanguage::System,
            {QStringLiteral("en-GB"), QStringLiteral("ru-RU")}
        ),
        QStringLiteral("en")
    );
}

void TrustConfigurationTests::unsupportedSystemInterfaceLanguageFallsBackToEnglish()
{
    QCOMPARE(
        LocalizationManager::resolveLanguage(
            InterfaceLanguage::System,
            {QStringLiteral("fi-FI"), QStringLiteral("sv-SE")}
        ),
        QStringLiteral("en")
    );
    QCOMPARE(
        LocalizationManager::resolveLanguage(InterfaceLanguage::System, {}),
        QStringLiteral("en")
    );
}

void TrustConfigurationTests::embeddedTranslationCatalogsLoad()
{
    QTranslator russian;
    QVERIFY(russian.load(QStringLiteral(":/i18n/panbrowser_ru.qm")));
    QVERIFY(QCoreApplication::installTranslator(&russian));
    QCOMPARE(
        QCoreApplication::translate("SettingsDialog", "Settings"),
        QStringLiteral("Настройки")
    );
    QCOMPARE(
        QCoreApplication::translate("QPlatformTheme", "Cancel"),
        QStringLiteral("Отмена")
    );
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 1),
        QStringLiteral("1 закладка")
    );
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 2),
        QStringLiteral("2 закладки")
    );
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 5),
        QStringLiteral("5 закладок")
    );
    QVERIFY(QCoreApplication::removeTranslator(&russian));

    QTranslator english;
    QVERIFY(english.load(QStringLiteral(":/i18n/panbrowser_en.qm")));
    QVERIFY(QCoreApplication::installTranslator(&english));
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 1),
        QStringLiteral("1 bookmark")
    );
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 2),
        QStringLiteral("2 bookmarks")
    );
    QVERIFY(QCoreApplication::removeTranslator(&english));
}

void TrustConfigurationTests::bookmarksRoundTripNormalizeAndSearch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookmarkStore store(directory.filePath(QStringLiteral("bookmarks.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));

    const QUrl source(
        QStringLiteral("https://user:secret@Example.COM:443/docs?q=one#section")
    );
    QVERIFY2(store.addOrUpdate(
        source,
        QStringLiteral("Documentation"),
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const std::optional<Bookmark> saved = store.bookmarkForUrl(source, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(saved.has_value());
    QCOMPARE(saved->url.scheme(), QStringLiteral("https"));
    QCOMPARE(saved->url.host(), QStringLiteral("example.com"));
    QCOMPARE(saved->url.port(), -1);
    QCOMPARE(saved->url.userName(), QString());
    QCOMPARE(saved->url.password(), QString());
    QCOMPARE(saved->url.query(), QStringLiteral("q=one"));
    QCOMPARE(saved->url.fragment(), QStringLiteral("section"));
    QCOMPARE(saved->title, QStringLiteral("Documentation"));

    QVERIFY2(store.addOrUpdate(
        saved->url,
        QStringLiteral("Updated documentation"),
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    const QList<Bookmark> matches = store.bookmarks(QStringLiteral("updated"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.first().title, QStringLiteral("Updated documentation"));
    QCOMPARE(matches.first().createdAt, QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC));
    QCOMPARE(matches.first().updatedAt, QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC));
}

void TrustConfigurationTests::bookmarksCanBeEditedAndRemoved()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookmarkStore store(directory.filePath(QStringLiteral("bookmarks.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));
    QVERIFY2(store.addOrUpdate(
        QUrl(QStringLiteral("https://one.example")),
        QStringLiteral("One"),
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.addOrUpdate(
        QUrl(QStringLiteral("https://two.example")),
        QStringLiteral("Two"),
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const std::optional<Bookmark> first = store.bookmarkForUrl(
        QUrl(QStringLiteral("https://one.example/")),
        &error
    );
    QVERIFY(first.has_value());
    QVERIFY2(store.update(
        first->id,
        QUrl(QStringLiteral("https://renamed.example/path#part")),
        QStringLiteral("Renamed"),
        QDateTime::fromSecsSinceEpoch(3000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY(!store.bookmarkForUrl(QUrl(QStringLiteral("https://one.example")), &error));
    QVERIFY(store.bookmarkForUrl(QUrl(QStringLiteral("https://renamed.example/path#part")), &error));

    QVERIFY2(store.removeUrl(QUrl(QStringLiteral("https://two.example")), &error), qPrintable(error));
    QCOMPARE(store.bookmarks({}, &error).size(), 1);
    QVERIFY2(store.clear(&error), qPrintable(error));
    QVERIFY(store.bookmarks({}, &error).isEmpty());
}

void TrustConfigurationTests::bookmarkEditRejectsDuplicateAddress()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookmarkStore store(directory.filePath(QStringLiteral("bookmarks.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));
    QVERIFY2(store.addOrUpdate(
        QUrl(QStringLiteral("https://one.example/")),
        QStringLiteral("One"),
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.addOrUpdate(
        QUrl(QStringLiteral("https://two.example/")),
        QStringLiteral("Two"),
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const std::optional<Bookmark> first = store.bookmarkForUrl(
        QUrl(QStringLiteral("https://one.example/")),
        &error
    );
    QVERIFY(first.has_value());
    error.clear();
    QVERIFY(!store.update(
        first->id,
        QUrl(QStringLiteral("https://two.example/")),
        QStringLiteral("Duplicate"),
        QDateTime::fromSecsSinceEpoch(3000, QTimeZone::UTC),
        &error
    ));
    QCOMPARE(error, QStringLiteral("A bookmark for this address already exists"));

    error.clear();
    const QList<Bookmark> bookmarks = store.bookmarks({}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(bookmarks.size(), 2);
    const std::optional<Bookmark> unchanged = store.bookmarkForUrl(
        QUrl(QStringLiteral("https://one.example/")),
        &error
    );
    QVERIFY(unchanged.has_value());
    QCOMPARE(unchanged->title, QStringLiteral("One"));
}

void TrustConfigurationTests::corruptBookmarksArePreservedAndDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("bookmarks.sqlite"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("not a sqlite database"), 21);
    file.close();

    BookmarkStore store(path);
    QString error;
    QVERIFY(!store.open(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!store.isOpen());
    QVERIFY(QFile::exists(path));
    QCOMPARE(QFileInfo(path).size(), 21);
}

void TrustConfigurationTests::sessionRoundTripFiltersInvalidUrls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.json"));
    SessionStore store(path);

    BrowserSession source;
    source.activeIndex = 8;
    source.tabs = {
        {QUrl(QStringLiteral("https://one.example/path")), QStringLiteral("One")},
        {QUrl(QStringLiteral("file:///tmp/private")), QStringLiteral("Private")},
        {QUrl(QStringLiteral("http://two.example")), QStringLiteral("Two")},
    };

    QString error;
    QVERIFY2(store.save(source, &error), qPrintable(error));
    const BrowserSession restored = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.tabs.size(), 2);
    QCOMPARE(restored.tabs.at(0).title, QStringLiteral("One"));
    QCOMPARE(restored.tabs.at(1).url, QUrl(QStringLiteral("http://two.example")));
    QCOMPARE(restored.activeIndex, 1);
}

void TrustConfigurationTests::invalidSessionFileFailsClosed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not json");
    file.close();

    SessionStore store(path);
    QString error;
    const BrowserSession restored = store.load(&error);
    QVERIFY(restored.tabs.isEmpty());
    QVERIFY(!error.isEmpty());
}

void TrustConfigurationTests::managedDataCleanupStaysInsideRoot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString managedRoot = directory.filePath(QStringLiteral("managed"));
    const QString profile = QDir(managedRoot).filePath(QStringLiteral("WebEngine/Profile"));
    QVERIFY(QDir().mkpath(profile));
    QFile marker(QDir(profile).filePath(QStringLiteral("marker")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();

    QString error;
    QVERIFY2(removeManagedDataDirectory(profile, managedRoot, &error), qPrintable(error));
    QVERIFY(!QFile::exists(profile));

    QVERIFY(!removeManagedDataDirectory(managedRoot, managedRoot, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!removeManagedDataDirectory(
        QDir(managedRoot).filePath(QStringLiteral("../outside")),
        managedRoot,
        &error
    ));
}

void TrustConfigurationTests::downloadHistoryRoundTripAndLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DownloadHistoryStore store(directory.filePath(QStringLiteral("downloads.json")));

    QList<DownloadRecord> records;
    for (int index = 0; index < DownloadHistoryStore::maximumRecords + 5; ++index) {
        DownloadRecord record;
        record.id = QString::number(index);
        record.filePath = directory.filePath(QStringLiteral("file-%1.pdf").arg(index));
        record.fileName = QStringLiteral("file-%1.pdf").arg(index);
        record.sourceHost = QStringLiteral("files.example");
        record.startedAt = QDateTime::fromSecsSinceEpoch(1000 + index, QTimeZone::UTC);
        record.finishedAt = QDateTime::fromSecsSinceEpoch(1100 + index, QTimeZone::UTC);
        record.receivedBytes = index * 100;
        record.totalBytes = index * 100;
        record.status = DownloadStatus::Completed;
        records.append(record);
    }

    QString error;
    QVERIFY2(store.save(records, &error), qPrintable(error));
    const QList<DownloadRecord> restored = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.size(), DownloadHistoryStore::maximumRecords);
    QCOMPARE(restored.first().id, QStringLiteral("0"));
    QCOMPARE(restored.last().id, QString::number(DownloadHistoryStore::maximumRecords - 1));
    QCOMPARE(restored.at(5).sourceHost, QStringLiteral("files.example"));
    QCOMPARE(restored.at(5).status, DownloadStatus::Completed);
    QCOMPARE(restored.at(5).receivedBytes, 500);
}

void TrustConfigurationTests::sensitivePermissionsRequireSecureOrigin()
{
    const QUrl secureOrigin(QStringLiteral("https://bank.example"));
    const QUrl insecureOrigin(QStringLiteral("http://bank.example"));

    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Camera),
        PermissionDisposition::Prompt
    );
    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Microphone),
        PermissionDisposition::Prompt
    );
    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::CameraAndMicrophone),
        PermissionDisposition::Prompt
    );
    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Location),
        PermissionDisposition::Prompt
    );
    QCOMPARE(
        permissionDisposition(insecureOrigin, BrowserPermissionKind::Camera),
        PermissionDisposition::Deny
    );
}

void TrustConfigurationTests::unsupportedPermissionsAreDenied()
{
    const QUrl secureOrigin(QStringLiteral("https://bank.example"));

    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Notifications),
        PermissionDisposition::Deny
    );
    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Other),
        PermissionDisposition::Deny
    );
    QVERIFY(!permissionTitle(BrowserPermissionKind::Location).isEmpty());
    QVERIFY(!permissionDescription(BrowserPermissionKind::Location).isEmpty());
}

void TrustConfigurationTests::webSchemesStayInsideBrowser()
{
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("https://example.com")), true),
        ExternalNavigationDisposition::Browse
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("http://example.com")), false),
        ExternalNavigationDisposition::Browse
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("blob:https://example.com/id")), true),
        ExternalNavigationDisposition::Browse
    );
}

void TrustConfigurationTests::externalSchemesRequireMainFrameConfirmation()
{
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("mailto:user@example.com")), true),
        ExternalNavigationDisposition::Prompt
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("bank-app://open/payment")), true),
        ExternalNavigationDisposition::Prompt
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("bank-app://open/payment")), false),
        ExternalNavigationDisposition::Block
    );
}

void TrustConfigurationTests::dangerousLocalSchemesAreBlocked()
{
    QCOMPARE(
        externalNavigationDisposition(QUrl::fromLocalFile(QStringLiteral("/tmp/file")), true),
        ExternalNavigationDisposition::Block
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("javascript:alert(1)")), true),
        ExternalNavigationDisposition::Block
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(), true),
        ExternalNavigationDisposition::Block
    );
}

void TrustConfigurationTests::popupGeometryIsVisibleAndUsable()
{
    const QRect screen(0, 0, 1440, 900);
    const QRect owner(120, 80, 1180, 760);

    QCOMPARE(
        popupWindowGeometry(QRect(), owner, {screen}, screen),
        QRect(152, 112, 1100, 760)
    );
    QCOMPARE(
        popupWindowGeometry(QRect(2000, 100, 200, 100), owner, {screen}, screen),
        QRect(360, 190, 720, 520)
    );
}

void TrustConfigurationTests::searchSettingsRoundTripAndCreateBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("search-engines.json"));

    SearchSettings settings = SearchSettings::defaults();
    SearchEngineSettings custom;
    custom.id = QStringLiteral("custom-example");
    custom.name = QStringLiteral("Example Search");
    custom.keyword = QStringLiteral("ex");
    custom.urlTemplate = QStringLiteral("https://search.example/?q={searchTerms}");
    settings.engines().append(custom);
    settings.setDefaultEngineId(custom.id);

    QString error;
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    settings.setDefaultEngineId(QStringLiteral("builtin-duckduckgo"));
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    QVERIFY(QFile::exists(path + QStringLiteral(".backup")));

    SearchSettings loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(loaded.engines().size(), 5);
    QCOMPARE(loaded.defaultEngine()->name, QStringLiteral("DuckDuckGo"));
    QCOMPARE(loaded.engineForKeyword(QStringLiteral("@ex"))->name, QStringLiteral("Example Search"));
}

void TrustConfigurationTests::searchSettingsRejectInvalidTemplatesAndDuplicates()
{
    SearchSettings settings = SearchSettings::defaults();
    QString error;

    settings.engines()[0].urlTemplate = QStringLiteral("https://example.com/search");
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("{searchTerms}")));

    settings = SearchSettings::defaults();
    settings.engines()[1].keyword = settings.engines()[0].keyword;
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Duplicate search keyword")));

    settings = SearchSettings::defaults();
    settings.engines()[0].urlTemplate = QStringLiteral("https://user:secret@example.com/?q={searchTerms}");
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("credentials")));
}

void TrustConfigurationTests::addressInputDistinguishesUrlsAndSearches()
{
    const SearchSettings settings = SearchSettings::defaults();

    ResolvedAddressInput result = resolveAddressInput(QStringLiteral("example.com/path"), settings);
    QCOMPARE(result.kind, AddressInputKind::Navigate);
    QCOMPARE(result.url.host(), QStringLiteral("example.com"));

    result = resolveAddressInput(QStringLiteral("localhost:8080/test"), settings);
    QCOMPARE(result.kind, AddressInputKind::Navigate);
    QCOMPARE(result.url.port(), 8080);

    result = resolveAddressInput(QStringLiteral("как войти в банк"), settings);
    QCOMPARE(result.kind, AddressInputKind::Search);
    QCOMPARE(result.url.host(), QStringLiteral("duckduckgo.com"));

    result = resolveAddressInput(QStringLiteral("singleword"), settings);
    QCOMPARE(result.kind, AddressInputKind::Search);

    result = resolveAddressInput(QStringLiteral("mailto:user@example.com"), settings);
    QCOMPARE(result.kind, AddressInputKind::Error);
}

void TrustConfigurationTests::addressInputSupportsSearchKeywords()
{
    SearchSettings settings = SearchSettings::defaults();
    ResolvedAddressInput result = resolveAddressInput(QStringLiteral("@g qt webengine"), settings);
    QCOMPARE(result.kind, AddressInputKind::Search);
    QCOMPARE(result.url.host(), QStringLiteral("www.google.com"));
    QCOMPARE(result.engineId, QStringLiteral("builtin-google"));

    result = resolveAddressInput(QStringLiteral("? example.com"), settings);
    QCOMPARE(result.kind, AddressInputKind::Search);
    QCOMPARE(result.url.host(), QStringLiteral("duckduckgo.com"));

    result = resolveAddressInput(QStringLiteral("@missing query"), settings);
    QCOMPARE(result.kind, AddressInputKind::Error);
    QVERIFY(result.error.contains(QStringLiteral("Unknown")));
}

void TrustConfigurationTests::searchTermsAreEncodedExactlyOnce()
{
    const SearchSettings settings = SearchSettings::defaults();
    QString error;
    const QUrl url = settings.searchUrl(QStringLiteral("C++ 100% русский"), {}, &error);
    QVERIFY2(url.isValid(), qPrintable(error));
    const QString encoded = url.toString(QUrl::FullyEncoded);
    QVERIFY(encoded.contains(QStringLiteral("C%2B%2B%20100%25%20")));
    QVERIFY(!encoded.contains(QStringLiteral("%252B")));
}

void TrustConfigurationTests::historySanitizesAndStoresSuccessfulWebVisits()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));

    const QUrl source(QStringLiteral("https://user:secret@example.com/path?q=one#token"));
    QVERIFY2(store.recordVisit(
        source,
        QStringLiteral("Example Page"),
        HistoryTransition::Typed,
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const QList<HistoryVisit> visits = store.visits({}, 10, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(visits.size(), 1);
    QCOMPARE(visits.first().url.userName(), QString());
    QCOMPARE(visits.first().url.password(), QString());
    QCOMPARE(visits.first().url.fragment(), QString());
    QCOMPARE(visits.first().url.query(), QStringLiteral("q=one"));
    QCOMPARE(visits.first().title, QStringLiteral("Example Page"));

    QVERIFY(!store.recordVisit(
        QUrl(QStringLiteral("file:///tmp/private")),
        QStringLiteral("Private"),
        HistoryTransition::Other,
        QDateTime::currentDateTimeUtc(),
        &error
    ));
}

void TrustConfigurationTests::historySuggestionsPreferRelevanceThenRecency()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));

    QVERIFY2(store.recordVisit(
        QUrl(QStringLiteral("https://example.com/old")),
        QStringLiteral("Example"),
        HistoryTransition::Typed,
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.recordVisit(
        QUrl(QStringLiteral("https://recent.test/path?source=example")),
        QStringLiteral("Recent page"),
        HistoryTransition::Link,
        QDateTime::fromSecsSinceEpoch(3000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.recordVisit(
        QUrl(QStringLiteral("https://example.net/new")),
        QStringLiteral("New example"),
        HistoryTransition::Link,
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const QList<HistorySuggestion> suggestions = store.suggestions(
        QStringLiteral("example"),
        8,
        &error
    );
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(suggestions.size(), 3);
    QCOMPARE(suggestions.at(0).url.host(), QStringLiteral("example.net"));
    QCOMPARE(suggestions.at(1).url.host(), QStringLiteral("example.com"));
    QCOMPARE(suggestions.at(2).url.host(), QStringLiteral("recent.test"));
}

void TrustConfigurationTests::historySuggestionsConsiderOlderExactMatches()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));

    const QUrl exactUrl(QStringLiteral("https://needle.example/"));
    QVERIFY2(store.recordVisit(
        exactUrl,
        QStringLiteral("Old exact match"),
        HistoryTransition::Typed,
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    for (int index = 0; index < 201; ++index) {
        QVERIFY2(store.recordVisit(
            QUrl(QStringLiteral("https://recent-%1.test/?q=needle.example").arg(index)),
            QStringLiteral("Recent weak match"),
            HistoryTransition::Link,
            QDateTime::fromSecsSinceEpoch(2000 + index, QTimeZone::UTC),
            &error
        ), qPrintable(error));
    }

    const QList<HistorySuggestion> suggestions = store.suggestions(
        QStringLiteral("needle.example"),
        8,
        &error
    );
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(suggestions.size(), 8);
    QCOMPARE(suggestions.first().url, exactUrl);
}

void TrustConfigurationTests::historyCanDeleteIndividualVisitsAndClearAll()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));
    const QUrl url(QStringLiteral("https://example.com/"));
    QVERIFY2(store.recordVisit(
        url,
        QStringLiteral("First title"),
        HistoryTransition::Typed,
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.recordVisit(
        url,
        QStringLiteral("Updated title"),
        HistoryTransition::Link,
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    QList<HistoryVisit> visits = store.visits({}, 10, &error);
    QCOMPARE(visits.size(), 2);
    QVERIFY2(store.removeVisits({visits.first().id}, &error), qPrintable(error));
    visits = store.visits({}, 10, &error);
    QCOMPARE(visits.size(), 1);
    const QList<HistorySuggestion> suggestions = store.suggestions(
        QStringLiteral("example"),
        8,
        &error
    );
    QCOMPARE(suggestions.size(), 1);
    QCOMPARE(suggestions.first().visitCount, 1);
    QCOMPARE(suggestions.first().typedCount, 1);

    QVERIFY2(store.clear(&error), qPrintable(error));
    QVERIFY(store.visits({}, 10, &error).isEmpty());
}

void TrustConfigurationTests::corruptHistoryIsPreservedAndDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("history.sqlite"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("not a sqlite database"), 21);
    file.close();

    HistoryStore store(path);
    QString error;
    QVERIFY(!store.open(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!store.isOpen());
    QVERIFY(QFile::exists(path));
    QCOMPARE(QFileInfo(path).size(), 21);
}

void TrustConfigurationTests::ghostCompletionAcceptsOnlyAddressPrefixes()
{
    AddressLineEdit address;
    const QUrl url(QStringLiteral("https://sports.example/news"));
    address.setText(QStringLiteral("spo"));
    address.setGhostCompletion(QStringLiteral("sports.example/news"), url);
    QVERIFY(address.hasGhostCompletion());
    QCOMPARE(address.ghostCompletionUrl(), url);
    QVERIFY(address.acceptGhostCompletion());
    QCOMPARE(address.text(), QStringLiteral("sports.example/news"));

    address.setText(QStringLiteral("news"));
    address.setGhostCompletion(QStringLiteral("sports.example/news"), url);
    QVERIFY(!address.hasGhostCompletion());
    QVERIFY(!address.acceptGhostCompletion());
    QCOMPARE(address.text(), QStringLiteral("news"));
}

void TrustConfigurationTests::addressSuggestionsPreferRelevanceThenBookmarks()
{
    const QDateTime old = QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC);
    const QDateTime recent = QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC);
    const QList<AddressSuggestion> candidates = {
        {
            QUrl(QStringLiteral("https://recent.test/?q=example.com")),
            QStringLiteral("Recent weak bookmark"),
            recent,
            AddressSuggestionSource::Bookmark,
        },
        {
            QUrl(QStringLiteral("https://example.com/")),
            QStringLiteral("Exact history result"),
            old,
            AddressSuggestionSource::History,
        },
        {
            QUrl(QStringLiteral("https://example.net/history")),
            QStringLiteral("Prefix history"),
            recent,
            AddressSuggestionSource::History,
        },
        {
            QUrl(QStringLiteral("https://example.net/bookmark")),
            QStringLiteral("Prefix bookmark"),
            old,
            AddressSuggestionSource::Bookmark,
        },
        {
            QUrl(QStringLiteral("https://example.net/bookmark")),
            QStringLiteral("Duplicate history"),
            recent,
            AddressSuggestionSource::History,
        },
    };

    const QList<AddressSuggestion> ranked = rankedAddressSuggestions(
        candidates,
        QStringLiteral("example.com"),
        8
    );
    QCOMPARE(ranked.first().url.host(), QStringLiteral("example.com"));

    const QList<AddressSuggestion> prefixRanked = rankedAddressSuggestions(
        candidates,
        QStringLiteral("example.net"),
        8
    );
    QCOMPARE(prefixRanked.size(), 2);
    QCOMPARE(prefixRanked.first().source, AddressSuggestionSource::Bookmark);
    QCOMPARE(prefixRanked.first().title, QStringLiteral("Prefix bookmark"));
}

void TrustConfigurationTests::findBarSupportsKeyboardNavigationAndCounts()
{
    FindBar bar;
    auto *input = bar.findChild<QLineEdit *>(QStringLiteral("findInput"));
    auto *result = bar.findChild<QLabel *>(QStringLiteral("findResult"));
    QVERIFY(input);
    QVERIFY(result);

    QSignalSpy querySpy(&bar, &FindBar::queryChanged);
    QSignalSpy navigationSpy(&bar, &FindBar::navigationRequested);
    QSignalSpy closeSpy(&bar, &FindBar::closeRequested);
    input->setText(QStringLiteral("needle"));
    QCOMPARE(querySpy.count(), 1);

    bar.setResults(2, 5);
    QCOMPARE(result->text(), QStringLiteral("2 of 5"));
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(navigationSpy.takeFirst().at(0).toBool(), false);

    QTest::keyClick(input, Qt::Key_Return, Qt::ShiftModifier);
    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(navigationSpy.takeFirst().at(0).toBool(), true);

    QTest::keyClick(input, Qt::Key_Escape);
    QCOMPARE(closeSpy.count(), 1);
    bar.setResults(0, 0);
    QCOMPARE(result->text(), QStringLiteral("No matches"));
}

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
void TrustConfigurationTests::nativeCertificateValidatorTrustsConfiguredAnchor()
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

void TrustConfigurationTests::nativeCertificateValidatorRejectsWrongHostname()
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

void TrustConfigurationTests::nativeCertificateValidatorRejectsWeakKey()
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
void TrustConfigurationTests::linuxCertificateValidatorSupportsSystemPlusCustom()
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

void TrustConfigurationTests::linuxCertificateValidatorBuildsIntermediateChain()
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

void TrustConfigurationTests::linuxCertificateValidatorTrustsIntermediateAnchor()
{
    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        testCertificates(linuxValidatorLeafCertificate),
        testCertificates(linuxValidatorIntermediateCertificate),
        QStringLiteral("linux-validator.panbrowser.test"),
        true
    );
    QVERIFY2(result.trusted, qPrintable(result.explanation));
}

void TrustConfigurationTests::linuxCertificateValidatorMatchesIpSan()
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

QTEST_MAIN(TrustConfigurationTests)
#include "TrustConfigurationTests.moc"
