#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QStyleFactory>

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

    MainWindow window;
    window.show();
    return application.exec();
}
