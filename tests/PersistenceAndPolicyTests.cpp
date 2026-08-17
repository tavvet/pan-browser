#include "PanBrowserTestCommon.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QMutex>
#include <QMutexLocker>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>
#include <QWebEngineProfileBuilder>
#include <QWebEngineScriptCollection>

#include <memory>

namespace {

// Public test-only TLS material. Never use this private key outside this test.
const QByteArray votTestCertificatePem = QByteArrayLiteral(R"PEM(
-----BEGIN CERTIFICATE-----
MIIDCzCCAfOgAwIBAgIJAJMfmW/W62FAMA0GCSqGSIb3DQEBCwUAMCMxITAfBgNV
BAMMGFBhbkJyb3dzZXIgVk9UIFRlc3QgUm9vdDAeFw0yNjA4MTcwOTE2MDlaFw0z
NjA4MTQwOTE2MDlaMCMxITAfBgNVBAMMGFBhbkJyb3dzZXIgVk9UIFRlc3QgUm9v
dDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAOmXgOAxdRR1ouy7vYt6
yRwUQelFEjtQAw2J9/TGFnZWAMni0zn2NDKlPMkvufPVk+czVp/CV1x9xjmdArN6
zuk/MxwNdDmc3MMdOzV0e7tiWhv2Q0sU93cPoOYJplpcUJfW1TD31nPCqNYF9m8X
ihHJp2yJC8enD8gM1ForQ0zIV3IlmAy3tPDDRMK6AsO0IGdqm+caLtKKJTUDKEUx
4K7C+bWoaamdKKyxhtN7bmK1jciT3yRWrC2wFIA4700Xrb0zKVI/yuZ/ij9/626d
XZDewiMv6bzS/+EG1A0nDgCoq7yGToApfQEc1RfnJIMCk++6o37JNGVUYmPUwGZf
sI0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYD
VR0OBBYEFLB4B5AvsJkdAv2PN345ApePv9YYMA0GCSqGSIb3DQEBCwUAA4IBAQCq
w/lPVV1lX8v2R/eLxerfjsHAo28qPIdETqqNil+lxjXsj571Xp802QS1Z3aj0ng3
+lhHu/V6Q7+k5dN2oJKUcdEDd1ZFDPsa445L7qp14TYwppcTxX6W1c5oTGlxT2K9
zcNqUgXf6YEiEjF8vHdVPPvYTzECHb36zApiV4VgdtB9BNDlYAYjgmXCt+YM907x
D8tyvYd+B3PDO4CiZA24i0ABgAkpZsb1dY/1JFH6cRkAoDfjZPzTnAd6HkZGq1Rv
cooumxdbqkG7UJdnnNr3/mI+eAH3oO/m06VMqqlVWftupNHwagqzm+ZSWJpKLlg2
mH4W08LHQWjaFxjw0Ci7
-----END CERTIFICATE-----
)PEM");

const QByteArray votTestServerCertificatePem = QByteArrayLiteral(R"PEM(
-----BEGIN CERTIFICATE-----
MIIDGDCCAgCgAwIBAgIJAPcoJB62ho3uMA0GCSqGSIb3DQEBCwUAMCMxITAfBgNV
BAMMGFBhbkJyb3dzZXIgVk9UIFRlc3QgUm9vdDAeFw0yNjA4MTcwOTMzMzlaFw0z
NjA4MTQwOTMzMzlaMBQxEjAQBgNVBAMMCWxvY2FsaG9zdDCCASIwDQYJKoZIhvcN
AQEBBQADggEPADCCAQoCggEBALEjdSdoAIfyttMG7eCzP5Ths1KZWI4+iRN2EEKn
kRQcDDCmJJW5sgsb8NSsnLOt+pWtgOjZElNdeU0CzCzbI9SBZzCf5+w9bVfbNzWZ
1oLLTtkHUhaqbRPdK6n/L0RZtucFQJBl2+hAS6Lv2XgtvpOyMyUG5Mgktj3hhoBI
X6hhX8mWf24/eH5qQQhaP8xQl1/tx+3ac78xADqH2E+jQmoVd+BW33xl+3ifPbhj
JSmZ+gpGLsRf+xO3z7eq8Ig5FayZmfp1usDLQOGw2dkKNz8LkrH4UCEpCSHOEgR4
TO4x8oT/G1tp47oWIYuXz9CfBeonEzm7Ev0ftumklThixB8CAwEAAaNeMFwwGgYD
VR0RBBMwEYIJbG9jYWxob3N0hwR/AAABMB0GA1UdDgQWBBSwYsGDyP3C94WEadF3
Yi5c6rOs5jAfBgNVHSMEGDAWgBSweAeQL7CZHQL9jzd+OQKXj7/WGDANBgkqhkiG
9w0BAQsFAAOCAQEAbytYDCmCYZrmmyAO088ESwweJy6vE9R0ANkRPJCF+CkVMhtr
XKLx/p0UC+4V0nKg42UAK5rJMgVOdqxqJh36Wswy8nbd9v3LqfUBtNCARlnPDDp+
0fyb1YwIl1trR7dyn2PhTcLlMEmzCt3TSGBZuMv+9qQaJCPDAJ79NptKpADYbE/l
w8AO9dllq8+sv2h8hw2XNT15uYMQfn71isz2lLmxUATShPZqpYJw5M3ZamQ4O/Iu
kgKNWii5SYhoOpn+dbE3SlY/MvL0UZ6Vo8W/WkJqcqBwTp0kRoNH1clb6HjYXO/c
HPa/R5C1/BOONMoSctvLT78xemFkVy87LCXszA==
-----END CERTIFICATE-----
)PEM");

const QByteArray votTestPrivateKeyPem = QByteArrayLiteral(R"PEM(
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCxI3UnaACH8rbT
Bu3gsz+U4bNSmViOPokTdhBCp5EUHAwwpiSVubILG/DUrJyzrfqVrYDo2RJTXXlN
Asws2yPUgWcwn+fsPW1X2zc1mdaCy07ZB1IWqm0T3Sup/y9EWbbnBUCQZdvoQEui
79l4Lb6TsjMlBuTIJLY94YaASF+oYV/Jln9uP3h+akEIWj/MUJdf7cft2nO/MQA6
h9hPo0JqFXfgVt98Zft4nz24YyUpmfoKRi7EX/sTt8+3qvCIORWsmZn6dbrAy0Dh
sNnZCjc/C5Kx+FAhKQkhzhIEeEzuMfKE/xtbaeO6FiGLl8/QnwXqJxM5uxL9H7bp
pJU4YsQfAgMBAAECggEAHPjerX5OL+4bxvIoMAtBR/mOWeEo4cMKqnsx96Tujqpz
t/VEoJSJsVV1k2NUEfNPu/Fv1yXB4I62S0k82c1R0+0CUqqkxTfazXbWVdb+hc/4
6nhPDdP8GTagcKKDOZuu1aQ/Rh7S0+7IMDMoj4Zz2DZaNSEmhDG0+PQNCqhLtP9K
aw7Wa90IrT9NCpePVvJs0nRxV2S9GC97NhrwodLqBZ9Rl4GR0996J23/TRxl3RgP
u/GpL7saFbNmBwxaSjsIvAkl+P7PigQLxQqJd6fMTaWSDbgBUXI8/PH3/phyuK9I
vEyBs20Fe/4iBh+/1ra1C7qJQGBbt4vX5WxQNvX/GQKBgQDf7N0epq7Ovtev8a0m
TJgmkDF7s9v9JJozvbKTrO9IOqm5+72bmKBeOKoExfTNISO+18FPhyKQH0VmEpVE
DQHtJQGuJ9v93Sdap53YhYGZxoJqMzs6z/D12FFrDJrGMuI8dJByHrY5r2xdfPUr
bsILB7eaItQifc0Gw9zW5ZTIdQKBgQDKgvdj6lPeTsZEJAMmq40TMZO/5RzOhLLa
A+MH6DydVwKtFWlM7kXpgoojZvBKetCJS0atk9UHxjK/X5/T6/t/9ogupLPyzAGS
qKT3ARuRLzfwmjPT6lGMRefQ3n2/pCs1TEbfTze26gZDfWKpMg+FleamFjgE95t/
CcK8mOtnwwKBgB6m4sHOWUFtuEKaV3RVqcIlnNBtF2D4gK8yeV5jnsGJXjBaOGMz
KkibgxJyModd0PHPwONtARsvXKWTR6FwEmJu3WMEi8WdX0S2ixHXfJ0bIkD9UE9F
pZtRiBuaNxmIX5Wc8yTb9V/CFphZgYn3eFJgNQ7BU76A7+7MIs+7z3j9AoGBAMV/
GBlfK8/Ab2eA33nVEE5JqVgZ7xARJgOsirrpaEPh2YBHQ1x9e70RS/repzVbtQhQ
W2toovdj+ZXdghfBKpNPMNycT8pX3qjSw58Ie4QJ0rpZCHaBLGpqunteoLBHQMRH
0U5zCVjfvqAPJirv4WdcG4aaYKOnfwRT1pDraZjVAoGBANr6moZmdCmGYWiaz7E2
81kLbtGOTFPYk/pyHzhuyl5MfwKNzN1CPdeF0KibPvM0L3OAw6OKvjS+VMk5S7AY
rfA8pMAUMDVvJfjWSe3gZNGYArn58hyZB1VBXDLLaR6d4GizNDbOiQw5b/Hg/AtP
cUgKJed8CwzZyI8RZ6CidHH6
-----END PRIVATE KEY-----
)PEM");

} // namespace

class PersistenceAndPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void privateDataFilesUseOwnerOnlyPermissions();
    void settingsSaveTransactionRestoresFiles();
    void siteDomainsUsePublicSuffixBoundaries();
    void domainPatternMatcherUsesLabelBoundaries();
    void bundledConnectionPresetsAreValidAndRespectUserRules();
    void crossDomainPolicyEvaluatesRulesAndExceptions();
    void compiledCrossDomainPolicyPreservesPrecedence();
    void crossDomainSourceResolutionFailsClosed();
    void crossDomainSettingsRoundTripAndRejectCorruption();
    void videoTranslationSettingsRoundTripAndValidateSource();
    void votUserscriptPackageRejectsUnverifiedFilesAndMatchesUrls();
    void votUserscriptPackageLoadsOfficialFileWhenProvided();
    void votUserscriptStorageIsScriptScopedAndPersistent();
    void votChromiumTransportExtensionIsPreparedSafely();
    void votChromiumTransportLoadsInBrowserProfile();
    void votChromiumTransportFailsRequestsBeforePublishingError();
    void votChromiumTransportRetriesAfterInitializationError();
    void votNetworkDestinationsRequireHttpsAndDeclaredHosts();
    void votChromiumTransportRegistryFailsClosed();
    void votRequestsUseCrossDomainAndFailClosedPolicies();
    void votUserscriptBridgeInjectsOnlyOnMatchingPages();
    void votUserscriptBridgeInjectsIntoMatchingSubframes();
    void crossDomainPendingRequestsAreBoundedAndDeduplicated();
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
    void oversizedSessionPrioritizesPinnedTabsWithinBound();
    void invalidSessionFileFailsClosed();
    void managedDataCleanupStaysInsideRoot();
    void downloadHistoryRoundTripAndLimit();
    void sensitivePermissionsRequireSecureOrigin();
    void unsupportedPermissionsAreDenied();
    void webSchemesStayInsideBrowser();
    void externalSchemesRequireMainFrameConfirmation();
    void dangerousLocalSchemesAreBlocked();
};

void PersistenceAndPolicyTests::privateDataFilesUseOwnerOnlyPermissions()
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

    const QString siteConnectionsPath = QDir(privateDirectory).filePath(
        QStringLiteral("site-connections.json")
    );
    CrossDomainSettings siteConnections;
    QVERIFY2(siteConnections.save(siteConnectionsPath, &error), qPrintable(error));

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
    QVERIFY(isOwnerOnly(siteConnectionsPath));
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

void PersistenceAndPolicyTests::settingsSaveTransactionRestoresFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString existingPath = directory.filePath(QStringLiteral("existing.json"));
    const QString newPath = directory.filePath(QStringLiteral("new.json"));

    QFile existing(existingPath);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    QCOMPARE(existing.write("before"), qint64(6));
    existing.close();

    QString error;
    SettingsSaveTransaction transaction;
    QVERIFY2(transaction.capture({existingPath, newPath}, &error), qPrintable(error));

    QVERIFY(existing.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(existing.write("after"), qint64(5));
    existing.close();
    QFile created(newPath);
    QVERIFY(created.open(QIODevice::WriteOnly));
    QCOMPARE(created.write("temporary"), qint64(9));
    created.close();

    const QString rollbackError = transaction.rollback();
    QVERIFY2(rollbackError.isEmpty(), qPrintable(rollbackError));
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("before"));
    existing.close();
    QVERIFY(!QFile::exists(newPath));
}

void PersistenceAndPolicyTests::siteDomainsUsePublicSuffixBoundaries()
{
    QCOMPARE(
        SiteDomain::registrableDomain(QStringLiteral("online.example.co.uk")),
        QStringLiteral("example.co.uk")
    );
    QCOMPARE(
        SiteDomain::registrableDomain(QStringLiteral("project.github.io")),
        QStringLiteral("project.github.io")
    );
    QCOMPARE(
        SiteDomain::registrableDomain(QStringLiteral("sub.project.github.io")),
        QStringLiteral("project.github.io")
    );
    QCOMPARE(
        SiteDomain::registrableDomain(QStringLiteral("LOCALHOST")),
        QStringLiteral("localhost")
    );
    QVERIFY(SiteDomain::hostMatchesPattern(
        QStringLiteral("img.cdn.example.net"),
        QStringLiteral("*.cdn.example.net")
    ));
    QVERIFY(!SiteDomain::hostMatchesPattern(
        QStringLiteral("evilcdn.example.net"),
        QStringLiteral("cdn.example.net")
    ));
    QCOMPARE(
        SiteDomain::normalizedPageUrl(
            QUrl(QStringLiteral("https://user@example.com:443/path/../page#section"))
        ),
        QUrl(QStringLiteral("https://example.com/page"))
    );
}

void PersistenceAndPolicyTests::domainPatternMatcherUsesLabelBoundaries()
{
    DomainPatternMatcher matcher;
    matcher.setPatterns({
        QStringLiteral("cdn.example.net"),
        QStringLiteral("metrics.example.org"),
    });
    QVERIFY(matcher.matches(QStringLiteral("cdn.example.net")));
    QVERIFY(matcher.matches(QStringLiteral("images.cdn.example.net")));
    QVERIFY(!matcher.matches(QStringLiteral("evilcdn.example.net")));
    QVERIFY(!matcher.matches(QStringLiteral("example.net")));
    QCOMPARE(
        matcher.patterns(),
        QStringList({
            QStringLiteral("cdn.example.net"),
            QStringLiteral("metrics.example.org"),
        })
    );
}

void PersistenceAndPolicyTests::bundledConnectionPresetsAreValidAndRespectUserRules()
{
    const CrossDomainPresetCatalog &catalog = CrossDomainPresetCatalog::bundled();
    QVERIFY(!catalog.revision().isEmpty());
    QVERIFY(catalog.preset(QStringLiteral("common-trackers")));
    QVERIFY(catalog.preset(QStringLiteral("public-cdns")));
    QVERIFY(
        catalog.evaluate(
            {QStringLiteral("common-trackers")},
            QStringLiteral("www.google-analytics.com")
        ) == std::optional(CrossDomainPresetDecision::Block)
    );
    QVERIFY(
        catalog.evaluate(
            {QStringLiteral("public-cdns")},
            QStringLiteral("cdn.jsdelivr.net")
        ) == std::optional(CrossDomainPresetDecision::Allow)
    );
    QVERIFY(!catalog.evaluate({}, QStringLiteral("google-analytics.com")).has_value());

    CrossDomainPresetCatalog invalid;
    QString error;
    QVERIFY(!invalid.loadJson(
        R"({"version":1,"revision":"test","presets":[{"id":"unsafe","decision":"allow","recommended":false,"patterns":["co.uk"]}]})",
        &error
    ));
    QVERIFY(!error.isEmpty());

    CrossDomainSettings settings;
    settings.setEnabled(true);
    settings.setEnabledPresetIds({QStringLiteral("common-trackers")});
    const QString source = QStringLiteral("example.com");
    const QString tracker = QStringLiteral("google-analytics.com");
    QCOMPARE(settings.evaluate(source, tracker), CrossDomainEvaluation::Block);

    settings.setRule(source, tracker, CrossDomainRuleDecision::Allow);
    QCOMPARE(settings.evaluate(source, tracker), CrossDomainEvaluation::Allow);
    QCOMPARE(
        settings.evaluate(source, QStringLiteral("www.google-analytics.com")),
        CrossDomainEvaluation::Block
    );
    settings.setRules({});
    settings.setGlobalAllowPatterns({tracker});
    QCOMPARE(settings.evaluate(source, tracker), CrossDomainEvaluation::Allow);

    settings.setEnabledPresetIds({QStringLiteral("public-cdns")});
    settings.setGlobalAllowPatterns({});
    settings.setGlobalBlockPatterns({QStringLiteral("cdn.jsdelivr.net")});
    QCOMPARE(
        settings.evaluate(source, QStringLiteral("cdn.jsdelivr.net")),
        CrossDomainEvaluation::Block
    );
}

void PersistenceAndPolicyTests::crossDomainPolicyEvaluatesRulesAndExceptions()
{
    CrossDomainSettings settings = CrossDomainSettings::defaults();
    const QUrl source(QStringLiteral("https://login.example.com/page"));
    const QUrl sameSite(QStringLiteral("https://api.example.com/data"));
    const QUrl thirdParty(QStringLiteral("https://assets.example.net/app.js"));
    const QUrl tracker(QStringLiteral("https://metrics.example.org/pixel"));

    QCOMPARE(settings.evaluate(source, thirdParty), CrossDomainEvaluation::Allow);
    settings.setEnabled(true);
    QCOMPARE(settings.evaluate(source, sameSite), CrossDomainEvaluation::Allow);
    QCOMPARE(settings.evaluate(source, thirdParty), CrossDomainEvaluation::Ask);

    settings.setGlobalAllowPatterns({QStringLiteral("example.net")});
    QCOMPARE(settings.evaluate(source, thirdParty), CrossDomainEvaluation::Allow);
    settings.setRule(
        QStringLiteral("example.com"),
        QStringLiteral("assets.example.net"),
        CrossDomainRuleDecision::Block
    );
    QCOMPARE(settings.evaluate(source, thirdParty), CrossDomainEvaluation::Block);
    settings.setRule(
        QStringLiteral("example.com"),
        QStringLiteral("assets.example.net"),
        CrossDomainRuleDecision::Allow
    );
    QCOMPARE(settings.evaluate(source, thirdParty), CrossDomainEvaluation::Allow);
    settings.setGlobalAllowPatterns({});
    QCOMPARE(
        settings.evaluate(
            source,
            QUrl(QStringLiteral("https://child.assets.example.net/app.js"))
        ),
        CrossDomainEvaluation::Ask
    );

    settings.setGlobalBlockPatterns({QStringLiteral("metrics.example.org")});
    settings.setRule(
        QStringLiteral("example.com"),
        QStringLiteral("metrics.example.org"),
        CrossDomainRuleDecision::Allow
    );
    QCOMPARE(settings.evaluate(source, tracker), CrossDomainEvaluation::Block);

    settings.setGlobalAllowPatterns({QStringLiteral("*.example.org")});
    QString validationError;
    QVERIFY(!settings.validate(&validationError));
    QVERIFY(validationError.contains(QStringLiteral("both globally allowed")));

    settings.setGlobalAllowPatterns({QStringLiteral("https://invalid.example")});
    QVERIFY(!settings.validate(&validationError));
    QVERIFY(!validationError.isEmpty());

    QCOMPARE(
        settings.evaluate(
            QUrl(QStringLiteral("https://other.example.org")),
            QUrl(QStringLiteral("data:text/plain,hello"))
        ),
        CrossDomainEvaluation::Allow
    );
}

void PersistenceAndPolicyTests::compiledCrossDomainPolicyPreservesPrecedence()
{
    CrossDomainSettings settings;
    settings.setEnabled(true);
    settings.setGlobalAllowPatterns({QStringLiteral("cdn.example.net")});
    settings.setGlobalBlockPatterns({QStringLiteral("metrics.example.org")});
    settings.setEnabledPresetIds({
        QStringLiteral("common-trackers"),
        QStringLiteral("public-cdns"),
    });
    settings.setRule(
        QStringLiteral("shop.example.com"),
        QStringLiteral("api.vendor.example"),
        CrossDomainRuleDecision::Allow
    );

    const CrossDomainPolicySnapshot policy(settings);
    const QUrl source(QStringLiteral("https://shop.example.com/account"));
    QCOMPARE(
        policy.evaluateUserPolicy(
            source,
            QUrl(QStringLiteral("https://metrics.example.org/pixel"))
        ).decision,
        CrossDomainEvaluation::Block
    );
    QCOMPARE(
        policy.evaluateUserPolicy(
            source,
            QUrl(QStringLiteral("https://api.vendor.example/data"))
        ).decision,
        CrossDomainEvaluation::Allow
    );
    QCOMPARE(
        policy.evaluateUserPolicy(
            source,
            QUrl(QStringLiteral("https://child.api.vendor.example/data"))
        ).decision,
        CrossDomainEvaluation::Ask
    );
    QCOMPARE(
        policy.evaluatePresetPolicy(QStringLiteral("google-analytics.com")),
        CrossDomainEvaluation::Block
    );
    QCOMPARE(
        policy.evaluatePresetPolicy(QStringLiteral("cdn.jsdelivr.net")),
        CrossDomainEvaluation::Allow
    );
}

void PersistenceAndPolicyTests::crossDomainSourceResolutionFailsClosed()
{
    const QUrl firstParty(QStringLiteral("https://app.example.com/page"));
    const QUrl initiator(QStringLiteral("https://frame.example.net/"));
    const CrossDomainResolvedSource firstPartySource = crossDomainRequestSource(
        firstParty,
        initiator
    );
    QCOMPARE(firstPartySource.url, firstParty);
    QVERIFY(!firstPartySource.originOnly);

    const CrossDomainResolvedSource initiatorSource = crossDomainRequestSource(
        QUrl(QStringLiteral("blob:https://app.example.com/id")),
        initiator
    );
    QCOMPARE(initiatorSource.url, initiator);
    QVERIFY(initiatorSource.originOnly);

    const CrossDomainResolvedSource opaqueSource = crossDomainRequestSource(
        QUrl(QStringLiteral("data:text/html,opaque")),
        QUrl(QStringLiteral("about:blank"))
    );
    QVERIFY(opaqueSource.url.isEmpty());
    QVERIFY(!opaqueSource.originOnly);

    QVERIFY(crossDomainRequestSource(
        QUrl(QStringLiteral("https:///missing-host")),
        QUrl()
    ).url.isEmpty());

    CrossDomainPendingRequestTracker fallbackTracker;
    QUrl promptSourceUrl;
    bool promptSourceIsOriginOnly = false;
    QVERIFY(fallbackTracker.record(
        QStringLiteral("example.net\ntracker.example"),
        initiatorSource.url,
        &promptSourceUrl,
        &promptSourceIsOriginOnly,
        initiatorSource.originOnly
    ));
    QCOMPARE(promptSourceUrl, initiator);
    QVERIFY(promptSourceIsOriginOnly);

    CrossDomainSettings settings;
    settings.setEnabled(true);
    const QUrl target(QStringLiteral("https://tracker.example/pixel"));
    const QList<QUrl> unattributableSources = {
        QUrl(),
        QUrl(QStringLiteral("about:blank")),
        QUrl(QStringLiteral("blob:https://app.example.com/id")),
        QUrl(QStringLiteral("data:text/html,opaque")),
        QUrl(QStringLiteral("https:///missing-host")),
    };
    const CrossDomainPolicySnapshot policy(settings);
    for (const QUrl &source : unattributableSources) {
        QCOMPARE(settings.evaluate(source, target), CrossDomainEvaluation::Block);
        QCOMPARE(
            policy.evaluateUserPolicy(source, target).decision,
            CrossDomainEvaluation::Block
        );
    }

    settings.setEnabled(false);
    QCOMPARE(
        settings.evaluate(QUrl(QStringLiteral("about:blank")), target),
        CrossDomainEvaluation::Allow
    );
}

void PersistenceAndPolicyTests::videoTranslationSettingsRoundTripAndValidateSource()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString scriptPath = directory.filePath(QStringLiteral("vot.user.js"));
    QFile script(scriptPath);
    QVERIFY(script.open(QIODevice::WriteOnly));
    QCOMPARE(script.write("// test"), qint64(7));
    script.close();

    VideoTranslationSettings settings;
    QVERIFY(!settings.enabled());
    settings.setSourcePath(scriptPath);
    settings.setEnabled(true);
    QString error;
    QVERIFY2(settings.validate(&error), qPrintable(error));

    const QString settingsPath = directory.filePath(
        QStringLiteral("video-translation.json")
    );
    QVERIFY2(settings.save(settingsPath, &error), qPrintable(error));
    VideoTranslationSettings loaded;
    QVERIFY2(loaded.load(settingsPath, &error), qPrintable(error));
    QVERIFY(loaded.enabled());
    QCOMPARE(loaded.sourcePath(), QFileInfo(scriptPath).absoluteFilePath());
    loaded.setSourcePath(directory.filePath(QStringLiteral("replacement.user.js")));
    QVERIFY(!loaded.validate(&error));

    VideoTranslationSettings disabled;
    disabled.setSourcePath(directory.filePath(QStringLiteral("missing.user.js")));
    QVERIFY(disabled.validate(&error));
}

void PersistenceAndPolicyTests::votUserscriptPackageRejectsUnverifiedFilesAndMatchesUrls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString scriptPath = directory.filePath(QStringLiteral("vot.user.js"));
    QFile script(scriptPath);
    QVERIFY(script.open(QIODevice::WriteOnly));
    QVERIFY(script.write(
        "// ==UserScript==\n// @version 1.11.8\n// ==/UserScript==\n"
    ) > 0);
    script.close();

    VotUserscript userscript;
    QString error;
    QVERIFY(!VotUserscriptPackage::load(scriptPath, &userscript, &error));
    QVERIFY(error.contains(QStringLiteral("verified"), Qt::CaseInsensitive));

    QVERIFY(VotUserscriptPackage::matchesUrlPattern(
        QStringLiteral("*://*.youtube.com/*"),
        QUrl(QStringLiteral("https://www.youtube.com/watch?v=1"))
    ));
    QVERIFY(VotUserscriptPackage::matchesUrlPattern(
        QStringLiteral("*://*.youtube.com/*"),
        QUrl(QStringLiteral("https://youtube.com/"))
    ));
    QVERIFY(!VotUserscriptPackage::matchesUrlPattern(
        QStringLiteral("*://*.youtube.com/*"),
        QUrl(QStringLiteral("https://notyoutube.com/"))
    ));
    QVERIFY(VotUserscriptPackage::matchesUrlPattern(
        QStringLiteral("*://*/*.mp4*"),
        QUrl(QStringLiteral("https://media.example/video.mp4?token=1"))
    ));
}

void PersistenceAndPolicyTests::votUserscriptPackageLoadsOfficialFileWhenProvided()
{
    const QString scriptPath = qEnvironmentVariable(
        "PANBROWSER_VOT_USERSCRIPT_TEST_PATH"
    );
    if (scriptPath.isEmpty())
        QSKIP("PANBROWSER_VOT_USERSCRIPT_TEST_PATH is not set");

    VotUserscript userscript;
    QString error;
    QVERIFY2(
        VotUserscriptPackage::load(scriptPath, &userscript, &error),
        qPrintable(error)
    );
    QCOMPARE(userscript.version, VotUserscriptPackage::supportedVersion());
    QCOMPARE(userscript.sha256, VotUserscriptPackage::expectedSha256Hex());
    QVERIFY(!userscript.matchPatterns.isEmpty());
    QVERIFY(userscript.connectHosts.contains(QStringLiteral("yandex.ru")));
    const QString injected = VotUserscriptBridge::injectedSource(
        userscript,
        QStringLiteral("test-token"),
        {{QStringLiteral("seed"), QStringLiteral("value")}}
    );
    QVERIFY(injected.contains(userscript.sourceCode));
    const qsizetype userscriptOffset = injected.lastIndexOf(userscript.sourceCode);
    QVERIFY(userscriptOffset >= 0);
    QVERIFY(!injected.left(userscriptOffset).contains(QStringLiteral("localStorage")));
    QVERIFY(injected.contains(QStringLiteral("\"seed\":\"value\"")));
}

void PersistenceAndPolicyTests::votUserscriptStorageIsScriptScopedAndPersistent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("vot-storage.json"));

    VotUserscriptStore store(path);
    QString error;
    QVERIFY2(store.load(&error), qPrintable(error));
    QVERIFY2(store.setValue(
        QStringLiteral("account"),
        QJsonObject{
            {QStringLiteral("token"), QStringLiteral("secret")},
            {QStringLiteral("expires"), 12345},
        },
        &error
    ), qPrintable(error));

    VotUserscriptStore restored(path);
    QVERIFY2(restored.load(&error), qPrintable(error));
    QCOMPARE(
        restored.values().value(QStringLiteral("account")).toObject()
            .value(QStringLiteral("token")).toString(),
        QStringLiteral("secret")
    );
    QVERIFY2(restored.removeValue(QStringLiteral("account"), &error), qPrintable(error));
    QVERIFY(!restored.values().contains(QStringLiteral("account")));
}

void PersistenceAndPolicyTests::votChromiumTransportExtensionIsPreparedSafely()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString extensionPath = directory.filePath(QStringLiteral("vot-network"));
    QString error;
    QVERIFY2(
        VotChromiumNetworkTransport::prepareExtensionDirectory(
            extensionPath,
            &error
        ),
        qPrintable(error)
    );

    QFile manifestFile(QDir(extensionPath).filePath(QStringLiteral("manifest.json")));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument manifest = QJsonDocument::fromJson(
        manifestFile.readAll(),
        &parseError
    );
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(manifest.isObject());
    const QJsonObject object = manifest.object();
    QCOMPARE(object.value(QStringLiteral("manifest_version")).toInt(), 3);
    QCOMPARE(
        object.value(QStringLiteral("host_permissions")).toArray(),
        QJsonArray{QStringLiteral("https://*/*")}
    );
    QVERIFY(!object.contains(QStringLiteral("background")));
    QVERIFY(!object.contains(QStringLiteral("content_scripts")));
    QVERIFY(QFileInfo::exists(
        QDir(extensionPath).filePath(QStringLiteral("transport.html"))
    ));
    QVERIFY(QFileInfo::exists(
        QDir(extensionPath).filePath(QStringLiteral("transport.js"))
    ));
    QFile transportScript(
        QDir(extensionPath).filePath(QStringLiteral("transport.js"))
    );
    QVERIFY(transportScript.open(QIODevice::ReadOnly));
    QVERIFY(!transportScript.readAll().contains("X-PanBrowser"));

    const QString extensionId = QStringLiteral("abcdefghijklmnop");
    QCOMPARE(
        CrossDomainRequestInterceptor::votTransportRequestId(
            QUrl(QStringLiteral(
                "chrome-extension://abcdefghijklmnop/transport.html?request=request-1"
            )),
            extensionId
        ),
        QStringLiteral("request-1")
    );
    QVERIFY(CrossDomainRequestInterceptor::votTransportRequestId(
        QUrl(QStringLiteral(
            "chrome-extension://abcdefghijklmnop/transport.html?request=request-1&request=request-2"
        )),
        extensionId
    ).isEmpty());
    QVERIFY(CrossDomainRequestInterceptor::votTransportRequestId(
        QUrl(QStringLiteral(
            "chrome-extension://different/transport.html?request=request-1"
        )),
        extensionId
    ).isEmpty());
}

void PersistenceAndPolicyTests::votChromiumTransportLoadsInBrowserProfile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QList<QSslCertificate> certificates = QSslCertificate::fromData(
        votTestCertificatePem,
        QSsl::Pem
    );
    QCOMPARE(certificates.size(), 1);
    const QList<QSslCertificate> serverCertificates = QSslCertificate::fromData(
        votTestServerCertificatePem,
        QSsl::Pem
    );
    QCOMPARE(serverCertificates.size(), 1);
    const QList<QSslCertificate> trustedCertificates{
        serverCertificates.constFirst(),
        certificates.constFirst(),
    };
    const QSslKey privateKey(
        votTestPrivateKeyPem,
        QSsl::Rsa,
        QSsl::Pem,
        QSsl::PrivateKey
    );
    QVERIFY(!privateKey.isNull());

    QSslConfiguration sslConfiguration = QSslConfiguration::defaultConfiguration();
    sslConfiguration.setLocalCertificateChain(serverCertificates);
    sslConfiguration.setPrivateKey(privateKey);
    sslConfiguration.addCaCertificates(certificates);
    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslServer server;
    QSslServer redirectServer;
    server.setSslConfiguration(sslConfiguration);
    redirectServer.setSslConfiguration(sslConfiguration);
    int handshakeCount = 0;
    QStringList serverErrors;
    const auto configureServerSignals = [
        &handshakeCount,
        &serverErrors
    ](QSslServer *configuredServer) {
        connect(
            configuredServer,
            &QSslServer::startedEncryptionHandshake,
            configuredServer,
            [&handshakeCount] { ++handshakeCount; }
        );
        connect(
            configuredServer,
            &QSslServer::errorOccurred,
            configuredServer,
            [&serverErrors](QSslSocket *socket, QAbstractSocket::SocketError) {
                if (socket)
                    serverErrors.append(socket->errorString());
            }
        );
        connect(
            configuredServer,
            &QSslServer::sslErrors,
            configuredServer,
            [&serverErrors](QSslSocket *, const QList<QSslError> &errors) {
                for (const QSslError &error : errors)
                    serverErrors.append(error.errorString());
            }
        );
    };
    configureServerSignals(&server);
    configureServerSignals(&redirectServer);

    bool originAuthorizationSeen = false;
    bool redirectAuthorizationSeen = false;
    int loopRequestCount = 0;
    connect(
        &server,
        &QTcpServer::newConnection,
        &server,
        [
            &server,
            &redirectServer,
            &originAuthorizationSeen,
            &loopRequestCount
        ] {
        while (auto *socket = qobject_cast<QSslSocket *>(
                   server.nextPendingConnection()
               )) {
            socket->setParent(&server);
            auto responded = std::make_shared<bool>(false);
            auto requestBytes = std::make_shared<QByteArray>();
            connect(
                socket,
                &QIODevice::readyRead,
                socket,
                [
                    socket,
                    requestBytes,
                    responded,
                    &redirectServer,
                    &originAuthorizationSeen,
                    &loopRequestCount
                ] {
                    if (*responded)
                        return;
                    requestBytes->append(socket->readAll());
                    if (!requestBytes->contains("\r\n\r\n"))
                        return;
                    *responded = true;
                    const QByteArray requestLine = requestBytes->left(
                        requestBytes->indexOf("\r\n")
                    );
                    const QList<QByteArray> fields = requestLine.split(' ');
                    const QByteArray method = fields.isEmpty()
                        ? QByteArray()
                        : fields.constFirst();
                    const QByteArray path = fields.size() >= 2
                        ? fields.at(1)
                        : QByteArray();
                    QByteArray response;
                    if (method == QByteArrayLiteral("OPTIONS")) {
                        response = QByteArrayLiteral(
                            "HTTP/1.1 204 No Content\r\n"
                            "Access-Control-Allow-Origin: *\r\n"
                            "Access-Control-Allow-Headers: Authorization\r\n"
                            "Access-Control-Allow-Methods: GET\r\n"
                            "Content-Length: 0\r\n"
                            "Connection: close\r\n\r\n"
                        );
                    } else if (path == QByteArrayLiteral("/start")) {
                        originAuthorizationSeen = requestBytes->toLower().contains(
                            QByteArrayLiteral(
                                "\r\nauthorization: bearer panbrowser-secret\r\n"
                            )
                        );
                        response = QByteArrayLiteral(
                            "HTTP/1.1 302 Found\r\n"
                            "Location: https://127.0.0.1:"
                        ) + QByteArray::number(redirectServer.serverPort())
                            + QByteArrayLiteral(
                                "/final\r\n"
                                "Content-Length: 0\r\n"
                                "Connection: close\r\n\r\n"
                            );
                    } else if (path == QByteArrayLiteral("/loop")) {
                        ++loopRequestCount;
                        response = QByteArrayLiteral(
                            "HTTP/1.1 302 Found\r\n"
                            "Location: /loop\r\n"
                            "Content-Length: 0\r\n"
                            "Connection: close\r\n\r\n"
                        );
                    } else {
                        response = QByteArrayLiteral(
                            "HTTP/1.1 404 Not Found\r\n"
                            "Content-Length: 0\r\n"
                            "Connection: close\r\n\r\n"
                        );
                    }
                    socket->write(response);
                    socket->disconnectFromHost();
                }
            );
        }
    });
    connect(
        &redirectServer,
        &QTcpServer::newConnection,
        &redirectServer,
        [&redirectServer, &redirectAuthorizationSeen] {
        while (auto *socket = qobject_cast<QSslSocket *>(
                   redirectServer.nextPendingConnection()
               )) {
            socket->setParent(&redirectServer);
            auto responded = std::make_shared<bool>(false);
            auto requestBytes = std::make_shared<QByteArray>();
            connect(
                socket,
                &QIODevice::readyRead,
                socket,
                [
                    socket,
                    requestBytes,
                    responded,
                    &redirectAuthorizationSeen
                ] {
                    if (*responded)
                        return;
                    requestBytes->append(socket->readAll());
                    if (!requestBytes->contains("\r\n\r\n"))
                        return;
                    *responded = true;
                    const QByteArray requestLine = requestBytes->left(
                        requestBytes->indexOf("\r\n")
                    );
                    const QList<QByteArray> fields = requestLine.split(' ');
                    const QByteArray method = fields.isEmpty()
                        ? QByteArray()
                        : fields.constFirst();
                    const QByteArray path = fields.size() >= 2
                        ? fields.at(1)
                        : QByteArray();
                    QByteArray response;
                    if (method == QByteArrayLiteral("OPTIONS")) {
                        response = QByteArrayLiteral(
                            "HTTP/1.1 204 No Content\r\n"
                            "Access-Control-Allow-Origin: *\r\n"
                            "Access-Control-Allow-Headers: Authorization\r\n"
                            "Access-Control-Allow-Methods: GET\r\n"
                            "Content-Length: 0\r\n"
                            "Connection: close\r\n\r\n"
                        );
                    } else if (path == QByteArrayLiteral("/final")) {
                        redirectAuthorizationSeen = requestBytes->toLower().contains(
                            QByteArrayLiteral("\r\nauthorization:")
                        );
                        response = QByteArrayLiteral(
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "X-PanBrowser-Test: success\r\n"
                            "Content-Length: 21\r\n"
                            "Connection: close\r\n\r\n"
                            "chromium-transport-ok"
                        );
                    } else {
                        response = QByteArrayLiteral(
                            "HTTP/1.1 404 Not Found\r\n"
                            "Content-Length: 0\r\n"
                            "Connection: close\r\n\r\n"
                        );
                    }
                    socket->write(response);
                    socket->disconnectFromHost();
                }
            );
        }
    });

    QWebEngineProfileBuilder profileBuilder;
    profileBuilder.setPersistentStoragePath(
        directory.filePath(QStringLiteral("source-profile"))
    ).setCachePath(directory.filePath(QStringLiteral("source-cache")));
    std::unique_ptr<QWebEngineProfile> profile(profileBuilder.createProfile(
        QStringLiteral("PanBrowserVotTransportTest"),
        nullptr
    ));
    QVERIFY(profile);
    VotChromiumNetworkTransport transport(
        profile.get(),
        directory.filePath(QStringLiteral("transport-cache")),
        trustedCertificates
    );
    QSignalSpy stateSpy(
        &transport,
        &VotChromiumNetworkTransport::stateChanged
    );
    transport.ensureReady();
    QTRY_VERIFY_WITH_TIMEOUT(
        transport.state() == VotChromiumTransportState::Ready
            || transport.state() == VotChromiumTransportState::Error,
        15'000
    );
    QVERIFY2(
        transport.state() == VotChromiumTransportState::Ready,
        qPrintable(transport.errorString())
    );
    QVERIFY(!stateSpy.isEmpty());

#if defined(Q_OS_MACOS)
    QSKIP(
        "Qt SecureTransport can block local QSslServer connections on macOS; "
        "transport initialization remains covered here, while the successful "
        "HTTPS path is exercised on Windows and Linux."
    );
#endif
    QVERIFY(redirectServer.listen(QHostAddress::LocalHost));
    QVERIFY(server.listen(QHostAddress::LocalHost));

    const QUrl requestUrl(QStringLiteral("https://127.0.0.1:%1/start").arg(
        server.serverPort()
    ));
    const QString requestId = QStringLiteral("request-1");
    QMutex authorizerMutex;
    QStringList authorizedPaths;
    QVERIFY(transport.registerRequestAuthorizer(
        requestId,
        [
            &authorizerMutex,
            &authorizedPaths,
            originPort = server.serverPort(),
            redirectPort = redirectServer.serverPort()
        ](const QUrl &url, int) {
            QMutexLocker locker(&authorizerMutex);
            authorizedPaths.append(url.path());
            return url.scheme() == QStringLiteral("https")
                && url.host() == QStringLiteral("127.0.0.1")
                && ((url.port() == originPort
                        && url.path() == QStringLiteral("/start"))
                    || (url.port() == redirectPort
                        && url.path() == QStringLiteral("/final")));
        }
    ));
    QSignalSpy responseSpy(
        &transport,
        &VotChromiumNetworkTransport::responseReady
    );
    VotChromiumRequest request;
    request.id = requestId;
    request.url = requestUrl;
    request.method = QByteArrayLiteral("GET");
    request.headers.insert(
        QStringLiteral("Authorization"),
        QStringLiteral("Bearer panbrowser-secret")
    );
    request.redirectMode = QStringLiteral("follow");
    request.timeoutMilliseconds = 30'000;
    transport.sendRequest(request);
    QTRY_VERIFY_WITH_TIMEOUT(!responseSpy.isEmpty(), 35'000);
    const QJsonObject response = responseSpy.constFirst().constFirst().toJsonObject();
    QCOMPARE(response.value(QStringLiteral("id")).toString(), requestId);
    QStringList observedPaths;
    {
        QMutexLocker locker(&authorizerMutex);
        observedPaths = authorizedPaths;
    }
    const QString failureDetails = QStringLiteral(
        "Response: %1; authorized paths: %2; handshakes: %3; server errors: %4"
    ).arg(
        QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)),
        observedPaths.join(QStringLiteral(", ")),
        QString::number(handshakeCount),
        serverErrors.join(QStringLiteral("; "))
    );
    QVERIFY2(
        response.value(QStringLiteral("type")).toString()
            == QStringLiteral("load"),
        qPrintable(failureDetails)
    );
    QCOMPARE(response.value(QStringLiteral("status")).toInt(), 200);
    QCOMPARE(
        QUrl(response.value(QStringLiteral("finalUrl")).toString()).path(),
        QStringLiteral("/final")
    );
    QVERIFY(
        response.value(QStringLiteral("responseHeaders")).toString()
            .contains(
                QStringLiteral("x-panbrowser-test: success"),
                Qt::CaseInsensitive
            )
    );
    QCOMPARE(
        QByteArray::fromBase64(
            response.value(QStringLiteral("body")).toString().toLatin1()
        ),
        QByteArrayLiteral("chromium-transport-ok")
    );
    QVERIFY(observedPaths.contains(QStringLiteral("/start")));
    QVERIFY(observedPaths.contains(QStringLiteral("/final")));
    QVERIFY(originAuthorizationSeen);
    QVERIFY(!redirectAuthorizationSeen);
    transport.unregisterRequestAuthorizer(requestId);

    responseSpy.clear();
    const QString loopRequestId = QStringLiteral("request-loop");
    QVERIFY(transport.registerRequestAuthorizer(
        loopRequestId,
        [port = server.serverPort()](const QUrl &url, int) {
            return url.scheme() == QStringLiteral("https")
                && url.host() == QStringLiteral("127.0.0.1")
                && url.port() == port
                && url.path() == QStringLiteral("/loop");
        }
    ));
    VotChromiumRequest loopRequest;
    loopRequest.id = loopRequestId;
    loopRequest.url = QUrl(
        QStringLiteral("https://127.0.0.1:%1/loop").arg(
            server.serverPort()
        )
    );
    loopRequest.method = QByteArrayLiteral("GET");
    loopRequest.redirectMode = QStringLiteral("follow");
    loopRequest.timeoutMilliseconds = 30'000;
    transport.sendRequest(loopRequest);
    QTRY_VERIFY_WITH_TIMEOUT(!responseSpy.isEmpty(), 35'000);
    const QJsonObject loopResponse = responseSpy.constFirst()
        .constFirst().toJsonObject();
    QCOMPARE(
        loopResponse.value(QStringLiteral("id")).toString(),
        loopRequestId
    );
    QCOMPARE(
        loopResponse.value(QStringLiteral("type")).toString(),
        QStringLiteral("error")
    );
    QCOMPARE(loopRequestCount, 6);
    transport.unregisterRequestAuthorizer(loopRequestId);
}

void PersistenceAndPolicyTests::votChromiumTransportRetriesAfterInitializationError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QWebEngineProfileBuilder profileBuilder;
    profileBuilder.setPersistentStoragePath(
        directory.filePath(QStringLiteral("source-profile"))
    ).setCachePath(directory.filePath(QStringLiteral("source-cache")));
    std::unique_ptr<QWebEngineProfile> profile(profileBuilder.createProfile(
        QStringLiteral("PanBrowserVotTransportRetryTest"),
        nullptr
    ));
    QVERIFY(profile);

    const QString cacheRoot = directory.filePath(QStringLiteral("blocked-cache"));
    QFile blocker(cacheRoot);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    QCOMPARE(blocker.write("blocked"), qint64(7));
    blocker.close();

    VotChromiumNetworkTransport transport(profile.get(), cacheRoot);
    transport.ensureReady();
    QCOMPARE(transport.state(), VotChromiumTransportState::Error);
    QVERIFY(!transport.errorString().isEmpty());

    QVERIFY(QFile::remove(cacheRoot));
    transport.ensureReady();
    QCOMPARE(transport.state(), VotChromiumTransportState::Loading);
    const QString queuedAuthorizerId = QStringLiteral(
        "request-authorized-during-transport-load"
    );
    QVERIFY(transport.registerRequestAuthorizer(
        queuedAuthorizerId,
        [](const QUrl &, int) { return true; }
    ));
    QTRY_VERIFY_WITH_TIMEOUT(
        transport.state() == VotChromiumTransportState::Ready
            || transport.state() == VotChromiumTransportState::Error,
        15'000
    );
    QVERIFY2(
        transport.state() == VotChromiumTransportState::Ready,
        qPrintable(transport.errorString())
    );
    QVERIFY(!transport.registerRequestAuthorizer(
        queuedAuthorizerId,
        [](const QUrl &, int) { return true; }
    ));
    transport.unregisterRequestAuthorizer(queuedAuthorizerId);

    const QString requestId = QStringLiteral("request-aborted-before-page-load");
    QVERIFY(transport.registerRequestAuthorizer(
        requestId,
        [](const QUrl &, int) { return true; }
    ));
    QSignalSpy responseSpy(
        &transport,
        &VotChromiumNetworkTransport::responseReady
    );
    VotChromiumRequest request;
    request.id = requestId;
    request.url = QUrl(QStringLiteral("https://127.0.0.1:1/abort"));
    request.method = QByteArrayLiteral("GET");
    transport.sendRequest(request);
    transport.abortRequest(requestId);
    QCOMPARE(responseSpy.size(), 1);
    QCOMPARE(
        responseSpy.constFirst().constFirst().toJsonObject()
            .value(QStringLiteral("type")).toString(),
        QStringLiteral("abort")
    );
}

void PersistenceAndPolicyTests::votChromiumTransportFailsRequestsBeforePublishingError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QWebEngineProfileBuilder profileBuilder;
    profileBuilder.setPersistentStoragePath(
        directory.filePath(QStringLiteral("source-profile"))
    ).setCachePath(directory.filePath(QStringLiteral("source-cache")));
    std::unique_ptr<QWebEngineProfile> profile(profileBuilder.createProfile(
        QStringLiteral("PanBrowserVotTransportFailureOrderTest"),
        nullptr
    ));
    QVERIFY(profile);

    const QString cacheRoot = directory.filePath(QStringLiteral("blocked-cache"));
    QFile blocker(cacheRoot);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    QCOMPARE(blocker.write("blocked"), qint64(7));
    blocker.close();

    VotChromiumNetworkTransport transport(profile.get(), cacheRoot);
    QStringList events;
    QJsonObject terminalResponse;
    connect(
        &transport,
        &VotChromiumNetworkTransport::responseReady,
        &transport,
        [&events, &terminalResponse](const QJsonObject &response) {
            events.append(QStringLiteral("response"));
            terminalResponse = response;
        }
    );
    connect(
        &transport,
        &VotChromiumNetworkTransport::stateChanged,
        &transport,
        [&transport, &events] {
            if (transport.state() == VotChromiumTransportState::Error)
                events.append(QStringLiteral("state"));
        }
    );

    VotChromiumRequest request;
    request.id = QStringLiteral("request-failed-initialization");
    request.url = QUrl(QStringLiteral("https://example.com/audio"));
    request.method = QByteArrayLiteral("GET");
    request.timeoutMilliseconds = 30'000;
    transport.sendRequest(request);

    QCOMPARE(transport.state(), VotChromiumTransportState::Error);
    QCOMPARE(
        events,
        QStringList({QStringLiteral("response"), QStringLiteral("state")})
    );
    QCOMPARE(
        terminalResponse.value(QStringLiteral("id")).toString(),
        request.id
    );
    QCOMPARE(
        terminalResponse.value(QStringLiteral("type")).toString(),
        QStringLiteral("error")
    );
    QVERIFY(!terminalResponse.value(QStringLiteral("error")).toString().isEmpty());
}

void PersistenceAndPolicyTests::votNetworkDestinationsRequireHttpsAndDeclaredHosts()
{
    const QStringList hosts{
        QStringLiteral("yandex.ru"),
        QStringLiteral("googlevideo.com"),
    };
    QVERIFY(VotUserscriptPackage::isAllowedConnectUrl(
        hosts,
        QUrl(QStringLiteral("https://api.browser.yandex.ru/video"))
    ));
    QVERIFY(VotUserscriptPackage::isAllowedConnectUrl(
        hosts,
        QUrl(QStringLiteral("https://rr1.googlevideo.com/audio"))
    ));
    QVERIFY(!VotUserscriptPackage::isAllowedConnectUrl(
        hosts,
        QUrl(QStringLiteral("http://api.browser.yandex.ru/video"))
    ));
    QVERIFY(!VotUserscriptPackage::isAllowedConnectUrl(
        hosts,
        QUrl(QStringLiteral("https://yandex.ru@example.test/video"))
    ));
    QVERIFY(!VotUserscriptPackage::isAllowedConnectUrl(
        hosts,
        QUrl(QStringLiteral("https://notyandex.ru/video"))
    ));
}

void PersistenceAndPolicyTests::votRequestsUseCrossDomainAndFailClosedPolicies()
{
    CrossDomainSettings settings;
    settings.setEnabled(true);
    settings.setGlobalBlockPatterns({QStringLiteral("blocked.example")});
    settings.setGlobalAllowPatterns({QStringLiteral("allowed.example")});
    CrossDomainRequestInterceptor interceptor(false, settings);
    QSignalSpy blockedSpy(
        &interceptor,
        &CrossDomainRequestInterceptor::requestBlocked
    );
    const QUrl source(QStringLiteral("https://video.example/watch"));

    QVERIFY(interceptor.allowRequest(
        source,
        QUrl(QStringLiteral("https://allowed.example/audio")),
        static_cast<int>(QWebEngineUrlRequestInfo::ResourceTypeXhr)
    ));
    QVERIFY(!interceptor.allowRequest(
        source,
        QUrl(QStringLiteral("https://blocked.example/collect")),
        static_cast<int>(QWebEngineUrlRequestInfo::ResourceTypeXhr)
    ));
    QCOMPARE(blockedSpy.size(), 0);
    QVERIFY(!interceptor.allowRequest(
        source,
        QUrl(QStringLiteral("https://unknown.example/audio")),
        static_cast<int>(QWebEngineUrlRequestInfo::ResourceTypeXhr)
    ));
    QCOMPARE(blockedSpy.size(), 1);

    CrossDomainRequestInterceptor failClosed(true, CrossDomainSettings::defaults());
    QVERIFY(!failClosed.allowRequest(
        source,
        QUrl(QStringLiteral("https://allowed.example/audio")),
        static_cast<int>(QWebEngineUrlRequestInfo::ResourceTypeXhr)
    ));
}

void PersistenceAndPolicyTests::votChromiumTransportRegistryFailsClosed()
{
    CrossDomainRequestInterceptor interceptor(
        false,
        CrossDomainSettings::defaults()
    );
    QVERIFY(interceptor.registerVotTransportRequest(
        QStringLiteral("request-1"),
        [](const QUrl &, int) { return true; }
    ));
    interceptor.setVotTransportExtensionId(QStringLiteral("extension-id"));
    QVERIFY(!interceptor.registerVotTransportRequest(
        QStringLiteral("request-1"),
        [](const QUrl &, int) { return true; }
    ));
    interceptor.unregisterVotTransportRequest(QStringLiteral("request-1"));
    QVERIFY(interceptor.registerVotTransportRequest(
        QStringLiteral("request-1"),
        [](const QUrl &, int) { return true; }
    ));
    interceptor.setVotTransportExtensionId(QStringLiteral("replacement-id"));
    QVERIFY(interceptor.registerVotTransportRequest(
        QStringLiteral("request-1"),
        [](const QUrl &, int) { return false; }
    ));
    interceptor.setVotTransportExtensionId(QString());
    QVERIFY(interceptor.registerVotTransportRequest(
        QStringLiteral("request-1"),
        [](const QUrl &, int) { return true; }
    ));
}

void PersistenceAndPolicyTests::votUserscriptBridgeInjectsOnlyOnMatchingPages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QWebEngineProfile profile(QStringLiteral("PanBrowserVotBridgeTest"));
    profile.setPersistentStoragePath(directory.filePath(QStringLiteral("profile")));
    profile.setCachePath(directory.filePath(QStringLiteral("cache")));
    BrowserPage page(&profile);

    VotUserscript userscript;
    userscript.version = VotUserscriptPackage::supportedVersion();
    userscript.matchPatterns = {QStringLiteral("*://*.youtube.com/*")};
    userscript.connectHosts = {QStringLiteral("yandex.ru")};
    userscript.sourceCode = QStringLiteral(
        "globalThis.__panBrowserVotProbe = {"
        "handler: GM_info.scriptHandler,"
        "xhr: typeof GM_xmlhttpRequest,"
        "storage: typeof GM_getValue"
        "};"
    );
    VotUserscriptStore store(directory.filePath(QStringLiteral("vot-storage.json")));
    QString storageError;
    QVERIFY2(store.load(&storageError), qPrintable(storageError));
    VotUserscriptBridge bridge(&page, userscript, &store, nullptr, &page);
    const QList<QWebEngineScript> installedScripts = page.scripts().find(
        VotUserscriptBridge::scriptName()
    );
    QCOMPARE(installedScripts.size(), 1);
    QVERIFY(installedScripts.constFirst().runsOnSubFrames());

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(
        QStringLiteral("<html><body>matching page</body></html>"),
        QUrl(QStringLiteral("https://www.youtube.com/watch?v=1"))
    );
    QVERIFY(loadSpy.wait(15'000));

    bool callbackFinished = false;
    QVariant result;
    page.runJavaScript(
        QStringLiteral("JSON.stringify(globalThis.__panBrowserVotProbe)"),
        QWebEngineScript::ApplicationWorld,
        [&](const QVariant &value) {
            result = value;
            callbackFinished = true;
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(callbackFinished, 15'000);
    QCOMPARE(
        result.toString(),
        QStringLiteral(
            R"({"handler":"PanBrowser","xhr":"function","storage":"function"})"
        )
    );

    QVERIFY2(store.setValue(
        QStringLiteral("live-update"),
        QStringLiteral("shared"),
        &storageError
    ), qPrintable(storageError));
    callbackFinished = false;
    result.clear();
    page.runJavaScript(
        QStringLiteral("GM_getValue('live-update', 'missing')"),
        QWebEngineScript::ApplicationWorld,
        [&](const QVariant &value) {
            result = value;
            callbackFinished = true;
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(callbackFinished, 15'000);
    QCOMPARE(result.toString(), QStringLiteral("shared"));

    loadSpy.clear();
    page.setHtml(
        QStringLiteral("<html><body>unmatched page</body></html>"),
        QUrl(QStringLiteral("https://example.com/"))
    );
    QVERIFY(loadSpy.wait(15'000));
    callbackFinished = false;
    result.clear();
    page.runJavaScript(
        QStringLiteral("typeof globalThis.__panBrowserVotProbe"),
        QWebEngineScript::ApplicationWorld,
        [&](const QVariant &value) {
            result = value;
            callbackFinished = true;
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(callbackFinished, 15'000);
    QCOMPARE(result.toString(), QStringLiteral("undefined"));
}

void PersistenceAndPolicyTests::votUserscriptBridgeInjectsIntoMatchingSubframes()
{
    QTcpServer server;
    connect(&server, &QTcpServer::newConnection, &server, [&server] {
        while (QTcpSocket *socket = server.nextPendingConnection()) {
            socket->setParent(&server);
            connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                if (!socket->canReadLine())
                    return;
                const QByteArray requestLine = socket->readLine();
                const QByteArray body = requestLine.contains(" /frame ")
                    ? QByteArrayLiteral("<html><body>frame</body></html>")
                    : QByteArrayLiteral(
                        "<html><body><iframe src=\"/frame\"></iframe></body></html>"
                    );
                const QByteArray response = QByteArrayLiteral(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                    "Connection: close\r\nContent-Length: "
                ) + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body;
                socket->write(response);
                socket->disconnectFromHost();
            });
        }
    });
    QVERIFY(server.listen(QHostAddress::LocalHost));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QWebEngineProfile profile(QStringLiteral("PanBrowserVotFrameTest"));
    profile.setPersistentStoragePath(directory.filePath(QStringLiteral("profile")));
    profile.setCachePath(directory.filePath(QStringLiteral("cache")));
    BrowserPage page(&profile);
    VotUserscript userscript;
    userscript.version = VotUserscriptPackage::supportedVersion();
    userscript.matchPatterns = {QStringLiteral("http://127.0.0.1/*")};
    userscript.connectHosts = {QStringLiteral("yandex.ru")};
    userscript.sourceCode = QStringLiteral(
        "globalThis.__panBrowserVotFrameProbe = location.pathname;"
    );
    VotUserscriptBridge bridge(&page, userscript, nullptr, nullptr, &page);

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort())));
    QVERIFY(loadSpy.wait(15'000));
    QTRY_COMPARE_WITH_TIMEOUT(page.mainFrame().children().size(), 1, 15'000);
    QWebEngineFrame child = page.mainFrame().children().constFirst();
    bool callbackFinished = false;
    QVariant result;
    child.runJavaScript(
        QStringLiteral("globalThis.__panBrowserVotFrameProbe"),
        QWebEngineScript::ApplicationWorld,
        [&](const QVariant &value) {
            result = value;
            callbackFinished = true;
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(callbackFinished, 15'000);
    QCOMPARE(result.toString(), QStringLiteral("/frame"));
}

void PersistenceAndPolicyTests::crossDomainSettingsRoundTripAndRejectCorruption()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("site-connections.json"));

    CrossDomainSettings source;
    source.setEnabled(true);
    source.setGlobalAllowPatterns({
        QStringLiteral("*.cdn.example.net"),
        QStringLiteral("cdn.example.net"),
    });
    source.setGlobalBlockPatterns({
        QStringLiteral("*.metrics.example.org"),
        QStringLiteral("metrics.example.org"),
    });
    source.setEnabledPresetIds({
        QStringLiteral("common-trackers"),
        QStringLiteral("future-preset"),
    });
    source.setRule(
        QStringLiteral("shop.example.com"),
        QStringLiteral("payments.example.org"),
        CrossDomainRuleDecision::Allow
    );
    QString error;
    QVERIFY2(source.save(path, &error), qPrintable(error));

    CrossDomainSettings restored;
    QVERIFY2(restored.load(path, &error), qPrintable(error));
    QVERIFY(restored.enabled());
    QCOMPARE(restored.globalAllowPatterns(), QStringList{QStringLiteral("cdn.example.net")});
    QCOMPARE(
        restored.globalBlockPatterns(),
        QStringList{QStringLiteral("metrics.example.org")}
    );
    QCOMPARE(
        restored.enabledPresetIds(),
        QStringList({QStringLiteral("common-trackers"), QStringLiteral("future-preset")})
    );
    QCOMPARE(restored.rules().size(), 1);
    QCOMPARE(restored.rules().constFirst().sourceSite, QStringLiteral("example.com"));
    QCOMPARE(
        restored.rules().constFirst().targetHost,
        QStringLiteral("payments.example.org")
    );

    const QString legacyPath = directory.filePath(QStringLiteral("legacy-site-connections.json"));
    QFile legacyFile(legacyPath);
    QVERIFY(legacyFile.open(QIODevice::WriteOnly));
    const QByteArray legacyContents = R"({
        "version": 1,
        "enabled": true,
        "globalAllowPatterns": [],
        "rules": [{
            "sourceSite": "shop.example.com",
            "targetPattern": "cdn.example.net",
            "decision": "allow"
        }]
    })";
    QCOMPARE(legacyFile.write(legacyContents), qint64(legacyContents.size()));
    legacyFile.close();
    CrossDomainSettings legacy;
    QVERIFY2(legacy.load(legacyPath, &error), qPrintable(error));
    QVERIFY(legacy.globalBlockPatterns().isEmpty());
    QVERIFY(legacy.enabledPresetIds().isEmpty());
    QCOMPARE(legacy.rules().size(), 1);
    QCOMPARE(legacy.rules().constFirst().targetHost, QStringLiteral("cdn.example.net"));

    const QString ambiguousPath = directory.filePath(
        QStringLiteral("ambiguous-site-connections.json")
    );
    QFile ambiguousFile(ambiguousPath);
    QVERIFY(ambiguousFile.open(QIODevice::WriteOnly));
    const QByteArray ambiguousContents = R"({
        "version": 1,
        "enabled": true,
        "globalAllowPatterns": [],
        "rules": [{
            "sourceSite": "example.com",
            "targetHost": "cdn.example.net",
            "targetPattern": "tracker.example.net",
            "decision": "allow"
        }]
    })";
    QCOMPARE(
        ambiguousFile.write(ambiguousContents),
        qint64(ambiguousContents.size())
    );
    ambiguousFile.close();
    CrossDomainSettings ambiguous;
    QVERIFY(!ambiguous.load(ambiguousPath, &error));
    QVERIFY(!error.isEmpty());

    const QString backupPath = path + QStringLiteral(".backup");
    QVERIFY(QFile::copy(path, backupPath));
    QFile corrupt(path);
    QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(corrupt.write("not json"), qint64(8));
    corrupt.close();
    CrossDomainSettings invalid;
    QVERIFY(!invalid.load(path, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(QFileInfo(path).size(), qint64(8));

    CrossDomainSettings recovered;
    bool recoveredFromBackup = false;
    QVERIFY(recovered.loadRecoveringBackup(path, &recoveredFromBackup, &error));
    QVERIFY(recoveredFromBackup);
    QVERIFY(!error.isEmpty());
    QVERIFY(recovered.enabled());
    QCOMPARE(recovered.rules().size(), 1);

    recovered.setRule(
        QStringLiteral("example.com"),
        QStringLiteral("images.example.net"),
        CrossDomainRuleDecision::Block
    );
    QVERIFY2(recovered.save(path, &error), qPrintable(error));

    CrossDomainSettings preservedBackup;
    QVERIFY2(preservedBackup.load(backupPath, &error), qPrintable(error));
    QCOMPARE(preservedBackup.rules().size(), 1);

    CrossDomainSettings repairedPrimary;
    QVERIFY2(repairedPrimary.load(path, &error), qPrintable(error));
    QCOMPARE(repairedPrimary.rules().size(), 2);

    QVERIFY(QFile::remove(path));
    CrossDomainSettings recoveredWithoutPrimary;
    recoveredFromBackup = false;
    QVERIFY(
        recoveredWithoutPrimary.loadRecoveringBackup(
            path,
            &recoveredFromBackup,
            &error
        )
    );
    QVERIFY(recoveredFromBackup);
    QCOMPARE(recoveredWithoutPrimary.rules().size(), 1);

    QFile corruptAgain(path);
    QVERIFY(corruptAgain.open(QIODevice::WriteOnly));
    QCOMPARE(corruptAgain.write("not json"), qint64(8));
    corruptAgain.close();

    QFile corruptBackup(backupPath);
    QVERIFY(corruptBackup.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(corruptBackup.write("bad backup"), qint64(10));
    corruptBackup.close();
    QVERIFY(!recovered.loadRecoveringBackup(path, &recoveredFromBackup, &error));
    QVERIFY(!recoveredFromBackup);
}

void PersistenceAndPolicyTests::crossDomainPendingRequestsAreBoundedAndDeduplicated()
{
    CrossDomainPendingRequestTracker tracker;
    const QString sharedRequest = QStringLiteral("example.com\ntracker.example");
    QVERIFY(tracker.record(
        sharedRequest,
        QUrl(QStringLiteral("https://example.com/page#first"))
    ));
    QVERIFY(tracker.contains(
        sharedRequest,
        QUrl(QStringLiteral("https://example.com/page#second"))
    ));
    QVERIFY(!tracker.record(
        sharedRequest,
        QUrl(QStringLiteral("https://example.com/page#second"))
    ));

    CrossDomainPendingRequestTracker oversizedTracker;
    const QString oversizedQuery(
        2 * CrossDomainPendingRequestTracker::maximumPendingSources
            * CrossDomainPendingRequestTracker::maximumSourcesPerRequest,
        QLatin1Char('x')
    );
    const QUrl oversizedSource(
        QStringLiteral("https://example.com/page?value=%1#first").arg(oversizedQuery)
    );
    QUrl oversizedPromptSource;
    bool oversizedSourceIsOriginOnly = false;
    QVERIFY(oversizedTracker.record(
        sharedRequest,
        oversizedSource,
        &oversizedPromptSource,
        &oversizedSourceIsOriginOnly
    ));
    QVERIFY(oversizedSourceIsOriginOnly);
    QCOMPARE(
        oversizedPromptSource,
        QUrl(QStringLiteral("https://example.com/"))
    );
    QVERIFY(oversizedTracker.contains(
        sharedRequest,
        oversizedPromptSource,
        true
    ));
    QVERIFY(!oversizedTracker.contains(
        sharedRequest,
        oversizedPromptSource,
        false
    ));
    QUrl rootPromptSource;
    bool rootSourceIsOriginOnly = true;
    QVERIFY(oversizedTracker.record(
        sharedRequest,
        QUrl(QStringLiteral("https://example.com/")),
        &rootPromptSource,
        &rootSourceIsOriginOnly
    ));
    QVERIFY(!rootSourceIsOriginOnly);
    QCOMPARE(rootPromptSource, QUrl(QStringLiteral("https://example.com/")));
    QCOMPARE(oversizedTracker.size(), qsizetype(2));
    QVERIFY(oversizedTracker.contains(sharedRequest, rootPromptSource, false));
    QVERIFY(oversizedTracker.contains(sharedRequest, oversizedPromptSource, true));
    QUrl sameOversizedSource = oversizedSource;
    sameOversizedSource.setFragment(QStringLiteral("second"));
    QVERIFY(oversizedTracker.contains(sharedRequest, sameOversizedSource));
    const QUrl anotherOversizedPageAtTheSameOrigin(
        QStringLiteral("https://example.com/other?value=%1").arg(
            QString(oversizedQuery.size(), QLatin1Char('y'))
        )
    );
    QVERIFY(oversizedTracker.contains(
        sharedRequest,
        anotherOversizedPageAtTheSameOrigin
    ));
    QVERIFY(!oversizedTracker.contains(
        sharedRequest,
        QUrl(QStringLiteral("https://other.example/"))
    ));
    oversizedTracker.removeSource(sharedRequest, oversizedPromptSource, true);
    QVERIFY(!oversizedTracker.contains(sharedRequest, oversizedPromptSource, true));
    QVERIFY(oversizedTracker.contains(sharedRequest, rootPromptSource, false));
    QCOMPARE(oversizedTracker.size(), qsizetype(1));
    oversizedTracker.removeSource(sharedRequest, rootPromptSource, false);
    QVERIFY(!oversizedTracker.contains(sharedRequest, rootPromptSource, false));
    QCOMPARE(oversizedTracker.size(), qsizetype(0));
    oversizedTracker.removeSource(sharedRequest, rootPromptSource, false);
    QCOMPARE(oversizedTracker.size(), qsizetype(0));

    QUrl oversizedFragmentSource(QStringLiteral("https://fragment.example/page"));
    oversizedFragmentSource.setFragment(
        QString(oversizedQuery.size() + 1, QLatin1Char('f'))
    );
    CrossDomainPendingRequestTracker fragmentTracker;
    QUrl fragmentPromptSource;
    bool fragmentIsOriginOnly = false;
    QVERIFY(fragmentTracker.record(
        sharedRequest,
        oversizedFragmentSource,
        &fragmentPromptSource,
        &fragmentIsOriginOnly
    ));
    QVERIFY(fragmentIsOriginOnly);
    QCOMPARE(
        fragmentPromptSource,
        QUrl(QStringLiteral("https://fragment.example/"))
    );

    QUrl oversizedUserInfoSource(QStringLiteral("https://userinfo.example/page"));
    oversizedUserInfoSource.setUserInfo(
        QString(oversizedQuery.size() + 1, QLatin1Char('u'))
    );
    CrossDomainPendingRequestTracker userInfoTracker;
    QUrl userInfoPromptSource;
    bool userInfoIsOriginOnly = false;
    QVERIFY(userInfoTracker.record(
        sharedRequest,
        oversizedUserInfoSource,
        &userInfoPromptSource,
        &userInfoIsOriginOnly
    ));
    QVERIFY(userInfoIsOriginOnly);
    QCOMPARE(
        userInfoPromptSource,
        QUrl(QStringLiteral("https://userinfo.example/"))
    );

    CrossDomainPendingRequestTracker resolutionTracker;
    const QString unrelatedRequest = QStringLiteral(
        "example.com\nsecond-tracker.example"
    );
    const QUrl unrelatedSource(QStringLiteral("https://example.com/other"));
    QVERIFY(resolutionTracker.record(sharedRequest, oversizedSource));
    QVERIFY(resolutionTracker.record(unrelatedRequest, unrelatedSource));
    resolutionTracker.remove(sharedRequest);
    QVERIFY(resolutionTracker.contains(unrelatedRequest, unrelatedSource));

    for (qsizetype index = 1;
         index < CrossDomainPendingRequestTracker::maximumSourcesPerRequest;
         ++index) {
        QVERIFY(tracker.record(
            sharedRequest,
            QUrl(QStringLiteral("https://example.com/page/%1").arg(index))
        ));
    }
    QCOMPARE(
        tracker.size(),
        CrossDomainPendingRequestTracker::maximumSourcesPerRequest
    );
    QVERIFY(!tracker.record(
        sharedRequest,
        QUrl(QStringLiteral("https://example.com/overflow"))
    ));

    tracker.remove(sharedRequest);
    QCOMPARE(tracker.size(), qsizetype(0));
    QVERIFY(!tracker.contains(
        sharedRequest,
        QUrl(QStringLiteral("https://example.com/page"))
    ));
    for (qsizetype index = 0;
         index < CrossDomainPendingRequestTracker::maximumPendingSources;
         ++index) {
        QVERIFY(tracker.record(
            QStringLiteral("source-%1.example\ntarget.example").arg(index),
            QUrl(QStringLiteral("https://source-%1.example/").arg(index))
        ));
    }
    QCOMPARE(
        tracker.size(),
        CrossDomainPendingRequestTracker::maximumPendingSources
    );
    QVERIFY(!tracker.record(
        QStringLiteral("overflow.example\ntarget.example"),
        QUrl(QStringLiteral("https://overflow.example/"))
    ));

    tracker.remove(QStringLiteral("source-0.example\ntarget.example"));
    QVERIFY(tracker.record(
        QStringLiteral("replacement.example\ntarget.example"),
        QUrl(QStringLiteral("https://replacement.example/"))
    ));
    tracker.clear();
    QCOMPARE(tracker.size(), qsizetype(0));
    QVERIFY(!tracker.contains(
        QStringLiteral("replacement.example\ntarget.example"),
        QUrl(QStringLiteral("https://replacement.example/"))
    ));
}

void PersistenceAndPolicyTests::interfaceLanguagePreferenceRoundTrips()
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

void PersistenceAndPolicyTests::interfaceLanguageSettingsParsing()
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

void PersistenceAndPolicyTests::systemInterfaceLanguageUsesFirstSupportedLanguage()
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

void PersistenceAndPolicyTests::unsupportedSystemInterfaceLanguageFallsBackToEnglish()
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

void PersistenceAndPolicyTests::embeddedTranslationCatalogsLoad()
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
        QCoreApplication::translate("CrossDomainSettingsPage", "Site Connections"),
        QStringLiteral("Подключения сайтов")
    );
    QCOMPARE(
        QCoreApplication::translate("CrossDomainSettingsPage", "Common trackers"),
        QStringLiteral("Распространённые трекеры")
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

void PersistenceAndPolicyTests::bookmarksRoundTripNormalizeAndSearch()
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

void PersistenceAndPolicyTests::bookmarksCanBeEditedAndRemoved()
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

void PersistenceAndPolicyTests::bookmarkEditRejectsDuplicateAddress()
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

void PersistenceAndPolicyTests::corruptBookmarksArePreservedAndDisabled()
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

void PersistenceAndPolicyTests::sessionRoundTripFiltersInvalidUrls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("private/session.json"));
    SessionStore store(path);

    BrowserSession source;
    source.activeIndex = 2;
    source.tabs = {
        {QUrl(QStringLiteral("https://alice:secret@one.example/path")), QStringLiteral("One"), false},
        {QUrl(QStringLiteral("file:///tmp/private")), QStringLiteral("Private")},
        {QUrl(QStringLiteral("http://two.example")), QStringLiteral("Two"), true},
    };

    QString error;
    QVERIFY2(store.save(source, &error), qPrintable(error));
    const BrowserSession restored = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.tabs.size(), 2);
    QCOMPARE(restored.tabs.at(0).title, QStringLiteral("Two"));
    QCOMPARE(restored.tabs.at(0).url, QUrl(QStringLiteral("http://two.example")));
    QVERIFY(restored.tabs.at(0).pinned);
    QCOMPARE(restored.tabs.at(1).url, QUrl(QStringLiteral("https://one.example/path")));
    QVERIFY(!restored.tabs.at(1).pinned);
    QCOMPARE(restored.activeIndex, 0);

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
    QVERIFY(!migrated.tabs.constFirst().pinned);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    const QByteArray migratedContents = saved.readAll();
    QVERIFY(!migratedContents.contains("legacy-secret"));
    QVERIFY(migratedContents.contains("\"version\": 2"));
    QVERIFY(migratedContents.contains("\"pinned\": false"));
}

void PersistenceAndPolicyTests::invalidSessionFileFailsClosed()
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

    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"json({"version":2,"activeIndex":0,"tabs":[{"url":"https://example.com","title":"Example"}]})json"
    );
    file.close();

    error.clear();
    const BrowserSession missingPinState = store.load(&error);
    QVERIFY(missingPinState.tabs.isEmpty());
    QVERIFY(!error.isEmpty());

    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
        R"json({"version":2,"activeIndex":-2147483648,"tabs":[{"url":"https://example.com","title":"Example","pinned":false}]})json"
    );
    file.close();

    error.clear();
    const BrowserSession extremeActiveIndex = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(extremeActiveIndex.tabs.size(), 1);
    QCOMPARE(extremeActiveIndex.activeIndex, 0);
}

void PersistenceAndPolicyTests::oversizedSessionPrioritizesPinnedTabsWithinBound()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.json"));

    QJsonArray tabs;
    for (int index = 0; index < 40; ++index) {
        tabs.append(QJsonObject{
            {QStringLiteral("url"), QStringLiteral("https://regular-%1.example").arg(index)},
            {QStringLiteral("title"), QStringLiteral("Regular %1").arg(index)},
            {QStringLiteral("pinned"), false},
        });
    }
    tabs.append(QJsonObject{
        {QStringLiteral("url"), QStringLiteral("https://pinned.example")},
        {QStringLiteral("title"), QStringLiteral("Pinned")},
        {QStringLiteral("pinned"), true},
    });
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray contents = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 2},
        {QStringLiteral("activeIndex"), 40},
        {QStringLiteral("tabs"), tabs},
    }).toJson();
    QCOMPARE(file.write(contents), contents.size());
    file.close();

    SessionStore store(path);
    QString error;
    const BrowserSession restored = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.tabs.size(), 30);
    QCOMPARE(restored.tabs.constFirst().url, QUrl(QStringLiteral("https://pinned.example")));
    QVERIFY(restored.tabs.constFirst().pinned);
    QCOMPARE(restored.activeIndex, 0);

    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument rewritten = QJsonDocument::fromJson(file.readAll());
    QCOMPARE(rewritten.object().value(QStringLiteral("tabs")).toArray().size(), 30);
}

void PersistenceAndPolicyTests::managedDataCleanupStaysInsideRoot()
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

void PersistenceAndPolicyTests::downloadHistoryRoundTripAndLimit()
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

void PersistenceAndPolicyTests::sensitivePermissionsRequireSecureOrigin()
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

void PersistenceAndPolicyTests::unsupportedPermissionsAreDenied()
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

void PersistenceAndPolicyTests::webSchemesStayInsideBrowser()
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

void PersistenceAndPolicyTests::externalSchemesRequireMainFrameConfirmation()
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

void PersistenceAndPolicyTests::dangerousLocalSchemesAreBlocked()
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


int runPersistenceAndPolicyTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    PersistenceAndPolicyTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "PersistenceAndPolicyTests.moc"
