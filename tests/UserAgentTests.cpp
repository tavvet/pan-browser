#include "PanBrowserTestCommon.h"

class UserAgentTests final : public QObject {
    Q_OBJECT

private slots:
    void userAgentSettingsRoundTripAndRejectUnsafeValues();
    void userAgentSettingsApplyProfileAndCompareEffectiveConfiguration();
    void userAgentSettingsPageSelectsAndProtectsProfiles();
};

void UserAgentTests::userAgentSettingsRoundTripAndRejectUnsafeValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("user-agents.json"));

    UserAgentSettings settings = UserAgentSettings::defaults();
    QCOMPARE(settings.selectedProfileId(), defaultUserAgentProfileId());
    QCOMPARE(settings.profiles().size(), 5);
    QVERIFY(settings.profileById(QStringLiteral("builtin-chromium-android")));

    UserAgentProfile custom;
    custom.id = QStringLiteral("test-windows");
    custom.name = QStringLiteral("Test Windows profile");
    custom.userAgent = QStringLiteral(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) TestBrowser/1.0"
    );
    custom.platform = UserAgentPlatform::Windows;
    settings.profiles().append(custom);
    settings.setSelectedProfileId(custom.id);

    QString error;
    QVERIFY2(settings.validate(&error), qPrintable(error));
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    QVERIFY2(settings.save(path, &error), qPrintable(error));
    const QString backupPath = path + QStringLiteral(".backup");
    QVERIFY(QFileInfo::exists(backupPath));
#if defined(Q_OS_UNIX)
    for (const QString &privatePath : {path, backupPath}) {
        const QFileDevice::Permissions permissions = QFileInfo(privatePath).permissions();
        QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
        QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
        QVERIFY(!permissions.testFlag(QFileDevice::ReadGroup));
        QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
    }
#endif

    UserAgentSettings loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(loaded.selectedProfileId(), custom.id);
    const UserAgentProfile *restored = loaded.profileById(custom.id);
    QVERIFY(restored);
    QCOMPARE(restored->name, custom.name);
    QCOMPARE(restored->userAgent, custom.userAgent);
    QCOMPARE(restored->platform, UserAgentPlatform::Windows);
    QVERIFY(!restored->mobile);
    QVERIFY(!restored->builtIn);

    UserAgentSettings unsafe = loaded;
    unsafe.profileById(custom.id)->userAgent = QStringLiteral("Valid/1.0\r\nInjected: yes");
    QVERIFY(!unsafe.validate(&error));
    QVERIFY(error.contains(QStringLiteral("printable ASCII")));

    unsafe = loaded;
    unsafe.setSelectedProfileId(QStringLiteral("missing"));
    QVERIFY(!unsafe.validate(&error));
    QVERIFY(error.contains(QStringLiteral("does not exist")));

    unsafe = loaded;
    unsafe.profileById(custom.id)->id = QStringLiteral("builtin-custom");
    QVERIFY(!unsafe.validate(&error));
    QVERIFY(error.contains(QStringLiteral("built-in prefix")));

    UserAgentSettings localizedBuiltInName = loaded;
    localizedBuiltInName.profileById(custom.id)->name =
        localizedBuiltInName.profileById(defaultUserAgentProfileId())->name;
    QVERIFY2(localizedBuiltInName.validate(&error), qPrintable(error));

    UserAgentProfile duplicateCustom = custom;
    duplicateCustom.id = QStringLiteral("duplicate-custom-name");
    loaded.profiles().append(duplicateCustom);
    QVERIFY(!loaded.validate(&error));
    QVERIFY(error.contains(QStringLiteral("Duplicate User-Agent profile name")));
}

void UserAgentTests::userAgentSettingsApplyProfileAndCompareEffectiveConfiguration()
{
    UserAgentSettings defaults = UserAgentSettings::defaults();
    QString error;
    QVERIFY2(defaults.validate(&error), qPrintable(error));

    QWebEngineProfile defaultProfile;
    const QString defaultUserAgent = defaultProfile.httpUserAgent();
    QVERIFY2(
        applyUserAgentSettings(&defaultProfile, defaults, &error),
        qPrintable(error)
    );
    QCOMPARE(defaultProfile.httpUserAgent(), defaultUserAgent);

    UserAgentSettings custom = defaults;
    UserAgentProfile profile;
    profile.id = QStringLiteral("test-android");
    profile.name = QStringLiteral("Test Android");
    profile.userAgent = QStringLiteral(
        "Mozilla/5.0 (Linux; Android 10; K) TestBrowser/2.0 Mobile"
    );
    profile.platform = UserAgentPlatform::Android;
    profile.mobile = true;
    custom.profiles().append(profile);
    custom.setSelectedProfileId(profile.id);

    QWebEngineProfile customProfile;
    QVERIFY2(
        applyUserAgentSettings(&customProfile, custom, &error),
        qPrintable(error)
    );
    QCOMPARE(customProfile.httpUserAgent(), profile.userAgent);
    QVERIFY(customProfile.clientHints());
    QCOMPARE(customProfile.clientHints()->platform(), QStringLiteral("Android"));
    QVERIFY(customProfile.clientHints()->isMobile());
    QVERIFY(!customProfile.clientHints()->isAllClientHintsEnabled());

    QWebEnginePage page(&customProfile);
    bool scriptFinished = false;
    QString navigatorUserAgent;
    page.runJavaScript(
        QStringLiteral("navigator.userAgent"),
        [&](const QVariant &result) {
            navigatorUserAgent = result.toString();
            scriptFinished = true;
        }
    );
    QTRY_VERIFY_WITH_TIMEOUT(scriptFinished, 5000);
    QCOMPARE(navigatorUserAgent, profile.userAgent);
    QVERIFY(!hasSameEffectiveUserAgentConfiguration(defaults, custom));

    UserAgentSettings renamed = custom;
    renamed.profileById(profile.id)->name = QStringLiteral("Renamed only");
    QVERIFY(hasSameEffectiveUserAgentConfiguration(custom, renamed));

    UserAgentSettings inactiveChanged = custom;
    inactiveChanged.profileById(QStringLiteral("builtin-chromium-windows"))->name =
        QStringLiteral("Inactive renamed profile");
    QVERIFY(hasSameEffectiveUserAgentConfiguration(custom, inactiveChanged));

    UserAgentSettings changed = custom;
    changed.profileById(profile.id)->mobile = false;
    QVERIFY(!hasSameEffectiveUserAgentConfiguration(custom, changed));
}

void UserAgentTests::userAgentSettingsPageSelectsAndProtectsProfiles()
{
    UserAgentSettings settings = UserAgentSettings::defaults();
    UserAgentProfile custom;
    custom.id = QStringLiteral("custom-page-test");
    custom.name = QStringLiteral("Custom page test");
    custom.userAgent = QStringLiteral("Mozilla/5.0 TestBrowser/1.0");
    custom.platform = UserAgentPlatform::Linux;
    settings.profiles().append(custom);

    UserAgentSettingsPage page(settings, QStringLiteral("Default/1.0"));
    auto *active = page.findChild<QComboBox *>(QStringLiteral("activeUserAgentProfile"));
    auto *profiles = page.findChild<QListWidget *>(QStringLiteral("userAgentProfilesList"));
    auto *edit = page.findChild<QPushButton *>(QStringLiteral("editUserAgentProfile"));
    auto *duplicate = page.findChild<QPushButton *>(
        QStringLiteral("duplicateUserAgentProfile")
    );
    auto *remove = page.findChild<QPushButton *>(QStringLiteral("removeUserAgentProfile"));
    QVERIFY(active);
    QVERIFY(profiles);
    QVERIFY(edit);
    QVERIFY(duplicate);
    QVERIFY(remove);
    QCOMPARE(active->count(), settings.profiles().size());
    QCOMPARE(profiles->count(), settings.profiles().size());

    const int customIndex = active->findData(custom.id);
    QVERIFY(customIndex >= 0);
    active->setCurrentIndex(customIndex);
    QCOMPARE(page.settings().selectedProfileId(), custom.id);

    profiles->setCurrentRow(profiles->count() - 1);
    QVERIFY(edit->isEnabled());
    QVERIFY(duplicate->isEnabled());
    QVERIFY(remove->isEnabled());

    profiles->setCurrentRow(0);
    QVERIFY(!edit->isEnabled());
    QVERIFY(!duplicate->isEnabled());
    QVERIFY(!remove->isEnabled());
}


int runUserAgentTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    UserAgentTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "UserAgentTests.moc"
