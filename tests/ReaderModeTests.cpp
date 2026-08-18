#include "PanBrowserTestCommon.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

class ReaderModeTests final : public QObject {
    Q_OBJECT

private slots:
    void readerModeSettingsAndUrlPolicy();
    void readerModeWaitsForCompletedLoadsBeforeProbing();
    void readerModeDetectsDelayedSinglePageArticles();
    void readerModeRejectsOversizedExtractedMarkup();
    void readerModeBoundsExtractedMetadata();
    void readerModeInvalidatesAvailabilityWhenArticleDisappears();
    void readerModePreservesResponsiveImages();
    void readerModeExtractsArticleAndRestoresOriginalPage();
    void readerModeRestoresPageBeforeSameDocumentNavigation();
};

void ReaderModeTests::readerModeSettingsAndUrlPolicy()
{
    ReaderSettings settings;
    QCOMPARE(settings.themeName(), QStringLiteral("system"));
    QCOMPARE(settings.typefaceName(), QStringLiteral("serif"));

    settings.setTextSize(ReaderSettings::minimumTextSize - 50);
    QCOMPARE(settings.textSize(), ReaderSettings::minimumTextSize);
    settings.setTextSize(ReaderSettings::maximumTextSize + 50);
    QCOMPARE(settings.textSize(), ReaderSettings::maximumTextSize);
    settings.setContentWidth(ReaderSettings::minimumContentWidth - 500);
    QCOMPARE(settings.contentWidth(), ReaderSettings::minimumContentWidth);
    settings.setContentWidth(ReaderSettings::maximumContentWidth + 500);
    QCOMPARE(settings.contentWidth(), ReaderSettings::maximumContentWidth);

    QString error;
    QVERIFY2(settings.validate(&error), qPrintable(error));
    QVERIFY(ReaderModeController::supportsUrl(
        QUrl(QStringLiteral("https://example.com/article#section"))
    ));
    QVERIFY(ReaderModeController::supportsUrl(
        QUrl(QStringLiteral("http://example.com/article"))
    ));
    QVERIFY(!ReaderModeController::supportsUrl(QUrl(QStringLiteral("file:///tmp/article"))));
    QVERIFY(!ReaderModeController::supportsUrl(QUrl(QStringLiteral("data:text/html,article"))));
    QVERIFY(!ReaderModeController::supportsUrl(QUrl(QStringLiteral("https:///missing-host"))));
}

void ReaderModeTests::readerModeWaitsForCompletedLoadsBeforeProbing()
{
    QTcpServer server;
    QList<QPointer<QTcpSocket>> pendingResources;
    connect(&server, &QTcpServer::newConnection, &server, [&server, &pendingResources] {
        while (QTcpSocket *socket = server.nextPendingConnection()) {
            socket->setParent(&server);
            connect(socket, &QTcpSocket::readyRead, socket, [socket, &pendingResources] {
                if (socket->property("panbrowserHandled").toBool() || !socket->canReadLine())
                    return;
                socket->setProperty("panbrowserHandled", true);
                const QByteArray requestLine = socket->readLine();
                socket->readAll();
                if (requestLine.contains(" /slow-resource ")) {
                    pendingResources.append(socket);
                    return;
                }

                const QByteArray body = QByteArrayLiteral(R"HTML(
<!doctype html>
<html>
<head><title>A deliberately slow article</title></head>
<body>
<article>
  <h1>Reader mode must wait for load completion</h1>
  <p>This substantial opening paragraph makes the partially loaded document look like a readable article while an intentionally delayed resource keeps the main-frame load active.</p>
  <p>The second paragraph verifies that URL changes from an ordinary navigation must not trigger the same retry path that is reserved for completed single-page documents.</p>
  <p>The third paragraph supplies enough editorial prose for the normal readerability heuristic to succeed immediately after the delayed resource has completed.</p>
  <p>The final paragraph ensures that the fixture remains stable and that Reader Mode becomes available only after QWebEnginePage reports the completed load.</p>
  <img src="/slow-resource" alt="Delayed test resource">
</article>
</body>
</html>
)HTML");
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

    BrowserPage page(QWebEngineProfile::defaultProfile());
    ReaderSettings settings;
    ReaderModeController controller(&page, &settings);
    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/article").arg(server.serverPort())));

    QTRY_VERIFY_WITH_TIMEOUT(page.isLoading() && !pendingResources.isEmpty(), 5000);
    QTest::qWait(1000);
    QCOMPARE(controller.availability(), ReaderModeController::Availability::Unknown);

    for (const QPointer<QTcpSocket> &socket : std::as_const(pendingResources)) {
        if (!socket)
            continue;
        socket->write(QByteArrayLiteral(
            "HTTP/1.1 204 No Content\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"
        ));
        socket->disconnectFromHost();
    }
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.availability(),
        ReaderModeController::Availability::Available,
        5000
    );
}

void ReaderModeTests::readerModeDetectsDelayedSinglePageArticles()
{
    BrowserPage page(QWebEngineProfile::defaultProfile());
    ReaderSettings settings;
    ReaderModeController controller(&page, &settings);

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html>
<head><title>Loading article</title></head>
<body>
<main id="content">Loading…</main>
<script>
setTimeout(() => {
    document.querySelector("#content").innerHTML = `
        <article class="article-presenter__content">
            <h1>A dynamically rendered article</h1>
            <p>This substantial opening paragraph represents an article delivered after the initial document load. It contains enough ordinary prose for the readerability heuristic to recognize the page once client-side rendering has completed.</p>
            <p>The second paragraph confirms that a single-page application may replace a loading shell without causing a traditional page load. Reader mode should notice that transition and offer a clean reading presentation.</p>
            <p>The final paragraph supplies additional editorial text and verifies that delayed content is not permanently classified using the empty loading shell that happened to exist at loadFinished time.</p>
        </article>`;
    history.pushState({}, "", "#article");
}, 1600);
</script>
</body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://reader.example/feed"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.availability(),
        ReaderModeController::Availability::Available,
        6000
    );
    QCOMPARE(page.url(), QUrl(QStringLiteral("https://reader.example/feed#article")));
}

void ReaderModeTests::readerModeRejectsOversizedExtractedMarkup()
{
    BrowserPage page(QWebEngineProfile::defaultProfile());
    ReaderSettings settings;
    ReaderModeController controller(&page, &settings);

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html>
<head><title>An oversized extracted article</title></head>
<body>
<article>
  <h1>Reader mode must bound extracted markup</h1>
  <p>This substantial opening paragraph makes the document readerable before activation while a generated attribute exercises the independent serialized-markup limit.</p>
  <p>The second paragraph ensures that the lightweight availability probe succeeds without needing to inspect or copy the unusually large attribute value.</p>
  <p>The third paragraph supplies enough editorial text for a stable readerability score and keeps the test focused on activation rather than initial detection.</p>
  <a id="oversized" href="#oversized">Oversized metadata</a>
</article>
<script>
document.querySelector("#oversized").title = "x".repeat(6100000);
</script>
</body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://reader.example/oversized"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.availability(),
        ReaderModeController::Availability::Available,
        5000
    );

    controller.activate();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isActivationPending(), 5000);
    QVERIFY(!controller.isActive());
    QCOMPARE(
        controller.availability(),
        ReaderModeController::Availability::Unavailable
    );
}

void ReaderModeTests::readerModeBoundsExtractedMetadata()
{
    BrowserPage page(QWebEngineProfile::defaultProfile());
    ReaderSettings settings;
    ReaderModeController controller(&page, &settings);

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html lang="en">
<head>
  <title>Initial title</title>
  <meta name="author" content="Initial author">
  <meta property="og:site_name" content="Initial site">
  <meta property="article:published_time" content="Initial publication time">
</head>
<body>
<article>
  <p>This substantial opening paragraph makes the document suitable for Reader Mode while oversized metadata remains outside the extracted article body.</p>
  <p>The second paragraph confirms that presentation metadata has independent limits and cannot create an unexpectedly large Reader Mode document.</p>
  <p>The third paragraph supplies enough ordinary editorial prose for reliable readerability detection without contributing unusual markup.</p>
</article>
<script>
document.title = "T".repeat(20000);
document.querySelector('meta[name="author"]').content = "A".repeat(20000);
document.querySelector('meta[property="og:site_name"]').content = "S".repeat(20000);
document.querySelector('meta[property="article:published_time"]').content = "P".repeat(20000);
document.documentElement.lang = "x".repeat(20000);
</script>
</body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://reader.example/metadata"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.availability(),
        ReaderModeController::Availability::Available,
        5000
    );

    controller.activate();
    QTRY_VERIFY_WITH_TIMEOUT(controller.isActive(), 5000);

    QString readerState;
    page.runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const state = globalThis.__panBrowserReader;
    const options = globalThis.__panBrowserReaderOptions;
    if (!state || !state.active || !options)
        return "";
    const title = state.root.querySelector(".title");
    const metadata = state.root.querySelector(".meta");
    const content = state.root.querySelector(".article");
    const metadataText = metadata ? metadata.textContent : "";
    return JSON.stringify({
        titleLength: title ? title.textContent.length : -1,
        titleEndsWithEllipsis: Boolean(title && title.textContent.endsWith("…")),
        metadataLength: metadataText.length,
        metadataEllipsisCount: (metadataText.match(/…/g) || []).length,
        languagePresent: Boolean(content && content.hasAttribute("lang")),
        maximumTitleLength: options.maximumTitleLength,
        maximumMetadataLength: options.maximumMetadataLength
    });
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&readerState](const QVariant &result) {
            readerState = result.toString();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(!readerState.isEmpty(), 5000);
    const QJsonObject readerObject = QJsonDocument::fromJson(readerState.toUtf8()).object();
    const int maximumTitle = readerObject.value(QStringLiteral("maximumTitleLength")).toInt();
    const int maximumMetadata = readerObject.value(QStringLiteral("maximumMetadataLength")).toInt();
    QCOMPARE(readerObject.value(QStringLiteral("titleLength")).toInt(), maximumTitle);
    QVERIFY(readerObject.value(QStringLiteral("titleEndsWithEllipsis")).toBool());
    QCOMPARE(
        readerObject.value(QStringLiteral("metadataLength")).toInt(),
        maximumMetadata * 3 + 6
    );
    QCOMPARE(readerObject.value(QStringLiteral("metadataEllipsisCount")).toInt(), 3);
    QVERIFY(!readerObject.value(QStringLiteral("languagePresent")).toBool());
}

void ReaderModeTests::readerModeInvalidatesAvailabilityWhenArticleDisappears()
{
    BrowserPage page(QWebEngineProfile::defaultProfile());
    ReaderSettings settings;
    ReaderModeController controller(&page, &settings);

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html>
<head><title>A transient article</title></head>
<body>
<article>
  <h1>An article that disappears before activation</h1>
  <p>This substantial opening paragraph lets the readerability probe offer Reader Mode before a client-side application replaces the article without changing its URL.</p>
  <p>The second paragraph provides ordinary editorial prose and confirms that successful detection is only a snapshot of the current document state.</p>
  <p>The third paragraph ensures the initial page comfortably passes the heuristic before the integration test removes the content.</p>
</article>
</body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://reader.example/transient"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.availability(),
        ReaderModeController::Availability::Available,
        5000
    );

    bool articleRemoved = false;
    page.runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const article = document.querySelector("article");
    article.replaceChildren(document.createTextNode("Article removed"));
    return true;
})()
)JS"),
        [&articleRemoved](const QVariant &result) {
            articleRemoved = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(articleRemoved, 3000);

    controller.activate();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isActivationPending(), 5000);
    QVERIFY(!controller.isActive());
    QCOMPARE(
        controller.availability(),
        ReaderModeController::Availability::Unavailable
    );
}

void ReaderModeTests::readerModePreservesResponsiveImages()
{
    BrowserPage page(QWebEngineProfile::defaultProfile());
    ReaderSettings settings;
    ReaderModeController controller(&page, &settings);

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html>
<head><title>An article with responsive images</title></head>
<body>
<article>
  <h1>Responsive images must survive Reader Mode</h1>
  <p>This substantial opening paragraph represents an ordinary illustrated article. It gives the readerability heuristic enough editorial prose to retain the surrounding content and its responsive image.</p>
  <img id="srcset-only" alt="Responsive illustration"
       sizes="(max-width: 900px) 90vw, 720px"
       data-srcset="/images/responsive-small.jpg 1x, /images/responsive-large.jpg 2x">
  <p>The second paragraph verifies that an image supplied only through a source set remains visible after sanitization instead of being discarded because it lacks a traditional source attribute.</p>
  <picture id="responsive-picture">
    <source type="image/x-panbrowser-unsupported"
            srcset="/images/picture-future.gif 1x">
    <source type="image/gif"
            srcset="/images/picture-small.gif 1x, /images/picture-large.gif 2x">
    <img id="picture-image" alt="Picture illustration"
         src="data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///ywAAAAAAQABAAACAUwAOw==#fallback">
  </picture>
  <p>The final paragraph exercises a picture element that carries alternative image candidates. Reader Mode should retain validated responsive markup so Chromium can select a supported format and an appropriately sized resource.</p>
</article>
</body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://reader.example/responsive"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.availability(),
        ReaderModeController::Availability::Available,
        5000
    );

    controller.activate();
    QTRY_VERIFY_WITH_TIMEOUT(controller.isActive(), 5000);

    QString imageState;
    page.runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const reader = globalThis.__panBrowserReader;
    const content = reader && reader.root.querySelector(".article");
    const responsive = content && content.querySelector("#srcset-only");
    const picture = content && content.querySelector("#responsive-picture");
    const pictureImage = content && content.querySelector("#picture-image");
    const pictureSources = picture ? Array.from(picture.querySelectorAll(":scope > source")) : [];
    return JSON.stringify({
        responsiveSource: responsive ? responsive.getAttribute("src") : "",
        responsiveSrcset: responsive ? responsive.getAttribute("srcset") : "",
        responsiveSizes: responsive ? responsive.getAttribute("sizes") : "",
        pictureSource: pictureImage ? pictureImage.getAttribute("src") : "",
        firstPictureType: pictureSources[0] ? pictureSources[0].getAttribute("type") : "",
        firstPictureSrcset: pictureSources[0]
            ? pictureSources[0].getAttribute("srcset")
            : "",
        secondPictureType: pictureSources[1] ? pictureSources[1].getAttribute("type") : "",
        secondPictureSrcset: pictureSources[1]
            ? pictureSources[1].getAttribute("srcset")
            : "",
        pictureCount: content ? content.querySelectorAll("picture").length : -1,
        sourceCount: content ? content.querySelectorAll("source").length : -1
    });
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&imageState](const QVariant &result) {
            imageState = result.toString();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(!imageState.isEmpty(), 5000);
    const QJsonObject imageObject = QJsonDocument::fromJson(imageState.toUtf8()).object();
    QCOMPARE(
        imageObject.value(QStringLiteral("responsiveSource")).toString(),
        QString()
    );
    QCOMPARE(
        imageObject.value(QStringLiteral("responsiveSrcset")).toString(),
        QStringLiteral(
            "https://reader.example/images/responsive-small.jpg 1x, "
            "https://reader.example/images/responsive-large.jpg 2x"
        )
    );
    QCOMPARE(
        imageObject.value(QStringLiteral("responsiveSizes")).toString(),
        QStringLiteral("(max-width: 900px) 90vw, 720px")
    );
    QCOMPARE(
        imageObject.value(QStringLiteral("pictureSource")).toString(),
        QStringLiteral(
            "data:image/gif;base64,"
            "R0lGODlhAQABAIAAAAAAAP///ywAAAAAAQABAAACAUwAOw==#fallback"
        )
    );
    QCOMPARE(
        imageObject.value(QStringLiteral("firstPictureType")).toString(),
        QStringLiteral("image/x-panbrowser-unsupported")
    );
    QCOMPARE(
        imageObject.value(QStringLiteral("firstPictureSrcset")).toString(),
        QStringLiteral("https://reader.example/images/picture-future.gif 1x")
    );
    QCOMPARE(
        imageObject.value(QStringLiteral("secondPictureType")).toString(),
        QStringLiteral("image/gif")
    );
    QCOMPARE(
        imageObject.value(QStringLiteral("secondPictureSrcset")).toString(),
        QStringLiteral(
            "https://reader.example/images/picture-small.gif 1x, "
            "https://reader.example/images/picture-large.gif 2x"
        )
    );
    QCOMPARE(imageObject.value(QStringLiteral("pictureCount")).toInt(), 1);
    QCOMPARE(imageObject.value(QStringLiteral("sourceCount")).toInt(), 2);

    controller.deactivate();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isActive(), 3000);
}

void ReaderModeTests::readerModeExtractsArticleAndRestoresOriginalPage()
{
    BrowserPage page(QWebEngineProfile::defaultProfile());
    ReaderSettings settings;
    ReaderModeController controller(&page, &settings);

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html>
<head><title>Original document title</title></head>
<body style="overflow: scroll" data-original="present">
<nav>Navigation that should not appear in the extracted article.</nav>
<article>
  <h1>A useful test article</h1>
  <p>This is a deliberately substantial opening paragraph. It contains enough prose to make the document a plausible article for reader-mode detection, while retaining a stable phrase for the integration test.</p>
  <p>The second paragraph continues with ordinary editorial writing. Reader mode should preserve this text, headings, emphasis, and safe links without replacing the underlying page or changing its URL.</p>
  <p>The third paragraph adds more meaningful content so the readability score crosses its normal threshold. It also includes <a href="/next">a relative article link</a> that should become an absolute HTTPS URL and <a href="#footnote">an internal footnote link</a> that should stay inside Reader Mode.</p>
  <p>The fourth paragraph verifies that a longer document remains readable after sanitization. Embedded forms, scripts, frames, and event handlers must never survive in PanBrowser's presentation layer.</p>
  <p id="footnote">The fifth paragraph provides a final block of prose. Closing reader mode should reveal this exact original document again without reloading it or losing its body attributes.</p>
  <a id="oversized-link" href="https://reader.example/oversized">Oversized URL</a>
  <div class="toolbar" id="page-controlled" style="position:fixed" tabindex="0">Styled page content.</div>
  <form action="/submit"><input name="secret" value="unsafe"></form>
  <iframe src="https://frames.example/"></iframe>
  <script>globalThis.__readerFixtureScriptRan = true;</script>
</article>
<script>
document.querySelector("#oversized-link").setAttribute(
    "href",
    "https://reader.example/" + "x".repeat(17000)
);
</script>
</body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://reader.example/article"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.availability(),
        ReaderModeController::Availability::Available,
        5000
    );

    controller.activate();
    QTRY_VERIFY_WITH_TIMEOUT(controller.isActive(), 5000);

    bool spoofMessageSent = false;
    page.runJavaScript(
        QStringLiteral(R"JS(
console.info("__PANBROWSER_READER_MODE__" + JSON.stringify({
    token: "page-controlled-token",
    action: "close",
    value: null
}));
true
)JS"),
        [&spoofMessageSent](const QVariant &result) {
            spoofMessageSent = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(spoofMessageSent, 3000);
    QTest::qWait(50);
    QVERIFY(controller.isActive());

    QString readerState;
    page.runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const state = globalThis.__panBrowserReader;
    if (!state || !state.active)
        return "";
    const content = state.root.querySelector(".article");
    const link = content && content.querySelector('a[href="https://reader.example/next"]');
    const internalLink = content && content.querySelector('a[href="#footnote"]');
    const internalTarget = content && content.querySelector("#footnote");
    const oversizedLink = content && content.querySelector("#oversized-link");
    const themeSelect = state.root.querySelector(".controls select");
    const themeOption = themeSelect && themeSelect.querySelector("option");
    state.applyAppearance({
        theme: "dark",
        typeface: "serif",
        textSize: 20,
        contentWidth: 720
    });
    const themeSelectStyle = themeSelect ? getComputedStyle(themeSelect) : null;
    const themeOptionStyle = themeOption ? getComputedStyle(themeOption) : null;
    return JSON.stringify({
        text: content ? content.textContent : "",
        unsafeCount: content
            ? content.querySelectorAll("script, form, input, iframe, object, embed").length
            : -1,
        unsafeAttributeCount: content
            ? content.querySelectorAll("[class], [style], [tabindex]").length
            : -1,
        href: link ? link.href : "",
        internalLinkPresent: Boolean(internalLink),
        internalTargetPresent: Boolean(internalTarget),
        oversizedHrefPresent: Boolean(oversizedLink && oversizedLink.hasAttribute("href")),
        original: document.body.dataset.original,
        overflow: document.body.style.getPropertyValue("overflow"),
        bodyInert: document.body.inert,
        themeColorScheme: themeSelectStyle ? themeSelectStyle.colorScheme : "",
        themeOptionColor: themeOptionStyle ? themeOptionStyle.color : "",
        themeOptionBackground: themeOptionStyle ? themeOptionStyle.backgroundColor : ""
    });
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&readerState](const QVariant &result) {
            readerState = result.toString();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(!readerState.isEmpty(), 5000);
    const QJsonObject readerObject = QJsonDocument::fromJson(readerState.toUtf8()).object();
    QVERIFY(readerObject.value(QStringLiteral("text")).toString().contains(
        QStringLiteral("deliberately substantial opening paragraph")
    ));
    QCOMPARE(readerObject.value(QStringLiteral("unsafeCount")).toInt(), 0);
    QCOMPARE(readerObject.value(QStringLiteral("unsafeAttributeCount")).toInt(), 0);
    QCOMPARE(
        readerObject.value(QStringLiteral("href")).toString(),
        QStringLiteral("https://reader.example/next")
    );
    QVERIFY(readerObject.value(QStringLiteral("internalLinkPresent")).toBool());
    QVERIFY(readerObject.value(QStringLiteral("internalTargetPresent")).toBool());
    QVERIFY(!readerObject.value(QStringLiteral("oversizedHrefPresent")).toBool());
    QCOMPARE(readerObject.value(QStringLiteral("original")).toString(), QStringLiteral("present"));
    QCOMPARE(readerObject.value(QStringLiteral("overflow")).toString(), QStringLiteral("hidden"));
    QVERIFY(readerObject.value(QStringLiteral("bodyInert")).toBool());
    QVERIFY(readerObject.value(QStringLiteral("themeColorScheme")).toString().contains(
        QStringLiteral("dark")
    ));
    QCOMPARE(
        readerObject.value(QStringLiteral("themeOptionColor")).toString(),
        QStringLiteral("rgb(232, 234, 240)")
    );
    QCOMPARE(
        readerObject.value(QStringLiteral("themeOptionBackground")).toString(),
        QStringLiteral("rgb(34, 38, 45)")
    );
    QCOMPARE(page.url(), QUrl(QStringLiteral("https://reader.example/article")));

    bool internalLinkClicked = false;
    page.runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const reader = globalThis.__panBrowserReader;
    const link = reader && reader.root.querySelector('a[href="#footnote"]');
    if (!link)
        return false;
    link.click();
    return true;
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&internalLinkClicked](const QVariant &result) {
            internalLinkClicked = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(internalLinkClicked, 3000);
    QTest::qWait(50);
    QVERIFY(controller.isActive());
    QCOMPARE(page.url(), QUrl(QStringLiteral("https://reader.example/article")));

    bool modifiedLinkClicked = false;
    page.runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const reader = globalThis.__panBrowserReader;
    const link = reader
        && reader.root.querySelector('a[href="https://reader.example/next"]');
    if (!link)
        return false;
    link.dispatchEvent(new MouseEvent("click", {
        bubbles: true,
        cancelable: true,
        button: 0,
        metaKey: true
    }));
    return true;
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&modifiedLinkClicked](const QVariant &result) {
            modifiedLinkClicked = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(modifiedLinkClicked, 3000);
    QTest::qWait(50);
    QVERIFY(controller.isActive());
    QCOMPARE(page.url(), QUrl(QStringLiteral("https://reader.example/article")));

    controller.deactivate();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isActive(), 3000);
    QString restoredState;
    page.runJavaScript(
        QStringLiteral(R"JS(
JSON.stringify({
    readerPresent: Boolean(globalThis.__panBrowserReader),
    original: document.body.dataset.original,
    overflow: document.body.style.getPropertyValue("overflow"),
    bodyInert: document.body.inert
})
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&restoredState](const QVariant &result) {
            restoredState = result.toString();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(!restoredState.isEmpty(), 3000);
    const QJsonObject restoredObject = QJsonDocument::fromJson(
        restoredState.toUtf8()
    ).object();
    QVERIFY(!restoredObject.value(QStringLiteral("readerPresent")).toBool());
    QCOMPARE(restoredObject.value(QStringLiteral("original")).toString(), QStringLiteral("present"));
    QCOMPARE(restoredObject.value(QStringLiteral("overflow")).toString(), QStringLiteral("scroll"));
    QVERIFY(!restoredObject.value(QStringLiteral("bodyInert")).toBool());
}

void ReaderModeTests::readerModeRestoresPageBeforeSameDocumentNavigation()
{
    BrowserPage page(QWebEngineProfile::defaultProfile());
    ReaderSettings settings;
    ReaderModeController controller(&page, &settings);

    QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<html>
<head><title>Reader navigation test</title></head>
<body style="overflow: scroll">
<article>
  <h1>An article before client-side navigation</h1>
  <p>This substantial opening paragraph provides enough ordinary prose for reader mode to recognize the document before a client-side route transition occurs.</p>
  <p>The second paragraph verifies that the extracted presentation can be opened while the original document remains available underneath the isolated overlay.</p>
  <p>The third paragraph exists so the article comfortably exceeds the extraction threshold and produces a stable reader-mode fixture for this lifecycle test.</p>
  <p>The final paragraph confirms that changing routes must remove the old presentation and restore the source page before detection begins for the new document state.</p>
</article>
</body>
</html>
)HTML"),
        QUrl(QStringLiteral("https://reader.example/article"))
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 5000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.availability(),
        ReaderModeController::Availability::Available,
        5000
    );

    controller.activate();
    QTRY_VERIFY_WITH_TIMEOUT(controller.isActive(), 5000);

    bool routeChanged = false;
    page.runJavaScript(
        QStringLiteral(R"JS(
(() => {
    document.querySelector("article").replaceWith(document.createElement("main"));
    history.pushState({}, "", "/landing");
    return true;
})()
)JS"),
        [&routeChanged](const QVariant &result) {
            routeChanged = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(routeChanged, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(
        page.url(),
        QUrl(QStringLiteral("https://reader.example/landing")),
        3000
    );
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isActive(), 3000);

    QString restoredState;
    page.runJavaScript(
        QStringLiteral(R"JS(
JSON.stringify({
    readerPresent: Boolean(globalThis.__panBrowserReader),
    htmlOverflow: document.documentElement.style.getPropertyValue("overflow"),
    bodyOverflow: document.body.style.getPropertyValue("overflow")
})
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&restoredState](const QVariant &result) {
            restoredState = result.toString();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(!restoredState.isEmpty(), 3000);
    const QJsonObject restoredObject = QJsonDocument::fromJson(
        restoredState.toUtf8()
    ).object();
    QVERIFY(!restoredObject.value(QStringLiteral("readerPresent")).toBool());
    QCOMPARE(restoredObject.value(QStringLiteral("htmlOverflow")).toString(), QString());
    QCOMPARE(
        restoredObject.value(QStringLiteral("bodyOverflow")).toString(),
        QStringLiteral("scroll")
    );
}


int runReaderModeTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    ReaderModeTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "ReaderModeTests.moc"
