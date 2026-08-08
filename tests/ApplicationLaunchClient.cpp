#include "ApplicationLaunch.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() < 2) {
        QTextStream(stderr) << "Missing launch test mode\n";
        return 2;
    }

    const QString mode = arguments.at(1);
    if (mode == QStringLiteral("server")) {
        if (arguments.size() != 4) {
            QTextStream(stderr)
                << "Usage: PanBrowserLaunchClient server <name> <request-count>\n";
            return 2;
        }
        bool countValid = false;
        const int expectedRequestCount = arguments.at(3).toInt(&countValid);
        if (!countValid || expectedRequestCount < 1) {
            QTextStream(stderr) << "Invalid expected request count\n";
            return 2;
        }

        SingleInstanceCoordinator coordinator(arguments.at(2));
        QString error;
        const SingleInstanceCoordinator::StartResult result = coordinator.start(
            ApplicationLaunchRequest::activate(),
            &error
        );
        if (result != SingleInstanceCoordinator::StartResult::Primary) {
            QTextStream(stderr)
                << "Server launch result " << static_cast<int>(result) << ": " << error << '\n';
            return 1;
        }

        QTextStream output(stdout);
        int receivedRequestCount = 0;
        QObject::connect(
            &coordinator,
            &SingleInstanceCoordinator::launchRequested,
            &application,
            [&](const ApplicationLaunchRequest &request) {
                output << request.toPayload().toBase64() << '\n';
                output.flush();
                ++receivedRequestCount;
                if (receivedRequestCount >= expectedRequestCount)
                    application.quit();
            }
        );
        QTimer::singleShot(10000, &application, [&] {
            QTextStream(stderr) << "Timed out waiting for launch requests\n";
            application.exit(3);
        });
        output << "READY\n";
        output.flush();
        return application.exec();
    }

    if (mode != QStringLiteral("client") || arguments.size() != 5) {
        QTextStream(stderr)
            << "Usage: PanBrowserLaunchClient client <server> <command> <value>\n";
        return 2;
    }

    ApplicationLaunchRequest request;
    const QString command = arguments.at(3);
    if (command == QStringLiteral("open-web-app")) {
        request = ApplicationLaunchRequest::openWebApp(arguments.at(4));
    } else if (command == QStringLiteral("open-url")) {
        request = ApplicationLaunchRequest::openUrl(QUrl(arguments.at(4)));
    } else {
        QTextStream(stderr) << "Unsupported launch command: " << command << '\n';
        return 2;
    }

    SingleInstanceCoordinator coordinator(arguments.at(2));
    QString error;
    const SingleInstanceCoordinator::StartResult result = coordinator.start(request, &error);
    if (result != SingleInstanceCoordinator::StartResult::Forwarded) {
        QTextStream(stderr)
            << "Launch result " << static_cast<int>(result) << ": " << error << '\n';
        return 1;
    }
    return 0;
}
