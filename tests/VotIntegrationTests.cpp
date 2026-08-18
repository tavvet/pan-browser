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

class VotIntegrationTests final : public QObject {
    Q_OBJECT

private slots:
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
};

void VotIntegrationTests::videoTranslationSettingsRoundTripAndValidateSource()
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

void VotIntegrationTests::votUserscriptPackageRejectsUnverifiedFilesAndMatchesUrls()
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

void VotIntegrationTests::votUserscriptPackageLoadsOfficialFileWhenProvided()
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

void VotIntegrationTests::votUserscriptStorageIsScriptScopedAndPersistent()
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

void VotIntegrationTests::votChromiumTransportExtensionIsPreparedSafely()
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

void VotIntegrationTests::votChromiumTransportLoadsInBrowserProfile()
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

void VotIntegrationTests::votChromiumTransportRetriesAfterInitializationError()
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

void VotIntegrationTests::votChromiumTransportFailsRequestsBeforePublishingError()
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

void VotIntegrationTests::votNetworkDestinationsRequireHttpsAndDeclaredHosts()
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

void VotIntegrationTests::votRequestsUseCrossDomainAndFailClosedPolicies()
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

void VotIntegrationTests::votChromiumTransportRegistryFailsClosed()
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

void VotIntegrationTests::votUserscriptBridgeInjectsOnlyOnMatchingPages()
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

void VotIntegrationTests::votUserscriptBridgeInjectsIntoMatchingSubframes()
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


int runVotIntegrationTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    VotIntegrationTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "VotIntegrationTests.moc"
