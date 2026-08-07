#include "BrowserDataCleanup.h"

#include <QDir>

namespace {

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

} // namespace

bool removeManagedDataDirectory(
    const QString &directoryPath,
    const QString &managedRoot,
    QString *error
)
{
    if (error)
        error->clear();

    const QString root = QDir::fromNativeSeparators(
        QDir::cleanPath(QDir(managedRoot).absolutePath())
    );
    const QString directory = QDir::fromNativeSeparators(
        QDir::cleanPath(QDir(directoryPath).absolutePath())
    );
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif
    const QString requiredPrefix = root + QLatin1Char('/');
    if (root.isEmpty()
        || directory.isEmpty()
        || directory.compare(root, pathCaseSensitivity) == 0
        || !directory.startsWith(requiredPrefix, pathCaseSensitivity)) {
        return fail(error, QStringLiteral("Refusing to remove data outside %1").arg(root));
    }

    QDir target(directory);
    if (!target.exists())
        return true;
    if (!target.removeRecursively())
        return fail(error, QStringLiteral("Cannot remove %1").arg(directory));
    return true;
}
