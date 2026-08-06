#include "ApplicationLaunch.h"
#include "MainWindow.h"
#include "BrowserPreferences.h"
#include "Localization.h"
#ifdef Q_OS_MACOS
#include "MacApplicationReopen.h"
#endif

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QLocale>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    application.setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.svg")));
    QCoreApplication::setApplicationName(QStringLiteral("PanBrowser"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    // Keep AppDataLocation stable at ~/Library/Application Support/PanBrowser.
    // Window preferences use an explicit QSettings identity instead.
    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setOrganizationDomain(QStringLiteral("panbrowser.dev"));

    LocalizationManager localization;
    localization.install(
        application,
        BrowserPreferences::loadInterfaceLanguage(),
        QLocale::system().uiLanguages()
    );

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("PanBrowser"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption appIdOption(
        QStringList{QStringLiteral("app-id")},
        QStringLiteral("Open an installed web app by its ID."),
        QStringLiteral("id")
    );
    parser.addOption(appIdOption);
    parser.addPositionalArgument(
        QStringLiteral("url"),
        QStringLiteral("Open an HTTP or HTTPS URL."),
        QStringLiteral("[url]")
    );
    parser.process(application);

    const QString startupAppId = parser.value(appIdOption).trimmed();
    const QStringList positionalArguments = parser.positionalArguments();
    ApplicationLaunchRequest initialRequest = ApplicationLaunchRequest::activate();
    if (!startupAppId.isEmpty()) {
        initialRequest = ApplicationLaunchRequest::openWebApp(startupAppId);
    } else if (!positionalArguments.isEmpty()) {
        initialRequest = ApplicationLaunchRequest::openUrl(
            QUrl::fromUserInput(positionalArguments.constFirst())
        );
    }
    SingleInstanceCoordinator instanceCoordinator;
#ifdef Q_OS_MACOS
    MacApplicationReopenHandler reopenHandler;
#endif
    QString instanceError;
    const SingleInstanceCoordinator::StartResult instanceResult =
        instanceCoordinator.start(initialRequest, &instanceError);
    if (instanceResult == SingleInstanceCoordinator::StartResult::Forwarded)
        return 0;
    if (instanceResult == SingleInstanceCoordinator::StartResult::Error) {
        QMessageBox::critical(nullptr, QStringLiteral("PanBrowser"), instanceError);
        return 1;
    }

    MainWindow window(
        initialRequest.command == ApplicationLaunchRequest::Command::Activate
            ? MainWindow::StartupPresentation::Browser
            : MainWindow::StartupPresentation::Background
    );
    const auto handleLaunchRequest = [&window](const ApplicationLaunchRequest &request) {
        switch (request.command) {
        case ApplicationLaunchRequest::Command::OpenWebApp:
            if (window.launchInstalledWebApp(request.webAppId))
                return;
            window.activatePrimaryWindow();
            QMessageBox::warning(
                &window,
                QCoreApplication::translate("main", "Cannot open web app"),
                QCoreApplication::translate(
                    "main",
                    "The requested web app is not installed or its registry is unavailable."
                )
            );
            return;
        case ApplicationLaunchRequest::Command::OpenUrl:
            window.openUrlInPrimaryWindow(request.url);
            return;
        case ApplicationLaunchRequest::Command::Activate:
            window.activatePrimaryWindow();
            return;
        }
    };
    QObject::connect(
        &instanceCoordinator,
        &SingleInstanceCoordinator::launchRequested,
        &window,
        handleLaunchRequest
    );
#ifdef Q_OS_MACOS
    QObject::connect(
        &reopenHandler,
        &MacApplicationReopenHandler::reopenRequested,
        &window,
        &MainWindow::activatePrimaryWindow
    );
#endif
    handleLaunchRequest(initialRequest);
    return application.exec();
}
