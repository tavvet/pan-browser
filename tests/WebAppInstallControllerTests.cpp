#include "PanBrowserTestCommon.h"
#include "WebAppInstallController.h"

#include <QAction>
#include <QWebEngineProfile>

class WebAppInstallControllerTests final : public QObject {
    Q_OBJECT

private slots:
    void detectedManifestUpdatesInstallAction();
    void clearedManifestIgnoresPendingDetection();
};

void WebAppInstallControllerTests::detectedManifestUpdatesInstallAction()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WebAppStore store(directory.filePath(QStringLiteral("web-apps.json")));
    QString error;
    QVERIFY2(store.load(&error), qPrintable(error));

    QAction installAction;
    QWidget dialogParent;
    WebAppInstallController controller(
        &store,
        &installAction,
        &dialogParent
    );

    QWebEngineView view;
    auto *page = new BrowserPage(QWebEngineProfile::defaultProfile(), &view);
    view.setPage(page);
    controller.currentViewChanged(&view);
    QVERIFY(!installAction.isEnabled());
    QCOMPARE(installAction.text(), QStringLiteral("Install Web App…"));

    QSignalSpy loadSpy(page, &QWebEnginePage::loadFinished);
    page->setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html>
<head>
  <title>Example Application</title>
  <link rel="manifest" href="/app/manifest.webmanifest">
</head>
<body></body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://example.com/app/index.html"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());

    controller.detectManifest(&view, page);
    QTRY_VERIFY_WITH_TIMEOUT(installAction.isEnabled(), 3000);
    QCOMPARE(
        installAction.text(),
        QStringLiteral("Install “Example Application”…")
    );

    std::optional<WebApp> app = WebAppStore::parseManifest(
        QByteArrayLiteral(
            "{\"name\":\"Installed App\",\"start_url\":\"/app/\","
            "\"scope\":\"/app/\"}"
        ),
        QUrl(QStringLiteral("https://example.com/app/manifest.webmanifest")),
        QUrl(QStringLiteral("https://example.com/app/index.html")),
        QString(),
        &error
    );
    QVERIFY2(app.has_value(), qPrintable(error));
    QVERIFY2(store.install(*app, &error), qPrintable(error));

    controller.currentViewChanged(&view);
    QVERIFY(installAction.isEnabled());
    QCOMPARE(installAction.text(), QStringLiteral("Open “Installed App”"));

    controller.clearManifest(&view);
    QVERIFY(!installAction.isEnabled());
    QCOMPARE(installAction.text(), QStringLiteral("Install Web App…"));
}

void WebAppInstallControllerTests::clearedManifestIgnoresPendingDetection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WebAppStore store(directory.filePath(QStringLiteral("web-apps.json")));
    QString error;
    QVERIFY2(store.load(&error), qPrintable(error));

    QAction installAction;
    QWidget dialogParent;
    WebAppInstallController controller(
        &store,
        &installAction,
        &dialogParent
    );

    QWebEngineView view;
    auto *page = new BrowserPage(QWebEngineProfile::defaultProfile(), &view);
    view.setPage(page);
    controller.currentViewChanged(&view);

    QSignalSpy loadSpy(page, &QWebEnginePage::loadFinished);
    page->setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html>
<head>
  <title>Stale Application</title>
  <link rel="manifest" href="/app/manifest.webmanifest">
</head>
<body></body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://example.com/app/index.html"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());

    controller.detectManifest(&view, page);
    controller.clearManifest(&view);

    bool javaScriptBarrierReached = false;
    page->runJavaScript(
        QStringLiteral("true"),
        QWebEngineScript::ApplicationWorld,
        [&javaScriptBarrierReached](const QVariant &) {
            javaScriptBarrierReached = true;
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(javaScriptBarrierReached, 3000);
    QTest::qWait(50);

    QVERIFY(!installAction.isEnabled());
    QCOMPARE(installAction.text(), QStringLiteral("Install Web App…"));
}

int runWebAppInstallControllerTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    WebAppInstallControllerTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "WebAppInstallControllerTests.moc"
