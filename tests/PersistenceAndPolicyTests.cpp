#include "PanBrowserTestCommon.h"

class PersistenceAndPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void privateDataFilesUseOwnerOnlyPermissions();
    void interfaceLanguagePreferenceRoundTrips();
    void interfaceLanguageSettingsParsing();
    void systemInterfaceLanguageUsesFirstSupportedLanguage();
    void unsupportedSystemInterfaceLanguageFallsBackToEnglish();
    void embeddedTranslationCatalogsLoad();
    void bookmarksRoundTripNormalizeAndSearch();
    void bookmarksCanBeEditedAndRemoved();
    void bookmarkEditRejectsDuplicateAddress();
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
};

void PersistenceAndPolicyTests::privateDataFilesUseOwnerOnlyPermissions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString privateDirectory = directory.filePath(QStringLiteral("private"));

    QString error;
    const QString sessionPath = QDir(privateDirectory).filePath(QStringLiteral("session.json"));
    SessionStore sessionStore(sessionPath);
    BrowserSession session;
    session.tabs = {
        {QUrl(QStringLiteral("https://example.com")), QStringLiteral("Example")},
    };
    QVERIFY2(sessionStore.save(session, &error), qPrintable(error));

    const QString downloadsPath = QDir(privateDirectory).filePath(
        QStringLiteral("downloads.json")
    );
    DownloadHistoryStore downloadStore(downloadsPath);
    QVERIFY2(downloadStore.save({}, &error), qPrintable(error));

    const QString historyPath = QDir(privateDirectory).filePath(
        QStringLiteral("history.sqlite")
    );
    HistoryStore historyStore(historyPath);
    QVERIFY2(historyStore.open(&error), qPrintable(error));

    const QString bookmarksPath = QDir(privateDirectory).filePath(
        QStringLiteral("bookmarks.sqlite")
    );
    BookmarkStore bookmarkStore(bookmarksPath);
    QVERIFY2(bookmarkStore.open(&error), qPrintable(error));

#if defined(Q_OS_UNIX)
    const QFileDevice::Permissions publicPermissions = QFileDevice::ReadGroup
        | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    const auto isOwnerOnly = [publicPermissions](const QString &path) {
        const QFileDevice::Permissions permissions = QFileInfo(path).permissions();
        return permissions.testFlag(QFileDevice::ReadOwner)
            && permissions.testFlag(QFileDevice::WriteOwner)
            && (permissions & publicPermissions) == QFileDevice::Permissions();
    };

    QVERIFY(isOwnerOnly(privateDirectory));
    QVERIFY(QFileInfo(privateDirectory).permissions().testFlag(QFileDevice::ExeOwner));
    QVERIFY(isOwnerOnly(sessionPath));
    QVERIFY(isOwnerOnly(downloadsPath));
    QVERIFY(isOwnerOnly(historyPath));
    QVERIFY(isOwnerOnly(bookmarksPath));
    if (QFileInfo::exists(historyPath + QStringLiteral("-wal")))
        QVERIFY(isOwnerOnly(historyPath + QStringLiteral("-wal")));
    if (QFileInfo::exists(historyPath + QStringLiteral("-shm")))
        QVERIFY(isOwnerOnly(historyPath + QStringLiteral("-shm")));
    if (QFileInfo::exists(bookmarksPath + QStringLiteral("-wal")))
        QVERIFY(isOwnerOnly(bookmarksPath + QStringLiteral("-wal")));
    if (QFileInfo::exists(bookmarksPath + QStringLiteral("-shm")))
        QVERIFY(isOwnerOnly(bookmarksPath + QStringLiteral("-shm")));
#endif
}

void PersistenceAndPolicyTests::interfaceLanguagePreferenceRoundTrips()
{
    BrowserPreferences preferences;
    QCOMPARE(preferences.interfaceLanguage(), InterfaceLanguage::System);
    preferences.setInterfaceLanguage(InterfaceLanguage::Russian);
    QCOMPARE(preferences.interfaceLanguage(), InterfaceLanguage::Russian);

    QCOMPARE(
        LocalizationManager::resolveLanguage(InterfaceLanguage::English, {QStringLiteral("ru-RU")}),
        QStringLiteral("en")
    );
    QCOMPARE(
        LocalizationManager::resolveLanguage(InterfaceLanguage::Russian, {QStringLiteral("en-US")}),
        QStringLiteral("ru")
    );
}

void PersistenceAndPolicyTests::interfaceLanguageSettingsParsing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("preferences.ini")), QSettings::IniFormat);

    QCOMPARE(
        BrowserPreferences::loadInterfaceLanguage(settings),
        InterfaceLanguage::System
    );
    settings.setValue(QStringLiteral("Browser/language"), QStringLiteral("ru"));
    QCOMPARE(
        BrowserPreferences::loadInterfaceLanguage(settings),
        InterfaceLanguage::Russian
    );
    settings.setValue(QStringLiteral("Browser/language"), QStringLiteral("en"));
    QCOMPARE(
        BrowserPreferences::loadInterfaceLanguage(settings),
        InterfaceLanguage::English
    );
    settings.setValue(QStringLiteral("Browser/language"), QStringLiteral("unsupported"));
    QCOMPARE(
        BrowserPreferences::loadInterfaceLanguage(settings),
        InterfaceLanguage::English
    );
}

void PersistenceAndPolicyTests::systemInterfaceLanguageUsesFirstSupportedLanguage()
{
    QCOMPARE(
        LocalizationManager::resolveLanguage(
            InterfaceLanguage::System,
            {QStringLiteral("fi-FI"), QStringLiteral("ru-RU"), QStringLiteral("en-US")}
        ),
        QStringLiteral("ru")
    );
    QCOMPARE(
        LocalizationManager::resolveLanguage(
            InterfaceLanguage::System,
            {QStringLiteral("en-GB"), QStringLiteral("ru-RU")}
        ),
        QStringLiteral("en")
    );
}

void PersistenceAndPolicyTests::unsupportedSystemInterfaceLanguageFallsBackToEnglish()
{
    QCOMPARE(
        LocalizationManager::resolveLanguage(
            InterfaceLanguage::System,
            {QStringLiteral("fi-FI"), QStringLiteral("sv-SE")}
        ),
        QStringLiteral("en")
    );
    QCOMPARE(
        LocalizationManager::resolveLanguage(InterfaceLanguage::System, {}),
        QStringLiteral("en")
    );
}

void PersistenceAndPolicyTests::embeddedTranslationCatalogsLoad()
{
    QTranslator russian;
    QVERIFY(russian.load(QStringLiteral(":/i18n/panbrowser_ru.qm")));
    QVERIFY(QCoreApplication::installTranslator(&russian));
    QCOMPARE(
        QCoreApplication::translate("SettingsDialog", "Settings"),
        QStringLiteral("Настройки")
    );
    QCOMPARE(
        QCoreApplication::translate("DnsSettingsPage", "Secure DNS only"),
        QStringLiteral("Только защищённый DNS")
    );
    QCOMPARE(
        QCoreApplication::translate("ProxySettingsPage", "Manual proxy"),
        QStringLiteral("Ручной прокси")
    );
    QCOMPARE(
        QCoreApplication::translate("QPlatformTheme", "Cancel"),
        QStringLiteral("Отмена")
    );
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 1),
        QStringLiteral("1 закладка")
    );
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 2),
        QStringLiteral("2 закладки")
    );
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 5),
        QStringLiteral("5 закладок")
    );
    QVERIFY(QCoreApplication::removeTranslator(&russian));

    QTranslator english;
    QVERIFY(english.load(QStringLiteral(":/i18n/panbrowser_en.qm")));
    QVERIFY(QCoreApplication::installTranslator(&english));
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 1),
        QStringLiteral("1 bookmark")
    );
    QCOMPARE(
        QCoreApplication::translate("BookmarksDialog", "%n bookmark(s)", nullptr, 2),
        QStringLiteral("2 bookmarks")
    );
    QVERIFY(QCoreApplication::removeTranslator(&english));
}

void PersistenceAndPolicyTests::bookmarksRoundTripNormalizeAndSearch()
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

void PersistenceAndPolicyTests::bookmarksCanBeEditedAndRemoved()
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

void PersistenceAndPolicyTests::bookmarkEditRejectsDuplicateAddress()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookmarkStore store(directory.filePath(QStringLiteral("bookmarks.sqlite")));
    QString error;
    QVERIFY2(store.open(&error), qPrintable(error));
    QVERIFY2(store.addOrUpdate(
        QUrl(QStringLiteral("https://one.example/")),
        QStringLiteral("One"),
        QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC),
        &error
    ), qPrintable(error));
    QVERIFY2(store.addOrUpdate(
        QUrl(QStringLiteral("https://two.example/")),
        QStringLiteral("Two"),
        QDateTime::fromSecsSinceEpoch(2000, QTimeZone::UTC),
        &error
    ), qPrintable(error));

    const std::optional<Bookmark> first = store.bookmarkForUrl(
        QUrl(QStringLiteral("https://one.example/")),
        &error
    );
    QVERIFY(first.has_value());
    error.clear();
    QVERIFY(!store.update(
        first->id,
        QUrl(QStringLiteral("https://two.example/")),
        QStringLiteral("Duplicate"),
        QDateTime::fromSecsSinceEpoch(3000, QTimeZone::UTC),
        &error
    ));
    QCOMPARE(error, QStringLiteral("A bookmark for this address already exists"));

    error.clear();
    const QList<Bookmark> bookmarks = store.bookmarks({}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(bookmarks.size(), 2);
    const std::optional<Bookmark> unchanged = store.bookmarkForUrl(
        QUrl(QStringLiteral("https://one.example/")),
        &error
    );
    QVERIFY(unchanged.has_value());
    QCOMPARE(unchanged->title, QStringLiteral("One"));
}

void PersistenceAndPolicyTests::corruptBookmarksArePreservedAndDisabled()
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

void PersistenceAndPolicyTests::sessionRoundTripFiltersInvalidUrls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("private/session.json"));
    SessionStore store(path);

    BrowserSession source;
    source.activeIndex = 8;
    source.tabs = {
        {QUrl(QStringLiteral("https://alice:secret@one.example/path")), QStringLiteral("One")},
        {QUrl(QStringLiteral("file:///tmp/private")), QStringLiteral("Private")},
        {QUrl(QStringLiteral("http://two.example")), QStringLiteral("Two")},
    };

    QString error;
    QVERIFY2(store.save(source, &error), qPrintable(error));
    const BrowserSession restored = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.tabs.size(), 2);
    QCOMPARE(restored.tabs.at(0).title, QStringLiteral("One"));
    QCOMPARE(restored.tabs.at(0).url, QUrl(QStringLiteral("https://one.example/path")));
    QCOMPARE(restored.tabs.at(1).url, QUrl(QStringLiteral("http://two.example")));
    QCOMPARE(restored.activeIndex, 1);

    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QVERIFY(!saved.readAll().contains("secret"));
    saved.close();

    QFile legacy(path);
    QVERIFY(legacy.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray legacyContents = QByteArrayLiteral(
        R"json({"version":1,"activeIndex":0,"tabs":[{"url":"https://bob:legacy-secret@legacy.example/path","title":"Legacy"}]})json"
    );
    QCOMPARE(legacy.write(legacyContents), legacyContents.size());
    legacy.close();

    const BrowserSession migrated = store.load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(migrated.tabs.size(), 1);
    QCOMPARE(
        migrated.tabs.constFirst().url,
        QUrl(QStringLiteral("https://legacy.example/path"))
    );
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QVERIFY(!saved.readAll().contains("legacy-secret"));
}

void PersistenceAndPolicyTests::invalidSessionFileFailsClosed()
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

void PersistenceAndPolicyTests::managedDataCleanupStaysInsideRoot()
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

void PersistenceAndPolicyTests::downloadHistoryRoundTripAndLimit()
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

void PersistenceAndPolicyTests::sensitivePermissionsRequireSecureOrigin()
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

void PersistenceAndPolicyTests::unsupportedPermissionsAreDenied()
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

void PersistenceAndPolicyTests::webSchemesStayInsideBrowser()
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

void PersistenceAndPolicyTests::externalSchemesRequireMainFrameConfirmation()
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

void PersistenceAndPolicyTests::dangerousLocalSchemesAreBlocked()
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


int runPersistenceAndPolicyTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    PersistenceAndPolicyTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "PersistenceAndPolicyTests.moc"
