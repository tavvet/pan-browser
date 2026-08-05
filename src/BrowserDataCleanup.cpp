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

    const QString root = QDir::cleanPath(QDir(managedRoot).absolutePath());
    const QString directory = QDir::cleanPath(QDir(directoryPath).absolutePath());
    const QString requiredPrefix = root + QDir::separator();
    if (root.isEmpty()
        || directory.isEmpty()
        || directory == root
        || !directory.startsWith(requiredPrefix)) {
        return fail(error, QStringLiteral("Refusing to remove data outside %1").arg(root));
    }

    QDir target(directory);
    if (!target.exists())
        return true;
    if (!target.removeRecursively())
        return fail(error, QStringLiteral("Cannot remove %1").arg(directory));
    return true;
}
