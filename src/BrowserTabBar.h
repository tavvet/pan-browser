#pragma once

#include <QHash>
#include <QTabBar>

#include <functional>

class QMouseEvent;
class QEvent;
class QResizeEvent;
class QVariantAnimation;

class BrowserTabBar final : public QTabBar {
public:
    explicit BrowserTabBar(QWidget *parent = nullptr);

    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] bool isTabPinned(int index) const;
    void setTabPinned(int index, bool pinned);
    [[nodiscard]] int pinnedTabCount() const;
    [[nodiscard]] int normalizedMoveDestination(int movedIndex) const;
    [[nodiscard]] QIcon tabIcon(int index) const;
    void setTabIcon(int index, const QIcon &icon);
    void setTabsClosable(bool closable);
    void setAvailableWidth(int width);
    void animateTabOpening(int index);
    bool animateTabClosing(int index, std::function<void()> completion);

protected:
    [[nodiscard]] QSize tabSizeHint(int index) const override;
    [[nodiscard]] QSize minimumTabSizeHint(int index) const override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;

private:
    [[nodiscard]] QSize naturalTabSizeHint(int index) const;
    [[nodiscard]] quint64 ensureTabIdentity(int index);
    [[nodiscard]] int indexForIdentity(quint64 identity) const;
    void configureLeadingButton(int index);
    void refreshTabLayout();
    void retargetOpeningAnimations();
    void scheduleHoveredTabRefresh();
    void setHoveredTab(int index);
    void updateTabWidths();
    void updateCloseButtonVisibility(int index);
    void updateLeadingButton(int index);

    quint64 m_nextTabIdentity = 0;
    quint64 m_draggedTabIdentity = 0;
    quint64 m_hoveredTabIdentity = 0;
    int m_availableWidth = -1;
    int m_regularTabWidth = 220;
    bool m_hoverRefreshScheduled = false;
    QHash<quint64, QVariantAnimation *> m_openingAnimations;
    QHash<quint64, QVariantAnimation *> m_closingAnimations;
};
