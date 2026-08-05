#include "DownloadsPanel.h"

#include "DownloadManager.h"

#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStyleOptionToolButton>
#include <QVBoxLayout>

#include <utility>

namespace {

bool isActive(DownloadStatus status)
{
    return status == DownloadStatus::InProgress || status == DownloadStatus::Paused;
}

QString byteDescription(qint64 bytes)
{
    return bytes < 0 ? QString() : QLocale().formattedDataSize(bytes);
}

QString statusDescription(const DownloadRecord &record)
{
    const QString host = record.sourceHost.isEmpty()
        ? QStringLiteral("Unknown source")
        : record.sourceHost;
    const QString finished = record.finishedAt.isValid()
        ? QLocale().toString(record.finishedAt.toLocalTime(), QLocale::ShortFormat)
        : QString();
    switch (record.status) {
    case DownloadStatus::InProgress: {
        const QString received = byteDescription(record.receivedBytes);
        const QString total = byteDescription(record.totalBytes);
        return total.isEmpty()
            ? QStringLiteral("%1 · %2 downloaded").arg(host, received)
            : QStringLiteral("%1 · %2 of %3").arg(host, received, total);
    }
    case DownloadStatus::Paused:
        return QStringLiteral("Paused · %1").arg(host);
    case DownloadStatus::Completed:
        if (!QFileInfo::exists(record.filePath))
            return QStringLiteral("File moved or deleted · %1").arg(host);
        return QStringLiteral("Completed · %1 · %2 · %3").arg(
            byteDescription(record.receivedBytes),
            host,
            finished
        );
    case DownloadStatus::Cancelled:
        return QStringLiteral("Cancelled · %1 · %2").arg(host, finished);
    case DownloadStatus::Failed:
        return record.error.isEmpty()
            ? QStringLiteral("Download failed · %1").arg(host)
            : QStringLiteral("%1 · %2").arg(record.error, host);
    case DownloadStatus::Interrupted:
        return QStringLiteral("Interrupted · %1 · %2").arg(host, finished);
    }
    return host;
}

} // namespace

class DownloadItemWidget final : public QFrame {
public:
    DownloadItemWidget(
        DownloadManager *manager,
        const DownloadRecord &record,
        QWidget *parent = nullptr
    )
        : QFrame(parent)
        , m_manager(manager)
        , m_id(record.id)
    {
        setObjectName(QStringLiteral("downloadItem"));
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(12, 10, 12, 10);
        rootLayout->setSpacing(7);

        m_name = new QLabel(this);
        m_name->setObjectName(QStringLiteral("downloadName"));
        m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rootLayout->addWidget(m_name);

        m_details = new QLabel(this);
        m_details->setObjectName(QStringLiteral("downloadDetails"));
        m_details->setWordWrap(true);
        rootLayout->addWidget(m_details);

        m_progress = new QProgressBar(this);
        m_progress->setObjectName(QStringLiteral("downloadProgress"));
        m_progress->setTextVisible(false);
        rootLayout->addWidget(m_progress);

        auto *actions = new QHBoxLayout();
        actions->setContentsMargins(0, 0, 0, 0);
        actions->setSpacing(6);
        m_primary = new QPushButton(this);
        m_secondary = new QPushButton(this);
        m_remove = new QPushButton(QStringLiteral("Remove"), this);
        m_remove->setObjectName(QStringLiteral("subtleButton"));
        actions->addWidget(m_primary);
        actions->addWidget(m_secondary);
        actions->addStretch();
        actions->addWidget(m_remove);
        rootLayout->addLayout(actions);

        connect(m_primary, &QPushButton::clicked, this, [this] {
            const DownloadRecord item = m_manager->record(m_id);
            if (isActive(item.status))
                m_manager->pauseOrResume(m_id);
            else
                m_manager->openFile(m_id);
        });
        connect(m_secondary, &QPushButton::clicked, this, [this] {
            const DownloadRecord item = m_manager->record(m_id);
            if (isActive(item.status))
                m_manager->cancel(m_id);
            else
                m_manager->revealFile(m_id);
        });
        connect(m_remove, &QPushButton::clicked, this, [this] {
            m_manager->removeFromHistory(m_id);
        });
        setRecord(record);
    }

    void setRecord(const DownloadRecord &record)
    {
        m_name->setText(record.fileName);
        m_name->setToolTip(record.filePath);
        m_details->setText(statusDescription(record));

        const bool active = isActive(record.status);
        m_progress->setVisible(active);
        if (active && record.totalBytes > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(static_cast<int>(
                qMin<qint64>(1000, record.receivedBytes * 1000 / record.totalBytes)
            ));
        } else if (active) {
            m_progress->setRange(0, 0);
        }

        if (active) {
            m_primary->setText(
                record.status == DownloadStatus::Paused
                    ? QStringLiteral("Resume")
                    : QStringLiteral("Pause")
            );
            m_secondary->setText(QStringLiteral("Cancel"));
            m_primary->setVisible(true);
            m_secondary->setVisible(true);
            m_remove->hide();
            return;
        }

        const bool completedFile = record.status == DownloadStatus::Completed
            && QFileInfo::exists(record.filePath);
        m_primary->setText(QStringLiteral("Open"));
        m_primary->setVisible(completedFile);
        m_secondary->setText(QStringLiteral("Show in Finder"));
        m_secondary->setVisible(completedFile);
        m_remove->show();
    }

private:
    DownloadManager *m_manager = nullptr;
    QString m_id;
    QLabel *m_name = nullptr;
    QLabel *m_details = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_primary = nullptr;
    QPushButton *m_secondary = nullptr;
    QPushButton *m_remove = nullptr;
};

DownloadButton::DownloadButton(QWidget *parent)
    : QToolButton(parent)
{
}

void DownloadButton::setActiveCount(int count)
{
    const int normalized = qMax(0, count);
    if (m_activeCount == normalized)
        return;
    m_activeCount = normalized;
    update();
}

void DownloadButton::paintEvent(QPaintEvent *event)
{
    QToolButton::paintEvent(event);
    if (m_activeCount <= 0)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int diameter = 15;
    const QPoint center(width() - 8, 8);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#5b8def")));
    painter.drawEllipse(center, diameter / 2, diameter / 2);
    painter.setPen(Qt::white);
    QFont badgeFont = font();
    badgeFont.setPixelSize(9);
    badgeFont.setBold(true);
    painter.setFont(badgeFont);
    const QRect textRect(
        center.x() - diameter / 2,
        center.y() - diameter / 2,
        diameter,
        diameter
    );
    painter.drawText(
        textRect,
        Qt::AlignCenter,
        m_activeCount > 9 ? QStringLiteral("9+") : QString::number(m_activeCount)
    );
}

DownloadsPanel::DownloadsPanel(DownloadManager *manager, QWidget *parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_manager(manager)
{
    setObjectName(QStringLiteral("downloadsPanel"));
    resize(480, 520);
    setMinimumSize(420, 340);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 13, 14, 14);
    rootLayout->setSpacing(10);

    auto *header = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("Downloads"), this);
    title->setObjectName(QStringLiteral("downloadsTitle"));
    header->addWidget(title);
    header->addStretch();
    auto *openFolder = new QPushButton(QStringLiteral("Open folder"), this);
    openFolder->setObjectName(QStringLiteral("subtleButton"));
    auto *clearHistory = new QPushButton(QStringLiteral("Clear history"), this);
    clearHistory->setObjectName(QStringLiteral("subtleButton"));
    header->addWidget(openFolder);
    header->addWidget(clearHistory);
    rootLayout->addLayout(header);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setObjectName(QStringLiteral("downloadsSeparator"));
    rootLayout->addWidget(separator);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("downloadsScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_itemsContainer = new QWidget(scroll);
    m_itemsContainer->setObjectName(QStringLiteral("downloadsItems"));
    m_itemsLayout = new QVBoxLayout(m_itemsContainer);
    m_itemsLayout->setContentsMargins(0, 0, 0, 0);
    m_itemsLayout->setSpacing(8);
    m_itemsLayout->addStretch();
    scroll->setWidget(m_itemsContainer);
    rootLayout->addWidget(scroll, 1);

    m_emptyState = new QLabel(
        QStringLiteral("Downloaded files will appear here."),
        m_itemsContainer
    );
    m_emptyState->setObjectName(QStringLiteral("downloadsEmpty"));
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_itemsLayout->insertWidget(0, m_emptyState, 1);

    connect(openFolder, &QPushButton::clicked, m_manager, &DownloadManager::openDownloadsFolder);
    connect(clearHistory, &QPushButton::clicked, m_manager, &DownloadManager::clearHistory);
    connect(m_manager, &DownloadManager::recordAdded, this, &DownloadsPanel::addRecord);
    connect(m_manager, &DownloadManager::recordUpdated, this, &DownloadsPanel::updateRecord);
    connect(m_manager, &DownloadManager::recordRemoved, this, &DownloadsPanel::removeRecord);
    connect(m_manager, &DownloadManager::historyReset, this, &DownloadsPanel::rebuild);
    rebuild();
}

void DownloadsPanel::showBelow(QWidget *anchor)
{
    if (!anchor)
        return;
    resize(480, 520);
    const QPoint anchorBottomRight = anchor->mapToGlobal(QPoint(anchor->width(), anchor->height()));
    QPoint position(anchorBottomRight.x() - width(), anchorBottomRight.y() + 7);
    const QScreen *screen = QGuiApplication::screenAt(anchorBottomRight);
    if (screen) {
        const QRect available = screen->availableGeometry();
        resize(width(), qMin(height(), available.height() - 40));
        position.setX(qBound(available.left(), position.x(), available.right() - width() + 1));
        if (position.y() + height() > available.bottom()) {
            position.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height() - 7);
        }
    }
    move(position);
    show();
    raise();
}

void DownloadsPanel::rebuild()
{
    for (DownloadItemWidget *item : std::as_const(m_items))
        delete item;
    m_items.clear();
    const QList<DownloadRecord> &records = m_manager->records();
    for (qsizetype index = records.size() - 1; index >= 0; --index)
        addRecord(records.at(index).id);
    updateEmptyState();
}

void DownloadsPanel::addRecord(const QString &id)
{
    if (m_items.contains(id))
        return;
    const DownloadRecord record = m_manager->record(id);
    if (record.id.isEmpty())
        return;
    auto *item = new DownloadItemWidget(m_manager, record, m_itemsContainer);
    m_items.insert(id, item);
    m_itemsLayout->insertWidget(0, item);
    updateEmptyState();
}

void DownloadsPanel::updateRecord(const QString &id)
{
    if (DownloadItemWidget *item = m_items.value(id))
        item->setRecord(m_manager->record(id));
}

void DownloadsPanel::removeRecord(const QString &id)
{
    DownloadItemWidget *item = m_items.take(id);
    delete item;
    updateEmptyState();
}

void DownloadsPanel::updateEmptyState()
{
    m_emptyState->setVisible(m_items.isEmpty());
}
