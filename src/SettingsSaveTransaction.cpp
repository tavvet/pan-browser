#include "SettingsSaveTransaction.h"

#include "PrivateData.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>

bool SettingsSaveTransaction::capture(const QStringList &paths, QString *error)
{
    m_snapshots.clear();
    m_snapshots.reserve(paths.size());
    for (const QString &path : paths) {
        FileSnapshot snapshot;
        if (!captureFile(path, &snapshot, error)) {
            m_snapshots.clear();
            return false;
        }
        m_snapshots.append(snapshot);
    }
    return true;
}

QString SettingsSaveTransaction::rollback() const
{
    QStringList failures;
    for (const FileSnapshot &snapshot : m_snapshots) {
        QString error;
        if (!restoreFile(snapshot, &error))
            failures.append(error);
    }
    return failures.join(QLatin1Char('\n'));
}

bool SettingsSaveTransaction::captureFile(
    const QString &path,
    FileSnapshot *snapshot,
    QString *error
)
{
    snapshot->path = path;
    snapshot->existed = QFile::exists(path);
    snapshot->contents.clear();
    if (!snapshot->existed)
        return true;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QCoreApplication::translate(
                "SettingsDialog",
                "Cannot snapshot %1: %2"
            ).arg(path, file.errorString());
        }
        return false;
    }
    snapshot->contents = file.readAll();
    return true;
}

bool SettingsSaveTransaction::restoreFile(
    const FileSnapshot &snapshot,
    QString *error
)
{
    if (!snapshot.existed) {
        if (!QFile::exists(snapshot.path) || QFile::remove(snapshot.path))
            return true;
        if (error) {
            *error = QCoreApplication::translate(
                "SettingsDialog",
                "Cannot remove %1 during rollback"
            ).arg(snapshot.path);
        }
        return false;
    }

    QSaveFile file(snapshot.path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QCoreApplication::translate(
                "SettingsDialog",
                "Cannot restore %1: %2"
            ).arg(snapshot.path, file.errorString());
        }
        return false;
    }
    if (file.write(snapshot.contents) != snapshot.contents.size() || !file.commit()) {
        if (error) {
            *error = QCoreApplication::translate(
                "SettingsDialog",
                "Cannot restore %1: %2"
            ).arg(snapshot.path, file.errorString());
        }
        return false;
    }
    return PrivateData::restrictFile(snapshot.path, error);
}
