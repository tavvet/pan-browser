#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

class SettingsSaveTransaction final {
public:
    bool capture(const QStringList &paths, QString *error = nullptr);
    [[nodiscard]] QString rollback() const;

private:
    struct FileSnapshot {
        QString path;
        QByteArray contents;
        bool existed = false;
    };

    static bool captureFile(
        const QString &path,
        FileSnapshot *snapshot,
        QString *error
    );
    static bool restoreFile(const FileSnapshot &snapshot, QString *error);

    QList<FileSnapshot> m_snapshots;
};
