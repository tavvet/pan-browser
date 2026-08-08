#include "PrivateData.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString nativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

} // namespace

namespace PrivateData {

bool ensureDirectory(const QString &path, QString *error)
{
    if (path.isEmpty() || !QDir().mkpath(path)) {
        return fail(
            error,
            QCoreApplication::translate(
                "PrivateData",
                "Cannot create private data directory %1"
            ).arg(nativePath(path))
        );
    }
#if defined(Q_OS_UNIX)
    const QFileDevice::Permissions permissions = QFileDevice::ReadOwner
        | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
    if (!QFile::setPermissions(path, permissions)) {
        return fail(
            error,
            QCoreApplication::translate(
                "PrivateData",
                "Cannot restrict permissions for %1"
            ).arg(nativePath(path))
        );
    }
#else
    Q_UNUSED(error)
#endif
    return true;
}

bool restrictFile(const QString &path, QString *error)
{
    if (!QFileInfo::exists(path))
        return true;
#if defined(Q_OS_UNIX)
    const QFileDevice::Permissions permissions =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    if (!QFile::setPermissions(path, permissions)) {
        return fail(
            error,
            QCoreApplication::translate(
                "PrivateData",
                "Cannot restrict permissions for %1"
            ).arg(nativePath(path))
        );
    }
#else
    Q_UNUSED(error)
#endif
    return true;
}

bool restrictDatabaseFiles(const QString &path, QString *error)
{
    if (!restrictFile(path, error))
        return false;
    for (const QString &suffix : {
             QStringLiteral("-wal"),
             QStringLiteral("-shm"),
             QStringLiteral("-journal"),
         }) {
        if (!restrictFile(path + suffix, error))
            return false;
    }
    return true;
}

} // namespace PrivateData
