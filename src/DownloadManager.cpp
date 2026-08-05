#include "DownloadManager.h"

#include "BrowserProfile.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QUrl>
#include <QWebEngineDownloadRequest>

namespace {

bool isActive(DownloadStatus status)
{
    return status == DownloadStatus::InProgress || status == DownloadStatus::Paused;
}

QString safeFileName(const QString &suggested)
{
    QString fileName = QFileInfo(suggested).fileName().trimmed();
    fileName.replace(QLatin1Char('\\'), QLatin1Char('_'));
    if (fileName.isEmpty() || fileName == QStringLiteral(".") || fileName == QStringLiteral(".."))
        return QStringLiteral("download");
    return fileName;
}

} // namespace

DownloadManager::DownloadManager(
    BrowserProfile *profile,
    QWidget *dialogParent,
    const QString &historyPath,
    QObject *parent
)
    : QObject(parent)
    , m_profile(profile)
    , m_dialogParent(dialogParent)
    , m_historyStore(historyPath)
{
    QString error;
    m_records = m_historyStore.load(&error);
    if (!error.isEmpty())
        qWarning().noquote() << "[PanBrowser downloads]" << error;

    bool changed = false;
    for (DownloadRecord &record : m_records) {
        if (isActive(record.status)) {
            record.status = DownloadStatus::Interrupted;
            record.finishedAt = QDateTime::currentDateTimeUtc();
            record.error = QStringLiteral("PanBrowser closed before the download finished");
            changed = true;
        }
    }
    if (changed)
        m_historyStore.save(m_records, &error);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);
    connect(m_saveTimer, &QTimer::timeout, this, &DownloadManager::saveNow);
    connect(m_profile, &QWebEngineProfile::downloadRequested, this, &DownloadManager::handleDownload);
}

DownloadManager::~DownloadManager()
{
    for (DownloadRecord &record : m_records) {
        if (isActive(record.status)) {
            record.status = DownloadStatus::Interrupted;
            record.finishedAt = QDateTime::currentDateTimeUtc();
            record.error = QStringLiteral("PanBrowser closed before the download finished");
        }
    }
    saveNow();
}

const QList<DownloadRecord> &DownloadManager::records() const
{
    return m_records;
}

DownloadRecord DownloadManager::record(const QString &id) const
{
    const int index = indexOf(id);
    return index >= 0 ? m_records.at(index) : DownloadRecord();
}

int DownloadManager::activeCount() const
{
    int count = 0;
    for (const DownloadRecord &record : m_records) {
        if (isActive(record.status))
            ++count;
    }
    return count;
}

void DownloadManager::pauseOrResume(const QString &id)
{
    QWebEngineDownloadRequest *download = m_requests.value(id);
    if (!download)
        return;
    if (download->isPaused())
        download->resume();
    else
        download->pause();
}

void DownloadManager::cancel(const QString &id)
{
    if (QWebEngineDownloadRequest *download = m_requests.value(id))
        download->cancel();
}

void DownloadManager::openFile(const QString &id) const
{
    const DownloadRecord item = record(id);
    if (item.status == DownloadStatus::Completed && QFileInfo::exists(item.filePath))
        QDesktopServices::openUrl(QUrl::fromLocalFile(item.filePath));
}

void DownloadManager::revealFile(const QString &id) const
{
    const DownloadRecord item = record(id);
    if (item.filePath.isEmpty())
        return;
#ifdef Q_OS_MACOS
    if (QFileInfo::exists(item.filePath)) {
        QProcess::startDetached(QStringLiteral("/usr/bin/open"), {
            QStringLiteral("-R"),
            item.filePath,
        });
        return;
    }
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(item.filePath).absolutePath()));
}

void DownloadManager::removeFromHistory(const QString &id)
{
    if (m_requests.contains(id))
        return;
    const int index = indexOf(id);
    if (index < 0)
        return;
    m_records.removeAt(index);
    emit recordRemoved(id);
    saveNow();
}

void DownloadManager::clearHistory()
{
    for (qsizetype index = m_records.size() - 1; index >= 0; --index) {
        if (!isActive(m_records.at(index).status))
            m_records.removeAt(index);
    }
    emit historyReset();
    saveNow();
}

void DownloadManager::openDownloadsFolder() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(downloadsDirectory()));
}

void DownloadManager::handleDownload(QWebEngineDownloadRequest *download)
{
    if (!download)
        return;

    const QString suggestedName = safeFileName(download->suggestedFileName());
    QWidget *dialogParent = QApplication::activeWindow();
    if (!dialogParent)
        dialogParent = m_dialogParent;
    const QString selectedPath = QFileDialog::getSaveFileName(
        dialogParent,
        QStringLiteral("Save download"),
        QDir(downloadsDirectory()).filePath(suggestedName)
    );
    if (selectedPath.isEmpty()) {
        download->cancel();
        return;
    }

    const QFileInfo destination(selectedPath);
    QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
    settings.setValue(QStringLiteral("Downloads/lastDirectory"), destination.absolutePath());
    settings.sync();

    DownloadRecord record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.filePath = destination.absoluteFilePath();
    record.fileName = destination.fileName();
    record.sourceHost = download->url().host();
    record.startedAt = QDateTime::currentDateTimeUtc();
    record.receivedBytes = download->receivedBytes();
    record.totalBytes = download->totalBytes();
    record.status = DownloadStatus::InProgress;

    download->setDownloadDirectory(destination.absolutePath());
    download->setDownloadFileName(destination.fileName());
    m_records.prepend(record);
    while (m_records.size() > DownloadHistoryStore::maximumRecords)
        m_records.removeLast();
    m_requests.insert(record.id, download);

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this, [this, id = record.id, download] {
        updateFromRequest(id, download);
    });
    connect(download, &QWebEngineDownloadRequest::totalBytesChanged, this, [this, id = record.id, download] {
        updateFromRequest(id, download);
    });
    connect(download, &QWebEngineDownloadRequest::isPausedChanged, this, [this, id = record.id, download] {
        updateFromRequest(id, download);
    });
    connect(download, &QWebEngineDownloadRequest::stateChanged, this, [this, id = record.id, download] {
        updateFromRequest(id, download);
    });
    connect(download, &QObject::destroyed, this, [this, id = record.id] {
        const int index = indexOf(id);
        if (index >= 0 && isActive(m_records.at(index).status)) {
            DownloadRecord &item = m_records[index];
            item.status = DownloadStatus::Interrupted;
            item.finishedAt = QDateTime::currentDateTimeUtc();
            item.error = QStringLiteral("The download ended unexpectedly");
            emit recordUpdated(id);
            saveNow();
        }
        m_requests.remove(id);
        emit activeCountChanged(activeCount());
    });

    emit recordAdded(record.id);
    emit activeCountChanged(activeCount());
    scheduleSave();
    download->accept();
}

void DownloadManager::updateFromRequest(
    const QString &id,
    QWebEngineDownloadRequest *download
)
{
    const int index = indexOf(id);
    if (index < 0 || !download)
        return;

    DownloadRecord &record = m_records[index];
    record.receivedBytes = download->receivedBytes();
    record.totalBytes = download->totalBytes();
    switch (download->state()) {
    case QWebEngineDownloadRequest::DownloadRequested:
    case QWebEngineDownloadRequest::DownloadInProgress:
        record.status = download->isPaused()
            ? DownloadStatus::Paused
            : DownloadStatus::InProgress;
        break;
    case QWebEngineDownloadRequest::DownloadCompleted:
        record.status = DownloadStatus::Completed;
        record.finishedAt = QDateTime::currentDateTimeUtc();
        record.error.clear();
        m_requests.remove(id);
        break;
    case QWebEngineDownloadRequest::DownloadCancelled:
        record.status = DownloadStatus::Cancelled;
        record.finishedAt = QDateTime::currentDateTimeUtc();
        record.error.clear();
        m_requests.remove(id);
        break;
    case QWebEngineDownloadRequest::DownloadInterrupted:
        record.status = DownloadStatus::Failed;
        record.finishedAt = QDateTime::currentDateTimeUtc();
        record.error = download->interruptReasonString();
        m_requests.remove(id);
        break;
    }

    emit recordUpdated(id);
    emit activeCountChanged(activeCount());
    if (isActive(record.status))
        scheduleSave();
    else
        saveNow();
}

int DownloadManager::indexOf(const QString &id) const
{
    for (qsizetype index = 0; index < m_records.size(); ++index) {
        if (m_records.at(index).id == id)
            return index;
    }
    return -1;
}

void DownloadManager::scheduleSave()
{
    m_saveTimer->start();
}

void DownloadManager::saveNow()
{
    if (m_saveTimer)
        m_saveTimer->stop();
    QString error;
    if (!m_historyStore.save(m_records, &error))
        qWarning().noquote() << "[PanBrowser downloads]" << error;
}

QString DownloadManager::downloadsDirectory() const
{
    QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
    const QString configured = settings.value(QStringLiteral("Downloads/lastDirectory")).toString();
    if (!configured.isEmpty() && QDir(configured).exists())
        return configured;
    const QString standard = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return standard.isEmpty() ? QDir::homePath() : standard;
}
