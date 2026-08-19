#include "BrowserTabBar.h"

#include <QEasingCurve>
#include <QCoreApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariantMap>
#include <QVariantAnimation>
#include <QWidget>

#include <algorithm>

namespace {

constexpr int pinnedTabWidth = 40;
constexpr int regularTabMinimumWidth = 112;
constexpr int regularTabMaximumWidth = 220;
constexpr int maximumTabAnimationDurationMilliseconds = 180;
constexpr auto pinnedKey = "pinned";
constexpr auto identityKey = "identity";
constexpr auto animatedWidthKey = "animatedWidth";
constexpr auto faviconKey = "favicon";
constexpr auto showsCloseKey = "showsClose";

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
    setMouseTracking(true);
    connect(this, &QTabBar::tabMoved, this, [this] {
        scheduleHoveredTabRefresh();
    });
#if defined(Q_OS_MACOS)
    setProperty("closeButtonsOnLeft", true);
#else
    setProperty("closeButtonsOnLeft", false);
#endif
}

QSize BrowserTabBar::minimumSizeHint() const
{
    QSize minimum = QTabBar::minimumSizeHint();
    if (count() > 0 && pinnedTabCount() == count()) {
        minimum.setWidth(std::min(
            minimum.width(),
            count() * pinnedTabWidth
        ));
    }
    return minimum;
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
    updateTabWidths();
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

QIcon BrowserTabBar::tabIcon(int index) const
{
#if defined(Q_OS_MACOS)
    if (index >= 0 && index < count()) {
        const QVariant storedIcon = tabMetadata(tabData(index)).value(
            QString::fromLatin1(faviconKey)
        );
        if (storedIcon.isValid())
            return qvariant_cast<QIcon>(storedIcon);
    }
#endif
    return QTabBar::tabIcon(index);
}

void BrowserTabBar::setTabIcon(int index, const QIcon &icon)
{
#if defined(Q_OS_MACOS)
    if (index < 0 || index >= count())
        return;
    if (tabsClosable()) {
        QVariantMap metadata = tabMetadata(tabData(index));
        metadata.insert(QString::fromLatin1(faviconKey), icon);
        setTabData(index, metadata);
        QTabBar::setTabIcon(index, QIcon());
        updateLeadingButton(index);
        return;
    }
#endif
    QTabBar::setTabIcon(index, icon);
}

void BrowserTabBar::setTabsClosable(bool closable)
{
#if defined(Q_OS_MACOS)
    QList<QIcon> storedIcons;
    if (!closable) {
        storedIcons.reserve(count());
        for (int index = 0; index < count(); ++index)
            storedIcons.append(tabIcon(index));
    }
#endif
    QTabBar::setTabsClosable(closable);
#if defined(Q_OS_MACOS)
    if (closable) {
        for (int index = 0; index < count(); ++index)
            configureLeadingButton(index);
    } else {
        for (int index = 0; index < count(); ++index)
            QTabBar::setTabIcon(index, storedIcons.value(index));
    }
#endif
    for (int index = 0; index < count(); ++index)
        updateCloseButtonVisibility(index);
}

void BrowserTabBar::setAvailableWidth(int width)
{
    const int boundedWidth = std::max(0, width);
    if (m_availableWidth == boundedWidth)
        return;
    m_availableWidth = boundedWidth;
    updateTabWidths();
}

QSize BrowserTabBar::tabSizeHint(int index) const
{
    QSize size = naturalTabSizeHint(index);
    const QVariantMap metadata = tabMetadata(tabData(index));
    const QVariant animatedWidth = metadata.value(QString::fromLatin1(animatedWidthKey));
    if (animatedWidth.isValid())
        size.setWidth(std::clamp(animatedWidth.toInt(), 1, size.width()));
    return size;
}

QSize BrowserTabBar::minimumTabSizeHint(int index) const
{
    QSize size = QTabBar::minimumTabSizeHint(index);
    size.setWidth(isTabPinned(index) ? pinnedTabWidth : regularTabMinimumWidth);
    return size;
}

void BrowserTabBar::leaveEvent(QEvent *event)
{
    setHoveredTab(-1);
    QTabBar::leaveEvent(event);
}

void BrowserTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event)
        setHoveredTab(tabAt(event->position().toPoint()));
    QTabBar::mouseMoveEvent(event);
}

void BrowserTabBar::resizeEvent(QResizeEvent *event)
{
    QTabBar::resizeEvent(event);
    if (m_availableWidth < 0 && event) {
        const int regularCount = count() - pinnedTabCount();
        if (regularCount > 0) {
            const int availableForRegularTabs = std::max(
                0,
                event->size().width() - pinnedTabCount() * pinnedTabWidth
            );
            const int width = std::clamp(
                availableForRegularTabs / regularCount,
                regularTabMinimumWidth,
                regularTabMaximumWidth
            );
            if (m_regularTabWidth != width) {
                m_regularTabWidth = width;
                refreshTabLayout();
            }
        }
    }
}

void BrowserTabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    configureLeadingButton(index);
    updateCloseButtonVisibility(index);
    updateTabWidths();
}

void BrowserTabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    if (indexForIdentity(m_hoveredTabIdentity) < 0)
        m_hoveredTabIdentity = 0;
    updateTabWidths();
    scheduleHoveredTabRefresh();
}

void BrowserTabBar::animateTabOpening(int index)
{
    if (index < 0 || index >= count() || !isVisible())
        return;

    const int styleDuration = style()->styleHint(
        QStyle::SH_Widget_Animation_Duration,
        nullptr,
        this
    );
    if (styleDuration <= 0)
        return;

    const quint64 identity = ensureTabIdentity(index);
    const QSize targetSize = naturalTabSizeHint(index);
    const int openingWidth = std::min(
        targetSize.width(),
        std::max(24, targetSize.height())
    );
    if (identity == 0 || openingWidth >= targetSize.width())
        return;

    if (QVariantAnimation *existing = m_openingAnimations.take(identity)) {
        existing->stop();
        existing->deleteLater();
    }

    QVariantMap metadata = tabMetadata(tabData(index));
    metadata.insert(QString::fromLatin1(animatedWidthKey), openingWidth);
    setTabData(index, metadata);
    refreshTabLayout();

    auto *animation = new QVariantAnimation(this);
    animation->setDuration(std::min(
        styleDuration,
        maximumTabAnimationDurationMilliseconds
    ));
    animation->setStartValue(openingWidth);
    animation->setEndValue(targetSize.width());
    animation->setEasingCurve(QEasingCurve::OutCubic);
    m_openingAnimations.insert(identity, animation);

    connect(
        animation,
        &QVariantAnimation::valueChanged,
        this,
        [this, identity](const QVariant &value) {
            const int animatedIndex = indexForIdentity(identity);
            if (animatedIndex < 0)
                return;
            QVariantMap animatedMetadata = tabMetadata(tabData(animatedIndex));
            animatedMetadata.insert(
                QString::fromLatin1(animatedWidthKey),
                value.toInt()
            );
            setTabData(animatedIndex, animatedMetadata);
            refreshTabLayout();
        }
    );
    connect(animation, &QVariantAnimation::finished, this, [this, identity, animation] {
        if (m_openingAnimations.value(identity) != animation) {
            animation->deleteLater();
            return;
        }
        m_openingAnimations.remove(identity);
        const int animatedIndex = indexForIdentity(identity);
        if (animatedIndex >= 0) {
            QVariantMap metadata = tabMetadata(tabData(animatedIndex));
            metadata.remove(QString::fromLatin1(animatedWidthKey));
            setTabData(animatedIndex, metadata);
            refreshTabLayout();
        }
        animation->deleteLater();
    });
    animation->start();
}

bool BrowserTabBar::animateTabClosing(
    int index,
    std::function<void()> completion
)
{
    if (index < 0 || index >= count() || !completion)
        return false;

    const quint64 identity = ensureTabIdentity(index);
    if (identity == 0)
        return false;
    if (m_closingAnimations.contains(identity))
        return true;

    if (QVariantAnimation *openingAnimation = m_openingAnimations.take(identity)) {
        openingAnimation->stop();
        openingAnimation->deleteLater();
    }

    const int styleDuration = style()->styleHint(
        QStyle::SH_Widget_Animation_Duration,
        nullptr,
        this
    );
    const int startWidth = tabSizeHint(index).width();
    if (!isVisible() || styleDuration <= 0 || startWidth <= 1) {
        completion();
        return true;
    }

    if (QWidget *button = tabButton(index, QTabBar::LeftSide))
        button->setEnabled(false);
    if (QWidget *button = tabButton(index, QTabBar::RightSide))
        button->setEnabled(false);

    auto *animation = new QVariantAnimation(this);
    animation->setDuration(std::min(
        styleDuration,
        maximumTabAnimationDurationMilliseconds
    ));
    animation->setStartValue(startWidth);
    animation->setEndValue(1);
    animation->setEasingCurve(QEasingCurve::InCubic);
    m_closingAnimations.insert(identity, animation);

    connect(
        animation,
        &QVariantAnimation::valueChanged,
        this,
        [this, identity](const QVariant &value) {
            const int animatedIndex = indexForIdentity(identity);
            if (animatedIndex < 0)
                return;
            QVariantMap metadata = tabMetadata(tabData(animatedIndex));
            metadata.insert(
                QString::fromLatin1(animatedWidthKey),
                value.toInt()
            );
            setTabData(animatedIndex, metadata);
            refreshTabLayout();
        }
    );
    connect(
        animation,
        &QVariantAnimation::finished,
        this,
        [this, identity, animation, completion = std::move(completion)] {
            if (m_closingAnimations.value(identity) != animation) {
                animation->deleteLater();
                return;
            }
            m_closingAnimations.remove(identity);
            completion();

            const int animatedIndex = indexForIdentity(identity);
            if (animatedIndex >= 0) {
                QVariantMap metadata = tabMetadata(tabData(animatedIndex));
                metadata.remove(QString::fromLatin1(animatedWidthKey));
                setTabData(animatedIndex, metadata);
                if (QWidget *button = tabButton(animatedIndex, QTabBar::LeftSide))
                    button->setEnabled(true);
                if (QWidget *button = tabButton(animatedIndex, QTabBar::RightSide))
                    button->setEnabled(true);
                refreshTabLayout();
            }
            animation->deleteLater();
        }
    );
    animation->start();
    return true;
}

QSize BrowserTabBar::naturalTabSizeHint(int index) const
{
    QSize size = QTabBar::tabSizeHint(index);
    size.setWidth(isTabPinned(index) ? pinnedTabWidth : m_regularTabWidth);
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

void BrowserTabBar::configureLeadingButton(int index)
{
#if defined(Q_OS_MACOS)
    if (!tabsClosable() || index < 0 || index >= count())
        return;

    setTabButton(index, QTabBar::LeftSide, nullptr);
    setTabButton(index, QTabBar::RightSide, nullptr);

    QVariantMap metadata = tabMetadata(tabData(index));
    const QIcon existingIcon = QTabBar::tabIcon(index);
    if (!existingIcon.isNull())
        metadata.insert(QString::fromLatin1(faviconKey), existingIcon);
    setTabData(index, metadata);
    QTabBar::setTabIcon(index, QIcon());

    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("tabLeadingButton"));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    connect(button, &QToolButton::clicked, this, [this, button] {
        if (!button->property(showsCloseKey).toBool())
            return;
        for (int tabIndex = 0; tabIndex < count(); ++tabIndex) {
            if (tabButton(tabIndex, QTabBar::LeftSide) == button) {
                emit tabCloseRequested(tabIndex);
                return;
            }
        }
    });
    setTabButton(index, QTabBar::LeftSide, button);
    updateLeadingButton(index);
#else
    Q_UNUSED(index);
#endif
}

void BrowserTabBar::setHoveredTab(int index)
{
#if defined(Q_OS_MACOS)
    const quint64 previousIdentity = m_hoveredTabIdentity;
    m_hoveredTabIdentity = index >= 0 ? ensureTabIdentity(index) : 0;
    if (previousIdentity == m_hoveredTabIdentity)
        return;

    const int previousIndex = indexForIdentity(previousIdentity);
    if (previousIndex >= 0)
        updateLeadingButton(previousIndex);
    if (index >= 0 && index < count())
        updateLeadingButton(index);
#else
    Q_UNUSED(index);
#endif
}

void BrowserTabBar::refreshTabLayout()
{
    QResizeEvent event(size(), size());
    QTabBar::resizeEvent(&event);
    updateGeometry();
    update();
    scheduleHoveredTabRefresh();
}

void BrowserTabBar::scheduleHoveredTabRefresh()
{
#if defined(Q_OS_MACOS)
    if (m_hoverRefreshScheduled)
        return;
    m_hoverRefreshScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_hoverRefreshScheduled = false;
        const QPoint localPosition = mapFromGlobal(QCursor::pos());
        setHoveredTab(rect().contains(localPosition) ? tabAt(localPosition) : -1);
    });
#endif
}

void BrowserTabBar::retargetOpeningAnimations()
{
    for (auto iterator = m_openingAnimations.cbegin();
         iterator != m_openingAnimations.cend();
         ++iterator) {
        QVariantAnimation *animation = iterator.value();
        const int index = indexForIdentity(iterator.key());
        if (!animation || index < 0)
            continue;

        const int targetWidth = naturalTabSizeHint(index).width();
        if (animation->endValue().toInt() != targetWidth)
            animation->setEndValue(targetWidth);
    }
}

void BrowserTabBar::updateTabWidths()
{
    const int pinnedCount = pinnedTabCount();
    const int regularCount = count() - pinnedCount;
    int nextRegularWidth = regularTabMaximumWidth;
    if (m_availableWidth >= 0 && regularCount > 0) {
        const int availableForRegularTabs = std::max(
            0,
            m_availableWidth - pinnedCount * pinnedTabWidth
        );
        nextRegularWidth = std::clamp(
            availableForRegularTabs / regularCount,
            regularTabMinimumWidth,
            regularTabMaximumWidth
        );
    }

    const int preferredWidth = pinnedCount * pinnedTabWidth
        + regularCount * nextRegularWidth;
    const int maximumStripWidth = m_availableWidth >= 0
        ? std::min(m_availableWidth, preferredWidth)
        : QWIDGETSIZE_MAX;
    const bool widthChanged = m_regularTabWidth != nextRegularWidth;
    m_regularTabWidth = nextRegularWidth;
    if (maximumWidth() != maximumStripWidth)
        setMaximumWidth(maximumStripWidth);
    retargetOpeningAnimations();

    updateGeometry();
    if (widthChanged)
        refreshTabLayout();
    else
        update();
}

void BrowserTabBar::updateCloseButtonVisibility(int index)
{
#if defined(Q_OS_MACOS)
    updateLeadingButton(index);
#else
    const bool visible = !isTabPinned(index);
    if (QWidget *button = tabButton(index, QTabBar::LeftSide))
        button->setVisible(visible);
    if (QWidget *button = tabButton(index, QTabBar::RightSide))
        button->setVisible(visible);
#endif
}

void BrowserTabBar::updateLeadingButton(int index)
{
#if defined(Q_OS_MACOS)
    if (index < 0 || index >= count())
        return;
    auto *button = qobject_cast<QToolButton *>(
        tabButton(index, QTabBar::LeftSide)
    );
    if (!button)
        return;

    const bool showsClose = !isTabPinned(index)
        && ensureTabIdentity(index) == m_hoveredTabIdentity;
    button->setProperty(showsCloseKey, showsClose);
    button->setAttribute(Qt::WA_TransparentForMouseEvents, !showsClose);
    button->setIcon(
        showsClose
            ? QIcon(QStringLiteral(":/assets/icons/tab-close.svg"))
            : tabIcon(index)
    );
    button->setIconSize(QSize(16, 16));
    button->setToolTip(
        showsClose
            ? QCoreApplication::translate("MainWindow", "Close Tab")
            : QString()
    );
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
#else
    Q_UNUSED(index);
#endif
}
