#include "PanBrowserTestCommon.h"

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
    void developerToolsPreferenceDefaultsToDisabled();
    void browserShortcutFallbackMatchesRegisteredKeys();
    void pageZoomUsesCanonicalOriginsAndDiscreteLevels();
    void pageZoomPersistsAndRemovesDefaults();
    void pageZoomShortcutsAndWheelDeltas();
    void activeBrowserViewSeparatesDetachedSurfaceFromDialogs();
    void fullScreenRequestPolicyValidatesOriginAndState();
    void detachedVideoSessionCoordinatesReturnAndFallback();
    void detachedVideoWindowMovesAndRestoresPage();
    void detachedVideoWindowCloseRequestsReturn();
    void popupGeometryIsVisibleAndUsable();
    void crossDomainPromptRoutesOnlyMatchingPageIdentities();
    void crossDomainPromptCoordinatesCanceledViewsAcrossControllers();
    void settingsDialogRegistersEveryPageAndSelectsInitialPage();
    void generalSettingsPageRoundTripsPreferences();
};

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
    SettingsDialogContext context;
    context.trustConfigurationPath = directory.filePath(QStringLiteral("trust.json"));
    context.searchConfigurationPath = directory.filePath(QStringLiteral("search.json"));
    context.dnsConfigurationPath = directory.filePath(QStringLiteral("dns.json"));
    context.proxyConfigurationPath = directory.filePath(QStringLiteral("proxy.json"));
    context.crossDomainConfigurationPath = directory.filePath(
        QStringLiteral("site-connections.json")
    );
    context.crossDomainSettings.setEnabledPresetIds({QStringLiteral("public-cdns")});
    context.profile = &profile;
    context.historyStore = &historyStore;
    context.webAppStore = &webAppStore;
    SettingsDialog dialog(
        context,
        QUrl(QStringLiteral("https://current.example/")),
        SettingsDialog::Page::WebApps
    );

    auto *sidebar = dialog.findChild<QListWidget *>(QStringLiteral("settingsSidebar"));
    auto *pages = dialog.findChild<QStackedWidget *>(QStringLiteral("settingsPages"));
    QVERIFY(sidebar);
    QVERIFY(pages);
    QCOMPARE(sidebar->count(), 10);
    QCOMPARE(pages->count(), 10);
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
        SettingsDialog::Page::PrivacyData,
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
        QStringLiteral("privacyDataSettingsPage"),
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
    const FullScreenRequestDecision accepted = decideFullScreenRequest(
        true,
        true,
        false,
        QUrl(QStringLiteral("HTTPS://Example.COM:443/watch?v=1#player"))
    );
    QCOMPARE(accepted.action, FullScreenRequestAction::Detach);
    QCOMPARE(accepted.origin.scheme(), QStringLiteral("https"));
    QCOMPARE(accepted.origin.host(), QStringLiteral("example.com"));
    QCOMPARE(accepted.origin.port(), -1);
    QCOMPARE(fullScreenOriginDisplay(accepted.origin), QStringLiteral("https://example.com"));

    const FullScreenRequestDecision customPort = decideFullScreenRequest(
        true,
        true,
        false,
        QUrl(QStringLiteral("http://example.com:8080/video"))
    );
    QCOMPARE(customPort.action, FullScreenRequestAction::Detach);
    QCOMPARE(customPort.origin.port(), 8080);
    QCOMPARE(
        fullScreenOriginDisplay(customPort.origin),
        QStringLiteral("http://example.com:8080")
    );

    const FullScreenRequestDecision internationalizedHost = decideFullScreenRequest(
        true,
        true,
        false,
        QUrl(QStringLiteral("https://пример.рф/video"))
    );
    QCOMPARE(internationalizedHost.action, FullScreenRequestAction::Detach);
    QCOMPARE(
        fullScreenOriginDisplay(internationalizedHost.origin),
        QStringLiteral("https://xn--e1afmkfd.xn--p1ai")
    );

    QCOMPARE(
        decideFullScreenRequest(
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
            true,
            QUrl(QStringLiteral("https://example.com"))
        ).action,
        FullScreenRequestAction::Reject
    );
    QCOMPARE(
        decideFullScreenRequest(
            true,
            true,
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
            QUrl(QStringLiteral("https://user@example.com/video"))
        ).action,
        FullScreenRequestAction::Reject
    );
    QCOMPARE(
        decideFullScreenRequest(false, false, true, QUrl()).action,
        FullScreenRequestAction::Restore
    );
    QVERIFY(fullScreenOriginDisplay(QUrl(QStringLiteral("file:///tmp/video.html"))).isEmpty());
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
            QStringLiteral("Video — https://example.com"),
            QStringLiteral("Source:"),
            QStringLiteral("https://example.com")
        );
        QCOMPARE(detachedWindow.windowTitle(), QStringLiteral("Video — https://example.com"));
        QCOMPARE(
            detachedWindow.sourceOriginText(),
            QStringLiteral("https://example.com")
        );
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

        detachedWindow.restorePage();
        QCOMPARE(sourceView.page(), originalPage);
        QCOMPARE(QWebEngineView::forPage(originalPage), &sourceView);

        detachedWindow.restorePage();
        QCOMPARE(sourceView.page(), originalPage);
    }

    {
        DetachedVideoWindow detachedWindow(
            &sourceView,
            QStringLiteral("Video — https://example.com"),
            QStringLiteral("Source:"),
            QStringLiteral("https://example.com")
        );
        QCOMPARE(QWebEngineView::forPage(originalPage), detachedWindow.webView());
    }
    QCOMPARE(sourceView.page(), originalPage);
    QCOMPARE(QWebEngineView::forPage(originalPage), &sourceView);

    QWebEngineView narrowSourceView;
    const QString longOrigin = QStringLiteral(
        "https://online.vtb.ru.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.evil.example"
    );
    const QString longCaption = QStringLiteral(
        "A deliberately long translated source address caption:"
    );
    QString copiedOrigin;
    DetachedVideoWindow narrowWindow(
        &narrowSourceView,
        QStringLiteral("Video"),
        longCaption,
        longOrigin,
        nullptr,
        [&copiedOrigin](const QString &text) {
            copiedOrigin = text;
        }
    );
    QLabel *narrowOriginLabel = narrowWindow.findChild<QLabel *>(
        QStringLiteral("detachedVideoOrigin")
    );
    QVERIFY(narrowOriginLabel);
    narrowWindow.resize(360, 220);
    narrowWindow.show();
    QApplication::processEvents();
    QVERIFY(narrowOriginLabel->text().contains(QChar(0x2026)));
    QVERIFY2(
        narrowOriginLabel->text().startsWith(QStringLiteral("https://")),
        qPrintable(QStringLiteral("Displayed origin: %1").arg(narrowOriginLabel->text()))
    );
    QVERIFY(narrowOriginLabel->text().endsWith(QStringLiteral("evil.example")));
    QCOMPARE(narrowOriginLabel->toolTip(), longOrigin);
    QCOMPARE(narrowOriginLabel->accessibleName(), longOrigin);
    QCOMPARE(narrowWindow.sourceOriginText(), longOrigin);
    QLabel *originCaption = narrowWindow.findChild<QLabel *>(
        QStringLiteral("detachedVideoOriginCaption")
    );
    QVERIFY(originCaption);
    QVERIFY(originCaption->text().contains(QChar(0x2026)));
    QCOMPARE(originCaption->toolTip(), longCaption);
    QCOMPARE(originCaption->accessibleName(), longCaption);

    const QKeyCombination copyCombination = QKeySequence(QKeySequence::Copy)[0];
    QTest::keyClick(
        narrowOriginLabel,
        copyCombination.key(),
        copyCombination.keyboardModifiers()
    );
    QCOMPARE(copiedOrigin, longOrigin);
}

void WindowInteractionTests::detachedVideoWindowCloseRequestsReturn()
{
    QWebEngineView sourceView;
    QWebEnginePage *originalPage = sourceView.page();
    DetachedVideoWindow detachedWindow(
        &sourceView,
        QStringLiteral("Video — https://example.com"),
        QStringLiteral("Source:"),
        QStringLiteral("https://example.com")
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
