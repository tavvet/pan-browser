#include "ApplicationLaunch.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() != 4) {
        QTextStream(stderr) << "Usage: PanBrowserLaunchClient <server> <command> <value>\n";
        return 2;
    }

    ApplicationLaunchRequest request;
    const QString command = arguments.at(2);
    if (command == QStringLiteral("open-web-app")) {
        request = ApplicationLaunchRequest::openWebApp(arguments.at(3));
    } else if (command == QStringLiteral("open-url")) {
        request = ApplicationLaunchRequest::openUrl(QUrl(arguments.at(3)));
    } else {
        QTextStream(stderr) << "Unsupported launch command: " << command << '\n';
        return 2;
    }

    SingleInstanceCoordinator coordinator(arguments.at(1));
    QString error;
    const SingleInstanceCoordinator::StartResult result = coordinator.start(request, &error);
    if (result != SingleInstanceCoordinator::StartResult::Forwarded) {
        QTextStream(stderr)
            << "Launch result " << static_cast<int>(result) << ": " << error << '\n';
        return 1;
    }
    return 0;
}
