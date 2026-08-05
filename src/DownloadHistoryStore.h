#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

enum class DownloadStatus {
    InProgress,
    Paused,
    Completed,
    Cancelled,
    Failed,
    Interrupted,
};

struct DownloadRecord {
    QString id;
    QString filePath;
    QString fileName;
    QString sourceHost;
    QDateTime startedAt;
    QDateTime finishedAt;
    qint64 receivedBytes = 0;
    qint64 totalBytes = -1;
    DownloadStatus status = DownloadStatus::InProgress;
    QString error;
};

class DownloadHistoryStore {
public:
    static constexpr int maximumRecords = 200;

    explicit DownloadHistoryStore(QString path);

    [[nodiscard]] QList<DownloadRecord> load(QString *error = nullptr) const;
    bool save(const QList<DownloadRecord> &records, QString *error = nullptr) const;
    bool clear(QString *error = nullptr) const;

    [[nodiscard]] QString path() const;

private:
    QString m_path;
};

[[nodiscard]] QString downloadStatusToString(DownloadStatus status);
bool downloadStatusFromString(const QString &source, DownloadStatus *status);
