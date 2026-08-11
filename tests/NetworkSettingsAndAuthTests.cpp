#include "PanBrowserTestCommon.h"

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
    void httpAuthenticationAcceptsCredentialsAndSanitizesDisplay();
    void httpAuthenticationCancelClearsAuthenticator();
    void httpAuthenticationRetriesAndWarnsForPlainHttp();
    void httpAuthenticationPolicyRejectsUnsafePromptContexts();
    void httpAuthenticationRealmDisplayRemovesControlCharacters();
    void proxyAuthenticationUsesSharedCredentialDialog();
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

void NetworkSettingsAndAuthTests::httpAuthenticationAcceptsCredentialsAndSanitizesDisplay()
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

void NetworkSettingsAndAuthTests::httpAuthenticationCancelClearsAuthenticator()
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

void NetworkSettingsAndAuthTests::httpAuthenticationRetriesAndWarnsForPlainHttp()
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


int runNetworkSettingsAndAuthTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    NetworkSettingsAndAuthTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "NetworkSettingsAndAuthTests.moc"
