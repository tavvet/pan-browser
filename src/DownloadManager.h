#pragma once

#include "DownloadHistoryStore.h"

#include <QHash>
#include <QObject>
#include <QPointer>

class BrowserProfile;
class QTimer;
class QWebEngineDownloadRequest;
class QWidget;

class DownloadManager final : public QObject {
    Q_OBJECT

public:
    DownloadManager(
        BrowserProfile *profile,
        QWidget *dialogParent,
        const QString &historyPath,
        QObject *parent = nullptr
    );
    ~DownloadManager() override;

    [[nodiscard]] const QList<DownloadRecord> &records() const;
    [[nodiscard]] DownloadRecord record(const QString &id) const;
    [[nodiscard]] int activeCount() const;

    void pauseOrResume(const QString &id);
    void cancel(const QString &id);
    void openFile(const QString &id) const;
    void revealFile(const QString &id) const;
    void removeFromHistory(const QString &id);
    void clearHistory();
    void openDownloadsFolder() const;

signals:
    void recordAdded(const QString &id);
    void recordUpdated(const QString &id);
    void recordRemoved(const QString &id);
    void historyReset();
    void activeCountChanged(int count);

private:
    void handleDownload(QWebEngineDownloadRequest *download);
    void updateFromRequest(const QString &id, QWebEngineDownloadRequest *download);
    int indexOf(const QString &id) const;
    void scheduleSave();
    void saveNow();
    QString downloadsDirectory() const;

    BrowserProfile *m_profile = nullptr;
    QWidget *m_dialogParent = nullptr;
    DownloadHistoryStore m_historyStore;
    QList<DownloadRecord> m_records;
    QHash<QString, QPointer<QWebEngineDownloadRequest>> m_requests;
    QTimer *m_saveTimer = nullptr;
};
