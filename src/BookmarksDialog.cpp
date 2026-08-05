#include "BookmarksDialog.h"

#include "BookmarkDialog.h"
#include "BookmarkStore.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int urlRole = Qt::UserRole + 1;
constexpr int titleRole = Qt::UserRole + 2;

} // namespace

BookmarksDialog::BookmarksDialog(BookmarkStore *store, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
{
    setObjectName(QStringLiteral("bookmarksDialog"));
    setWindowTitle(tr("Bookmarks"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/star.svg")));
    resize(760, 620);
    setMinimumSize(620, 460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 22, 26, 20);
    layout->setSpacing(13);
    auto *title = new QLabel(tr("Bookmarks"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Open, edit, and remove pages saved in PanBrowser."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    layout->addWidget(subtitle);

    m_filter = new QLineEdit(this);
    m_filter->setObjectName(QStringLiteral("bookmarkFilter"));
    m_filter->setPlaceholderText(tr("Search titles and addresses"));
    m_filter->addAction(
        QIcon(QStringLiteral(":/assets/icons/search.svg")),
        QLineEdit::LeadingPosition
    );
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_bookmarks = new QListWidget(this);
    m_bookmarks->setObjectName(QStringLiteral("bookmarksList"));
    m_bookmarks->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_bookmarks->setSpacing(2);
    layout->addWidget(m_bookmarks, 1);

    auto *actions = new QHBoxLayout();
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("fieldHint"));
    actions->addWidget(m_status, 1);
    m_edit = new QPushButton(tr("Edit…"), this);
    m_remove = new QPushButton(tr("Remove"), this);
    m_remove->setObjectName(QStringLiteral("dangerButton"));
    auto *clear = new QPushButton(tr("Clear All…"), this);
    clear->setObjectName(QStringLiteral("dangerButton"));
    actions->addWidget(m_edit);
    actions->addWidget(m_remove);
    actions->addWidget(clear);
    layout->addLayout(actions);

    auto *openActions = new QHBoxLayout();
    openActions->addStretch();
    auto *close = new QPushButton(tr("Close"), this);
    m_openNewTab = new QPushButton(tr("Open in New Tab"), this);
    m_open = new QPushButton(tr("Open"), this);
    m_open->setDefault(true);
    openActions->addWidget(close);
    openActions->addWidget(m_openNewTab);
    openActions->addWidget(m_open);
    layout->addLayout(openActions);

    m_filterTimer = new QTimer(this);
    m_filterTimer->setSingleShot(true);
    m_filterTimer->setInterval(120);
    connect(m_filter, &QLineEdit::textChanged, m_filterTimer, qOverload<>(&QTimer::start));
    connect(m_filterTimer, &QTimer::timeout, this, &BookmarksDialog::reload);
    connect(m_bookmarks, &QListWidget::itemSelectionChanged, this, &BookmarksDialog::updateActions);
    connect(m_bookmarks, &QListWidget::itemDoubleClicked, this, [this] {
        openSelected(false);
    });
    connect(m_edit, &QPushButton::clicked, this, &BookmarksDialog::editSelected);
    connect(m_remove, &QPushButton::clicked, this, &BookmarksDialog::removeSelected);
    connect(clear, &QPushButton::clicked, this, &BookmarksDialog::clearAll);
    connect(m_open, &QPushButton::clicked, this, [this] { openSelected(false); });
    connect(m_openNewTab, &QPushButton::clicked, this, [this] { openSelected(true); });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    if (m_store)
        connect(m_store, &BookmarkStore::bookmarksChanged, this, &BookmarksDialog::reload);
    reload();
}

void BookmarksDialog::reload()
{
    m_bookmarks->clear();
    if (!m_store || !m_store->isOpen()) {
        m_status->setText(tr("Bookmarks are unavailable."));
        m_filter->setEnabled(false);
        updateActions();
        return;
    }

    QString error;
    const QList<Bookmark> bookmarks = m_store->bookmarks(m_filter->text(), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        updateActions();
        return;
    }
    for (const Bookmark &bookmark : bookmarks) {
        const QString address = bookmark.url.toDisplayString(QUrl::RemovePassword);
        const QString updated = QLocale().toString(
            bookmark.updatedAt.toLocalTime(),
            QLocale::ShortFormat
        );
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2   ·   %3").arg(bookmark.title, address, updated),
            m_bookmarks
        );
        item->setIcon(QIcon(QStringLiteral(":/assets/icons/star-filled.svg")));
        item->setData(Qt::UserRole, bookmark.id);
        item->setData(urlRole, bookmark.url.toString(QUrl::FullyEncoded));
        item->setData(titleRole, bookmark.title);
        item->setToolTip(address);
        item->setSizeHint(QSize(0, 54));
    }
    if (bookmarks.isEmpty()) {
        auto *empty = new QListWidgetItem(
            m_filter->text().trimmed().isEmpty()
                ? tr("No bookmarks yet")
                : tr("No matching bookmarks"),
            m_bookmarks
        );
        empty->setFlags(Qt::NoItemFlags);
    }
    m_status->setText(
        tr("%n bookmark(s)", nullptr, bookmarks.size())
    );
    updateActions();
}

void BookmarksDialog::updateActions()
{
    int selectedBookmarks = 0;
    for (const QListWidgetItem *item : m_bookmarks->selectedItems()) {
        if (item->data(Qt::UserRole).toLongLong() > 0)
            ++selectedBookmarks;
    }
    const bool one = selectedBookmarks == 1;
    m_open->setEnabled(one);
    m_openNewTab->setEnabled(one);
    m_edit->setEnabled(one);
    m_remove->setEnabled(selectedBookmarks > 0);
}

void BookmarksDialog::editSelected()
{
    const QList<QListWidgetItem *> selected = m_bookmarks->selectedItems();
    if (selected.size() != 1)
        return;
    const QListWidgetItem *item = selected.first();
    Bookmark bookmark;
    bookmark.id = item->data(Qt::UserRole).toLongLong();
    bookmark.url = QUrl(item->data(urlRole).toString(), QUrl::StrictMode);
    bookmark.title = item->data(titleRole).toString();
    BookmarkDialog dialog(m_store, bookmark.url, bookmark.title, bookmark, this);
    dialog.exec();
}

void BookmarksDialog::removeSelected()
{
    QList<qint64> ids;
    for (const QListWidgetItem *item : m_bookmarks->selectedItems()) {
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        if (id > 0)
            ids.append(id);
    }
    if (ids.isEmpty())
        return;
    QString error;
    if (!m_store->remove(ids, &error))
        QMessageBox::warning(this, tr("Cannot remove bookmarks"), error);
}

void BookmarksDialog::clearAll()
{
    if (!m_store || !m_store->isOpen())
        return;
    if (QMessageBox::question(
            this,
            tr("Clear bookmarks"),
            tr("Remove every bookmark from PanBrowser?"),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_store->clear(&error))
        QMessageBox::warning(this, tr("Cannot clear bookmarks"), error);
}

void BookmarksDialog::openSelected(bool newTab)
{
    const QList<QListWidgetItem *> selected = m_bookmarks->selectedItems();
    if (selected.size() != 1)
        return;
    const QUrl url(selected.first()->data(urlRole).toString(), QUrl::StrictMode);
    if (!url.isValid())
        return;
    accept();
    emit openRequested(url, newTab);
}
