#pragma once

#include <QDialog>
#include <QHash>
#include <QToolButton>

class DownloadItemWidget;
class DownloadManager;
class QLabel;
class QVBoxLayout;

class DownloadButton final : public QToolButton {
public:
    explicit DownloadButton(QWidget *parent = nullptr);

    void setActiveCount(int count);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_activeCount = 0;
};

class DownloadsPanel final : public QDialog {
    Q_OBJECT

public:
    explicit DownloadsPanel(DownloadManager *manager, QWidget *parent = nullptr);

    void showBelow(QWidget *anchor);

private:
    void rebuild();
    void addRecord(const QString &id);
    void updateRecord(const QString &id);
    void removeRecord(const QString &id);
    void updateEmptyState();

    DownloadManager *m_manager = nullptr;
    QWidget *m_itemsContainer = nullptr;
    QVBoxLayout *m_itemsLayout = nullptr;
    QLabel *m_emptyState = nullptr;
    QHash<QString, DownloadItemWidget *> m_items;
};
