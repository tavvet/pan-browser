#include "AddressCompletionPopup.h"
#include "AddressLineEdit.h"
#include "AddressSuggestion.h"
#include "ApplicationLaunch.h"
#include "BookmarkStore.h"
#include "BrowserPreferences.h"
#include "BrowserShortcut.h"
#include "BrowserDataCleanup.h"
#include "BrowserProfile.h"
#include "CertificateTrustValidator.h"
#include "DownloadHistoryStore.h"
#include "DnsSettings.h"
#include "ExternalNavigationPolicy.h"
#include "FindBar.h"
#include "HistoryStore.h"
#include "HttpAuthenticationController.h"
#include "Localization.h"
#include "PermissionPolicy.h"
#include "ProxySettings.h"
#include "ProxyAuthenticationController.h"
#include "SessionStore.h"
#include "SearchSettings.h"
#include "TrustConfiguration.h"
#include "TrustSettings.h"
#include "WebAppStore.h"
#include "WebAppShortcutManager.h"
#include "WindowPlacement.h"
#include "WindowChrome.h"

#include <QApplication>
#include <QAuthenticator>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QPlatformSurfaceEvent>
#include <QProcess>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTimeZone>
#include <QTranslator>
#include <QUuid>
#include <QWindow>

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
    void integratedChromePreservesBaseMarginsAndAvoidsSystemControls();
    void integratedChromeSurvivesSurfaceAndLayoutTeardown();
    void browserPreferencesValidateStartPage();
    void developerToolsPreferenceDefaultsToDisabled();
    void browserShortcutFallbackMatchesRegisteredKeys();
    void privateDataFilesUseOwnerOnlyPermissions();
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
    void dnsSettingsDefaultToSystemAndIncludeBuiltIns();
    void dnsSettingsRoundTripCustomProvidersAndCreateBackup();
    void dnsSettingsRejectOversizedConfigurationWithoutWriting();
    void dnsSettingsRejectUnsafeTemplatesAndApplyModes();
    void proxySettingsDefaultToSystemAndRoundTrip();
    void proxySettingsRejectUnsafeManualConfiguration();
    void proxySettingsApplyGlobalModes();
    void proxySettingsCompareOnlyEffectiveConfiguration();
    void proxyFailureBlocksWebEngineNetworkSchemes();
    void httpAuthenticationAcceptsCredentialsAndSanitizesDisplay();
    void httpAuthenticationCancelClearsAuthenticator();
    void httpAuthenticationRetriesAndWarnsForPlainHttp();
    void httpAuthenticationPolicyRejectsUnsafePromptContexts();
    void httpAuthenticationRealmDisplayRemovesControlCharacters();
    void proxyAuthenticationUsesSharedCredentialDialog();
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
    void addressCompletionPopupActivatesMouseSelection();
    void addressSuggestionsPreferRelevanceThenBookmarks();
    void findBarSupportsKeyboardNavigationAndCounts();
    void applicationLaunchRequestsAreValidatedAndRoundTrip();
    void applicationLaunchRequestsAreForwardedToPrimaryInstance();
    void webAppManifestIsValidatedAndNormalized();
    void webAppManifestIdUsesStartOrigin();
    void webAppManifestRejectsUnsafeOriginsAndScopes();
    void webAppStoreRoundTripsAndRemovesApps();
    void corruptWebAppStoreIsPreservedAndDisabled();
    void webAppStoreRejectsNonArrayApps();
    void webAppShortcutNamesStayInsideTheirDirectory();
#if defined(Q_OS_MACOS)
    void macWebAppShortcutRoundTrips();
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

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
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

void TrustConfigurationTests::integratedChromePreservesBaseMarginsAndAvoidsSystemControls()
{
#if defined(Q_OS_MACOS)
    QVERIFY(WindowChromeController::platformSupportsIntegratedTitleBar());
    QWidget window;
    WindowChromeController::applyIntegratedTitleBar(&window);
    QVERIFY(window.windowFlags().testFlag(Qt::ExpandedClientAreaHint));
    QVERIFY(window.windowFlags().testFlag(Qt::NoTitleBarBackgroundHint));
    QVERIFY(window.testAttribute(Qt::WA_LayoutOnEntireRect));
    QVERIFY(!window.testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea));
#else
    QVERIFY(!WindowChromeController::platformSupportsIntegratedTitleBar());
    QWidget window;
    WindowChromeController::applyIntegratedTitleBar(&window);
    QVERIFY(!window.windowFlags().testFlag(Qt::ExpandedClientAreaHint));
    QVERIFY(!window.windowFlags().testFlag(Qt::NoTitleBarBackgroundHint));
    QVERIFY(!window.testAttribute(Qt::WA_LayoutOnEntireRect));
#endif

    QCOMPARE(
        integratedChromeContentMargins(
            QMargins(8, 5, 10, 1),
            QMargins(72, 28, 138, 0)
        ),
        QMargins(72, 5, 138, 1)
    );
    QCOMPARE(
        integratedChromeContentMargins(
            QMargins(8, 5, 10, 1),
            QMargins(4, 40, 6, 20)
        ),
        QMargins(8, 5, 10, 1)
    );
    QCOMPARE(
        integratedChromeContentMargins(
            QMargins(8, 5, 10, 1),
            QMargins(0, 28, 0, 0),
            QMargins(82, 0, 0, 0)
        ),
        QMargins(82, 5, 10, 1)
    );
}

void TrustConfigurationTests::integratedChromeSurvivesSurfaceAndLayoutTeardown()
{
    QWidget window;
    auto *container = new QWidget(&window);
    auto *layout = new QHBoxLayout(container);
    auto *dragRegion = new QWidget(container);
    layout->addWidget(dragRegion);
    WindowChromeController controller(&window, layout, {dragRegion});

    QMouseEvent doubleClick(
        QEvent::MouseButtonDblClick,
        QPointF(5, 5),
        QPointF(5, 5),
        QPointF(5, 5),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QVERIFY(QCoreApplication::sendEvent(dragRegion, &doubleClick));
    QVERIFY(window.isMaximized());
    window.showNormal();

    QPlatformSurfaceEvent created(QPlatformSurfaceEvent::SurfaceCreated);
    QCoreApplication::sendEvent(&window, &created);
    QPlatformSurfaceEvent destroying(
        QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed
    );
    QCoreApplication::sendEvent(&window, &destroying);

    delete container;
    QEvent shown(QEvent::Show);
    QCoreApplication::sendEvent(&window, &shown);
    QCoreApplication::processEvents();
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

    preferences.setStartPage(QUrl(QStringLiteral(
        "https://alice:secret@Example.COM/private#section"
    )));
    QVERIFY2(preferences.validate(&error), qPrintable(error));
    QCOMPARE(
        preferences.startPage(),
        QUrl(QStringLiteral("https://example.com/private#section"))
    );
}

void TrustConfigurationTests::developerToolsPreferenceDefaultsToDisabled()
{
    BrowserPreferences preferences;
    QVERIFY(!preferences.developerToolsEnabled());

    preferences.setDeveloperToolsEnabled(true);
    QVERIFY(preferences.developerToolsEnabled());
}

void TrustConfigurationTests::browserShortcutFallbackMatchesRegisteredKeys()
{
    const QList<QKeySequence> shortcuts{
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I),
        QKeySequence(Qt::Key_F12),
    };
    const QKeyEvent developerTools(
        QEvent::KeyPress,
        Qt::Key_I,
        Qt::ControlModifier | Qt::AltModifier
    );
    QVERIFY(BrowserShortcut::matches(developerTools, shortcuts));

    const QKeyEvent functionKey(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);
    QVERIFY(BrowserShortcut::matches(functionKey, shortcuts));

    const QKeyEvent extraModifier(
        QEvent::KeyPress,
        Qt::Key_I,
        Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier
    );
    QVERIFY(!BrowserShortcut::matches(extraModifier, shortcuts));

    const QKeyEvent released(
        QEvent::KeyRelease,
        Qt::Key_I,
        Qt::ControlModifier | Qt::AltModifier
    );
    QVERIFY(!BrowserShortcut::matches(released, shortcuts));
}

void TrustConfigurationTests::privateDataFilesUseOwnerOnlyPermissions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString privateDirectory = directory.filePath(QStringLiteral("private"));

    QString error;
    const QString sessionPath = QDir(privateDirectory).filePath(QStringLiteral("session.json"));
    SessionStore sessionStore(sessionPath);
    BrowserSession session;
    session.tabs = {
        {QUrl(QStringLiteral("https://example.com")), QStringLiteral("Example")},
    };
    QVERIFY2(sessionStore.save(session, &error), qPrintable(error));

    const QString downloadsPath = QDir(privateDirectory).filePath(
        QStringLiteral("downloads.json")
    );
    DownloadHistoryStore downloadStore(downloadsPath);
    QVERIFY2(downloadStore.save({}, &error), qPrintable(error));

    const QString historyPath = QDir(privateDirectory).filePath(
        QStringLiteral("history.sqlite")
    );
    HistoryStore historyStore(historyPath);
    QVERIFY2(historyStore.open(&error), qPrintable(error));

    const QString bookmarksPath = QDir(privateDirectory).filePath(
        QStringLiteral("bookmarks.sqlite")
    );
    BookmarkStore bookmarkStore(bookmarksPath);
    QVERIFY2(bookmarkStore.open(&error), qPrintable(error));

#if defined(Q_OS_UNIX)
    const QFileDevice::Permissions publicPermissions = QFileDevice::ReadGroup
        | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    const auto isOwnerOnly = [publicPermissions](const QString &path) {
        const QFileDevice::Permissions permissions = QFileInfo(path).permissions();
        return permissions.testFlag(QFileDevice::ReadOwner)
            && permissions.testFlag(QFileDevice::WriteOwner)
            && (permissions & publicPermissions) == QFileDevice::Permissions();
    };

    QVERIFY(isOwnerOnly(privateDirectory));
    QVERIFY(QFileInfo(privateDirectory).permissions().testFlag(QFileDevice::ExeOwner));
    QVERIFY(isOwnerOnly(sessionPath));
    QVERIFY(isOwnerOnly(downloadsPath));
    QVERIFY(isOwnerOnly(historyPath));
    QVERIFY(isOwnerOnly(bookmarksPath));
    if (QFileInfo::exists(historyPath + QStringLiteral("-wal")))
        QVERIFY(isOwnerOnly(historyPath + QStringLiteral("-wal")));
    if (QFileInfo::exists(historyPath + QStringLiteral("-shm")))
        QVERIFY(isOwnerOnly(historyPath + QStringLiteral("-shm")));
    if (QFileInfo::exists(bookmarksPath + QStringLiteral("-wal")))
        QVERIFY(isOwnerOnly(bookmarksPath + QStringLiteral("-wal")));
    if (QFileInfo::exists(bookmarksPath + QStringLiteral("-shm")))
        QVERIFY(isOwnerOnly(bookmarksPath + QStringLiteral("-shm")));
#endif
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
        QCoreApplication::translate("DnsSettingsPage", "Secure DNS only"),
        QStringLiteral("Только защищённый DNS")
    );
    QCOMPARE(
        QCoreApplication::translate("ProxySettingsPage", "Manual proxy"),
        QStringLiteral("Ручной прокси")
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
    const QString path = directory.filePath(QStringLiteral("private/session.json"));
    SessionStore store(path);

    BrowserSession source;
    source.activeIndex = 8;
    source.tabs = {
        {QUrl(QStringLiteral("https://alice:secret@one.example/path")), QStringLiteral("One")},
        {QUrl(QStringLiteral("file:///tmp/private")), QStringLiteral("Private")},
        {QUrl(QStringLiteral("http://two.example")), QStringLiteral("Two")},
    };

    QString error;
    QVERIFY2(store.save(source, &error), qPrintable(error));
    const BrowserSession restored = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.tabs.size(), 2);
    QCOMPARE(restored.tabs.at(0).title, QStringLiteral("One"));
    QCOMPARE(restored.tabs.at(0).url, QUrl(QStringLiteral("https://one.example/path")));
    QCOMPARE(restored.tabs.at(1).url, QUrl(QStringLiteral("http://two.example")));
    QCOMPARE(restored.activeIndex, 1);

    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QVERIFY(!saved.readAll().contains("secret"));
    saved.close();

    QFile legacy(path);
    QVERIFY(legacy.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray legacyContents = QByteArrayLiteral(
        R"json({"version":1,"activeIndex":0,"tabs":[{"url":"https://bob:legacy-secret@legacy.example/path","title":"Legacy"}]})json"
    );
    QCOMPARE(legacy.write(legacyContents), legacyContents.size());
    legacy.close();

    const BrowserSession migrated = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(migrated.tabs.size(), 1);
    QCOMPARE(
        migrated.tabs.constFirst().url,
        QUrl(QStringLiteral("https://legacy.example/path"))
    );
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QVERIFY(!saved.readAll().contains("legacy-secret"));
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

void TrustConfigurationTests::dnsSettingsDefaultToSystemAndIncludeBuiltIns()
{
    const DnsSettings settings = DnsSettings::defaults();
    QCOMPARE(settings.mode(), DnsResolutionMode::System);
    QCOMPARE(settings.selectedProviderId(), QStringLiteral("builtin-adguard"));
    QCOMPARE(settings.providers().size(), 6);
    const DnsProvider *adguard = settings.providerById(QStringLiteral("builtin-adguard"));
    QVERIFY(adguard);
    QCOMPARE(adguard->name, QStringLiteral("AdGuard DNS"));
    QCOMPARE(
        adguard->serverTemplates,
        QStringList{QStringLiteral("https://dns.adguard-dns.com/dns-query{?dns}")}
    );
    QString error;
    QVERIFY2(settings.validate(&error), qPrintable(error));
    QVERIFY2(applyDnsSettings(settings, &error), qPrintable(error));
}

void TrustConfigurationTests::dnsSettingsRoundTripCustomProvidersAndCreateBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("dns-settings.json"));

    DnsSettings settings = DnsSettings::defaults();
    DnsProvider custom;
    custom.id = QStringLiteral("custom-test");
    custom.name = QStringLiteral("Private resolver");
    custom.description = QStringLiteral("Test provider");
    custom.serverTemplates = {
        QStringLiteral("https://resolver.example/profile-id/dns-query{?dns}"),
        QStringLiteral("https://backup.example/dns-query"),
    };
    settings.providers().append(custom);
    settings.setSelectedProviderId(custom.id);
    settings.setMode(DnsResolutionMode::SecureOnly);

    QString error;
    QVERIFY2(settings.save(path, &error), qPrintable(error));
#if defined(Q_OS_UNIX)
    const QFileDevice::Permissions permissions = QFileInfo(path).permissions();
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
#endif

    settings.setMode(DnsResolutionMode::SecureWithFallback);
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    const QString backupPath = path + QStringLiteral(".backup");
    QVERIFY(QFileInfo::exists(backupPath));
#if defined(Q_OS_UNIX)
    const QFileDevice::Permissions backupPermissions = QFileInfo(backupPath).permissions();
    QVERIFY(backupPermissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(!backupPermissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!backupPermissions.testFlag(QFileDevice::ReadOther));
#endif

    DnsSettings loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(loaded.mode(), DnsResolutionMode::SecureWithFallback);
    QCOMPARE(loaded.selectedProviderId(), custom.id);
    const DnsProvider *restored = loaded.providerById(custom.id);
    QVERIFY(restored);
    QCOMPARE(restored->name, custom.name);
    QCOMPARE(restored->serverTemplates, custom.serverTemplates);
    QVERIFY(!restored->builtIn);
}

void TrustConfigurationTests::dnsSettingsRejectOversizedConfigurationWithoutWriting()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("dns-settings.json"));

    DnsSettings settings = DnsSettings::defaults();
    QString error;
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    QFile original(path);
    QVERIFY(original.open(QIODevice::ReadOnly));
    const QByteArray originalContents = original.readAll();
    original.close();

    DnsProvider custom;
    custom.id = QStringLiteral("custom-oversized");
    custom.name = QStringLiteral("Oversized resolver");
    custom.description = QString(300 * 1024, QLatin1Char('x'));
    custom.serverTemplates = {QStringLiteral("https://resolver.example/dns-query")};
    settings.providers().append(custom);

    QVERIFY(!settings.save(path, &error));
    QVERIFY(error.contains(QStringLiteral("too large")));
    QFile unchanged(path);
    QVERIFY(unchanged.open(QIODevice::ReadOnly));
    QCOMPARE(unchanged.readAll(), originalContents);
    QVERIFY(!QFileInfo::exists(path + QStringLiteral(".backup")));
}

void TrustConfigurationTests::dnsSettingsRejectUnsafeTemplatesAndApplyModes()
{
    DnsSettings settings = DnsSettings::defaults();
    DnsProvider custom;
    custom.id = QStringLiteral("custom-invalid");
    custom.name = QStringLiteral("Invalid resolver");
    custom.serverTemplates = {QStringLiteral("http://resolver.example/dns-query")};
    settings.providers().append(custom);
    settings.setSelectedProviderId(custom.id);

    QString error;
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("HTTPS")));

    settings.providers().last().serverTemplates = {
        QStringLiteral("https://user:secret@resolver.example/dns-query")
    };
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("credentials")));

    settings.providers().last().serverTemplates = {
        QStringLiteral("https://resolver.example/dns-query{?unsupported}")
    };
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("unsupported")));

    settings.providers().removeLast();
    settings.setSelectedProviderId(QStringLiteral("builtin-adguard"));
    settings.setMode(DnsResolutionMode::SecureWithFallback);
    const bool fallbackApplied = applyDnsSettings(settings, &error);
    settings.setMode(DnsResolutionMode::SecureOnly);
    const bool strictApplied = applyDnsSettings(settings, &error);
    settings.setMode(DnsResolutionMode::System);
    QString resetError;
    const bool systemRestored = applyDnsSettings(settings, &resetError);
    QVERIFY2(fallbackApplied, qPrintable(error));
    QVERIFY2(strictApplied, qPrintable(error));
    QVERIFY2(systemRestored, qPrintable(resetError));
}

void TrustConfigurationTests::proxySettingsDefaultToSystemAndRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("proxy-settings.json"));

    ProxySettings settings = ProxySettings::defaults();
    QCOMPARE(settings.mode(), ProxyMode::System);
    settings.setMode(ProxyMode::Manual);
    settings.setManualType(ManualProxyType::Http);
    settings.setHost(QStringLiteral("proxy.example.com"));
    settings.setPort(3128);
    settings.setUsername(QStringLiteral("alice"));

    QString error;
    QVERIFY2(settings.save(path, &error), qPrintable(error));
#if defined(Q_OS_UNIX)
    const QFileDevice::Permissions permissions = QFileInfo(path).permissions();
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
#endif
    settings.setManualType(ManualProxyType::Socks5);
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    const QString backupPath = path + QStringLiteral(".backup");
    QVERIFY(QFileInfo::exists(backupPath));
#if defined(Q_OS_UNIX)
    const QFileDevice::Permissions backupPermissions = QFileInfo(backupPath).permissions();
    QVERIFY(backupPermissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(!backupPermissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!backupPermissions.testFlag(QFileDevice::ReadOther));
#endif

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray contents = file.readAll();
    QVERIFY(!contents.contains("password"));
    QVERIFY(!contents.contains("secret"));

    ProxySettings loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(loaded, settings);
}

void TrustConfigurationTests::proxySettingsRejectUnsafeManualConfiguration()
{
    ProxySettings settings = ProxySettings::defaults();
    settings.setMode(ProxyMode::Manual);
    QString error;
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("host"), Qt::CaseInsensitive));

    settings.setHost(QStringLiteral("https://proxy.example.com/path"));
    QVERIFY(!settings.validate(&error));
    settings.setHost(QStringLiteral("user@proxy.example.com"));
    QVERIFY(!settings.validate(&error));
    settings.setHost(QStringLiteral("2001:db8::1"));
    settings.setPort(1080);
    QVERIFY2(settings.validate(&error), qPrintable(error));
    settings.setUsername(QStringLiteral("bad\nuser"));
    QVERIFY(!settings.validate(&error));
}

void TrustConfigurationTests::proxySettingsApplyGlobalModes()
{
    QString error;
    ProxySettings settings = ProxySettings::defaults();
    const bool systemApplied = applyProxySettings(settings, &error);
    const bool systemEnabled = QNetworkProxyFactory::usesSystemConfiguration();

    settings.setMode(ProxyMode::NoProxy);
    const bool directApplied = applyProxySettings(settings, &error);
    const bool systemDisabledForDirect = !QNetworkProxyFactory::usesSystemConfiguration();
    const QNetworkProxy directProxy = QNetworkProxy::applicationProxy();

    settings.setMode(ProxyMode::Manual);
    settings.setManualType(ManualProxyType::Http);
    settings.setHost(QStringLiteral("proxy.example.com"));
    settings.setPort(3128);
    settings.setUsername(QStringLiteral("alice"));
    const bool httpApplied = applyProxySettings(settings, &error);
    const QNetworkProxy httpProxy = QNetworkProxy::applicationProxy();

    settings.setManualType(ManualProxyType::Socks5);
    settings.setPort(1080);
    const bool socksApplied = applyProxySettings(settings, &error);
    const QNetworkProxy socksProxy = QNetworkProxy::applicationProxy();

    ProxySettings restore = ProxySettings::defaults();
    QString restoreError;
    const bool restored = applyProxySettings(restore, &restoreError);

    QVERIFY2(systemApplied, qPrintable(error));
    QVERIFY(systemEnabled);
    QVERIFY2(directApplied, qPrintable(error));
    QVERIFY(systemDisabledForDirect);
    QCOMPARE(directProxy.type(), QNetworkProxy::NoProxy);
    QVERIFY2(httpApplied, qPrintable(error));
    QCOMPARE(httpProxy.type(), QNetworkProxy::HttpProxy);
    QCOMPARE(httpProxy.hostName(), QStringLiteral("proxy.example.com"));
    QCOMPARE(httpProxy.port(), quint16(3128));
    QVERIFY(httpProxy.user().isEmpty());
    QVERIFY(httpProxy.password().isEmpty());
    QVERIFY2(socksApplied, qPrintable(error));
    QCOMPARE(socksProxy.type(), QNetworkProxy::Socks5Proxy);
    QCOMPARE(socksProxy.port(), quint16(1080));
    QVERIFY2(restored, qPrintable(restoreError));
}

void TrustConfigurationTests::proxySettingsCompareOnlyEffectiveConfiguration()
{
    QVERIFY(manualProxyAuthenticationSupported(ManualProxyType::Http));
    QVERIFY(!manualProxyAuthenticationSupported(ManualProxyType::Socks5));

    ProxySettings active = ProxySettings::defaults();
    ProxySettings configured = active;
    configured.setHost(QStringLiteral("draft.example.com"));
    configured.setPort(3128);
    configured.setUsername(QStringLiteral("draft-user"));
    QVERIFY(hasSameEffectiveProxyConfiguration(active, configured));

    configured.setMode(ProxyMode::Manual);
    QVERIFY(!hasSameEffectiveProxyConfiguration(active, configured));
    active = configured;
    QVERIFY(hasSameEffectiveProxyConfiguration(active, configured));

    configured.setUsername(QStringLiteral("different-user"));
    QVERIFY(!hasSameEffectiveProxyConfiguration(active, configured));
    active.setManualType(ManualProxyType::Socks5);
    configured.setManualType(ManualProxyType::Socks5);
    QVERIFY(hasSameEffectiveProxyConfiguration(active, configured));

    configured.setPort(1080);
    QVERIFY(!hasSameEffectiveProxyConfiguration(active, configured));
}

void TrustConfigurationTests::proxyFailureBlocksWebEngineNetworkSchemes()
{
    for (const QString &url : {
             QStringLiteral("http://example.com"),
             QStringLiteral("https://example.com"),
             QStringLiteral("ws://example.com/socket"),
             QStringLiteral("wss://example.com/socket"),
         }) {
        QVERIFY(BrowserProfile::shouldBlockForProxyConfigurationError(QUrl(url)));
    }
    QVERIFY(!BrowserProfile::shouldBlockForProxyConfigurationError(
        QUrl(QStringLiteral("about:blank"))
    ));
    QVERIFY(!BrowserProfile::shouldBlockForProxyConfigurationError(
        QUrl(QStringLiteral("data:text/plain,offline"))
    ));
}

void TrustConfigurationTests::httpAuthenticationAcceptsCredentialsAndSanitizesDisplay()
{
    HttpAuthenticationController controller;
    QAuthenticator authenticator;
    authenticator.setUser(QStringLiteral("suggested-user"));
    bool handled = false;
    bool originWasSanitized = false;
    QString suggestedUsername;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        handled = true;
        QString visibleText;
        for (const QLabel *label : dialog->findChildren<QLabel *>())
            visibleText += label->text() + QLatin1Char('\n');
        originWasSanitized = visibleText.contains(QStringLiteral("https://example.com"))
            && !visibleText.contains(QStringLiteral("alice"))
            && !visibleText.contains(QStringLiteral("url-secret"))
            && !visibleText.contains(QStringLiteral("private/path"));

        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        if (!username || !password) {
            dialog->reject();
            return;
        }
        suggestedUsername = username->text();
        username->setText(QStringLiteral("alice"));
        password->setText(QStringLiteral("top-secret"));
        dialog->accept();
    });

    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral(
            "https://alice:url-secret@Example.COM/private/path?token=url-secret"
        )),
        &authenticator
    );

    QVERIFY(handled);
    QVERIFY(originWasSanitized);
    QCOMPARE(suggestedUsername, QStringLiteral("suggested-user"));
    QCOMPARE(authenticator.user(), QStringLiteral("alice"));
    QCOMPARE(authenticator.password(), QStringLiteral("top-secret"));
}

void TrustConfigurationTests::httpAuthenticationCancelClearsAuthenticator()
{
    HttpAuthenticationController controller;
    QAuthenticator authenticator;
    authenticator.setUser(QStringLiteral("stale-user"));
    authenticator.setPassword(QStringLiteral("stale-password"));
    bool handled = false;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        handled = true;
        dialog->reject();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/protected")),
        &authenticator
    );

    QVERIFY(handled);
    QVERIFY(authenticator.isNull());
    QVERIFY(authenticator.user().isEmpty());
    QVERIFY(authenticator.password().isEmpty());
}

void TrustConfigurationTests::httpAuthenticationRetriesAndWarnsForPlainHttp()
{
    HttpAuthenticationController controller;
    QAuthenticator firstAttempt;
    bool firstHandled = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        firstHandled = true;
        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        if (!username || !password) {
            dialog->reject();
            return;
        }
        username->setText(QStringLiteral("wrong-user"));
        password->setText(QStringLiteral("wrong-password"));
        dialog->accept();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("http://example.com/private")),
        &firstAttempt
    );
    QVERIFY(firstHandled);

    QAuthenticator retryAttempt;
    bool retryHandled = false;
    bool retryWasVisible = false;
    bool warningWasVisible = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        retryHandled = true;
        retryWasVisible = dialog->findChild<QLabel *>(QStringLiteral("errorText"));
        warningWasVisible = dialog->findChild<QLabel *>(
            QStringLiteral("insecureTransportWarning")
        );
        dialog->reject();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("http://EXAMPLE.com:80/another-path")),
        &retryAttempt
    );

    QVERIFY(retryHandled);
    QVERIFY(retryWasVisible);
    QVERIFY(warningWasVisible);
    QVERIFY(retryAttempt.isNull());
}

void TrustConfigurationTests::httpAuthenticationPolicyRejectsUnsafePromptContexts()
{
    const QUrl requestUrl(QStringLiteral("https://auth.example.com/private"));
    const QUrl sameOrigin(QStringLiteral("https://AUTH.example.com:443/login"));

    QVERIFY(HttpAuthenticationPolicy::promptAllowed(
        requestUrl,
        sameOrigin,
        true,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        requestUrl,
        sameOrigin,
        false,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        requestUrl,
        sameOrigin,
        true,
        false
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        QUrl(QStringLiteral("https://third-party.example/protected.png")),
        sameOrigin,
        true,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        QUrl(QStringLiteral("http://auth.example.com/private")),
        sameOrigin,
        true,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        QUrl(QStringLiteral("https://auth.example.com:444/private")),
        sameOrigin,
        true,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        requestUrl,
        QUrl(QStringLiteral("about:blank")),
        true,
        true
    ));
}

void TrustConfigurationTests::httpAuthenticationRealmDisplayRemovesControlCharacters()
{
    const QString maliciousRealm = QStringLiteral("  Bank")
        + QChar(0x202e) + QStringLiteral("evil") + QChar(0x202c)
        + QLatin1Char('\n') + QStringLiteral("Admin")
        + QChar(0x2066) + QStringLiteral("test") + QChar(0x2069)
        + QStringLiteral("  ");
    const QString displayed = HttpAuthenticationPolicy::realmForDisplay(maliciousRealm);
    QCOMPARE(displayed, QStringLiteral("Bank evil Admin test"));
    for (const QChar character : displayed) {
        QVERIFY(character.category() != QChar::Other_Control);
        QVERIFY(character.category() != QChar::Other_Format);
        QVERIFY(character.category() != QChar::Separator_Line);
        QVERIFY(character.category() != QChar::Separator_Paragraph);
    }

    const QString truncated = HttpAuthenticationPolicy::realmForDisplay(
        QString(400, QLatin1Char('x'))
    );
    QCOMPARE(truncated.size(), 300);
    QVERIFY(truncated.endsWith(QChar(0x2026)));
}

void TrustConfigurationTests::proxyAuthenticationUsesSharedCredentialDialog()
{
    ProxySettings settings = ProxySettings::defaults();
    settings.setMode(ProxyMode::Manual);
    settings.setManualType(ManualProxyType::Http);
    settings.setHost(QStringLiteral("proxy.example.com"));
    settings.setPort(3128);
    settings.setUsername(QStringLiteral("configured-user"));
    ProxyAuthenticationController controller(settings);
    QAuthenticator authenticator;
    bool handled = false;
    QString suggestedUsername;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        handled = dialog->objectName() == QStringLiteral("proxyAuthenticationDialog");
        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        if (!username || !password) {
            dialog->reject();
            return;
        }
        suggestedUsername = username->text();
        password->setText(QStringLiteral("proxy-password"));
        dialog->accept();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator,
        QStringLiteral("proxy.example.com")
    );

    QVERIFY(handled);
    QCOMPARE(suggestedUsername, QStringLiteral("configured-user"));
    QCOMPARE(authenticator.user(), QStringLiteral("configured-user"));
    QCOMPARE(authenticator.password(), QStringLiteral("proxy-password"));
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

void TrustConfigurationTests::addressCompletionPopupActivatesMouseSelection()
{
    QWidget window;
    window.resize(700, 240);
    AddressLineEdit address(&window);
    address.setGeometry(20, 20, 660, 36);
    window.show();
    QTRY_VERIFY(window.isVisible());

    AddressCompletionPopup popup(&address, &window);
    const QUrl expectedUrl(QStringLiteral("https://example.com/path"));
    AddressSuggestion suggestion;
    suggestion.url = expectedUrl;
    suggestion.title = QStringLiteral("Example");
    suggestion.lastUsedAt = QDateTime::currentDateTimeUtc();
    suggestion.source = AddressSuggestionSource::History;

    QSignalSpy activated(&popup, &AddressCompletionPopup::urlActivated);
    popup.showSuggestions({suggestion});
    QTRY_VERIFY(popup.isVisible());
    auto *list = popup.findChild<QListWidget *>(
        QStringLiteral("addressCompletionList")
    );
    QVERIFY(list);
    QVERIFY(list->count() == 1);
    const QRect itemRect = list->visualItemRect(list->item(0));
    QVERIFY(itemRect.isValid());

    const QPoint globalItemCenter = list->viewport()->mapToGlobal(itemRect.center());
    const QPoint popupLocalCenter = popup.mapFromGlobal(globalItemCenter);
    QMouseEvent windowPress(
        QEvent::MouseButtonPress,
        QPointF(popupLocalCenter),
        QPointF(popupLocalCenter),
        QPointF(globalItemCenter),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QVERIFY(QApplication::sendEvent(popup.windowHandle(), &windowPress));
    QVERIFY2(
        popup.isVisible(),
        "A native popup-window mouse press must not be mistaken for an outside click"
    );

    QTest::mouseClick(
        list->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        itemRect.center()
    );
    QTRY_COMPARE(activated.count(), 1);
    QCOMPARE(activated.takeFirst().at(0).toUrl(), expectedUrl);
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

void TrustConfigurationTests::applicationLaunchRequestsAreValidatedAndRoundTrip()
{
    const ApplicationLaunchRequest activate = ApplicationLaunchRequest::activate();
    QVERIFY(activate.isValid());
    const std::optional<ApplicationLaunchRequest> restoredActivate =
        ApplicationLaunchRequest::fromPayload(activate.toPayload());
    QVERIFY(restoredActivate.has_value());
    QCOMPARE(restoredActivate->command, ApplicationLaunchRequest::Command::Activate);

    const QString appId(64, QLatin1Char('a'));
    const ApplicationLaunchRequest open = ApplicationLaunchRequest::openWebApp(appId);
    QVERIFY(open.isValid());
    const std::optional<ApplicationLaunchRequest> restoredOpen =
        ApplicationLaunchRequest::fromPayload(open.toPayload());
    QVERIFY(restoredOpen.has_value());
    QCOMPARE(restoredOpen->command, ApplicationLaunchRequest::Command::OpenWebApp);
    QCOMPARE(restoredOpen->webAppId, appId);

    const QUrl url(QStringLiteral("https://example.com/account?section=cards"));
    const ApplicationLaunchRequest openUrl = ApplicationLaunchRequest::openUrl(url);
    QVERIFY(openUrl.isValid());
    const std::optional<ApplicationLaunchRequest> restoredUrl =
        ApplicationLaunchRequest::fromPayload(openUrl.toPayload());
    QVERIFY(restoredUrl.has_value());
    QCOMPARE(restoredUrl->command, ApplicationLaunchRequest::Command::OpenUrl);
    QCOMPARE(restoredUrl->url, url);

    QVERIFY(!ApplicationLaunchRequest::openWebApp(QStringLiteral("../unsafe")).isValid());
    QVERIFY(!ApplicationLaunchRequest::openUrl(QUrl(QStringLiteral("file:///etc/passwd"))).isValid());
    QVERIFY(!ApplicationLaunchRequest::fromPayload(
        QByteArrayLiteral("{\"version\":1,\"command\":\"open-web-app\",\"appId\":\"bad\"}")
    ));
    QVERIFY(!ApplicationLaunchRequest::fromPayload(QByteArray(5000, 'x')));
}

void TrustConfigurationTests::applicationLaunchRequestsAreForwardedToPrimaryInstance()
{
    const QString serverName = QStringLiteral("panbrowser-test-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
    );
    QString launchClientPath = QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("PanBrowserLaunchClient")
    );
#if defined(Q_OS_WIN)
    launchClientPath += QStringLiteral(".exe");
#endif
    QVERIFY2(QFileInfo::exists(launchClientPath), qPrintable(launchClientPath));

    QProcess server;
    server.start(
        launchClientPath,
        {QStringLiteral("server"), serverName, QStringLiteral("2")}
    );
    QVERIFY2(server.waitForStarted(3000), qPrintable(server.errorString()));
    QVERIFY2(server.waitForReadyRead(3000), server.readAllStandardError().constData());
    QCOMPARE(server.readLine().trimmed(), QByteArrayLiteral("READY"));

    const QString appId(64, QLatin1Char('c'));
    QProcess webAppClient;
    webAppClient.start(
        launchClientPath,
        {QStringLiteral("client"), serverName, QStringLiteral("open-web-app"), appId}
    );
    QVERIFY2(webAppClient.waitForStarted(3000), qPrintable(webAppClient.errorString()));
    QVERIFY2(webAppClient.waitForFinished(5000), qPrintable(webAppClient.errorString()));
    const QByteArray webAppClientError = webAppClient.readAllStandardError();
    QVERIFY2(
        webAppClient.exitStatus() == QProcess::NormalExit && webAppClient.exitCode() == 0,
        webAppClientError.constData()
    );

    const QUrl url(QStringLiteral("https://example.com/from-secondary"));
    QProcess urlClient;
    urlClient.start(
        launchClientPath,
        {
            QStringLiteral("client"),
            serverName,
            QStringLiteral("open-url"),
            url.toString(QUrl::FullyEncoded),
        }
    );
    QVERIFY2(urlClient.waitForStarted(3000), qPrintable(urlClient.errorString()));
    QVERIFY2(urlClient.waitForFinished(5000), qPrintable(urlClient.errorString()));
    const QByteArray urlClientError = urlClient.readAllStandardError();
    QVERIFY2(
        urlClient.exitStatus() == QProcess::NormalExit && urlClient.exitCode() == 0,
        urlClientError.constData()
    );

    QVERIFY2(server.waitForFinished(5000), server.readAllStandardError().constData());
    const QList<QByteArray> receivedPayloads = server.readAllStandardOutput()
        .split('\n');
    QVERIFY(receivedPayloads.size() >= 2);
    QCOMPARE(
        receivedPayloads.at(0).trimmed(),
        ApplicationLaunchRequest::openWebApp(appId).toPayload().toBase64()
    );
    QCOMPARE(
        receivedPayloads.at(1).trimmed(),
        ApplicationLaunchRequest::openUrl(url).toPayload().toBase64()
    );
}

void TrustConfigurationTests::webAppManifestIsValidatedAndNormalized()
{
    const QByteArray manifest = QByteArray(
        "{\"id\":\"/apps/mail/\","
        "\"name\":\"  Example   Mail  \","
        "\"short_name\":\"Mail\","
        "\"description\":\"A focused mail app\","
        "\"start_url\":\"./inbox?source=install#ignored\","
        "\"scope\":\"./\","
        "\"display\":\"minimal-ui\"}"
    );
    QString error;
    const std::optional<WebApp> app = WebAppStore::parseManifest(
        manifest,
        QUrl(QStringLiteral("https://example.com/apps/mail/manifest.webmanifest")),
        QUrl(QStringLiteral("https://example.com/apps/mail/welcome")),
        QStringLiteral("Fallback"),
        &error
    );
    QVERIFY2(app.has_value(), qPrintable(error));
    QCOMPARE(app->id.size(), 64);
    QCOMPARE(app->name, QStringLiteral("Example Mail"));
    QCOMPARE(app->shortName, QStringLiteral("Mail"));
    QCOMPARE(app->displayMode, QStringLiteral("minimal-ui"));
    QCOMPARE(app->startUrl, QUrl(QStringLiteral("https://example.com/apps/mail/inbox?source=install")));
    QCOMPARE(app->scope, QUrl(QStringLiteral("https://example.com/apps/mail/")));
    QVERIFY(WebAppStore::containsUrl(
        *app,
        QUrl(QStringLiteral("https://example.com/apps/mail/settings"))
    ));
    QVERIFY(!WebAppStore::containsUrl(
        *app,
        QUrl(QStringLiteral("https://example.com/apps/calendar/"))
    ));
    QVERIFY(!WebAppStore::containsUrl(
        *app,
        QUrl(QStringLiteral("http://example.com/apps/mail/"))
    ));
}

void TrustConfigurationTests::webAppManifestIdUsesStartOrigin()
{
    const QByteArray manifest = QByteArrayLiteral(
        "{\"id\":\"mail\",\"name\":\"Mail\","
        "\"start_url\":\"/apps/mail/start\",\"scope\":\"/apps/mail/\"}"
    );
    QString error;
    const std::optional<WebApp> first = WebAppStore::parseManifest(
        manifest,
        QUrl(QStringLiteral("https://example.com/assets/first/manifest.webmanifest")),
        QUrl(QStringLiteral("https://example.com/apps/mail/")),
        QString(),
        &error
    );
    QVERIFY2(first.has_value(), qPrintable(error));

    const std::optional<WebApp> second = WebAppStore::parseManifest(
        manifest,
        QUrl(QStringLiteral("https://example.com/other/manifest.webmanifest")),
        QUrl(QStringLiteral("https://example.com/apps/mail/")),
        QString(),
        &error
    );
    QVERIFY2(second.has_value(), qPrintable(error));
    QCOMPARE(first->id, second->id);
}

void TrustConfigurationTests::webAppManifestRejectsUnsafeOriginsAndScopes()
{
    QString error;
    QVERIFY(!WebAppStore::parseManifest(
        QByteArray("{\"name\":\"Bad\",\"start_url\":\"https://evil.example/\"}"),
        QUrl(QStringLiteral("https://example.com/manifest.webmanifest")),
        QUrl(QStringLiteral("https://example.com/")),
        QString(),
        &error
    ));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!WebAppStore::parseManifest(
        QByteArray("{\"name\":\"Bad\",\"scope\":\"https://evil.example/\"}"),
        QUrl(QStringLiteral("https://example.com/manifest.webmanifest")),
        QUrl(QStringLiteral("https://example.com/")),
        QString(),
        &error
    ));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!WebAppStore::parseManifest(
        QByteArray("{\"name\":\"Bad\"}"),
        QUrl(QStringLiteral("http://example.com/manifest.webmanifest")),
        QUrl(QStringLiteral("http://example.com/")),
        QString(),
        &error
    ));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!WebAppStore::parseManifest(
        QByteArray("{\"name\":\"Bad\"}"),
        QUrl(QStringLiteral("https://user:secret@example.com/manifest.webmanifest")),
        QUrl(QStringLiteral("https://example.com/")),
        QString(),
        &error
    ));
    QVERIFY(!error.isEmpty());
}

void TrustConfigurationTests::webAppStoreRoundTripsAndRemovesApps()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("web-apps.json"));
    WebAppStore store(path);
    QString error;
    QVERIFY2(store.load(&error), qPrintable(error));

    std::optional<WebApp> app = WebAppStore::parseManifest(
        QByteArray("{\"name\":\"Example App\",\"start_url\":\"/app/home\",\"scope\":\"/app/\"}"),
        QUrl(QStringLiteral("https://example.com/app/manifest.webmanifest")),
        QUrl(QStringLiteral("https://example.com/app/")),
        QString(),
        &error
    );
    QVERIFY2(app.has_value(), qPrintable(error));
    app->iconPng = QByteArrayLiteral("not-a-real-png-but-bounded");
    QVERIFY2(store.install(*app, &error), qPrintable(error));
    QCOMPARE(store.apps().size(), 1);

    WebAppStore restored(path);
    QVERIFY2(restored.load(&error), qPrintable(error));
    QCOMPARE(restored.apps().size(), 1);
    const std::optional<WebApp> saved = restored.app(app->id);
    QVERIFY(saved.has_value());
    QCOMPARE(saved->name, app->name);
    QCOMPARE(saved->startUrl, app->startUrl);
    QCOMPARE(saved->scope, app->scope);
    QCOMPARE(saved->iconPng, app->iconPng);

    QVERIFY2(restored.remove(app->id, &error), qPrintable(error));
    QVERIFY(restored.apps().isEmpty());
    WebAppStore empty(path);
    QVERIFY2(empty.load(&error), qPrintable(error));
    QVERIFY(empty.apps().isEmpty());
}

void TrustConfigurationTests::corruptWebAppStoreIsPreservedAndDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("web-apps.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray corrupt = QByteArrayLiteral("{not valid json");
    QCOMPARE(file.write(corrupt), corrupt.size());
    file.close();

    WebAppStore store(path);
    QString error;
    QVERIFY(!store.load(&error));
    QVERIFY(!store.isAvailable());
    QVERIFY(!error.isEmpty());

    WebApp app;
    app.id = QString(64, QLatin1Char('a'));
    app.name = QStringLiteral("Should not overwrite");
    app.startUrl = QUrl(QStringLiteral("https://example.com/app/"));
    app.scope = app.startUrl;
    app.manifestUrl = QUrl(QStringLiteral("https://example.com/app/manifest.webmanifest"));
    QVERIFY(!store.install(app, &error));

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), corrupt);
}

void TrustConfigurationTests::webAppStoreRejectsNonArrayApps()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("web-apps.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray invalid = QByteArrayLiteral("{\"version\":1,\"apps\":{}}");
    QCOMPARE(file.write(invalid), invalid.size());
    file.close();

    WebAppStore store(path);
    QString error;
    QVERIFY(!store.load(&error));
    QVERIFY(!store.isAvailable());
    QVERIFY(!error.isEmpty());

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), invalid);
}

void TrustConfigurationTests::webAppShortcutNamesStayInsideTheirDirectory()
{
    const QString unsafeName = QStringLiteral("../../Bank:/")
        + QChar(0x202e)
        + QString(100, QLatin1Char('x'));
    const QString safeName = WebAppShortcutManager::safeShortcutName(unsafeName);
    QVERIFY(!safeName.contains(QLatin1Char('/')));
    QVERIFY(!safeName.contains(QLatin1Char(':')));
    QVERIFY(!safeName.contains(QChar(0x202e)));
    QVERIFY(!safeName.startsWith(QLatin1Char('.')));
    QVERIFY(safeName.size() <= 80);

    const QString emoji = QString::fromUtf8("\xF0\x9F\x9A\x80");
    const QString unicodeBoundaryName = QString(79, QLatin1Char('x'))
        + emoji
        + QStringLiteral("tail");
    const QString unicodeBoundarySafeName =
        WebAppShortcutManager::safeShortcutName(unicodeBoundaryName);
    QCOMPARE(unicodeBoundarySafeName.size(), 79);
    QCOMPARE(QString::fromUtf8(unicodeBoundarySafeName.toUtf8()), unicodeBoundarySafeName);

    WebApp app;
    app.id = QString(64, QLatin1Char('a'));
    app.name = unsafeName;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WebAppShortcutManager manager(directory.path(), directory.filePath(QStringLiteral("host.app")));
    QCOMPARE(
        QFileInfo(manager.shortcutPath(app)).absolutePath(),
        QDir(directory.path()).absolutePath()
    );
}

#if defined(Q_OS_MACOS)
void TrustConfigurationTests::macWebAppShortcutRoundTrips()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString hostBundle = QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("PanBrowser.app")
    );
    WebAppShortcutManager manager(directory.path(), hostBundle);
    QVERIFY(manager.isSupported());

    WebApp app;
    app.id = QString(64, QLatin1Char('b'));
    app.name = QStringLiteral("Shortcut Test");
    app.shortName = QStringLiteral("Test");
    QString error;
    QVERIFY2(manager.createOrUpdate(app, &error), qPrintable(error));
    QVERIFY(manager.shortcutExists(app));
    QVERIFY(QFileInfo(QDir(manager.shortcutPath(app)).filePath(
        QStringLiteral("Contents/MacOS/PanBrowserWebAppLauncher")
    )).isExecutable());
    QVERIFY2(manager.remove(app, &error), qPrintable(error));
    QVERIFY(!manager.shortcutExists(app));
}
#endif

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
