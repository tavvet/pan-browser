#include "PanBrowserTestCommon.h"

class ApplicationAndWebAppTests final : public QObject {
    Q_OBJECT

private slots:
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
};

void ApplicationAndWebAppTests::applicationLaunchRequestsAreValidatedAndRoundTrip()
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

void ApplicationAndWebAppTests::applicationLaunchRequestsAreForwardedToPrimaryInstance()
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

void ApplicationAndWebAppTests::webAppManifestIsValidatedAndNormalized()
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

void ApplicationAndWebAppTests::webAppManifestIdUsesStartOrigin()
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

void ApplicationAndWebAppTests::webAppManifestRejectsUnsafeOriginsAndScopes()
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

void ApplicationAndWebAppTests::webAppStoreRoundTripsAndRemovesApps()
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

void ApplicationAndWebAppTests::corruptWebAppStoreIsPreservedAndDisabled()
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

void ApplicationAndWebAppTests::webAppStoreRejectsNonArrayApps()
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

void ApplicationAndWebAppTests::webAppShortcutNamesStayInsideTheirDirectory()
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
void ApplicationAndWebAppTests::macWebAppShortcutRoundTrips()
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

int runApplicationAndWebAppTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    ApplicationAndWebAppTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "ApplicationAndWebAppTests.moc"
