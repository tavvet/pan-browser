#include "BrowserTabBar.h"

#include <QMouseEvent>
#include <QVariantMap>
#include <QWidget>

#include <algorithm>

namespace {

constexpr int pinnedTabMinimumWidth = 42;
constexpr auto pinnedKey = "pinned";
constexpr auto identityKey = "identity";

QVariantMap tabMetadata(const QVariant &data)
{
    QVariantMap metadata = data.toMap();
    if (metadata.isEmpty() && data.isValid())
        metadata.insert(QString::fromLatin1(pinnedKey), data.toBool());
    return metadata;
}

} // namespace

BrowserTabBar::BrowserTabBar(QWidget *parent)
    : QTabBar(parent)
{
}

bool BrowserTabBar::isTabPinned(int index) const
{
    if (index < 0 || index >= count())
        return false;
    return tabMetadata(tabData(index)).value(QString::fromLatin1(pinnedKey)).toBool();
}

void BrowserTabBar::setTabPinned(int index, bool pinned)
{
    if (index < 0 || index >= count() || isTabPinned(index) == pinned)
        return;

    QVariantMap metadata = tabMetadata(tabData(index));
    metadata.insert(QString::fromLatin1(pinnedKey), pinned);
    setTabData(index, metadata);
    updateCloseButtonVisibility(index);
    updateGeometry();
    update();
}

int BrowserTabBar::pinnedTabCount() const
{
    int result = 0;
    for (int index = 0; index < count(); ++index) {
        if (isTabPinned(index))
            ++result;
    }
    return result;
}

int BrowserTabBar::normalizedMoveDestination(int movedIndex) const
{
    if (movedIndex < 0 || movedIndex >= count())
        return movedIndex;

    const int pinnedCount = pinnedTabCount();
    if (isTabPinned(movedIndex))
        return std::min(movedIndex, pinnedCount - 1);
    return std::max(movedIndex, pinnedCount);
}

QSize BrowserTabBar::tabSizeHint(int index) const
{
    QSize size = QTabBar::tabSizeHint(index);
    if (isTabPinned(index))
        size.setWidth(std::max(pinnedTabMinimumWidth, size.height() + 8));
    return size;
}

void BrowserTabBar::mousePressEvent(QMouseEvent *event)
{
    m_draggedTabIdentity = 0;
    if (event && event->button() == Qt::LeftButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0)
            m_draggedTabIdentity = ensureTabIdentity(index);
    }
    QTabBar::mousePressEvent(event);
}

void BrowserTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    const quint64 draggedTabIdentity = m_draggedTabIdentity;
    m_draggedTabIdentity = 0;
    QTabBar::mouseReleaseEvent(event);

    const int index = indexForIdentity(draggedTabIdentity);
    const int destination = normalizedMoveDestination(index);
    if (index >= 0 && destination != index)
        moveTab(index, destination);
}

quint64 BrowserTabBar::ensureTabIdentity(int index)
{
    if (index < 0 || index >= count())
        return 0;

    QVariantMap metadata = tabMetadata(tabData(index));
    quint64 identity = metadata.value(QString::fromLatin1(identityKey)).toULongLong();
    if (identity == 0) {
        identity = ++m_nextTabIdentity;
        metadata.insert(QString::fromLatin1(identityKey), identity);
        setTabData(index, metadata);
    }
    return identity;
}

int BrowserTabBar::indexForIdentity(quint64 identity) const
{
    if (identity == 0)
        return -1;
    for (int index = 0; index < count(); ++index) {
        const QVariantMap metadata = tabMetadata(tabData(index));
        if (metadata.value(QString::fromLatin1(identityKey)).toULongLong() == identity)
            return index;
    }
    return -1;
}

void BrowserTabBar::updateCloseButtonVisibility(int index)
{
    const bool visible = !isTabPinned(index);
    if (QWidget *button = tabButton(index, QTabBar::LeftSide))
        button->setVisible(visible);
    if (QWidget *button = tabButton(index, QTabBar::RightSide))
        button->setVisible(visible);
}
