#include "HistorySettingsPage.h"

#include "HistoryStore.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDate>
#include <QFrame>
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

QString dayLabel(const QDate &date)
{
    const QDate today = QDate::currentDate();
    if (date == today)
        return QCoreApplication::translate("HistorySettingsPage", "TODAY");
    if (date == today.addDays(-1))
        return QCoreApplication::translate("HistorySettingsPage", "YESTERDAY");
    return QLocale().toString(date, QLocale::LongFormat).toUpper();
}

} // namespace

HistorySettingsPage::HistorySettingsPage(
    HistoryStore *store,
    bool saveHistoryEnabled,
    QWidget *parent
)
    : QWidget(parent)
    , m_store(store)
{
    setObjectName(QStringLiteral("historySettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("History"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Review and remove pages saved in PanBrowser’s local history."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    layout->addWidget(subtitle);

    auto *privacyCard = new QFrame(this);
    privacyCard->setObjectName(QStringLiteral("settingsCard"));
    auto *privacyLayout = new QVBoxLayout(privacyCard);
    privacyLayout->setContentsMargins(18, 14, 18, 14);
    privacyLayout->setSpacing(7);
    m_saveHistory = new QCheckBox(tr("Save browsing history"), privacyCard);
    m_saveHistory->setChecked(saveHistoryEnabled);
    privacyLayout->addWidget(m_saveHistory);
    auto *privacyHint = new QLabel(
        tr("Disabling this stops new entries and address-bar suggestions. Existing history is kept until you remove it."),
        privacyCard
    );
    privacyHint->setObjectName(QStringLiteral("fieldHint"));
    privacyHint->setWordWrap(true);
    privacyLayout->addWidget(privacyHint);
    layout->addWidget(privacyCard);

    auto *historyLabel = new QLabel(tr("VISITS"), this);
    historyLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(historyLabel);

    m_filter = new QLineEdit(this);
    m_filter->setObjectName(QStringLiteral("historyFilter"));
    m_filter->setPlaceholderText(tr("Search titles and addresses"));
    m_filter->addAction(
        QIcon(QStringLiteral(":/assets/icons/search.svg")),
        QLineEdit::LeadingPosition
    );
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    m_visits = new QListWidget(this);
    m_visits->setObjectName(QStringLiteral("historyVisits"));
    m_visits->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_visits->setSpacing(2);
    layout->addWidget(m_visits, 1);

    auto *footer = new QHBoxLayout();
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("fieldHint"));
    footer->addWidget(m_status, 1);
    m_remove = new QPushButton(tr("Remove selected"), this);
    m_remove->setObjectName(QStringLiteral("dangerButton"));
    auto *clear = new QPushButton(tr("Clear all history…"), this);
    clear->setObjectName(QStringLiteral("dangerButton"));
    footer->addWidget(m_remove);
    footer->addWidget(clear);
    layout->addLayout(footer);

    m_filterTimer = new QTimer(this);
    m_filterTimer->setSingleShot(true);
    m_filterTimer->setInterval(150);
    connect(m_filter, &QLineEdit::textChanged, m_filterTimer, qOverload<>(&QTimer::start));
    connect(m_filterTimer, &QTimer::timeout, this, &HistorySettingsPage::reload);
    connect(m_visits, &QListWidget::itemSelectionChanged, this, &HistorySettingsPage::updateActions);
    connect(m_remove, &QPushButton::clicked, this, &HistorySettingsPage::removeSelected);
    connect(clear, &QPushButton::clicked, this, &HistorySettingsPage::clearAll);
    if (m_store)
        connect(m_store, &HistoryStore::historyChanged, this, &HistorySettingsPage::reload);

    reload();
}

bool HistorySettingsPage::saveHistoryEnabled() const
{
    return m_saveHistory->isChecked();
}

void HistorySettingsPage::reload()
{
    m_visits->clear();
    if (!m_store || !m_store->isOpen()) {
        m_status->setText(tr("History is unavailable."));
        m_filter->setEnabled(false);
        m_remove->setEnabled(false);
        return;
    }

    QString error;
    const QList<HistoryVisit> visits = m_store->visits(m_filter->text(), 1000, &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        m_remove->setEnabled(false);
        return;
    }

    QDate currentDay;
    for (const HistoryVisit &visit : visits) {
        const QDateTime localTime = visit.visitedAt.toLocalTime();
        if (localTime.date() != currentDay) {
            currentDay = localTime.date();
            auto *header = new QListWidgetItem(dayLabel(currentDay), m_visits);
            header->setFlags(Qt::NoItemFlags);
            header->setData(Qt::UserRole + 1, true);
        }
        const QString title = visit.title.trimmed().isEmpty()
            ? visit.url.host()
            : visit.title.trimmed();
        const QString time = QLocale().toString(localTime.time(), QLocale::ShortFormat);
        const QString address = visit.url.toDisplayString(QUrl::RemovePassword);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1   %2\n%3").arg(time, title, address),
            m_visits
        );
        item->setData(Qt::UserRole, visit.id);
        item->setToolTip(address);
        item->setSizeHint(QSize(0, 50));
    }
    if (visits.isEmpty()) {
        auto *empty = new QListWidgetItem(
            m_filter->text().trimmed().isEmpty()
                ? tr("No browsing history yet")
                : tr("No matching history"),
            m_visits
        );
        empty->setFlags(Qt::NoItemFlags);
    }
    m_status->setText(
        visits.size() == 1000
            ? tr("Showing the latest 1,000 visits")
            : tr("%n visit(s)", nullptr, visits.size())
    );
    updateActions();
}

void HistorySettingsPage::updateActions()
{
    bool hasVisits = false;
    for (const QListWidgetItem *item : m_visits->selectedItems()) {
        if (item->data(Qt::UserRole).toLongLong() > 0) {
            hasVisits = true;
            break;
        }
    }
    m_remove->setEnabled(hasVisits);
}

void HistorySettingsPage::removeSelected()
{
    QList<qint64> ids;
    for (const QListWidgetItem *item : m_visits->selectedItems()) {
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        if (id > 0)
            ids.append(id);
    }
    if (ids.isEmpty())
        return;
    QString error;
    if (!m_store->removeVisits(ids, &error))
        QMessageBox::warning(this, tr("Cannot remove history"), error);
}

void HistorySettingsPage::clearAll()
{
    if (!m_store || !m_store->isOpen())
        return;
    if (QMessageBox::question(
            this,
            tr("Clear browsing history"),
            tr("Remove all browsing history and address-bar history suggestions? Cookies, sign-ins, and downloads will be kept."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_store->clear(&error))
        QMessageBox::warning(this, tr("Cannot clear history"), error);
}
