#include "PanBrowserTestCommon.h"
#include "CredentialStorePayload.h"

#include <QDataStream>
#include <QIODevice>

#include <algorithm>

namespace {

class FakeCredentialStore final : public CredentialStore {
public:
    explicit FakeCredentialStore(bool available = true)
        : m_available(available)
    {
    }

    [[nodiscard]] bool isAvailable() const override
    {
        return m_available;
    }

    [[nodiscard]] std::optional<StoredCredential> read(
        const CredentialTarget &target,
        CredentialStoreError *error
    ) override
    {
        if (error)
            error->clear();
        ++readCount;
        lastTarget = target;
        const auto found = credentials.constFind(target.identifier());
        if (found == credentials.cend()) {
            if (error)
                error->code = CredentialStoreErrorCode::NotFound;
            return std::nullopt;
        }
        return *found;
    }

    bool write(
        const CredentialTarget &target,
        const StoredCredential &credential,
        CredentialStoreError *error
    ) override
    {
        if (error)
            error->clear();
        ++writeCount;
        lastTarget = target;
        credentials.insert(target.identifier(), credential);
        targets.insert(target.identifier(), target);
        return true;
    }

    bool remove(
        const CredentialTarget &target,
        CredentialStoreError *error
    ) override
    {
        if (error)
            error->clear();
        ++removeCount;
        lastTarget = target;
        credentials.remove(target.identifier());
        targets.remove(target.identifier());
        return true;
    }

    [[nodiscard]] QList<StoredCredentialSummary> list(
        CredentialStoreError *error
    ) override
    {
        if (error)
            error->clear();
        QList<StoredCredentialSummary> summaries;
        for (auto iterator = targets.cbegin(); iterator != targets.cend(); ++iterator) {
            const auto credential = credentials.constFind(iterator.key());
            if (credential == credentials.cend())
                continue;
            summaries.append(StoredCredentialSummary{
                iterator.value(),
                credential->username,
                {},
            });
        }
        return summaries;
    }

    bool m_available = true;
    int readCount = 0;
    int writeCount = 0;
    int removeCount = 0;
    CredentialTarget lastTarget;
    QHash<QString, StoredCredential> credentials;
    QHash<QString, CredentialTarget> targets;
};

} // namespace

class NetworkSettingsAndAuthTests final : public QObject {
    Q_OBJECT

private slots:
    void dnsSettingsDefaultToSystemAndIncludeBuiltIns();
    void dnsSettingsRoundTripCustomProvidersAndCreateBackup();
    void dnsSettingsRejectOversizedConfigurationWithoutWriting();
    void dnsSettingsRejectUnsafeTemplatesAndApplyModes();
    void proxySettingsDefaultToSystemAndRoundTrip();
    void proxySettingsRejectUnsafeManualConfiguration();
    void proxySettingsApplyGlobalModes();
    void proxySettingsCompareOnlyEffectiveConfiguration();
    void proxyFailureBlocksWebEngineNetworkSchemes();
    void credentialTargetsAreOriginScopedAndDeterministic();
    void credentialPayloadRoundTripsMetadataAndLegacyRecords();
    void httpAuthenticationAcceptsCredentialsAndSanitizesDisplay();
    void httpAuthenticationCancelClearsAuthenticator();
    void httpAuthenticationRetriesAndWarnsForPlainHttp();
    void httpAuthenticationUsesAndRejectsSavedCredentialsWithoutLooping();
    void httpAuthenticationDoesNotPersistNonRealmSchemes();
    void httpAuthenticationDoesNotSaveWithoutOptIn();
    void httpAuthenticationSavesOnlyWithExplicitOptIn();
    void httpAuthenticationPolicyRejectsUnsafePromptContexts();
    void httpAuthenticationRealmDisplayRemovesControlCharacters();
    void proxyAuthenticationUsesSharedCredentialDialog();
    void manualProxyAuthenticationUsesSavedCredentials();
    void nativeCredentialStoreRoundTripsWhenEnabled();
};

void NetworkSettingsAndAuthTests::dnsSettingsDefaultToSystemAndIncludeBuiltIns()
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

void NetworkSettingsAndAuthTests::dnsSettingsRoundTripCustomProvidersAndCreateBackup()
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

void NetworkSettingsAndAuthTests::dnsSettingsRejectOversizedConfigurationWithoutWriting()
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

void NetworkSettingsAndAuthTests::dnsSettingsRejectUnsafeTemplatesAndApplyModes()
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

void NetworkSettingsAndAuthTests::proxySettingsDefaultToSystemAndRoundTrip()
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

void NetworkSettingsAndAuthTests::proxySettingsRejectUnsafeManualConfiguration()
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

void NetworkSettingsAndAuthTests::proxySettingsApplyGlobalModes()
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

void NetworkSettingsAndAuthTests::proxySettingsCompareOnlyEffectiveConfiguration()
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

void NetworkSettingsAndAuthTests::proxyFailureBlocksWebEngineNetworkSchemes()
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

void NetworkSettingsAndAuthTests::credentialTargetsAreOriginScopedAndDeterministic()
{
    const auto first = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://EXAMPLE.com/private?token=secret")),
        QStringLiteral("members")
    );
    const auto sameOrigin = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://example.com:443/other")),
        QStringLiteral("members")
    );
    const auto otherRealm = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://example.com/")),
        QStringLiteral("administrators")
    );
    const auto otherPort = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://example.com:444/")),
        QStringLiteral("members")
    );
    QVERIFY(first);
    QVERIFY(sameOrigin);
    QVERIFY(otherRealm);
    QVERIFY(otherPort);
    QCOMPARE(first->identifier(), sameOrigin->identifier());
    QVERIFY(first->identifier() != otherRealm->identifier());
    QVERIFY(first->identifier() != otherPort->identifier());
    QVERIFY(!CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("http://example.com/")),
        QStringLiteral("members")
    ));
    QVERIFY(!first->identifier().contains(QStringLiteral("example")));
}

void NetworkSettingsAndAuthTests::credentialPayloadRoundTripsMetadataAndLegacyRecords()
{
    const auto target = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://example.com/private")),
        QStringLiteral("members")
    );
    QVERIFY(target);
    const StoredCredential credential{
        QStringLiteral("alice"),
        QStringLiteral("secret"),
    };
    QByteArray payload = encodeCredentialPayload(*target, credential);
    QVERIFY(!payload.isEmpty());
    const auto decoded = decodeCredentialPayload(payload);
    QVERIFY(decoded);
    QVERIFY(decoded->target);
    QCOMPARE(decoded->target->identifier(), target->identifier());
    QCOMPARE(decoded->credential.username, credential.username);
    QCOMPARE(decoded->credential.password, credential.password);

    for (qsizetype size = 0; size < payload.size(); ++size)
        QVERIFY(!decodeCredentialPayload(payload.first(size)));

    QByteArray impossibleLengthPayload = payload;
    QVERIFY(impossibleLengthPayload.size() > 12);
    impossibleLengthPayload.replace(8, 4, QByteArray::fromHex("7fffffff"));
    QVERIFY(!decodeCredentialPayload(impossibleLengthPayload));

    QByteArray oddStringLengthPayload = payload;
    oddStringLengthPayload.replace(8, 4, QByteArray::fromHex("00000001"));
    QVERIFY(!decodeCredentialPayload(oddStringLengthPayload));

    QByteArray oversizedPayload(128 * 1024 + 1, '\0');
    QVERIFY(!decodeCredentialPayload(oversizedPayload));

    payload.append('x');
    QVERIFY(!decodeCredentialPayload(payload));

    QByteArray legacyPayload;
    QDataStream stream(&legacyPayload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint32(0x50424352)
           << quint16(1)
           << QStringLiteral("legacy-user")
           << QStringLiteral("legacy-password");
    const auto legacy = decodeCredentialPayload(legacyPayload);
    QVERIFY(legacy);
    QVERIFY(!legacy->target);
    QCOMPARE(legacy->credential.username, QStringLiteral("legacy-user"));
    QCOMPARE(legacy->credential.password, QStringLiteral("legacy-password"));

    CredentialStoreError notFound;
    notFound.code = CredentialStoreErrorCode::NotFound;
    QVERIFY(!notFound.shouldReport());
    notFound.code = CredentialStoreErrorCode::AccessDenied;
    QVERIFY(notFound.shouldReport());
    notFound.clear();
    QCOMPARE(notFound.code, CredentialStoreErrorCode::None);
}

void NetworkSettingsAndAuthTests::httpAuthenticationAcceptsCredentialsAndSanitizesDisplay()
{
    FakeCredentialStore store(false);
    HttpAuthenticationController controller(nullptr, &store);
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

void NetworkSettingsAndAuthTests::httpAuthenticationCancelClearsAuthenticator()
{
    FakeCredentialStore store(false);
    HttpAuthenticationController controller(nullptr, &store);
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

void NetworkSettingsAndAuthTests::httpAuthenticationRetriesAndWarnsForPlainHttp()
{
    FakeCredentialStore store(true);
    HttpAuthenticationController controller(nullptr, &store);
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
    bool rememberWasHidden = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        retryHandled = true;
        retryWasVisible = dialog->findChild<QLabel *>(QStringLiteral("errorText"));
        warningWasVisible = dialog->findChild<QLabel *>(
            QStringLiteral("insecureTransportWarning")
        );
        rememberWasHidden = !dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
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
    QVERIFY(rememberWasHidden);
    QVERIFY(retryAttempt.isNull());
    QCOMPARE(store.readCount, 0);
    QCOMPARE(store.writeCount, 0);
}

void NetworkSettingsAndAuthTests::httpAuthenticationUsesAndRejectsSavedCredentialsWithoutLooping()
{
    FakeCredentialStore store;
    const QUrl url(QStringLiteral("https://example.com/private"));
    const QString realm = QStringLiteral("members");
    const auto target = CredentialTarget::forHttpServer(url, realm);
    QVERIFY(target);
    store.credentials.insert(
        target->identifier(),
        StoredCredential{QStringLiteral("saved-user"), QStringLiteral("saved-password")}
    );
    HttpAuthenticationController controller(nullptr, &store);

    QAuthenticator firstAttempt;
    firstAttempt.setRealm(realm);
    controller.requestAuthentication(nullptr, url, &firstAttempt);
    QCOMPARE(firstAttempt.user(), QStringLiteral("saved-user"));
    QCOMPARE(firstAttempt.password(), QStringLiteral("saved-password"));
    QCOMPARE(store.readCount, 1);

    bool retryHandled = false;
    bool savedCredentialWarningVisible = false;
    bool replacementWasSelected = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        retryHandled = true;
        const QLabel *error = dialog->findChild<QLabel *>(QStringLiteral("errorText"));
        savedCredentialWarningVisible = error
            && error->text().contains(QStringLiteral("were removed"));
        const auto *remember = dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        replacementWasSelected = remember && remember->isChecked();
        dialog->reject();
    });
    QAuthenticator retryAttempt;
    retryAttempt.setRealm(realm);
    controller.requestAuthentication(nullptr, url, &retryAttempt);
    QVERIFY(retryHandled);
    QVERIFY(savedCredentialWarningVisible);
    QVERIFY(replacementWasSelected);
    QVERIFY(retryAttempt.isNull());
    QCOMPARE(store.readCount, 1);
    QCOMPARE(store.removeCount, 1);
    QVERIFY(!store.credentials.contains(target->identifier()));
}

void NetworkSettingsAndAuthTests::httpAuthenticationDoesNotPersistNonRealmSchemes()
{
    FakeCredentialStore store;
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator authenticator;
    bool rememberWasHidden = false;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        rememberWasHidden = !dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        dialog->reject();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator
    );

    QVERIFY(rememberWasHidden);
    QCOMPARE(store.readCount, 0);
    QCOMPARE(store.writeCount, 0);
}

void NetworkSettingsAndAuthTests::httpAuthenticationDoesNotSaveWithoutOptIn()
{
    FakeCredentialStore store;
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator authenticator;
    authenticator.setRealm(QStringLiteral("members"));

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
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
        username->setText(QStringLiteral("session-user"));
        password->setText(QStringLiteral("session-secret"));
        dialog->accept();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator
    );

    QCOMPARE(store.writeCount, 0);
    QCOMPARE(authenticator.user(), QStringLiteral("session-user"));
    QCOMPARE(authenticator.password(), QStringLiteral("session-secret"));
}

void NetworkSettingsAndAuthTests::httpAuthenticationSavesOnlyWithExplicitOptIn()
{
    FakeCredentialStore store;
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator authenticator;
    authenticator.setRealm(QStringLiteral("members"));
    bool rememberWasOffByDefault = false;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        auto *remember = dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        if (!username || !password || !remember) {
            dialog->reject();
            return;
        }
        rememberWasOffByDefault = !remember->isChecked();
        username->setText(QStringLiteral("alice"));
        password->setText(QStringLiteral("secret"));
        remember->setChecked(true);
        dialog->accept();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator
    );

    QVERIFY(rememberWasOffByDefault);
    QCOMPARE(store.writeCount, 1);
    QCOMPARE(store.lastTarget.host, QStringLiteral("example.com"));
    QCOMPARE(store.lastTarget.port, 443);
    QCOMPARE(store.lastTarget.realm, QStringLiteral("members"));
    QCOMPARE(authenticator.user(), QStringLiteral("alice"));
    QCOMPARE(authenticator.password(), QStringLiteral("secret"));

    bool rejectedSavedCredentialWasReported = false;
    bool replacementWasSelected = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        const auto *error = dialog->findChild<QLabel *>(QStringLiteral("errorText"));
        rejectedSavedCredentialWasReported = error
            && error->text().contains(QStringLiteral("were removed"));
        const auto *remember = dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        replacementWasSelected = remember && remember->isChecked();
        dialog->reject();
    });
    QAuthenticator retryAttempt;
    retryAttempt.setRealm(QStringLiteral("members"));
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &retryAttempt
    );

    QVERIFY(rejectedSavedCredentialWasReported);
    QVERIFY(replacementWasSelected);
    QCOMPARE(store.removeCount, 1);
    QVERIFY(store.credentials.isEmpty());
}

void NetworkSettingsAndAuthTests::httpAuthenticationPolicyRejectsUnsafePromptContexts()
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

void NetworkSettingsAndAuthTests::httpAuthenticationRealmDisplayRemovesControlCharacters()
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

void NetworkSettingsAndAuthTests::proxyAuthenticationUsesSharedCredentialDialog()
{
    ProxySettings settings = ProxySettings::defaults();
    settings.setMode(ProxyMode::Manual);
    settings.setManualType(ManualProxyType::Http);
    settings.setHost(QStringLiteral("proxy.example.com"));
    settings.setPort(3128);
    settings.setUsername(QStringLiteral("configured-user"));
    FakeCredentialStore store(false);
    ProxyAuthenticationController controller(settings, nullptr, &store);
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

void NetworkSettingsAndAuthTests::manualProxyAuthenticationUsesSavedCredentials()
{
    ProxySettings settings = ProxySettings::defaults();
    settings.setMode(ProxyMode::Manual);
    settings.setManualType(ManualProxyType::Http);
    settings.setHost(QStringLiteral("proxy.example.com"));
    settings.setPort(3128);
    FakeCredentialStore store;
    const auto target = CredentialTarget::forHttpProxy(
        QStringLiteral("proxy.example.com"),
        3128,
        QStringLiteral("proxy-realm")
    );
    QVERIFY(target);
    store.credentials.insert(
        target->identifier(),
        StoredCredential{QStringLiteral("proxy-user"), QStringLiteral("proxy-secret")}
    );
    ProxyAuthenticationController controller(settings, nullptr, &store);
    QAuthenticator authenticator;
    authenticator.setRealm(QStringLiteral("proxy-realm"));

    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator,
        QStringLiteral("proxy.example.com")
    );

    QCOMPARE(store.readCount, 1);
    QCOMPARE(authenticator.user(), QStringLiteral("proxy-user"));
    QCOMPARE(authenticator.password(), QStringLiteral("proxy-secret"));
}

void NetworkSettingsAndAuthTests::nativeCredentialStoreRoundTripsWhenEnabled()
{
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    QSKIP("The native credential-store integration test is unavailable on this platform");
#else
    if (!qEnvironmentVariableIsSet("PANBROWSER_RUN_CREDENTIAL_STORE_TESTS")
        && !qEnvironmentVariableIsSet("PANBROWSER_RUN_KEYCHAIN_TESTS")) {
        QSKIP("Set PANBROWSER_RUN_CREDENTIAL_STORE_TESTS=1 to exercise the native credential store");
    }

    std::unique_ptr<CredentialStore> store = createSystemCredentialStore();
    QVERIFY(store);
    QVERIFY(store->isAvailable());
    const QString uniqueHost = QStringLiteral("credential-store-test-%1.invalid").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
    );
    const auto target = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://%1/").arg(uniqueHost)),
        QStringLiteral("PanBrowser automated credential-store test")
    );
    QVERIFY(target);

    struct Cleanup final {
        CredentialStore *store = nullptr;
        CredentialTarget target;
        bool active = true;
        ~Cleanup()
        {
            if (!active || !store)
                return;
            CredentialStoreError ignoredError;
            store->remove(target, &ignoredError);
        }
    } cleanup{store.get(), *target};

    CredentialStoreError error;
    QVERIFY2(store->write(
        *target,
        StoredCredential{QStringLiteral("test-user"), QStringLiteral("test-password")},
        &error
    ), qPrintable(error.message));
    auto loaded = store->read(*target, &error);
    QVERIFY2(loaded.has_value(), qPrintable(error.message));
    QCOMPARE(loaded->username, QStringLiteral("test-user"));
    QCOMPARE(loaded->password, QStringLiteral("test-password"));

    const QList<StoredCredentialSummary> initialSummaries = store->list(&error);
    QVERIFY2(!error.shouldReport(), qPrintable(error.message));
    const auto initialSummary = std::find_if(
        initialSummaries.cbegin(),
        initialSummaries.cend(),
        [&](const StoredCredentialSummary &summary) {
            return summary.target.identifier() == target->identifier();
        }
    );
    QVERIFY(initialSummary != initialSummaries.cend());
    QCOMPARE(initialSummary->username, QStringLiteral("test-user"));
    QVERIFY(initialSummary->lastModified.isValid());

    QVERIFY2(store->write(
        *target,
        StoredCredential{QStringLiteral("updated-user"), QStringLiteral("updated-password")},
        &error
    ), qPrintable(error.message));
    loaded = store->read(*target, &error);
    QVERIFY2(loaded.has_value(), qPrintable(error.message));
    QCOMPARE(loaded->username, QStringLiteral("updated-user"));
    QCOMPARE(loaded->password, QStringLiteral("updated-password"));
    QVERIFY2(store->remove(*target, &error), qPrintable(error.message));
    loaded = store->read(*target, &error);
    QVERIFY2(!loaded.has_value(), qPrintable(error.message));
    QCOMPARE(error.code, CredentialStoreErrorCode::NotFound);
    cleanup.active = false;
#endif
}


int runNetworkSettingsAndAuthTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    NetworkSettingsAndAuthTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "NetworkSettingsAndAuthTests.moc"
