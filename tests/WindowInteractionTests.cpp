#include "PanBrowserTestCommon.h"

#include <QHostAddress>
#include <QStyle>
#include <QTcpServer>
#include <QTcpSocket>

class UnavailableTestCredentialStore final : public CredentialStore {
public:
    [[nodiscard]] bool isAvailable() const override
    {
        return false;
    }

    [[nodiscard]] std::optional<StoredCredential> read(
        const CredentialTarget &,
        CredentialStoreError *
    ) override
    {
        return std::nullopt;
    }

    bool write(
        const CredentialTarget &,
        const StoredCredential &,
        CredentialStoreError *
    ) override
    {
        return false;
    }

    bool remove(const CredentialTarget &, CredentialStoreError *) override
    {
        return false;
    }

    bool removeAll(CredentialStoreError *) override
    {
        return false;
    }

    [[nodiscard]] QList<StoredCredentialSummary> list(
        CredentialStoreError *
    ) override
    {
        return {};
    }
};

class WindowInteractionTests final : public QObject {
    Q_OBJECT

private slots:
    void visibleWindowPlacementIsPreserved();
    void disconnectedScreenFallsBackToPrimary();
    void inaccessibleTitleAreaIsRecentered();
    void oversizedWindowFitsSmallerResolution();
    void integratedChromePreservesBaseMarginsAndAvoidsSystemControls();
    void integratedChromeSurvivesSurfaceAndLayoutTeardown();
    void browserPreferencesValidateStartPage();
    void browserTabBarKeepsPinnedTabsInTheirGroup();
    void browserTabBarFitsPinnedOnlyContent();
    void browserTabBarRestoresBoundaryAfterMouseDrag();
    void browserTabBarAnimatesNewTabExpansion();
    void developerToolsPreferenceDefaultsToDisabled();
    void browserShortcutFallbackMatchesRegisteredKeys();
    void tabNavigationWrapsAndTargetsNumberedTabs();
    void tabNavigationShortcutsMatchRegisteredKeys();
    void pageZoomUsesCanonicalOriginsAndDiscreteLevels();
    void pageZoomPersistsAndRemovesDefaults();
    void pageZoomShortcutsAndWheelDeltas();
    void readerModeSettingsAndUrlPolicy();
    void readerModeWaitsForCompletedLoadsBeforeProbing();
    void readerModeDetectsDelayedSinglePageArticles();
    void readerModeRejectsOversizedExtractedMarkup();
    void readerModeInvalidatesAvailabilityWhenArticleDisappears();
    void readerModeExtractsArticleAndRestoresOriginalPage();
    void readerModeRestoresPageBeforeSameDocumentNavigation();
    void activeBrowserViewSeparatesDetachedSurfaceFromDialogs();
    void fullScreenRequestPolicyValidatesOriginAndState();
    void videoElementBridgeUsesTrustedOverlayClick();
    void browserFullScreenRestoresWindowChrome();
    void detachedVideoSessionCoordinatesReturnAndFallback();
    void detachedVideoWindowMovesAndRestoresPage();
    void detachedVideoWindowPreservesAspectRatioWhenResized();
    void detachedVideoWindowCloseRequestsReturn();
    void popupGeometryIsVisibleAndUsable();
    void crossDomainPromptRoutesOnlyMatchingPageIdentities();
    void crossDomainPromptCoordinatesCanceledViewsAcrossControllers();
    void settingsDialogRegistersEveryPageAndSelectsInitialPage();
    void generalSettingsPageRoundTripsPreferences();
};

void WindowInteractionTests::browserTabBarKeepsPinnedTabsInTheirGroup()
{
    BrowserTabBar tabBar;
    tabBar.setObjectName(QStringLiteral("browserTabBar"));
    QFile theme(QStringLiteral(":/assets/theme.qss"));
    QVERIFY(theme.open(QIODevice::ReadOnly));
    tabBar.setStyleSheet(QString::fromUtf8(theme.readAll()));
    tabBar.setTabsClosable(true);
    tabBar.setMovable(true);
    tabBar.setExpanding(false);
    tabBar.resize(600, 40);
    tabBar.addTab(QStringLiteral("Pinned one"));
    tabBar.addTab(QStringLiteral("Pinned two"));
    tabBar.addTab(QStringLiteral("A much longer regular tab title"));
    tabBar.setTabPinned(0, true);
    tabBar.setTabPinned(1, true);
    tabBar.setTabText(0, QString());
    tabBar.setTabText(1, QString());
    tabBar.setTabIcon(0, QIcon(QStringLiteral(":/assets/app-icon.png")));
    tabBar.setTabIcon(1, QIcon(QStringLiteral(":/assets/app-icon.png")));

    QCOMPARE(tabBar.pinnedTabCount(), 2);
    QVERIFY(tabBar.isTabPinned(0));
    QVERIFY(tabBar.isTabPinned(1));
    QVERIFY(!tabBar.isTabPinned(2));
    QVERIFY(tabBar.tabRect(0).width() < tabBar.tabRect(2).width());
    tabBar.setCurrentIndex(0);
    const int activePinnedWidth = tabBar.tabRect(0).width();
    QCOMPARE(activePinnedWidth, 42);
    tabBar.setCurrentIndex(2);
    QCOMPARE(tabBar.tabRect(0).width(), activePinnedWidth);
    QWidget *pinnedCloseButton = tabBar.tabButton(0, QTabBar::RightSide);
    if (!pinnedCloseButton)
        pinnedCloseButton = tabBar.tabButton(0, QTabBar::LeftSide);
    QVERIFY(pinnedCloseButton);
    QVERIFY(pinnedCloseButton->isHidden());

    tabBar.moveTab(0, 2);
    QCOMPARE(tabBar.normalizedMoveDestination(2), 1);
    tabBar.moveTab(2, 1);
    QVERIFY(tabBar.isTabPinned(1));

    tabBar.moveTab(2, 0);
    QCOMPARE(tabBar.normalizedMoveDestination(0), 2);
    tabBar.moveTab(0, 2);
    QVERIFY(!tabBar.isTabPinned(2));

    tabBar.setTabPinned(1, false);
    QCOMPARE(tabBar.pinnedTabCount(), 1);
    QWidget *unpinnedCloseButton = tabBar.tabButton(1, QTabBar::RightSide);
    if (!unpinnedCloseButton)
        unpinnedCloseButton = tabBar.tabButton(1, QTabBar::LeftSide);
    QVERIFY(unpinnedCloseButton);
    QVERIFY(!unpinnedCloseButton->isHidden());
}

void WindowInteractionTests::browserTabBarFitsPinnedOnlyContent()
{
    QWidget container;
    auto *layout = new QHBoxLayout(&container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *tabBar = new BrowserTabBar(&container);
    tabBar->setObjectName(QStringLiteral("browserTabBar"));
    QFile theme(QStringLiteral(":/assets/theme.qss"));
    QVERIFY(theme.open(QIODevice::ReadOnly));
    tabBar->setStyleSheet(QString::fromUtf8(theme.readAll()));
    tabBar->setTabsClosable(true);
    tabBar->setExpanding(false);
    tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    const int index = tabBar->addTab(
        QIcon(QStringLiteral(":/assets/app-icon.png")),
        QString()
    );
    tabBar->setTabPinned(index, true);

    auto *newTabButton = new QToolButton(&container);
    layout->addWidget(tabBar);
    layout->addWidget(newTabButton);
    layout->addStretch(1);
    container.resize(400, 48);
    container.show();
    QVERIFY(QTest::qWaitForWindowExposed(&container));

    QCOMPARE(tabBar->sizeHint().width(), 42);
    QCOMPARE(tabBar->minimumSizeHint().width(), 42);
    QCOMPARE(tabBar->width(), 42);
    QCOMPARE(
        newTabButton->geometry().left() - tabBar->geometry().right() - 1,
        layout->spacing()
    );
}

void WindowInteractionTests::browserTabBarRestoresBoundaryAfterMouseDrag()
{
    BrowserTabBar tabBar;
    tabBar.setMovable(true);
    tabBar.setExpanding(false);
    tabBar.resize(640, 40);
    tabBar.addTab(QStringLiteral("Pinned one"));
    tabBar.addTab(QStringLiteral("Pinned two"));
    tabBar.addTab(QStringLiteral("Regular one"));
    tabBar.addTab(QStringLiteral("Regular two"));
    tabBar.setTabPinned(0, true);
    tabBar.setTabPinned(1, true);

    const QPoint pressPosition = tabBar.tabRect(0).center();
    const QPoint destination(tabBar.rect().right() - 2, pressPosition.y());
    QTest::mousePress(&tabBar, Qt::LeftButton, Qt::NoModifier, pressPosition);
    tabBar.moveTab(0, 3);
    QCOMPARE(tabBar.tabText(3), QStringLiteral("Pinned one"));
    QTest::mouseRelease(&tabBar, Qt::LeftButton, Qt::NoModifier, destination);

    QCOMPARE(tabBar.tabText(0), QStringLiteral("Pinned two"));
    QCOMPARE(tabBar.tabText(1), QStringLiteral("Pinned one"));
    QVERIFY(tabBar.isTabPinned(0));
    QVERIFY(tabBar.isTabPinned(1));
    QVERIFY(!tabBar.isTabPinned(2));
    QVERIFY(!tabBar.isTabPinned(3));
}

void WindowInteractionTests::browserTabBarAnimatesNewTabExpansion()
{
    BrowserTabBar tabBar;
    tabBar.setObjectName(QStringLiteral("browserTabBar"));
    QFile theme(QStringLiteral(":/assets/theme.qss"));
    QVERIFY(theme.open(QIODevice::ReadOnly));
    tabBar.setStyleSheet(QString::fromUtf8(theme.readAll()));
    tabBar.setTabsClosable(true);
    tabBar.setExpanding(false);
    tabBar.resize(640, 40);
    const int pinnedIndex = tabBar.addTab(
        QIcon(QStringLiteral(":/assets/app-icon.png")),
        QString()
    );
    tabBar.setTabPinned(pinnedIndex, true);
    tabBar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tabBar));

    const int index = tabBar.addTab(QStringLiteral("Animated new tab"));
    const int naturalWidth = tabBar.tabRect(index).width();
    const QSize stableMinimum = tabBar.minimumSizeHint();
    if (tabBar.style()->styleHint(
            QStyle::SH_Widget_Animation_Duration,
            nullptr,
            &tabBar
        ) <= 0) {
        QSKIP("The active Qt style disables widget animations");
    }
    tabBar.animateTabOpening(index);

    QCOMPARE(tabBar.minimumSizeHint(), stableMinimum);
    QVERIFY(tabBar.tabRect(index).width() < naturalWidth);
    QTRY_COMPARE_WITH_TIMEOUT(tabBar.tabRect(index).width(), naturalWidth, 1000);
}

void WindowInteractionTests::visibleWindowPlacementIsPreserved()
{
    const QRect screen(0, 0, 1440, 900);
    const QRect window(120, 80, 1180, 760);
    QCOMPARE(adjustedWindowGeometry(window, {screen}, screen), window);
}

void WindowInteractionTests::disconnectedScreenFallsBackToPrimary()
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

void WindowInteractionTests::inaccessibleTitleAreaIsRecentered()
{
    const QRect screen(0, 0, 1440, 900);
    const QRect windowWithBodyOnly(-900, -500, 1000, 700);
    const QRect restored = adjustedWindowGeometry(windowWithBodyOnly, {screen}, screen);

    QVERIFY(restored != windowWithBodyOnly);
    QVERIFY(screen.contains(restored));
    QCOMPARE(restored.center(), screen.center());
}

void WindowInteractionTests::oversizedWindowFitsSmallerResolution()
{
    const QRect smallerScreen(0, 0, 1280, 720);
    const QRect largeWindow(80, 40, 2560, 1400);
    const QRect restored = adjustedWindowGeometry(largeWindow, {smallerScreen}, smallerScreen);

    QCOMPARE(restored, smallerScreen);
}

void WindowInteractionTests::integratedChromePreservesBaseMarginsAndAvoidsSystemControls()
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

void WindowInteractionTests::integratedChromeSurvivesSurfaceAndLayoutTeardown()
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

void WindowInteractionTests::browserPreferencesValidateStartPage()
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

void WindowInteractionTests::crossDomainPromptCoordinatesCanceledViewsAcrossControllers()
{
    CrossDomainPromptController originalController(nullptr, nullptr);
    CrossDomainPromptController remainingController(nullptr, nullptr);
    QWebEngineView originalView;
    QWebEngineView remainingView;
    const QUrl expectedUrl(QStringLiteral("https://example.com/page"));
    const QString sourceSite = QStringLiteral("example.com");
    const QString targetHost = QStringLiteral("tracker.example");
    const QList<CrossDomainPromptSource> promptSources = {
        {expectedUrl, false},
    };

    int anchorChanges = 0;
    QWebEngineView *newAnchor = nullptr;
    connect(
        &originalController,
        &CrossDomainPromptController::requestReanchorRequested,
        &originalController,
        [&](QWebEngineView *webView,
            const QList<CrossDomainAffectedView> &affectedViews,
            const QList<CrossDomainPromptSource> &sources,
            const QString &changedSourceSite,
            const QString &changedTargetHost,
            int resourceType) {
            QCOMPARE(changedSourceSite, sourceSite);
            QCOMPARE(changedTargetHost, targetHost);
            QCOMPARE(sources.size(), 1);
            QCOMPARE(sources.constFirst().url, expectedUrl);
            QVERIFY(!sources.constFirst().originOnly);
            ++anchorChanges;
            newAnchor = webView;
            remainingController.request(
                webView,
                affectedViews,
                sources,
                changedSourceSite,
                changedTargetHost,
                resourceType
            );
        }
    );
    int finishedRequests = 0;
    connect(
        &originalController,
        &CrossDomainPromptController::requestFinished,
        &originalController,
        [&](const QString &, const QString &) {
            ++finishedRequests;
        }
    );
    connect(
        &remainingController,
        &CrossDomainPromptController::requestFinished,
        &remainingController,
        [&](const QString &, const QString &) {
            ++finishedRequests;
        }
    );

    originalController.currentViewChanged(&originalView);
    remainingController.currentViewChanged(&remainingView);
    originalController.request(
        &originalView,
        {
            {&originalView, expectedUrl},
            {&remainingView, expectedUrl},
        },
        promptSources,
        sourceSite,
        targetHost,
        int(QWebEngineUrlRequestInfo::ResourceTypeScript)
    );
    originalController.cancelForView(&originalView);
    QCOMPARE(anchorChanges, 1);
    QCOMPARE(newAnchor, &remainingView);
    QCOMPARE(finishedRequests, 0);

    remainingController.cancelForView(&remainingView);
    QCOMPARE(finishedRequests, 1);

    CrossDomainPromptController ownerController(nullptr, nullptr);
    CrossDomainPromptController nonOwnerController(nullptr, nullptr);
    QWebEngineView ownerView;
    QWebEngineView nonOwnerView;
    int unexpectedReanchors = 0;
    int coordinatedFinishes = 0;
    connect(
        &ownerController,
        &CrossDomainPromptController::requestReanchorRequested,
        &ownerController,
        [&](QWebEngineView *,
            const QList<CrossDomainAffectedView> &,
            const QList<CrossDomainPromptSource> &,
            const QString &,
            const QString &,
            int) {
            ++unexpectedReanchors;
        }
    );
    connect(
        &ownerController,
        &CrossDomainPromptController::requestFinished,
        &ownerController,
        [&](const QString &, const QString &) {
            ++coordinatedFinishes;
        }
    );
    ownerController.currentViewChanged(&ownerView);
    ownerController.request(
        &ownerView,
        {
            {&ownerView, expectedUrl},
            {&nonOwnerView, expectedUrl},
        },
        promptSources,
        sourceSite,
        targetHost,
        int(QWebEngineUrlRequestInfo::ResourceTypeScript)
    );

    for (CrossDomainPromptController *candidate : {
             &nonOwnerController,
             &ownerController,
         }) {
        candidate->cancelForView(&nonOwnerView);
    }
    QCOMPARE(coordinatedFinishes, 0);
    ownerController.cancelForView(&ownerView);
    QCOMPARE(unexpectedReanchors, 0);
    QCOMPARE(coordinatedFinishes, 1);

    CrossDomainPromptController preciseDismissController(nullptr, nullptr);
    QWebEngineView preciseDismissView;
    QList<CrossDomainPromptSource> dismissedSources;
    connect(
        &preciseDismissController,
        &CrossDomainPromptController::requestDismissed,
        &preciseDismissController,
        [&](const QString &dismissedSourceSite,
            const QString &dismissedTargetHost,
            const QList<CrossDomainPromptSource> &sources) {
            QCOMPARE(dismissedSourceSite, sourceSite);
            QCOMPARE(dismissedTargetHost, targetHost);
            dismissedSources = sources;
        }
    );
    preciseDismissController.currentViewChanged(&preciseDismissView);
    preciseDismissController.request(
        &preciseDismissView,
        {{&preciseDismissView, expectedUrl}},
        {{QUrl(QStringLiteral("https://example.com/")), false}},
        sourceSite,
        targetHost,
        int(QWebEngineUrlRequestInfo::ResourceTypeScript)
    );
    preciseDismissController.request(
        &preciseDismissView,
        {{&preciseDismissView, expectedUrl}},
        {{QUrl(QStringLiteral("https://example.com/")), true}},
        sourceSite,
        targetHost,
        int(QWebEngineUrlRequestInfo::ResourceTypeScript)
    );
    preciseDismissController.cancelForView(&preciseDismissView);
    QCOMPARE(dismissedSources.size(), 2);
    QCOMPARE(dismissedSources.at(0).url, QUrl(QStringLiteral("https://example.com/")));
    QVERIFY(!dismissedSources.at(0).originOnly);
    QCOMPARE(dismissedSources.at(1).url, QUrl(QStringLiteral("https://example.com/")));
    QVERIFY(dismissedSources.at(1).originOnly);

    CrossDomainPromptController navigationController(nullptr, nullptr);
    QWebEngineView navigationAnchor;
    QWebEngineView navigationSurvivor;
    const QUrl originalRoute(QStringLiteral("https://example.com/route/one"));
    const QUrl currentRoute(QStringLiteral("https://example.com/route/two"));
    QList<CrossDomainAffectedView> reanchoredViews;
    connect(
        &navigationController,
        &CrossDomainPromptController::requestReanchorRequested,
        &navigationController,
        [&](QWebEngineView *,
            const QList<CrossDomainAffectedView> &affectedViews,
            const QList<CrossDomainPromptSource> &,
            const QString &,
            const QString &,
            int) {
            reanchoredViews = affectedViews;
        }
    );
    navigationController.currentViewChanged(&navigationAnchor);
    navigationController.request(
        &navigationAnchor,
        {
            {&navigationAnchor, originalRoute},
            {&navigationSurvivor, originalRoute},
        },
        {{originalRoute, false}},
        sourceSite,
        targetHost,
        int(QWebEngineUrlRequestInfo::ResourceTypeScript)
    );
    navigationController.request(
        &navigationAnchor,
        {
            {&navigationAnchor, currentRoute},
            {&navigationSurvivor, currentRoute},
        },
        {{currentRoute, false}},
        sourceSite,
        targetHost,
        int(QWebEngineUrlRequestInfo::ResourceTypeScript)
    );
    navigationController.cancelForView(&navigationAnchor);
    QCOMPARE(reanchoredViews.size(), 1);
    QCOMPARE(reanchoredViews.constFirst().webView, &navigationSurvivor);
    QCOMPARE(reanchoredViews.constFirst().expectedUrl, currentRoute);
}

void WindowInteractionTests::crossDomainPromptRoutesOnlyMatchingPageIdentities()
{
    const QUrl sourceUrl(QStringLiteral("https://example.com/old?request=1#fragment"));
    const QList<QUrl> candidates = {
        QUrl(QStringLiteral("https://example.com/new?request=1")),
        QUrl(QStringLiteral("https://example.com/old?request=1#other-fragment")),
        QUrl(QStringLiteral("https://other.example/page")),
    };

    QCOMPARE(
        crossDomainPromptCandidateIndexes(
            sourceUrl,
            QStringLiteral("example.com"),
            false,
            candidates
        ),
        QList<qsizetype>({1})
    );
    QVERIFY(
        crossDomainPromptCandidateIndexes(
            sourceUrl,
            QStringLiteral("example.com"),
            false,
            {candidates.constFirst()}
        ).isEmpty()
    );
    QCOMPARE(
        crossDomainPromptCandidateIndexes(
            QUrl(QStringLiteral("https://example.com/")),
            QStringLiteral("example.com"),
            true,
            {candidates.constFirst()}
        ),
        QList<qsizetype>({0})
    );
    QVERIFY(
        crossDomainPromptCandidateIndexes(
            QUrl(QStringLiteral("https://example.com/")),
            QStringLiteral("example.com"),
            true,
            candidates
        ).isEmpty()
    );

    const QList<QUrl> sameSiteDifferentOrigins = {
        QUrl(QStringLiteral("https://news.example.com/page")),
        QUrl(QStringLiteral("https://app.example.com/another-page")),
    };
    QCOMPARE(
        crossDomainPromptCandidateIndexes(
            QUrl(QStringLiteral("https://app.example.com/")),
            QStringLiteral("example.com"),
            true,
            sameSiteDifferentOrigins
        ),
        QList<qsizetype>({1})
    );
    QVERIFY(
        crossDomainPromptCandidateIndexes(
            QUrl(QStringLiteral("https://app.example.com/")),
            QStringLiteral("example.com"),
            true,
            {sameSiteDifferentOrigins.constFirst()}
        ).isEmpty()
    );
}

void WindowInteractionTests::settingsDialogRegistersEveryPageAndSelectsInitialPage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    BrowserProfile profile(false);
    HistoryStore historyStore(directory.filePath(QStringLiteral("history.sqlite")));
    WebAppStore webAppStore(directory.filePath(QStringLiteral("web-apps.json")));
    UnavailableTestCredentialStore credentialStore;
    SettingsDialogContext context;
    context.trustConfigurationPath = directory.filePath(QStringLiteral("trust.json"));
    context.searchConfigurationPath = directory.filePath(QStringLiteral("search.json"));
    context.dnsConfigurationPath = directory.filePath(QStringLiteral("dns.json"));
    context.proxyConfigurationPath = directory.filePath(QStringLiteral("proxy.json"));
    context.crossDomainConfigurationPath = directory.filePath(
        QStringLiteral("site-connections.json")
    );
    context.videoTranslationConfigurationPath = directory.filePath(
        QStringLiteral("video-translation.json")
    );
    context.crossDomainSettings.setEnabledPresetIds({QStringLiteral("public-cdns")});
    context.profile = &profile;
    context.historyStore = &historyStore;
    context.webAppStore = &webAppStore;
    context.credentialStore = &credentialStore;
    SettingsDialog dialog(
        context,
        QUrl(QStringLiteral("https://current.example/")),
        SettingsDialog::Page::WebApps
    );

    auto *sidebar = dialog.findChild<QListWidget *>(QStringLiteral("settingsSidebar"));
    auto *pages = dialog.findChild<QStackedWidget *>(QStringLiteral("settingsPages"));
    QVERIFY(sidebar);
    QVERIFY(pages);
    QCOMPARE(sidebar->count(), 12);
    QCOMPARE(pages->count(), 12);
    QCOMPARE(
        sidebar->currentItem()->data(Qt::UserRole).toInt(),
        static_cast<int>(SettingsDialog::Page::WebApps)
    );
    QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("webAppsSettingsPage"));
    auto *cdnPreset = dialog.findChild<QCheckBox *>(
        QStringLiteral("connectionPreset_public-cdns")
    );
    QVERIFY(cdnPreset);
    QVERIFY(cdnPreset->isChecked());
    QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("addCrossDomainRule")));

    const QList<SettingsDialog::Page> expectedPages{
        SettingsDialog::Page::General,
        SettingsDialog::Page::Search,
        SettingsDialog::Page::History,
        SettingsDialog::Page::WebApps,
        SettingsDialog::Page::VideoTranslation,
        SettingsDialog::Page::PrivacyData,
        SettingsDialog::Page::Credentials,
        SettingsDialog::Page::Dns,
        SettingsDialog::Page::Proxy,
        SettingsDialog::Page::SiteConnections,
        SettingsDialog::Page::TrustRules,
        SettingsDialog::Page::Diagnostics,
    };
    const QStringList expectedObjectNames{
        QStringLiteral("generalSettingsPage"),
        QStringLiteral("searchSettingsPage"),
        QStringLiteral("historySettingsPage"),
        QStringLiteral("webAppsSettingsPage"),
        QStringLiteral("videoTranslationSettingsPage"),
        QStringLiteral("privacyDataSettingsPage"),
        QStringLiteral("credentialsSettingsPage"),
        QStringLiteral("dnsSettingsPage"),
        QStringLiteral("proxySettingsPage"),
        QStringLiteral("settingsPageScrollArea"),
        QStringLiteral("trustRulesSettingsPage"),
        QStringLiteral("diagnosticsSettingsPage"),
    };
    for (int row = 0; row < expectedPages.size(); ++row) {
        QCOMPARE(
            sidebar->item(row)->data(Qt::UserRole).toInt(),
            static_cast<int>(expectedPages.at(row))
        );
        sidebar->setCurrentRow(row);
        QCOMPARE(pages->currentWidget()->objectName(), expectedObjectNames.at(row));
    }

    auto *credentialsPage = dialog.findChild<CredentialsSettingsPage *>(
        QStringLiteral("credentialsSettingsPage")
    );
    auto *saveButton = dialog.findChild<QPushButton *>(
        QStringLiteral("saveSettingsButton")
    );
    auto *cancelButton = dialog.findChild<QPushButton *>(
        QStringLiteral("cancelSettingsButton")
    );
    QVERIFY(credentialsPage);
    QVERIFY(saveButton);
    QVERIFY(cancelButton);
    QVERIFY(saveButton->isEnabled());
    QVERIFY(cancelButton->isEnabled());

    QVERIFY(QMetaObject::invokeMethod(
        credentialsPage,
        "destructiveOperationActiveChanged",
        Qt::DirectConnection,
        Q_ARG(bool, true)
    ));
    QVERIFY(!saveButton->isEnabled());
    QVERIFY(!cancelButton->isEnabled());
    dialog.setResult(QDialog::Accepted);
    dialog.reject();
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));

    QVERIFY(QMetaObject::invokeMethod(
        credentialsPage,
        "destructiveOperationActiveChanged",
        Qt::DirectConnection,
        Q_ARG(bool, false)
    ));
    QVERIFY(saveButton->isEnabled());
    QVERIFY(cancelButton->isEnabled());
    dialog.reject();
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
}

void WindowInteractionTests::generalSettingsPageRoundTripsPreferences()
{
    BrowserPreferences initial;
    initial.setStartPage(QUrl(QStringLiteral("https://start.example/path")));
    initial.setStartupMode(StartupMode::RestoreTabs);
    initial.setPersistSessionCookies(true);
    initial.setSaveBrowsingHistory(true);
    initial.setDeveloperToolsEnabled(true);
    initial.setInterfaceLanguage(InterfaceLanguage::Russian);

    GeneralSettingsPage page(
        initial,
        QUrl(QStringLiteral("https://current.example/"))
    );
    BrowserPreferences baseline;
    baseline.setSaveBrowsingHistory(false);
    const BrowserPreferences result = page.applyTo(baseline);

    QCOMPARE(result.startPage(), initial.startPage());
    QCOMPARE(result.startupMode(), initial.startupMode());
    QCOMPARE(result.persistSessionCookies(), initial.persistSessionCookies());
    QCOMPARE(result.developerToolsEnabled(), initial.developerToolsEnabled());
    QCOMPARE(result.interfaceLanguage(), initial.interfaceLanguage());
    QVERIFY(!result.saveBrowsingHistory());
}

void WindowInteractionTests::developerToolsPreferenceDefaultsToDisabled()
{
    BrowserPreferences preferences;
    QVERIFY(!preferences.developerToolsEnabled());

    preferences.setDeveloperToolsEnabled(true);
    QVERIFY(preferences.developerToolsEnabled());
}

void WindowInteractionTests::readerModeSettingsAndUrlPolicy()
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

void WindowInteractionTests::readerModeWaitsForCompletedLoadsBeforeProbing()
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

void WindowInteractionTests::readerModeDetectsDelayedSinglePageArticles()
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

void WindowInteractionTests::readerModeRejectsOversizedExtractedMarkup()
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

void WindowInteractionTests::readerModeInvalidatesAvailabilityWhenArticleDisappears()
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

void WindowInteractionTests::readerModeExtractsArticleAndRestoresOriginalPage()
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

void WindowInteractionTests::readerModeRestoresPageBeforeSameDocumentNavigation()
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

void WindowInteractionTests::browserShortcutFallbackMatchesRegisteredKeys()
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

void WindowInteractionTests::tabNavigationWrapsAndTargetsNumberedTabs()
{
    QCOMPARE(TabNavigation::adjacentTabIndex(0, 3, -1), 2);
    QCOMPARE(TabNavigation::adjacentTabIndex(2, 3, 1), 0);
    QCOMPARE(TabNavigation::adjacentTabIndex(1, 3, 1), 2);
    QCOMPARE(TabNavigation::adjacentTabIndex(-1, 3, 1), -1);
    QCOMPARE(TabNavigation::adjacentTabIndex(0, 0, 1), -1);

    QCOMPARE(TabNavigation::numberedTabIndex(1, 12), 0);
    QCOMPARE(TabNavigation::numberedTabIndex(8, 12), 7);
    QCOMPARE(TabNavigation::numberedTabIndex(9, 12), 11);
    QCOMPARE(TabNavigation::numberedTabIndex(3, 2), -1);
    QCOMPARE(TabNavigation::numberedTabIndex(0, 2), -1);
}

void WindowInteractionTests::tabNavigationShortcutsMatchRegisteredKeys()
{
    const auto eventFor = [](const QKeySequence &shortcut) {
        const QKeyCombination combination = shortcut[0];
        return QKeyEvent(
            QEvent::KeyPress,
            combination.key(),
            combination.keyboardModifiers()
        );
    };

    const QList<QKeySequence> newTabShortcuts = TabNavigation::newTabShortcuts();
    const QList<QKeySequence> nextShortcuts = TabNavigation::nextTabShortcuts();
    const QList<QKeySequence> previousShortcuts = TabNavigation::previousTabShortcuts();
    const QList<QKeySequence> reopenShortcuts = TabNavigation::reopenClosedTabShortcuts();
    QVERIFY(!newTabShortcuts.isEmpty());
    QVERIFY(!nextShortcuts.isEmpty());
    QVERIFY(!previousShortcuts.isEmpty());
    QVERIFY(!reopenShortcuts.isEmpty());
    QVERIFY(!newTabShortcuts.constFirst().toString(QKeySequence::NativeText).isEmpty());
    QVERIFY(!nextShortcuts.contains(QKeySequence(Qt::Key_Forward)));
    QVERIFY(!previousShortcuts.contains(QKeySequence(Qt::Key_Back)));
#if defined(Q_OS_MACOS)
    QVERIFY(nextShortcuts.contains(QKeySequence(Qt::META | Qt::Key_Tab)));
    QVERIFY(previousShortcuts.contains(
        QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Tab)
    ));
    QVERIFY(previousShortcuts.contains(
        QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Backtab)
    ));
    QVERIFY(nextShortcuts.contains(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right)
    ));
    QVERIFY(previousShortcuts.contains(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left)
    ));
#else
    QVERIFY(nextShortcuts.contains(QKeySequence(Qt::CTRL | Qt::Key_Tab)));
    QVERIFY(previousShortcuts.contains(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab)
    ));
    QVERIFY(previousShortcuts.contains(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab)
    ));
    QVERIFY(nextShortcuts.contains(QKeySequence(Qt::CTRL | Qt::Key_PageDown)));
    QVERIFY(previousShortcuts.contains(QKeySequence(Qt::CTRL | Qt::Key_PageUp)));
#endif
    QVERIFY(BrowserShortcut::matches(
        eventFor(newTabShortcuts.constFirst()),
        newTabShortcuts
    ));
    QVERIFY(BrowserShortcut::matches(eventFor(nextShortcuts.constFirst()), nextShortcuts));
    QVERIFY(BrowserShortcut::matches(
        eventFor(previousShortcuts.constFirst()),
        previousShortcuts
    ));
    QVERIFY(BrowserShortcut::matches(
        eventFor(reopenShortcuts.constFirst()),
        reopenShortcuts
    ));
#if defined(Q_OS_MACOS)
    const Qt::KeyboardModifiers previousModifiers =
        Qt::MetaModifier | Qt::ShiftModifier;
#else
    const Qt::KeyboardModifiers previousModifiers =
        Qt::ControlModifier | Qt::ShiftModifier;
#endif
    const QKeyEvent backtab(
        QEvent::KeyPress,
        Qt::Key_Backtab,
        previousModifiers
    );
    QVERIFY(BrowserShortcut::matches(backtab, previousShortcuts));

    const QKeySequence firstTab = TabNavigation::numberedTabShortcut(1);
    const QKeySequence lastTab = TabNavigation::numberedTabShortcut(9);
    QVERIFY(!firstTab.isEmpty());
    QVERIFY(!lastTab.isEmpty());
    QVERIFY(BrowserShortcut::matches(eventFor(firstTab), {firstTab}));
    QVERIFY(BrowserShortcut::matches(eventFor(lastTab), {lastTab}));
    QVERIFY(TabNavigation::numberedTabShortcut(0).isEmpty());
    QVERIFY(TabNavigation::numberedTabShortcut(10).isEmpty());
}

void WindowInteractionTests::pageZoomUsesCanonicalOriginsAndDiscreteLevels()
{
    const QString canonical = pageZoomSiteKey(QUrl(QStringLiteral(
        "https://Example.COM:443/path?query=1#fragment"
    )));
    QVERIFY(!canonical.isEmpty());
    QCOMPARE(
        canonical,
        pageZoomSiteKey(QUrl(QStringLiteral("https://example.com/other")))
    );
    QVERIFY(canonical != pageZoomSiteKey(QUrl(QStringLiteral("https://example.com:8443"))));
    QVERIFY(pageZoomSiteKey(QUrl(QStringLiteral("file:///tmp/page.html"))).isEmpty());

    QCOMPARE(normalizedPageZoomFactor(1.09), 1.10);
    QCOMPARE(normalizedPageZoomFactor(42.0), defaultPageZoomFactor);
    QCOMPARE(nextPageZoomFactor(1.0, true), 1.10);
    QCOMPARE(nextPageZoomFactor(1.0, false), 0.90);
    QCOMPARE(nextPageZoomFactor(maximumPageZoomFactor, true), maximumPageZoomFactor);
    QCOMPARE(nextPageZoomFactor(minimumPageZoomFactor, false), minimumPageZoomFactor);
    QCOMPARE(pageZoomPercentage(1.25), 125);
}

void WindowInteractionTests::pageZoomPersistsAndRemovesDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    const QUrl url(QStringLiteral("https://example.com/a"));

    QCOMPARE(storedPageZoomFactor(settings, url), defaultPageZoomFactor);
    QVERIFY(persistPageZoomFactor(settings, url, 1.25));
    QCOMPARE(storedPageZoomFactor(settings, QUrl(QStringLiteral("https://example.com/b"))), 1.25);
    QCOMPARE(storedPageZoomFactor(settings, QUrl(QStringLiteral("https://other.example"))), 1.0);

    QVERIFY(persistPageZoomFactor(settings, url, defaultPageZoomFactor));
    QCOMPARE(storedPageZoomFactor(settings, url), defaultPageZoomFactor);
}

void WindowInteractionTests::pageZoomShortcutsAndWheelDeltas()
{
    const QKeyEvent equal(
        QEvent::KeyPress,
        Qt::Key_Equal,
        Qt::ControlModifier
    );
    QVERIFY(BrowserShortcut::matches(equal, pageZoomInShortcuts()));

    const QKeyEvent shiftedPlus(
        QEvent::KeyPress,
        Qt::Key_Plus,
        Qt::ControlModifier | Qt::ShiftModifier
    );
    QVERIFY(BrowserShortcut::matches(shiftedPlus, pageZoomInShortcuts()));

    const QKeyEvent minus(
        QEvent::KeyPress,
        Qt::Key_Minus,
        Qt::ControlModifier
    );
    QVERIFY(BrowserShortcut::matches(minus, pageZoomOutShortcuts()));

    const QKeyEvent zero(QEvent::KeyPress, Qt::Key_0, Qt::ControlModifier);
    QVERIFY(BrowserShortcut::matches(zero, pageZoomResetShortcuts()));

    int remainder = 0;
    QCOMPARE(takePageZoomSteps(15, 40, remainder), 0);
    QCOMPARE(remainder, 15);
    QCOMPARE(takePageZoomSteps(25, 40, remainder), 1);
    QCOMPARE(remainder, 0);
    QCOMPARE(takePageZoomSteps(85, 40, remainder), 2);
    QCOMPARE(remainder, 5);
    QCOMPARE(takePageZoomSteps(-20, 40, remainder), 0);
    QCOMPARE(remainder, -20);
    QCOMPARE(takePageZoomSteps(-25, 40, remainder), -1);
    QCOMPARE(remainder, -5);
}

void WindowInteractionTests::activeBrowserViewSeparatesDetachedSurfaceFromDialogs()
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

void WindowInteractionTests::fullScreenRequestPolicyValidatesOriginAndState()
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

void WindowInteractionTests::browserFullScreenRestoresWindowChrome()
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

void WindowInteractionTests::videoElementBridgeUsesTrustedOverlayClick()
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

void WindowInteractionTests::detachedVideoSessionCoordinatesReturnAndFallback()
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

void WindowInteractionTests::detachedVideoWindowMovesAndRestoresPage()
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

void WindowInteractionTests::detachedVideoWindowCloseRequestsReturn()
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

void WindowInteractionTests::detachedVideoWindowPreservesAspectRatioWhenResized()
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

void WindowInteractionTests::popupGeometryIsVisibleAndUsable()
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


int runWindowInteractionTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    WindowInteractionTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "WindowInteractionTests.moc"
