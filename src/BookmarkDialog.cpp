#include "BookmarkDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

BookmarkDialog::BookmarkDialog(
    BookmarkStore *store,
    const QUrl &url,
    const QString &suggestedTitle,
    const std::optional<Bookmark> &existing,
    QWidget *parent
)
    : QDialog(parent)
    , m_store(store)
    , m_existing(existing)
{
    setObjectName(QStringLiteral("bookmarkDialog"));
    setWindowTitle(existing ? QStringLiteral("Edit Bookmark") : QStringLiteral("Add Bookmark"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/star.svg")));
    setModal(true);
    resize(520, 230);
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(14);
    auto *heading = new QLabel(
        existing ? QStringLiteral("Edit bookmark") : QStringLiteral("Add bookmark"),
        this
    );
    heading->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(heading);

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(11);
    m_title = new QLineEdit(existing ? existing->title : suggestedTitle, this);
    m_title->setObjectName(QStringLiteral("bookmarkTitle"));
    m_title->setPlaceholderText(QStringLiteral("Bookmark title"));
    m_url = new QLineEdit(
        (existing ? existing->url : url).toDisplayString(QUrl::RemovePassword),
        this
    );
    m_url->setObjectName(QStringLiteral("bookmarkUrl"));
    form->addRow(QStringLiteral("Title"), m_title);
    form->addRow(QStringLiteral("Address"), m_url);
    layout->addLayout(form);

    auto *buttonsLayout = new QHBoxLayout();
    QPushButton *remove = nullptr;
    if (existing) {
        remove = new QPushButton(QStringLiteral("Remove Bookmark"), this);
        remove->setObjectName(QStringLiteral("dangerButton"));
        buttonsLayout->addWidget(remove);
    }
    buttonsLayout->addStretch();
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        this
    );
    buttons->button(QDialogButtonBox::Save)->setText(
        existing ? QStringLiteral("Save Changes") : QStringLiteral("Add Bookmark")
    );
    buttonsLayout->addWidget(buttons);
    layout->addLayout(buttonsLayout);

    connect(buttons, &QDialogButtonBox::accepted, this, &BookmarkDialog::saveBookmark);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (remove)
        connect(remove, &QPushButton::clicked, this, &BookmarkDialog::removeBookmark);
    m_title->setFocus();
    m_title->selectAll();
}

void BookmarkDialog::saveBookmark()
{
    const QUrl url = QUrl::fromUserInput(m_url->text().trimmed());
    QString error;
    const bool saved = m_existing
        ? m_store->update(m_existing->id, url, m_title->text(), QDateTime::currentDateTimeUtc(), &error)
        : m_store->addOrUpdate(url, m_title->text(), QDateTime::currentDateTimeUtc(), &error);
    if (!saved) {
        QMessageBox::warning(this, QStringLiteral("Cannot save bookmark"), error);
        return;
    }
    accept();
}

void BookmarkDialog::removeBookmark()
{
    if (!m_existing)
        return;
    QString error;
    if (!m_store->remove(m_existing->id, &error)) {
        QMessageBox::warning(this, QStringLiteral("Cannot remove bookmark"), error);
        return;
    }
    accept();
}
