#include "AddressCompletionPopup.h"

#include "AddressLineEdit.h"

#include <QApplication>
#include <QDate>
#include <QGuiApplication>
#include <QHideEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QListWidget>
#include <QLocale>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString inlineCompletionText(const QString &sourceInput, const QUrl &url)
{
    const QString input = sourceInput.trimmed();
    if (input.isEmpty() || input.contains(QRegularExpression(QStringLiteral("\\s"))))
        return {};

    const QString full = url.toDisplayString(QUrl::RemovePassword | QUrl::RemoveFragment);
    QString withoutScheme = full;
    const qsizetype schemeEnd = withoutScheme.indexOf(QStringLiteral("://"));
    if (schemeEnd >= 0)
        withoutScheme.remove(0, schemeEnd + 3);
    QString withoutWww = withoutScheme;
    if (withoutWww.startsWith(QStringLiteral("www."), Qt::CaseInsensitive))
        withoutWww.remove(0, 4);

    const QStringList variants = {full, withoutScheme, withoutWww};
    for (const QString &variant : variants) {
        if (variant.size() > input.size()
            && variant.startsWith(input, Qt::CaseInsensitive)) {
            return input + variant.sliced(input.size());
        }
    }
    return {};
}

} // namespace

AddressCompletionPopup::AddressCompletionPopup(
    AddressLineEdit *addressBar,
    QWidget *parent
)
    : QFrame(
        parent,
        Qt::Tool
            | Qt::FramelessWindowHint
            | Qt::NoDropShadowWindowHint
            | Qt::WindowDoesNotAcceptFocus
    )
    , m_addressBar(addressBar)
{
    setObjectName(QStringLiteral("addressCompletionPopup"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("addressCompletionList"));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list);

    qApp->installEventFilter(this);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        activateItem(item);
    });
}

AddressCompletionPopup::~AddressCompletionPopup()
{
    if (qApp)
        qApp->removeEventFilter(this);
}

void AddressCompletionPopup::showSuggestions(
    const QList<AddressSuggestion> &suggestions
)
{
    m_list->clear();
    if (suggestions.isEmpty()) {
        hide();
        return;
    }

    const QDate today = QDate::currentDate();
    for (const AddressSuggestion &suggestion : suggestions) {
        const QDateTime localTime = suggestion.lastUsedAt.toLocalTime();
        const QString date = localTime.date() == today
            ? QLocale().toString(localTime.time(), QLocale::ShortFormat)
            : QLocale().toString(localTime, QLocale::ShortFormat);
        const QString title = suggestion.title.trimmed().isEmpty()
            ? suggestion.url.host()
            : suggestion.title.trimmed();
        const QString address = suggestion.url.toDisplayString(QUrl::RemovePassword);
        auto *item = new QListWidgetItem(
            suggestion.source == AddressSuggestionSource::Bookmark
                ? QStringLiteral("%1\n%2   ·   Bookmark   ·   %3").arg(title, address, date)
                : QStringLiteral("%1\n%2   ·   %3").arg(title, address, date),
            m_list
        );
        if (suggestion.source == AddressSuggestionSource::Bookmark)
            item->setIcon(QIcon(QStringLiteral(":/assets/icons/star-filled.svg")));
        item->setData(Qt::UserRole, suggestion.url.toString(QUrl::FullyEncoded));
        item->setSizeHint(QSize(0, 52));
        item->setToolTip(address);
    }
    m_addressBar->clearGhostCompletion();
    for (const AddressSuggestion &suggestion : suggestions) {
        const QString completion = inlineCompletionText(m_addressBar->text(), suggestion.url);
        if (completion.isEmpty())
            continue;
        m_addressBar->setGhostCompletion(completion, suggestion.url);
        break;
    }
    m_list->setCurrentRow(-1);
    positionBelowAddressBar();
    show();
    raise();
}

bool AddressCompletionPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (!isVisible())
        return QFrame::eventFilter(watched, event);

    const QWidget *target = qobject_cast<QWidget *>(watched);
    const bool eventBelongsToCompletion = target
        && (target == this || isAncestorOf(target));

    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint globalPosition = mouseEvent->globalPosition().toPoint();
        const QRect completionGeometry(mapToGlobal(QPoint(0, 0)), size());
        const QRect addressGeometry(
            m_addressBar->mapToGlobal(QPoint(0, 0)),
            m_addressBar->size()
        );
        if (!completionGeometry.contains(globalPosition)
            && !addressGeometry.contains(globalPosition)) {
            hide();
        }
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
        m_addressBar->clearGhostCompletion();
        const int next = m_list->currentRow() < m_list->count() - 1
            ? m_list->currentRow() + 1
            : 0;
        m_list->setCurrentRow(next);
        m_list->scrollToItem(m_list->currentItem());
        return true;
    }
    case Qt::Key_Up: {
        m_addressBar->clearGhostCompletion();
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
        if (m_addressBar->hasGhostCompletion()) {
            const QUrl url = m_addressBar->ghostCompletionUrl();
            hide();
            emit urlActivated(url);
            return true;
        }
        hide();
        break;
    case Qt::Key_Tab:
    case Qt::Key_Right:
        if (m_addressBar->hasGhostCompletion()
            && m_addressBar->cursorPosition() == m_addressBar->text().size()
            && !m_addressBar->hasSelectedText()) {
            m_addressBar->acceptGhostCompletion();
            hide();
            return true;
        }
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

void AddressCompletionPopup::hideEvent(QHideEvent *event)
{
    m_addressBar->clearGhostCompletion();
    updatePlacementStyle(QString());
    QFrame::hideEvent(event);
}

void AddressCompletionPopup::activateCurrent()
{
    activateItem(m_list->currentItem());
}

void AddressCompletionPopup::activateItem(const QListWidgetItem *item)
{
    if (!item)
        return;
    const QUrl url(item->data(Qt::UserRole).toString(), QUrl::StrictMode);
    hide();
    m_addressBar->setFocus(Qt::OtherFocusReason);
    if (url.isValid())
        emit urlActivated(url);
}

void AddressCompletionPopup::positionBelowAddressBar()
{
    const QPoint below = m_addressBar->mapToGlobal(QPoint(0, m_addressBar->height() - 1));
    const int width = m_addressBar->width();
    const int height = std::min(m_list->count() * 52 + 10, 430);
    QRect geometry(below, QSize(width, height));
    bool placedBelow = true;
    const QScreen *screen = QGuiApplication::screenAt(below);
    if (screen) {
        const QRect available = screen->availableGeometry();
        if (geometry.bottom() > available.bottom()) {
            const QPoint above = m_addressBar->mapToGlobal(QPoint(0, -height + 1));
            geometry.moveTopLeft(above);
            placedBelow = false;
        }
        if (geometry.right() > available.right())
            geometry.moveRight(available.right());
        if (geometry.left() < available.left())
            geometry.moveLeft(available.left());
    }
    updatePlacementStyle(placedBelow ? QStringLiteral("below") : QStringLiteral("above"));
    setGeometry(geometry);
}

void AddressCompletionPopup::updatePlacementStyle(const QString &placement)
{
    if (property("placement").toString() == placement
        && m_addressBar->property("completionPlacement").toString() == placement) {
        return;
    }
    setProperty("placement", placement);
    m_addressBar->setProperty("completionPlacement", placement);
    style()->unpolish(this);
    style()->polish(this);
    update();
    m_addressBar->style()->unpolish(m_addressBar);
    m_addressBar->style()->polish(m_addressBar);
    m_addressBar->update();
}
