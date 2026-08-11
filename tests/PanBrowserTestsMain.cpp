#include <QDebug>
#include <QString>
#include <QtGlobal>

int runTrustConfigurationTests(int argc, char **argv);
int runTrustRulesSettingsTests(int argc, char **argv);
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
int runCertificateTrustValidatorTests(int argc, char **argv);
#endif
int runWindowInteractionTests(int argc, char **argv);
int runPersistenceAndPolicyTests(int argc, char **argv);
int runNetworkSettingsAndAuthTests(int argc, char **argv);
int runBrowsingFeaturesTests(int argc, char **argv);
int runApplicationAndWebAppTests(int argc, char **argv);

int main(int argc, char *argv[])
{
    if (argc < 2) {
        qCritical("A PanBrowser test suite name is required.");
        return 2;
    }

    const QString suite = QString::fromLocal8Bit(argv[1]);
    for (int index = 1; index < argc - 1; ++index)
        argv[index] = argv[index + 1];
    --argc;
    argv[argc] = nullptr;

    if (suite == QStringLiteral("trust-configuration"))
        return runTrustConfigurationTests(argc, argv);
    if (suite == QStringLiteral("trust-rules"))
        return runTrustRulesSettingsTests(argc, argv);
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (suite == QStringLiteral("certificate-validator"))
        return runCertificateTrustValidatorTests(argc, argv);
#endif
    if (suite == QStringLiteral("window"))
        return runWindowInteractionTests(argc, argv);
    if (suite == QStringLiteral("persistence"))
        return runPersistenceAndPolicyTests(argc, argv);
    if (suite == QStringLiteral("network"))
        return runNetworkSettingsAndAuthTests(argc, argv);
    if (suite == QStringLiteral("browsing"))
        return runBrowsingFeaturesTests(argc, argv);
    if (suite == QStringLiteral("webapps"))
        return runApplicationAndWebAppTests(argc, argv);

    qCritical().noquote() << "Unknown PanBrowser test suite:" << suite;
    return 2;
}
