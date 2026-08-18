#include "PanBrowserTestCommon.h"

class VideoPresentationTests final : public QObject {
    Q_OBJECT

private slots:
    void activeBrowserViewSeparatesDetachedSurfaceFromDialogs();
    void fullScreenRequestPolicyValidatesOriginAndState();
    void browserFullScreenRestoresWindowChrome();
    void videoElementBridgeUsesTrustedOverlayClick();
    void detachedVideoSessionCoordinatesReturnAndFallback();
    void detachedVideoWindowMovesAndRestoresPage();
    void detachedVideoWindowCloseRequestsReturn();
    void detachedVideoWindowPreservesAspectRatioWhenResized();
};

void VideoPresentationTests::activeBrowserViewSeparatesDetachedSurfaceFromDialogs()
{
    QWidget mainWindow;
    QWebEngineView currentView(&mainWindow);
    QWidget detachedWindow;
    QWebEngineView detachedSource;
    QDialog detachedDialog(&detachedWindow);
    QWidget unrelatedWindow;
    const QList<BrowserInteractionSurface> surfaces = {
        {&detachedSource, &detachedWindow},
        {nullptr, &unrelatedWindow},
    };
    const std::span<const BrowserInteractionSurface> surfaceSpan(
        surfaces.constData(),
        static_cast<std::size_t>(surfaces.size())
    );

    QCOMPARE(
        resolveActiveBrowserView(
            &mainWindow,
            &mainWindow,
            &currentView,
            surfaceSpan
        ),
        &currentView
    );
    QCOMPARE(
        resolveActiveBrowserView(
            &detachedWindow,
            &mainWindow,
            &currentView,
            surfaceSpan
        ),
        &detachedSource
    );
    QVERIFY(!resolveActiveBrowserView(
        &detachedDialog,
        &mainWindow,
        &currentView,
        surfaceSpan
    ));
    QCOMPARE(
        resolveBrowserCommandTarget(&detachedSource, &currentView, &currentView),
        &detachedSource
    );
    QCOMPARE(
        resolveBrowserCommandTarget(nullptr, &detachedSource, &currentView),
        &detachedSource
    );
    QCOMPARE(
        resolveBrowserCommandTarget(nullptr, nullptr, &currentView),
        &currentView
    );
    QCOMPARE(
        browserCloseTarget(&detachedWindow, &detachedWindow),
        BrowserCloseTarget::DetachedVideo
    );
    QCOMPARE(
        browserCloseTarget(&mainWindow, &detachedWindow),
        BrowserCloseTarget::CurrentTab
    );
    QCOMPARE(
        browserCloseTarget(&mainWindow, nullptr),
        BrowserCloseTarget::CurrentTab
    );
    QVERIFY(!resolveActiveBrowserView(
        &unrelatedWindow,
        &mainWindow,
        &currentView,
        surfaceSpan
    ));
    QVERIFY(!resolveActiveBrowserView(
        nullptr,
        &mainWindow,
        &currentView,
        surfaceSpan
    ));
}

void VideoPresentationTests::fullScreenRequestPolicyValidatesOriginAndState()
{
    const FullScreenRequestDecision browserFullScreen = decideFullScreenRequest(
        true,
        true,
        false,
        false,
        false,
        false,
        QUrl(QStringLiteral("HTTPS://Example.COM:443/watch?v=1#player"))
    );
    QCOMPARE(browserFullScreen.action, FullScreenRequestAction::EnterBrowserFullScreen);
    QCOMPARE(browserFullScreen.origin.scheme(), QStringLiteral("https"));
    QCOMPARE(browserFullScreen.origin.host(), QStringLiteral("example.com"));
    QCOMPARE(browserFullScreen.origin.port(), -1);
    QCOMPARE(
        fullScreenOriginDisplay(browserFullScreen.origin),
        QStringLiteral("https://example.com")
    );

    const FullScreenRequestDecision videoPopout = decideFullScreenRequest(
        true,
        true,
        true,
        false,
        false,
        false,
        QUrl(QStringLiteral("https://example.com/watch"))
    );
    QCOMPARE(videoPopout.action, FullScreenRequestAction::DetachVideo);

    const FullScreenRequestDecision customPort = decideFullScreenRequest(
        true,
        true,
        false,
        false,
        false,
        false,
        QUrl(QStringLiteral("http://example.com:8080/video"))
    );
    QCOMPARE(customPort.action, FullScreenRequestAction::EnterBrowserFullScreen);
    QCOMPARE(customPort.origin.port(), 8080);
    QCOMPARE(
        fullScreenOriginDisplay(customPort.origin),
        QStringLiteral("http://example.com:8080")
    );

    const FullScreenRequestDecision internationalizedHost = decideFullScreenRequest(
        true,
        true,
        false,
        false,
        false,
        false,
        QUrl(QStringLiteral("https://пример.рф/video"))
    );
    QCOMPARE(
        internationalizedHost.action,
        FullScreenRequestAction::EnterBrowserFullScreen
    );
    QCOMPARE(
        fullScreenOriginDisplay(internationalizedHost.origin),
        QStringLiteral("https://xn--e1afmkfd.xn--p1ai")
    );

    QCOMPARE(
        decideFullScreenRequest(
            true,
            false,
            false,
            false,
            false,
            false,
            QUrl(QStringLiteral("https://example.com"))
        ).action,
        FullScreenRequestAction::Reject
    );
    QCOMPARE(
        decideFullScreenRequest(
            true,
            true,
            false,
            true,
            false,
            false,
            QUrl(QStringLiteral("https://example.com"))
        ).action,
        FullScreenRequestAction::Reject
    );
    QCOMPARE(
        decideFullScreenRequest(
            true,
            true,
            false,
            false,
            false,
            false,
            QUrl(QStringLiteral("ftp://example.com/video"))
        ).action,
        FullScreenRequestAction::Reject
    );
    QCOMPARE(
        decideFullScreenRequest(
            true,
            true,
            false,
            false,
            false,
            false,
            QUrl(QStringLiteral("https://user@example.com/video"))
        ).action,
        FullScreenRequestAction::Reject
    );
    QCOMPARE(
        decideFullScreenRequest(false, false, false, true, false, false, QUrl()).action,
        FullScreenRequestAction::RestoreDetachedVideo
    );
    QCOMPARE(
        decideFullScreenRequest(false, false, false, false, false, true, QUrl()).action,
        FullScreenRequestAction::RestoreBrowserFullScreen
    );
    QCOMPARE(
        decideFullScreenRequest(
            true,
            true,
            false,
            false,
            true,
            false,
            QUrl(QStringLiteral("https://example.com"))
        ).action,
        FullScreenRequestAction::Reject
    );
    QCOMPARE(
        decideFullScreenRequest(
            true,
            true,
            false,
            false,
            false,
            true,
            QUrl(QStringLiteral("https://example.com"))
        ).action,
        FullScreenRequestAction::Reject
    );
    QVERIFY(fullScreenOriginDisplay(QUrl(QStringLiteral("file:///tmp/video.html"))).isEmpty());
}

void VideoPresentationTests::browserFullScreenRestoresWindowChrome()
{
    QMainWindow window;
    auto *view = new QWebEngineView(&window);
    window.setCentralWidget(view);
    QToolBar *visibleToolBar = window.addToolBar(QStringLiteral("Visible"));
    QToolBar *hiddenToolBar = window.addToolBar(QStringLiteral("Hidden"));
    hiddenToolBar->hide();
    QMenuBar *menuBar = window.menuBar();
    menuBar->addMenu(QStringLiteral("Browser"));
    window.statusBar()->showMessage(QStringLiteral("Ready"));
    window.resize(720, 480);
    window.show();
    QTRY_VERIFY(window.isVisible());
    const bool menuBarWasVisible = menuBar->isVisible();

    BrowserFullScreenController controller(&window);
    QVERIFY(controller.enter(view));
    QVERIFY(controller.isActive());
    QCOMPARE(controller.webView(), view);
    QTRY_VERIFY_WITH_TIMEOUT(window.isFullScreen(), 3000);
    QVERIFY(visibleToolBar->isHidden());
    QVERIFY(hiddenToolBar->isHidden());
    QVERIFY(window.statusBar()->isHidden());
    QVERIFY(menuBar->isHidden());

    controller.exit();
    QVERIFY(!controller.isActive());
    QVERIFY(!controller.webView());
    QTRY_VERIFY_WITH_TIMEOUT(!window.isFullScreen(), 3000);
    QVERIFY(visibleToolBar->isVisible());
    QVERIFY(hiddenToolBar->isHidden());
    QVERIFY(window.statusBar()->isVisible());
    QCOMPARE(menuBar->isVisible(), menuBarWasVisible);

    int nativeExitRequestCount = 0;
    connect(
        &controller,
        &BrowserFullScreenController::nativeExitRequested,
        &window,
        [&controller, &nativeExitRequestCount](QWebEngineView *requestedView) {
            ++nativeExitRequestCount;
            QCOMPARE(requestedView, controller.webView());
            controller.exit();
        }
    );
    QVERIFY(controller.enter(view));
    QTRY_VERIFY_WITH_TIMEOUT(window.isFullScreen(), 3000);
    window.showNormal();
    QTRY_COMPARE_WITH_TIMEOUT(nativeExitRequestCount, 1, 3000);
    QVERIFY(!controller.isActive());
    QVERIFY(visibleToolBar->isVisible());
    QVERIFY(hiddenToolBar->isHidden());
    QVERIFY(window.statusBar()->isVisible());
    QCOMPARE(menuBar->isVisible(), menuBarWasVisible);

    window.showFullScreen();
    QTRY_VERIFY_WITH_TIMEOUT(window.isFullScreen(), 3000);
    QVERIFY(controller.enter(view));
    window.showNormal();
    QTRY_COMPARE_WITH_TIMEOUT(nativeExitRequestCount, 2, 3000);
    QVERIFY(!controller.isActive());
    QVERIFY(!window.isFullScreen());
    QVERIFY(visibleToolBar->isVisible());
    QVERIFY(hiddenToolBar->isHidden());
    QVERIFY(window.statusBar()->isVisible());
    QCOMPARE(menuBar->isVisible(), menuBarWasVisible);
}

void VideoPresentationTests::videoElementBridgeUsesTrustedOverlayClick()
{
    QWebEngineView view;
    auto *page = new BrowserPage(QWebEngineProfile::defaultProfile(), &view);
    page->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    VideoElementBridge::install(
        page,
        page->videoPopoutToken(),
        QStringLiteral("Open video separately")
    );
    view.setPage(page);
    view.resize(500, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view, 3000));
    view.raise();
    view.activateWindow();
    view.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();

    QStringList eventOrder;
    QUrl requestedFrameUrl;
    QSize requestedVideoSize;
    connect(page, &BrowserPage::videoPopoutRequested, &view, [
        &eventOrder,
        &requestedFrameUrl,
        &requestedVideoSize
    ](const QUrl &frameUrl, const QSize &videoSize) {
        eventOrder.append(QStringLiteral("bridge"));
        requestedFrameUrl = frameUrl;
        requestedVideoSize = videoSize;
    });
    connect(
        page,
        &QWebEnginePage::fullScreenRequested,
        &view,
        [&eventOrder](QWebEngineFullScreenRequest request) {
            eventOrder.append(QStringLiteral("fullscreen"));
            request.reject();
        }
    );

    QSignalSpy loadSpy(page, &QWebEnginePage::loadFinished);
    const QUrl longPageUrl(QStringLiteral(
        "https://video.example/player?payload=%1"
    ).arg(QString(2500, QLatin1Char('a'))));
    view.setHtml(
        QStringLiteral(R"HTML(
<!doctype html>
<style>
html, body { margin: 0; width: 100%; height: 100%; }
video { position: fixed; left: 40px; top: 40px; width: 320px; height: 180px; background: black; }
</style>
<video id="target"></video>
)HTML"),
        longPageUrl
    );
    QTRY_VERIFY_WITH_TIMEOUT(!loadSpy.isEmpty(), 3000);
    QVERIFY(loadSpy.constLast().constFirst().toBool());

    bool overlayAttached = false;
    page->runJavaScript(
        QStringLiteral(R"JS(
(() => {
    return Boolean(document.querySelector("[data-panbrowser-video-popout]"));
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&overlayAttached](const QVariant &result) {
            overlayAttached = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(overlayAttached, 3000);

    bool syntheticClickDispatched = false;
    page->runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const host = document.querySelector("[data-panbrowser-video-popout]");
    host.dispatchEvent(new MouseEvent("click", { bubbles: true, composed: true }));
    return true;
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&syntheticClickDispatched](const QVariant &result) {
            syntheticClickDispatched = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(syntheticClickDispatched, 3000);
    QTest::qWait(100);
    QVERIFY(eventOrder.isEmpty());

    bool spoofMessageDispatched = false;
    page->runJavaScript(
        QStringLiteral(R"JS(
(() => {
    console.info("__PANBROWSER_VIDEO_POPOUT_REQUEST__" + JSON.stringify({
        token: "page-controlled-token",
        url: "https://video.example/player"
    }));
    return true;
})()
)JS"),
        [&spoofMessageDispatched](const QVariant &result) {
            spoofMessageDispatched = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(spoofMessageDispatched, 3000);
    QTest::qWait(100);
    QVERIFY(eventOrder.isEmpty());

    bool overlayActivated = false;
    page->runJavaScript(
        QStringLiteral(R"JS(
globalThis.__panBrowserVideoPopoutController.showFor(
    document.getElementById("target")
)
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&overlayActivated](const QVariant &result) {
            overlayActivated = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(overlayActivated, 3000);

    QString overlayGeometryJson;
    page->runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const host = document.querySelector("[data-panbrowser-video-popout]");
    if (!host || getComputedStyle(host).display === "none")
        return "";
    const rect = host.getBoundingClientRect();
    return JSON.stringify({ x: rect.x, y: rect.y, width: rect.width, height: rect.height });
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [&overlayGeometryJson](const QVariant &result) {
            overlayGeometryJson = result.toString();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(!overlayGeometryJson.isEmpty(), 3000);
    const QJsonObject overlayGeometry = QJsonDocument::fromJson(
        overlayGeometryJson.toUtf8()
    ).object();
    QVERIFY(!overlayGeometry.isEmpty());
    const QPoint overlayCenter(
        qRound(overlayGeometry.value(QStringLiteral("x")).toDouble()
            + overlayGeometry.value(QStringLiteral("width")).toDouble() / 2.0),
        qRound(overlayGeometry.value(QStringLiteral("y")).toDouble()
            + overlayGeometry.value(QStringLiteral("height")).toDouble() / 2.0)
    );
    QWidget *eventTarget = view.focusProxy() ? view.focusProxy() : &view;
    const auto targetPosition = [&view, eventTarget](const QPoint &position) {
        return eventTarget == &view ? position : eventTarget->mapFrom(&view, position);
    };
    QTest::mouseMove(eventTarget, targetPosition(overlayCenter));
    QTest::mouseClick(
        eventTarget,
        Qt::LeftButton,
        Qt::NoModifier,
        targetPosition(overlayCenter)
    );

    QTRY_VERIFY_WITH_TIMEOUT(eventOrder.contains(QStringLiteral("fullscreen")), 3000);
    QCOMPARE(
        eventOrder,
        QStringList({QStringLiteral("bridge"), QStringLiteral("fullscreen")})
    );
    QCOMPARE(requestedFrameUrl, QUrl(QStringLiteral("https://video.example")));
    QCOMPARE(requestedVideoSize, QSize(320, 180));

    bool extremeAspectMessageSent = false;
    page->runJavaScript(
        QStringLiteral(R"JS(
console.info("__PANBROWSER_VIDEO_POPOUT_REQUEST__" + JSON.stringify({
    token: "%1",
    url: "https://video.example/player",
    videoWidth: 32768,
    videoHeight: 1
}));
true
)JS").arg(page->videoPopoutToken()),
        QWebEngineScript::ApplicationWorld,
        [&extremeAspectMessageSent](const QVariant &result) {
            extremeAspectMessageSent = result.toBool();
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(extremeAspectMessageSent, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(requestedVideoSize, QSize(4, 1), 3000);
}

void VideoPresentationTests::detachedVideoSessionCoordinatesReturnAndFallback()
{
    DetachedVideoSession session(nullptr, 20);
    QSignalSpy exitSpy(&session, &DetachedVideoSession::exitFullScreenRequested);
    QSignalSpy restoreSpy(&session, &DetachedVideoSession::restoreRequested);

    QCOMPARE(session.state(), DetachedVideoSession::State::Attached);
    QVERIFY(session.beginDetached());
    QVERIFY(session.isDetached());
    QVERIFY(!session.beginDetached());
    QVERIFY(session.requestReturn());
    QCOMPARE(session.state(), DetachedVideoSession::State::ReturnPending);
    QCOMPARE(exitSpy.count(), 1);
    QVERIFY(!session.requestReturn());
    QTRY_COMPARE_WITH_TIMEOUT(restoreSpy.count(), 1, 250);
    QCOMPARE(session.state(), DetachedVideoSession::State::Attached);

    QVERIFY(session.beginDetached());
    QVERIFY(session.requestReturn());
    QCOMPARE(exitSpy.count(), 2);
    session.browserExitedFullScreen();
    QCOMPARE(restoreSpy.count(), 2);
    QCOMPARE(session.state(), DetachedVideoSession::State::Attached);
    QTest::qWait(40);
    QCOMPARE(restoreSpy.count(), 2);

    QVERIFY(session.beginDetached());
    session.forceRestore();
    QCOMPARE(restoreSpy.count(), 3);
    QCOMPARE(session.state(), DetachedVideoSession::State::Attached);

    QVERIFY(session.beginDetached());
    QVERIFY(session.requestReturn());
    QCOMPARE(exitSpy.count(), 3);
    session.reset();
    QTest::qWait(40);
    QCOMPARE(restoreSpy.count(), 3);
    QCOMPARE(session.state(), DetachedVideoSession::State::Attached);
}

void VideoPresentationTests::detachedVideoWindowMovesAndRestoresPage()
{
    QWebEngineView sourceView;
    QWebEnginePage *originalPage = sourceView.page();
    QVERIFY(originalPage);

    {
        DetachedVideoWindow detachedWindow(
            &sourceView,
            QStringLiteral("Video — https://example.com")
        );
        QCOMPARE(detachedWindow.windowTitle(), QStringLiteral("Video — https://example.com"));
        QVERIFY(detachedWindow.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(detachedWindow.windowFlags().testFlag(Qt::FramelessWindowHint));
        QVERIFY(!detachedWindow.findChild<QWidget *>(
            QStringLiteral("detachedVideoOriginBar")
        ));
        QCOMPARE(detachedWindow.webView()->page(), originalPage);
        QCOMPARE(detachedWindow.webView()->contextMenuPolicy(), Qt::CustomContextMenu);
        QCOMPARE(QWebEngineView::forPage(originalPage), detachedWindow.webView());
        QVERIFY(sourceView.page() != originalPage);
        QSignalSpy contextMenuSpy(
            &detachedWindow,
            &DetachedVideoWindow::contextMenuRequested
        );
        const QPoint contextMenuPosition(24, 36);
        QVERIFY(QMetaObject::invokeMethod(
            detachedWindow.webView(),
            "customContextMenuRequested",
            Qt::DirectConnection,
            Q_ARG(QPoint, contextMenuPosition)
        ));
        QCOMPARE(contextMenuSpy.count(), 1);
        QCOMPARE(contextMenuSpy.takeFirst().at(0).toPoint(), contextMenuPosition);

        QToolButton *closeButton = detachedWindow.findChild<QToolButton *>(
            QStringLiteral("closeDetachedVideoButton")
        );
        QVERIFY(closeButton);
        QVERIFY(closeButton->isHidden());
        QSignalSpy returnSpy(&detachedWindow, &DetachedVideoWindow::returnRequested);
        detachedWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&detachedWindow, 3000));
        const QPoint hoverPosition = detachedWindow.webView()->rect().center();
        const QPoint hoverGlobal = detachedWindow.webView()->mapToGlobal(hoverPosition);
        QMouseEvent hoverEvent(
            QEvent::MouseMove,
            QPointF(hoverPosition),
            QPointF(hoverPosition),
            QPointF(hoverGlobal),
            Qt::NoButton,
            Qt::NoButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(detachedWindow.webView(), &hoverEvent);
        QVERIFY(closeButton->isVisible());

        QEvent leaveEvent(QEvent::Leave);
        QApplication::sendEvent(&detachedWindow, &leaveEvent);
        QVERIFY(closeButton->isHidden());
        QApplication::sendEvent(detachedWindow.webView(), &hoverEvent);
        QVERIFY(closeButton->isVisible());

        QEvent deactivateEvent(QEvent::WindowDeactivate);
        QApplication::sendEvent(&detachedWindow, &deactivateEvent);
        QVERIFY(closeButton->isHidden());
        QApplication::sendEvent(detachedWindow.webView(), &hoverEvent);
        QVERIFY(closeButton->isVisible());

        const QPoint dragStartPosition = detachedWindow.pos();
        const QPoint localPress = detachedWindow.webView()->rect().center();
        const QPoint globalPress = detachedWindow.webView()->mapToGlobal(localPress);
        const QPoint dragDelta(48, 32);
        QMouseEvent pressEvent(
            QEvent::MouseButtonPress,
            QPointF(localPress),
            QPointF(localPress),
            QPointF(globalPress),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(detachedWindow.webView(), &pressEvent);
        QMouseEvent moveEvent(
            QEvent::MouseMove,
            QPointF(localPress + dragDelta),
            QPointF(localPress + dragDelta),
            QPointF(globalPress + dragDelta),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(detachedWindow.webView(), &moveEvent);
        QCOMPARE(detachedWindow.pos(), dragStartPosition + dragDelta);
        QMouseEvent releaseEvent(
            QEvent::MouseButtonRelease,
            QPointF(localPress + dragDelta),
            QPointF(localPress + dragDelta),
            QPointF(globalPress + dragDelta),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(detachedWindow.webView(), &releaseEvent);

        QTest::mouseClick(closeButton, Qt::LeftButton);
        QCOMPARE(returnSpy.count(), 1);

        detachedWindow.restorePage();
        QCOMPARE(sourceView.page(), originalPage);
        QCOMPARE(QWebEngineView::forPage(originalPage), &sourceView);

        detachedWindow.restorePage();
        QCOMPARE(sourceView.page(), originalPage);
    }

    {
        DetachedVideoWindow detachedWindow(
            &sourceView,
            QStringLiteral("Video — https://example.com")
        );
        QCOMPARE(QWebEngineView::forPage(originalPage), detachedWindow.webView());
    }
    QCOMPARE(sourceView.page(), originalPage);
    QCOMPARE(QWebEngineView::forPage(originalPage), &sourceView);

}

void VideoPresentationTests::detachedVideoWindowCloseRequestsReturn()
{
    QWebEngineView sourceView;
    QWebEnginePage *originalPage = sourceView.page();
    DetachedVideoWindow detachedWindow(
        &sourceView,
        QStringLiteral("Video — https://example.com")
    );
    QSignalSpy returnSpy(&detachedWindow, &DetachedVideoWindow::returnRequested);

    QCloseEvent closeEvent;
    QApplication::sendEvent(&detachedWindow, &closeEvent);
    QCOMPARE(returnSpy.count(), 1);
    QVERIFY(!closeEvent.isAccepted());
    QCOMPARE(detachedWindow.webView()->page(), originalPage);

    detachedWindow.restorePage();
    QCOMPARE(sourceView.page(), originalPage);
}

void VideoPresentationTests::detachedVideoWindowPreservesAspectRatioWhenResized()
{
    QWebEngineView sourceView;
    DetachedVideoWindow detachedWindow(
        &sourceView,
        QStringLiteral("Video — https://example.com"),
        QSize(4, 3)
    );
    detachedWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&detachedWindow, 3000));
    QCOMPARE(detachedWindow.width() * 3, detachedWindow.height() * 4);

    QWebEngineView *webView = detachedWindow.webView();
    const QPoint localPress(webView->width() - 1, webView->height() / 2);
    const QPoint globalPress = webView->mapToGlobal(localPress);
    const QPoint resizeDelta(120, 0);
    QMouseEvent pressEvent(
        QEvent::MouseButtonPress,
        QPointF(localPress),
        QPointF(localPress),
        QPointF(globalPress),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(webView, &pressEvent);
    QMouseEvent moveEvent(
        QEvent::MouseMove,
        QPointF(localPress + resizeDelta),
        QPointF(localPress + resizeDelta),
        QPointF(globalPress + resizeDelta),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(webView, &moveEvent);
    QCOMPARE(detachedWindow.width() * 3, detachedWindow.height() * 4);
    QMouseEvent releaseEvent(
        QEvent::MouseButtonRelease,
        QPointF(localPress + resizeDelta),
        QPointF(localPress + resizeDelta),
        QPointF(globalPress + resizeDelta),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(webView, &releaseEvent);

    detachedWindow.resize(800, 800);
    QTRY_COMPARE_WITH_TIMEOUT(
        detachedWindow.width(),
        qRound(detachedWindow.height() * 4.0 / 3.0),
        3000
    );

    QWebEngineView extremeSourceView;
    DetachedVideoWindow extremeWindow(
        &extremeSourceView,
        QStringLiteral("Video — https://example.com"),
        QSize(32768, 1)
    );
    QCOMPARE(extremeWindow.minimumSize(), QSize(540, 135));
    QCOMPARE(extremeWindow.width(), extremeWindow.height() * 4);
}


int runVideoPresentationTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    VideoPresentationTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "VideoPresentationTests.moc"
