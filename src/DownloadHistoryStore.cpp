#include "DownloadHistoryStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

namespace {

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

} // namespace

QString downloadStatusToString(DownloadStatus status)
{
    switch (status) {
    case DownloadStatus::InProgress:
        return QStringLiteral("in-progress");
    case DownloadStatus::Paused:
        return QStringLiteral("paused");
    case DownloadStatus::Completed:
        return QStringLiteral("completed");
    case DownloadStatus::Cancelled:
        return QStringLiteral("cancelled");
    case DownloadStatus::Failed:
        return QStringLiteral("failed");
    case DownloadStatus::Interrupted:
        return QStringLiteral("interrupted");
    }
    return QStringLiteral("interrupted");
}

bool downloadStatusFromString(const QString &source, DownloadStatus *status)
{
    if (!status)
        return false;
    if (source == QStringLiteral("in-progress"))
        *status = DownloadStatus::InProgress;
    else if (source == QStringLiteral("paused"))
        *status = DownloadStatus::Paused;
    else if (source == QStringLiteral("completed"))
        *status = DownloadStatus::Completed;
    else if (source == QStringLiteral("cancelled"))
        *status = DownloadStatus::Cancelled;
    else if (source == QStringLiteral("failed"))
        *status = DownloadStatus::Failed;
    else if (source == QStringLiteral("interrupted"))
        *status = DownloadStatus::Interrupted;
    else
        return false;
    return true;
}

DownloadHistoryStore::DownloadHistoryStore(QString path)
    : m_path(std::move(path))
{
}

QList<DownloadRecord> DownloadHistoryStore::load(QString *error) const
{
    if (error)
        error->clear();
    QList<DownloadRecord> records;
    QFile file(m_path);
    if (!file.exists())
        return records;
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, QStringLiteral("Cannot open %1: %2").arg(m_path, file.errorString()));
        return records;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, QStringLiteral("Invalid download history: %1").arg(parseError.errorString()));
        return records;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1) {
        fail(error, QStringLiteral("Unsupported download history version"));
        return records;
    }

    const QJsonArray items = root.value(QStringLiteral("downloads")).toArray();
    for (const QJsonValue &value : items) {
        if (records.size() >= maximumRecords)
            break;
        const QJsonObject object = value.toObject();
        DownloadRecord record;
        record.id = object.value(QStringLiteral("id")).toString();
        record.filePath = object.value(QStringLiteral("filePath")).toString();
        record.fileName = object.value(QStringLiteral("fileName")).toString();
        record.sourceHost = object.value(QStringLiteral("sourceHost")).toString();
        record.startedAt = QDateTime::fromString(
            object.value(QStringLiteral("startedAt")).toString(),
            Qt::ISODateWithMs
        );
        record.finishedAt = QDateTime::fromString(
            object.value(QStringLiteral("finishedAt")).toString(),
            Qt::ISODateWithMs
        );
        record.receivedBytes = object.value(QStringLiteral("receivedBytes")).toVariant().toLongLong();
        record.totalBytes = object.value(QStringLiteral("totalBytes")).toVariant().toLongLong();
        record.error = object.value(QStringLiteral("error")).toString();
        if (record.id.isEmpty()
            || record.filePath.isEmpty()
            || !downloadStatusFromString(
                object.value(QStringLiteral("status")).toString(),
                &record.status
            )) {
            continue;
        }
        if (record.fileName.isEmpty())
            record.fileName = QFileInfo(record.filePath).fileName();
        records.append(record);
    }
    return records;
}

bool DownloadHistoryStore::save(const QList<DownloadRecord> &records, QString *error) const
{
    if (error)
        error->clear();
    QJsonArray items;
    for (const DownloadRecord &record : records) {
        if (items.size() >= maximumRecords)
            break;
        if (record.id.isEmpty() || record.filePath.isEmpty())
            continue;
        QJsonObject object;
        object.insert(QStringLiteral("id"), record.id);
        object.insert(QStringLiteral("filePath"), record.filePath);
        object.insert(QStringLiteral("fileName"), record.fileName);
        object.insert(QStringLiteral("sourceHost"), record.sourceHost);
        object.insert(QStringLiteral("startedAt"), record.startedAt.toUTC().toString(Qt::ISODateWithMs));
        object.insert(QStringLiteral("finishedAt"), record.finishedAt.toUTC().toString(Qt::ISODateWithMs));
        object.insert(QStringLiteral("receivedBytes"), record.receivedBytes);
        object.insert(QStringLiteral("totalBytes"), record.totalBytes);
        object.insert(QStringLiteral("status"), downloadStatusToString(record.status));
        object.insert(QStringLiteral("error"), record.error);
        items.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("downloads"), items);

    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return fail(error, QStringLiteral("Cannot create download history directory"));
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString()));
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size())
        return fail(error, QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString()));
    if (!file.commit())
        return fail(error, QStringLiteral("Cannot commit %1: %2").arg(m_path, file.errorString()));
    return true;
}

bool DownloadHistoryStore::clear(QString *error) const
{
    if (error)
        error->clear();
    if (!QFile::exists(m_path) || QFile::remove(m_path))
        return true;
    return fail(error, QStringLiteral("Cannot remove %1").arg(m_path));
}

QString DownloadHistoryStore::path() const
{
    return m_path;
}
