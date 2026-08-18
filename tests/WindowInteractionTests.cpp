#include "PanBrowserTestCommon.h"

#include <QStyle>

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
    context.userAgentConfigurationPath = directory.filePath(
        QStringLiteral("user-agents.json")
    );
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
    QCOMPARE(sidebar->count(), 13);
    QCOMPARE(pages->count(), 13);
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
        SettingsDialog::Page::UserAgent,
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
        QStringLiteral("userAgentSettingsPage"),
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
