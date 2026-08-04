#include "TrustConfiguration.h"
#include "TrustSettings.h"
#include "WindowPlacement.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TrustConfigurationTests final : public QObject {
    Q_OBJECT

private slots:
    void exactDomainIsCaseInsensitive();
    void wildcardMatchesSubdomainsOnly();
    void malformedWildcardsAreRejected();
    void settingsRoundTripAndCreateBackup();
    void overlappingEnabledDomainsAreRejected();
    void customModeRequiresCertificate();
    void disabledDraftMayBeIncomplete();
    void visibleWindowPlacementIsPreserved();
    void disconnectedScreenFallsBackToPrimary();
    void inaccessibleTitleAreaIsRecentered();
    void oversizedWindowFitsSmallerResolution();
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

QTEST_MAIN(TrustConfigurationTests)
#include "TrustConfigurationTests.moc"
