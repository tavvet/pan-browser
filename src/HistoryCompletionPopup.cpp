#include "HistoryCompletionPopup.h"

#include <QApplication>
#include <QDate>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>

HistoryCompletionPopup::HistoryCompletionPopup(QLineEdit *addressBar, QWidget *parent)
    : QFrame(
        parent,
        Qt::Tool
            | Qt::FramelessWindowHint
            | Qt::NoDropShadowWindowHint
            | Qt::WindowDoesNotAcceptFocus
    )
    , m_addressBar(addressBar)
{
    setObjectName(QStringLiteral("historyCompletionPopup"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("historyCompletionList"));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list);

    qApp->installEventFilter(this);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *) {
        activateCurrent();
    });
}

HistoryCompletionPopup::~HistoryCompletionPopup()
{
    if (qApp)
        qApp->removeEventFilter(this);
}

void HistoryCompletionPopup::showSuggestions(
    const QList<HistorySuggestion> &suggestions
)
{
    m_list->clear();
    if (suggestions.isEmpty()) {
        hide();
        return;
    }

    const QDate today = QDate::currentDate();
    for (const HistorySuggestion &suggestion : suggestions) {
        const QDateTime localTime = suggestion.lastVisitedAt.toLocalTime();
        const QString date = localTime.date() == today
            ? QLocale().toString(localTime.time(), QLocale::ShortFormat)
            : QLocale().toString(localTime, QLocale::ShortFormat);
        const QString title = suggestion.title.trimmed().isEmpty()
            ? suggestion.url.host()
            : suggestion.title.trimmed();
        const QString address = suggestion.url.toDisplayString(QUrl::RemovePassword);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2   ·   %3").arg(title, address, date),
            m_list
        );
        item->setData(Qt::UserRole, suggestion.url.toString(QUrl::FullyEncoded));
        item->setSizeHint(QSize(0, 52));
        item->setToolTip(address);
    }
    m_list->setCurrentRow(-1);
    positionBelowAddressBar();
    show();
    raise();
}

bool HistoryCompletionPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (!isVisible())
        return QFrame::eventFilter(watched, event);

    const QWidget *target = qobject_cast<QWidget *>(watched);
    const bool eventBelongsToCompletion = target
        && (target == this || isAncestorOf(target));

    if (event->type() == QEvent::MouseButtonPress) {
        const bool eventBelongsToAddress = target
            && (target == m_addressBar || m_addressBar->isAncestorOf(target));
        if (!eventBelongsToCompletion && !eventBelongsToAddress)
            hide();
        return QFrame::eventFilter(watched, event);
    }

    if (event->type() == QEvent::FocusIn
        && target
        && target != m_addressBar
        && !m_addressBar->isAncestorOf(target)
        && !eventBelongsToCompletion) {
        hide();
        return QFrame::eventFilter(watched, event);
    }

    if (event->type() == QEvent::ApplicationDeactivate) {
        hide();
        return QFrame::eventFilter(watched, event);
    }

    if (event->type() != QEvent::KeyPress)
        return QFrame::eventFilter(watched, event);

    if (!m_addressBar->hasFocus()
        && !eventBelongsToCompletion) {
        return QFrame::eventFilter(watched, event);
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    switch (keyEvent->key()) {
    case Qt::Key_Down: {
        const int next = m_list->currentRow() < m_list->count() - 1
            ? m_list->currentRow() + 1
            : 0;
        m_list->setCurrentRow(next);
        m_list->scrollToItem(m_list->currentItem());
        return true;
    }
    case Qt::Key_Up: {
        const int next = m_list->currentRow() > 0
            ? m_list->currentRow() - 1
            : m_list->count() - 1;
        m_list->setCurrentRow(next);
        m_list->scrollToItem(m_list->currentItem());
        return true;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_list->currentRow() >= 0) {
            activateCurrent();
            return true;
        }
        hide();
        break;
    case Qt::Key_Escape:
        hide();
        m_addressBar->setFocus(Qt::OtherFocusReason);
        return true;
    default:
        break;
    }
    return QFrame::eventFilter(watched, event);
}

void HistoryCompletionPopup::activateCurrent()
{
    const QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;
    const QUrl url(item->data(Qt::UserRole).toString(), QUrl::StrictMode);
    hide();
    m_addressBar->setFocus(Qt::OtherFocusReason);
    if (url.isValid())
        emit urlActivated(url);
}

void HistoryCompletionPopup::positionBelowAddressBar()
{
    const QPoint below = m_addressBar->mapToGlobal(QPoint(0, m_addressBar->height() + 3));
    const int width = m_addressBar->width();
    const int height = std::min(m_list->count() * 52 + 10, 430);
    QRect geometry(below, QSize(width, height));
    const QScreen *screen = QGuiApplication::screenAt(below);
    if (screen) {
        const QRect available = screen->availableGeometry();
        if (geometry.bottom() > available.bottom()) {
            const QPoint above = m_addressBar->mapToGlobal(QPoint(0, -height - 3));
            geometry.moveTopLeft(above);
        }
        if (geometry.right() > available.right())
            geometry.moveRight(available.right());
        if (geometry.left() < available.left())
            geometry.moveLeft(available.left());
    }
    setGeometry(geometry);
}
