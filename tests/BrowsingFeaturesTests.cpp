#include "PanBrowserTestCommon.h"

class BrowsingFeaturesTests final : public QObject {
    Q_OBJECT

private slots:
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
    void addressCompletionPopupActivatesMouseSelection();
    void addressSuggestionsPreferRelevanceThenBookmarks();
    void findBarSupportsKeyboardNavigationAndCounts();
};

void BrowsingFeaturesTests::searchSettingsRoundTripAndCreateBackup()
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

void BrowsingFeaturesTests::searchSettingsRejectInvalidTemplatesAndDuplicates()
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

void BrowsingFeaturesTests::addressInputDistinguishesUrlsAndSearches()
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

void BrowsingFeaturesTests::addressInputSupportsSearchKeywords()
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

void BrowsingFeaturesTests::searchTermsAreEncodedExactlyOnce()
{
    const SearchSettings settings = SearchSettings::defaults();
    QString error;
    const QUrl url = settings.searchUrl(QStringLiteral("C++ 100% русский"), {}, &error);
    QVERIFY2(url.isValid(), qPrintable(error));
    const QString encoded = url.toString(QUrl::FullyEncoded);
    QVERIFY(encoded.contains(QStringLiteral("C%2B%2B%20100%25%20")));
    QVERIFY(!encoded.contains(QStringLiteral("%252B")));
}

void BrowsingFeaturesTests::historySanitizesAndStoresSuccessfulWebVisits()
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

void BrowsingFeaturesTests::historySuggestionsPreferRelevanceThenRecency()
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

void BrowsingFeaturesTests::historySuggestionsConsiderOlderExactMatches()
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

void BrowsingFeaturesTests::historyCanDeleteIndividualVisitsAndClearAll()
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

void BrowsingFeaturesTests::corruptHistoryIsPreservedAndDisabled()
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

void BrowsingFeaturesTests::ghostCompletionAcceptsOnlyAddressPrefixes()
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

void BrowsingFeaturesTests::addressCompletionPopupActivatesMouseSelection()
{
    QWidget window;
    window.resize(700, 240);
    AddressLineEdit address(&window);
    address.setGeometry(20, 20, 660, 36);
    window.show();
    QTRY_VERIFY(window.isVisible());

    AddressCompletionPopup popup(&address, &window);
    const QUrl expectedUrl(QStringLiteral("https://example.com/path"));
    AddressSuggestion suggestion;
    suggestion.url = expectedUrl;
    suggestion.title = QStringLiteral("Example");
    suggestion.lastUsedAt = QDateTime::currentDateTimeUtc();
    suggestion.source = AddressSuggestionSource::History;

    QSignalSpy activated(&popup, &AddressCompletionPopup::urlActivated);
    popup.showSuggestions({suggestion});
    QTRY_VERIFY(popup.isVisible());
    auto *list = popup.findChild<QListWidget *>(
        QStringLiteral("addressCompletionList")
    );
    QVERIFY(list);
    QVERIFY(list->count() == 1);
    const QRect itemRect = list->visualItemRect(list->item(0));
    QVERIFY(itemRect.isValid());

    const QPoint globalItemCenter = list->viewport()->mapToGlobal(itemRect.center());
    const QPoint popupLocalCenter = popup.mapFromGlobal(globalItemCenter);
    QMouseEvent windowPress(
        QEvent::MouseButtonPress,
        QPointF(popupLocalCenter),
        QPointF(popupLocalCenter),
        QPointF(globalItemCenter),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QVERIFY(QApplication::sendEvent(popup.windowHandle(), &windowPress));
    QVERIFY2(
        popup.isVisible(),
        "A native popup-window mouse press must not be mistaken for an outside click"
    );

    QTest::mouseClick(
        list->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        itemRect.center()
    );
    QTRY_COMPARE(activated.count(), 1);
    QCOMPARE(activated.takeFirst().at(0).toUrl(), expectedUrl);
}

void BrowsingFeaturesTests::addressSuggestionsPreferRelevanceThenBookmarks()
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

void BrowsingFeaturesTests::findBarSupportsKeyboardNavigationAndCounts()
{
    FindBar bar;
    auto *input = bar.findChild<QLineEdit *>(QStringLiteral("findInput"));
    auto *result = bar.findChild<QLabel *>(QStringLiteral("findResult"));
    QVERIFY(input);
    QVERIFY(result);

    QSignalSpy querySpy(&bar, &FindBar::queryChanged);
    QSignalSpy navigationSpy(&bar, &FindBar::navigationRequested);
    QSignalSpy closeSpy(&bar, &FindBar::closeRequested);
    input->setText(QStringLiteral("needle"));
    QCOMPARE(querySpy.count(), 1);

    bar.setResults(2, 5);
    QCOMPARE(result->text(), QStringLiteral("2 of 5"));
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(navigationSpy.takeFirst().at(0).toBool(), false);

    QTest::keyClick(input, Qt::Key_Return, Qt::ShiftModifier);
    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(navigationSpy.takeFirst().at(0).toBool(), true);

    QTest::keyClick(input, Qt::Key_Escape);
    QCOMPARE(closeSpy.count(), 1);
    bar.setResults(0, 0);
    QCOMPARE(result->text(), QStringLiteral("No matches"));
}


int runBrowsingFeaturesTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    BrowsingFeaturesTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "BrowsingFeaturesTests.moc"
