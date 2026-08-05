#include "AddressLineEdit.h"
#include "AddressSuggestion.h"
#include "BookmarkStore.h"
#include "BrowserPreferences.h"
#include "BrowserDataCleanup.h"
#include "DownloadHistoryStore.h"
#include "ExternalNavigationPolicy.h"
#include "HistoryStore.h"
#include "PermissionPolicy.h"
#include "SessionStore.h"
#include "SearchSettings.h"
#include "TrustConfiguration.h"
#include "TrustSettings.h"
#include "WindowPlacement.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

class TrustConfigurationTests final : public QObject {
    Q_OBJECT

private slots:
    void exactDomainIsCaseInsensitive();
    void wildcardMatchesSubdomainsOnly();
    void malformedWildcardsAreRejected();
    void settingsRoundTripAndCreateBackup();
    void overlappingEnabledDomainsAreRejected();
    void runtimeRejectsOverlappingEnabledDomains();
    void customModeRequiresCertificate();
    void disabledDraftMayBeIncomplete();
    void visibleWindowPlacementIsPreserved();
    void disconnectedScreenFallsBackToPrimary();
    void inaccessibleTitleAreaIsRecentered();
    void oversizedWindowFitsSmallerResolution();
    void browserPreferencesValidateStartPage();
    void bookmarksRoundTripNormalizeAndSearch();
    void bookmarksCanBeEditedAndRemoved();
    void corruptBookmarksArePreservedAndDisabled();
    void sessionRoundTripFiltersInvalidUrls();
    void invalidSessionFileFailsClosed();
    void managedDataCleanupStaysInsideRoot();
    void downloadHistoryRoundTripAndLimit();
    void sensitivePermissionsRequireSecureOrigin();
    void unsupportedPermissionsAreDenied();
    void webSchemesStayInsideBrowser();
    void externalSchemesRequireMainFrameConfirmation();
    void dangerousLocalSchemesAreBlocked();
    void popupGeometryIsVisibleAndUsable();
    void searchSettingsRoundTripAndCreateBackup();
    void searchSettingsRejectInvalidTemplatesAndDuplicates();
    void addressInputDistinguishesUrlsAndSearches();
    void addressInputSupportsSearchKeywords();
    void searchTermsAreEncodedExactlyOnce();
    void historySanitizesAndStoresSuccessfulWebVisits();
    void historySuggestionsPreferRelevanceThenRecency();
    void historySuggestionsConsiderOlderExactMatches();
    void historyCanDeleteIndividualVisitsAndClearAll();
    void corruptHistoryIsPreservedAndDisabled();
    void ghostCompletionAcceptsOnlyAddressPrefixes();
    void addressSuggestionsPreferRelevanceThenBookmarks();
};

void TrustConfigurationTests::exactDomainIsCaseInsensitive()
{
    const DomainPattern pattern = DomainPattern::parse(QStringLiteral("Example.COM."));
    QVERIFY(pattern.isValid());
    QVERIFY(pattern.matches(QStringLiteral("example.com")));
    QVERIFY(pattern.matches(QStringLiteral("EXAMPLE.COM.")));
    QVERIFY(!pattern.matches(QStringLiteral("www.example.com")));
}

void TrustConfigurationTests::wildcardMatchesSubdomainsOnly()
{
    const DomainPattern pattern = DomainPattern::parse(QStringLiteral("*.example.com"));
    QVERIFY(pattern.isValid());
    QVERIFY(pattern.matches(QStringLiteral("www.example.com")));
    QVERIFY(pattern.matches(QStringLiteral("api.internal.example.com")));
    QVERIFY(!pattern.matches(QStringLiteral("example.com")));
    QVERIFY(!pattern.matches(QStringLiteral("notexample.com")));
}

void TrustConfigurationTests::malformedWildcardsAreRejected()
{
    QVERIFY(!DomainPattern::parse(QStringLiteral("*.com")).isValid());
    QVERIFY(!DomainPattern::parse(QStringLiteral("exam*ple.com")).isValid());
    QVERIFY(!DomainPattern::parse(QString()).isValid());
}

void TrustConfigurationTests::settingsRoundTripAndCreateBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("rules.json"));

    TrustSettings settings;
    settings.setStartPage(QUrl(QStringLiteral("https://start.example")));

    TrustRuleSettings rule;
    rule.name = QStringLiteral("Example");
    rule.enabled = true;
    rule.mode = TrustMode::SystemOnly;
    rule.domains = {QStringLiteral("example.com"), QStringLiteral("*.example.com")};
    settings.rules().append(rule);

    QString error;
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    QVERIFY(QFile::exists(path));

    settings.rules()[0].name = QStringLiteral("Renamed");
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    QVERIFY(QFile::exists(path + QStringLiteral(".backup")));

    TrustSettings loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(loaded.startPage(), QUrl(QStringLiteral("https://start.example")));
    QCOMPARE(loaded.rules().size(), 1);
    QCOMPARE(loaded.rules().at(0).name, QStringLiteral("Renamed"));
    QCOMPARE(loaded.rules().at(0).domains, rule.domains);
    QCOMPARE(loaded.rules().at(0).mode, TrustMode::SystemOnly);
}

void TrustConfigurationTests::overlappingEnabledDomainsAreRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TrustSettings settings;
    TrustRuleSettings first;
    first.name = QStringLiteral("First");
    first.mode = TrustMode::SystemOnly;
    first.domains = {QStringLiteral("*.Example.COM")};
    settings.rules().append(first);

    TrustRuleSettings second;
    second.name = QStringLiteral("Second");
    second.mode = TrustMode::SystemOnly;
    second.domains = {QStringLiteral("login.example.com.")};
    settings.rules().append(second);

    QString error;
    QVERIFY(!settings.validate(directory.filePath(QStringLiteral("rules.json")), &error));
    QVERIFY(error.contains(QStringLiteral("overlaps rule First")));
}

void TrustConfigurationTests::runtimeRejectsOverlappingEnabledDomains()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("rules.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray contents = R"({
        "version": 1,
        "rules": [
            {
                "name": "Broad",
                "domains": ["*.example.com"],
                "mode": "system-only",
                "anchors": []
            },
            {
                "name": "Narrow",
                "domains": ["login.example.com"],
                "mode": "system-only",
                "anchors": []
            }
        ]
    })";
    QCOMPARE(file.write(contents), contents.size());
    file.close();

    TrustPolicy policy;
    QString error;
    QVERIFY(!policy.load(path, &error));
    QVERIFY(error.contains(QStringLiteral("overlaps rule Broad")));
    QCOMPARE(policy.ruleCount(), 0);
}

void TrustConfigurationTests::customModeRequiresCertificate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TrustSettings settings;
    TrustRuleSettings rule;
    rule.name = QStringLiteral("Custom");
    rule.mode = TrustMode::CustomOnly;
    rule.domains = {QStringLiteral("example.com")};
    settings.rules().append(rule);

    QString error;
    QVERIFY(!settings.validate(directory.filePath(QStringLiteral("rules.json")), &error));
    QVERIFY(error.contains(QStringLiteral("requires at least one certificate")));
}

void TrustConfigurationTests::disabledDraftMayBeIncomplete()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TrustSettings settings;
    TrustRuleSettings rule;
    rule.name = QStringLiteral("Draft");
    rule.enabled = false;
    rule.mode = TrustMode::CustomOnly;
    settings.rules().append(rule);

    QString error;
    QVERIFY2(
        settings.validate(directory.filePath(QStringLiteral("rules.json")), &error),
        qPrintable(error)
    );
}

void TrustConfigurationTests::visibleWindowPlacementIsPreserved()
{
    const QRect screen(0, 0, 1440, 900);
    const QRect window(120, 80, 1180, 760);
    QCOMPARE(adjustedWindowGeometry(window, {screen}, screen), window);
}

void TrustConfigurationTests::disconnectedScreenFallsBackToPrimary()
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

void TrustConfigurationTests::inaccessibleTitleAreaIsRecentered()
{
    const QRect screen(0, 0, 1440, 900);
    const QRect windowWithBodyOnly(-900, -500, 1000, 700);
    const QRect restored = adjustedWindowGeometry(windowWithBodyOnly, {screen}, screen);

    QVERIFY(restored != windowWithBodyOnly);
    QVERIFY(screen.contains(restored));
    QCOMPARE(restored.center(), screen.center());
}

void TrustConfigurationTests::oversizedWindowFitsSmallerResolution()
{
    const QRect smallerScreen(0, 0, 1280, 720);
    const QRect largeWindow(80, 40, 2560, 1400);
    const QRect restored = adjustedWindowGeometry(largeWindow, {smallerScreen}, smallerScreen);

    QCOMPARE(restored, smallerScreen);
}

void TrustConfigurationTests::browserPreferencesValidateStartPage()
{
    BrowserPreferences preferences;
    QString error;
    preferences.setStartPage(QUrl(QStringLiteral("file:///tmp/private")));
    QVERIFY(!preferences.validate(&error));
    QVERIFY(error.contains(QStringLiteral("HTTP or HTTPS")));

    preferences.setStartPage(QUrl(QStringLiteral("https://example.com/start")));
    QVERIFY2(preferences.validate(&error), qPrintable(error));
}

void TrustConfigurationTests::bookmarksRoundTripNormalizeAndSearch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookmarkStore store(directory.filePath(QStringLiteral("bookmarks.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));

    const QUrl source(
        QStringLiteral("https://user:secret@Example.COM:443/docs?q=one#section")
    );
    QVERIFY2(store.addOrUpdate(
        source,
        QStringLiteral("Documentation"),
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const std::optional<Bookmark> saved = store.bookmarkForUrl(source, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(saved.has_value());
    QCOMPARE(saved->url.scheme(), QStringLiteral("https"));
    QCOMPARE(saved->url.host(), QStringLiteral("example.com"));
    QCOMPARE(saved->url.port(), -1);
    QCOMPARE(saved->url.userName(), QString());
    QCOMPARE(saved->url.password(), QString());
    QCOMPARE(saved->url.query(), QStringLiteral("q=one"));
    QCOMPARE(saved->url.fragment(), QStringLiteral("section"));
    QCOMPARE(saved->title, QStringLiteral("Documentation"));

    QVERIFY2(store.addOrUpdate(
        saved->url,
        QStringLiteral("Updated documentation"),
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    const QList<Bookmark> matches = store.bookmarks(QStringLiteral("updated"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.first().title, QStringLiteral("Updated documentation"));
    QCOMPARE(matches.first().createdAt, QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC));
    QCOMPARE(matches.first().updatedAt, QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC));
}

void TrustConfigurationTests::bookmarksCanBeEditedAndRemoved()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookmarkStore store(directory.filePath(QStringLiteral("bookmarks.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));
    QVERIFY2(store.addOrUpdate(
        QUrl(QStringLiteral("https://one.example")),
        QStringLiteral("One"),
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.addOrUpdate(
        QUrl(QStringLiteral("https://two.example")),
        QStringLiteral("Two"),
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const std::optional<Bookmark> first = store.bookmarkForUrl(
        QUrl(QStringLiteral("https://one.example/")),
        &error
    );
    QVERIFY(first.has_value());
    QVERIFY2(store.update(
        first->id,
        QUrl(QStringLiteral("https://renamed.example/path#part")),
        QStringLiteral("Renamed"),
        QDateTime::fromSecsSinceEpoch(3000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY(!store.bookmarkForUrl(QUrl(QStringLiteral("https://one.example")), &error));
    QVERIFY(store.bookmarkForUrl(QUrl(QStringLiteral("https://renamed.example/path#part")), &error));

    QVERIFY2(store.removeUrl(QUrl(QStringLiteral("https://two.example")), &error), qPrintable(error));
    QCOMPARE(store.bookmarks({}, &error).size(), 1);
    QVERIFY2(store.clear(&error), qPrintable(error));
    QVERIFY(store.bookmarks({}, &error).isEmpty());
}

void TrustConfigurationTests::corruptBookmarksArePreservedAndDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("bookmarks.sqlite"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("not a sqlite database"), 21);
    file.close();

    BookmarkStore store(path);
    QString error;
    QVERIFY(!store.open(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!store.isOpen());
    QVERIFY(QFile::exists(path));
    QCOMPARE(QFileInfo(path).size(), 21);
}

void TrustConfigurationTests::sessionRoundTripFiltersInvalidUrls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.json"));
    SessionStore store(path);

    BrowserSession source;
    source.activeIndex = 8;
    source.tabs = {
        {QUrl(QStringLiteral("https://one.example/path")), QStringLiteral("One")},
        {QUrl(QStringLiteral("file:///tmp/private")), QStringLiteral("Private")},
        {QUrl(QStringLiteral("http://two.example")), QStringLiteral("Two")},
    };

    QString error;
    QVERIFY2(store.save(source, &error), qPrintable(error));
    const BrowserSession restored = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.tabs.size(), 2);
    QCOMPARE(restored.tabs.at(0).title, QStringLiteral("One"));
    QCOMPARE(restored.tabs.at(1).url, QUrl(QStringLiteral("http://two.example")));
    QCOMPARE(restored.activeIndex, 1);
}

void TrustConfigurationTests::invalidSessionFileFailsClosed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not json");
    file.close();

    SessionStore store(path);
    QString error;
    const BrowserSession restored = store.load(&error);
    QVERIFY(restored.tabs.isEmpty());
    QVERIFY(!error.isEmpty());
}

void TrustConfigurationTests::managedDataCleanupStaysInsideRoot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString managedRoot = directory.filePath(QStringLiteral("managed"));
    const QString profile = QDir(managedRoot).filePath(QStringLiteral("WebEngine/Profile"));
    QVERIFY(QDir().mkpath(profile));
    QFile marker(QDir(profile).filePath(QStringLiteral("marker")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();

    QString error;
    QVERIFY2(removeManagedDataDirectory(profile, managedRoot, &error), qPrintable(error));
    QVERIFY(!QFile::exists(profile));

    QVERIFY(!removeManagedDataDirectory(managedRoot, managedRoot, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!removeManagedDataDirectory(
        QDir(managedRoot).filePath(QStringLiteral("../outside")),
        managedRoot,
        &error
    ));
}

void TrustConfigurationTests::downloadHistoryRoundTripAndLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DownloadHistoryStore store(directory.filePath(QStringLiteral("downloads.json")));

    QList<DownloadRecord> records;
    for (int index = 0; index < DownloadHistoryStore::maximumRecords + 5; ++index) {
        DownloadRecord record;
        record.id = QString::number(index);
        record.filePath = directory.filePath(QStringLiteral("file-%1.pdf").arg(index));
        record.fileName = QStringLiteral("file-%1.pdf").arg(index);
        record.sourceHost = QStringLiteral("files.example");
        record.startedAt = QDateTime::fromSecsSinceEpoch(1000 + index, QTimeZone::UTC);
        record.finishedAt = QDateTime::fromSecsSinceEpoch(1100 + index, QTimeZone::UTC);
        record.receivedBytes = index * 100;
        record.totalBytes = index * 100;
        record.status = DownloadStatus::Completed;
        records.append(record);
    }

    QString error;
    QVERIFY2(store.save(records, &error), qPrintable(error));
    const QList<DownloadRecord> restored = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.size(), DownloadHistoryStore::maximumRecords);
    QCOMPARE(restored.first().id, QStringLiteral("0"));
    QCOMPARE(restored.last().id, QString::number(DownloadHistoryStore::maximumRecords - 1));
    QCOMPARE(restored.at(5).sourceHost, QStringLiteral("files.example"));
    QCOMPARE(restored.at(5).status, DownloadStatus::Completed);
    QCOMPARE(restored.at(5).receivedBytes, 500);
}

void TrustConfigurationTests::sensitivePermissionsRequireSecureOrigin()
{
    const QUrl secureOrigin(QStringLiteral("https://bank.example"));
    const QUrl insecureOrigin(QStringLiteral("http://bank.example"));

    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Camera),
        PermissionDisposition::Prompt
    );
    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Microphone),
        PermissionDisposition::Prompt
    );
    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::CameraAndMicrophone),
        PermissionDisposition::Prompt
    );
    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Location),
        PermissionDisposition::Prompt
    );
    QCOMPARE(
        permissionDisposition(insecureOrigin, BrowserPermissionKind::Camera),
        PermissionDisposition::Deny
    );
}

void TrustConfigurationTests::unsupportedPermissionsAreDenied()
{
    const QUrl secureOrigin(QStringLiteral("https://bank.example"));

    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Notifications),
        PermissionDisposition::Deny
    );
    QCOMPARE(
        permissionDisposition(secureOrigin, BrowserPermissionKind::Other),
        PermissionDisposition::Deny
    );
    QVERIFY(!permissionTitle(BrowserPermissionKind::Location).isEmpty());
    QVERIFY(!permissionDescription(BrowserPermissionKind::Location).isEmpty());
}

void TrustConfigurationTests::webSchemesStayInsideBrowser()
{
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("https://example.com")), true),
        ExternalNavigationDisposition::Browse
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("http://example.com")), false),
        ExternalNavigationDisposition::Browse
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("blob:https://example.com/id")), true),
        ExternalNavigationDisposition::Browse
    );
}

void TrustConfigurationTests::externalSchemesRequireMainFrameConfirmation()
{
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("mailto:user@example.com")), true),
        ExternalNavigationDisposition::Prompt
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("bank-app://open/payment")), true),
        ExternalNavigationDisposition::Prompt
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("bank-app://open/payment")), false),
        ExternalNavigationDisposition::Block
    );
}

void TrustConfigurationTests::dangerousLocalSchemesAreBlocked()
{
    QCOMPARE(
        externalNavigationDisposition(QUrl::fromLocalFile(QStringLiteral("/tmp/file")), true),
        ExternalNavigationDisposition::Block
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(QStringLiteral("javascript:alert(1)")), true),
        ExternalNavigationDisposition::Block
    );
    QCOMPARE(
        externalNavigationDisposition(QUrl(), true),
        ExternalNavigationDisposition::Block
    );
}

void TrustConfigurationTests::popupGeometryIsVisibleAndUsable()
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

void TrustConfigurationTests::searchSettingsRoundTripAndCreateBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("search-engines.json"));

    SearchSettings settings = SearchSettings::defaults();
    SearchEngineSettings custom;
    custom.id = QStringLiteral("custom-example");
    custom.name = QStringLiteral("Example Search");
    custom.keyword = QStringLiteral("ex");
    custom.urlTemplate = QStringLiteral("https://search.example/?q={searchTerms}");
    settings.engines().append(custom);
    settings.setDefaultEngineId(custom.id);

    QString error;
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    settings.setDefaultEngineId(QStringLiteral("builtin-duckduckgo"));
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    QVERIFY(QFile::exists(path + QStringLiteral(".backup")));

    SearchSettings loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(loaded.engines().size(), 5);
    QCOMPARE(loaded.defaultEngine()->name, QStringLiteral("DuckDuckGo"));
    QCOMPARE(loaded.engineForKeyword(QStringLiteral("@ex"))->name, QStringLiteral("Example Search"));
}

void TrustConfigurationTests::searchSettingsRejectInvalidTemplatesAndDuplicates()
{
    SearchSettings settings = SearchSettings::defaults();
    QString error;

    settings.engines()[0].urlTemplate = QStringLiteral("https://example.com/search");
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("{searchTerms}")));

    settings = SearchSettings::defaults();
    settings.engines()[1].keyword = settings.engines()[0].keyword;
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Duplicate search keyword")));

    settings = SearchSettings::defaults();
    settings.engines()[0].urlTemplate = QStringLiteral("https://user:secret@example.com/?q={searchTerms}");
    QVERIFY(!settings.validate(&error));
    QVERIFY(error.contains(QStringLiteral("credentials")));
}

void TrustConfigurationTests::addressInputDistinguishesUrlsAndSearches()
{
    const SearchSettings settings = SearchSettings::defaults();

    ResolvedAddressInput result = resolveAddressInput(QStringLiteral("example.com/path"), settings);
    QCOMPARE(result.kind, AddressInputKind::Navigate);
    QCOMPARE(result.url.host(), QStringLiteral("example.com"));

    result = resolveAddressInput(QStringLiteral("localhost:8080/test"), settings);
    QCOMPARE(result.kind, AddressInputKind::Navigate);
    QCOMPARE(result.url.port(), 8080);

    result = resolveAddressInput(QStringLiteral("как войти в банк"), settings);
    QCOMPARE(result.kind, AddressInputKind::Search);
    QCOMPARE(result.url.host(), QStringLiteral("duckduckgo.com"));

    result = resolveAddressInput(QStringLiteral("singleword"), settings);
    QCOMPARE(result.kind, AddressInputKind::Search);

    result = resolveAddressInput(QStringLiteral("mailto:user@example.com"), settings);
    QCOMPARE(result.kind, AddressInputKind::Error);
}

void TrustConfigurationTests::addressInputSupportsSearchKeywords()
{
    SearchSettings settings = SearchSettings::defaults();
    ResolvedAddressInput result = resolveAddressInput(QStringLiteral("@g qt webengine"), settings);
    QCOMPARE(result.kind, AddressInputKind::Search);
    QCOMPARE(result.url.host(), QStringLiteral("www.google.com"));
    QCOMPARE(result.engineId, QStringLiteral("builtin-google"));

    result = resolveAddressInput(QStringLiteral("? example.com"), settings);
    QCOMPARE(result.kind, AddressInputKind::Search);
    QCOMPARE(result.url.host(), QStringLiteral("duckduckgo.com"));

    result = resolveAddressInput(QStringLiteral("@missing query"), settings);
    QCOMPARE(result.kind, AddressInputKind::Error);
    QVERIFY(result.error.contains(QStringLiteral("Unknown")));
}

void TrustConfigurationTests::searchTermsAreEncodedExactlyOnce()
{
    const SearchSettings settings = SearchSettings::defaults();
    QString error;
    const QUrl url = settings.searchUrl(QStringLiteral("C++ 100% русский"), {}, &error);
    QVERIFY2(url.isValid(), qPrintable(error));
    const QString encoded = url.toString(QUrl::FullyEncoded);
    QVERIFY(encoded.contains(QStringLiteral("C%2B%2B%20100%25%20")));
    QVERIFY(!encoded.contains(QStringLiteral("%252B")));
}

void TrustConfigurationTests::historySanitizesAndStoresSuccessfulWebVisits()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));

    const QUrl source(QStringLiteral("https://user:secret@example.com/path?q=one#token"));
    QVERIFY2(store.recordVisit(
        source,
        QStringLiteral("Example Page"),
        HistoryTransition::Typed,
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const QList<HistoryVisit> visits = store.visits({}, 10, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(visits.size(), 1);
    QCOMPARE(visits.first().url.userName(), QString());
    QCOMPARE(visits.first().url.password(), QString());
    QCOMPARE(visits.first().url.fragment(), QString());
    QCOMPARE(visits.first().url.query(), QStringLiteral("q=one"));
    QCOMPARE(visits.first().title, QStringLiteral("Example Page"));

    QVERIFY(!store.recordVisit(
        QUrl(QStringLiteral("file:///tmp/private")),
        QStringLiteral("Private"),
        HistoryTransition::Other,
        QDateTime::currentDateTimeUtc(),
        &error
    ));
}

void TrustConfigurationTests::historySuggestionsPreferRelevanceThenRecency()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));

    QVERIFY2(store.recordVisit(
        QUrl(QStringLiteral("https://example.com/old")),
        QStringLiteral("Example"),
        HistoryTransition::Typed,
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.recordVisit(
        QUrl(QStringLiteral("https://recent.test/path?source=example")),
        QStringLiteral("Recent page"),
        HistoryTransition::Link,
        QDateTime::fromSecsSinceEpoch(3000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.recordVisit(
        QUrl(QStringLiteral("https://example.net/new")),
        QStringLiteral("New example"),
        HistoryTransition::Link,
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const QList<HistorySuggestion> suggestions = store.suggestions(
        QStringLiteral("example"),
        8,
        &error
    );
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(suggestions.size(), 3);
    QCOMPARE(suggestions.at(0).url.host(), QStringLiteral("example.net"));
    QCOMPARE(suggestions.at(1).url.host(), QStringLiteral("example.com"));
    QCOMPARE(suggestions.at(2).url.host(), QStringLiteral("recent.test"));
}

void TrustConfigurationTests::historySuggestionsConsiderOlderExactMatches()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));

    const QUrl exactUrl(QStringLiteral("https://needle.example/"));
    QVERIFY2(store.recordVisit(
        exactUrl,
        QStringLiteral("Old exact match"),
        HistoryTransition::Typed,
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    for (int index = 0; index < 201; ++index) {
        QVERIFY2(store.recordVisit(
            QUrl(QStringLiteral("https://recent-%1.test/?q=needle.example").arg(index)),
            QStringLiteral("Recent weak match"),
            HistoryTransition::Link,
            QDateTime::fromSecsSinceEpoch(2000 + index, QTimeZone::UTC),
            &error
        ), qPrintable(error));
    }

    const QList<HistorySuggestion> suggestions = store.suggestions(
        QStringLiteral("needle.example"),
        8,
        &error
    );
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(suggestions.size(), 8);
    QCOMPARE(suggestions.first().url, exactUrl);
}

void TrustConfigurationTests::historyCanDeleteIndividualVisitsAndClearAll()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    HistoryStore store(directory.filePath(QStringLiteral("history.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));
    const QUrl url(QStringLiteral("https://example.com/"));
    QVERIFY2(store.recordVisit(
        url,
        QStringLiteral("First title"),
        HistoryTransition::Typed,
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.recordVisit(
        url,
        QStringLiteral("Updated title"),
        HistoryTransition::Link,
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    QList<HistoryVisit> visits = store.visits({}, 10, &error);
    QCOMPARE(visits.size(), 2);
    QVERIFY2(store.removeVisits({visits.first().id}, &error), qPrintable(error));
    visits = store.visits({}, 10, &error);
    QCOMPARE(visits.size(), 1);
    const QList<HistorySuggestion> suggestions = store.suggestions(
        QStringLiteral("example"),
        8,
        &error
    );
    QCOMPARE(suggestions.size(), 1);
    QCOMPARE(suggestions.first().visitCount, 1);
    QCOMPARE(suggestions.first().typedCount, 1);

    QVERIFY2(store.clear(&error), qPrintable(error));
    QVERIFY(store.visits({}, 10, &error).isEmpty());
}

void TrustConfigurationTests::corruptHistoryIsPreservedAndDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("history.sqlite"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("not a sqlite database"), 21);
    file.close();

    HistoryStore store(path);
    QString error;
    QVERIFY(!store.open(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!store.isOpen());
    QVERIFY(QFile::exists(path));
    QCOMPARE(QFileInfo(path).size(), 21);
}

void TrustConfigurationTests::ghostCompletionAcceptsOnlyAddressPrefixes()
{
    AddressLineEdit address;
    const QUrl url(QStringLiteral("https://sports.example/news"));
    address.setText(QStringLiteral("spo"));
    address.setGhostCompletion(QStringLiteral("sports.example/news"), url);
    QVERIFY(address.hasGhostCompletion());
    QCOMPARE(address.ghostCompletionUrl(), url);
    QVERIFY(address.acceptGhostCompletion());
    QCOMPARE(address.text(), QStringLiteral("sports.example/news"));

    address.setText(QStringLiteral("news"));
    address.setGhostCompletion(QStringLiteral("sports.example/news"), url);
    QVERIFY(!address.hasGhostCompletion());
    QVERIFY(!address.acceptGhostCompletion());
    QCOMPARE(address.text(), QStringLiteral("news"));
}

void TrustConfigurationTests::addressSuggestionsPreferRelevanceThenBookmarks()
{
    const QDateTime old = QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC);
    const QDateTime recent = QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC);
    const QList<AddressSuggestion> candidates = {
        {
            QUrl(QStringLiteral("https://recent.test/?q=example.com")),
            QStringLiteral("Recent weak bookmark"),
            recent,
            AddressSuggestionSource::Bookmark,
        },
        {
            QUrl(QStringLiteral("https://example.com/")),
            QStringLiteral("Exact history result"),
            old,
            AddressSuggestionSource::History,
        },
        {
            QUrl(QStringLiteral("https://example.net/history")),
            QStringLiteral("Prefix history"),
            recent,
            AddressSuggestionSource::History,
        },
        {
            QUrl(QStringLiteral("https://example.net/bookmark")),
            QStringLiteral("Prefix bookmark"),
            old,
            AddressSuggestionSource::Bookmark,
        },
        {
            QUrl(QStringLiteral("https://example.net/bookmark")),
            QStringLiteral("Duplicate history"),
            recent,
            AddressSuggestionSource::History,
        },
    };

    const QList<AddressSuggestion> ranked = rankedAddressSuggestions(
        candidates,
        QStringLiteral("example.com"),
        8
    );
    QCOMPARE(ranked.first().url.host(), QStringLiteral("example.com"));

    const QList<AddressSuggestion> prefixRanked = rankedAddressSuggestions(
        candidates,
        QStringLiteral("example.net"),
        8
    );
    QCOMPARE(prefixRanked.size(), 2);
    QCOMPARE(prefixRanked.first().source, AddressSuggestionSource::Bookmark);
    QCOMPARE(prefixRanked.first().title, QStringLiteral("Prefix bookmark"));
}

QTEST_MAIN(TrustConfigurationTests)
#include "TrustConfigurationTests.moc"
