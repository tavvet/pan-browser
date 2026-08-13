#pragma once

#include <QHash>
#include <QTabBar>

class QMouseEvent;
class QVariantAnimation;

class BrowserTabBar final : public QTabBar {
public:
    explicit BrowserTabBar(QWidget *parent = nullptr);

    [[nodiscard]] bool isTabPinned(int index) const;
    void setTabPinned(int index, bool pinned);
    [[nodiscard]] int pinnedTabCount() const;
    [[nodiscard]] int normalizedMoveDestination(int movedIndex) const;
    void animateTabOpening(int index);

protected:
    [[nodiscard]] QSize tabSizeHint(int index) const override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    [[nodiscard]] QSize naturalTabSizeHint(int index) const;
    [[nodiscard]] quint64 ensureTabIdentity(int index);
    [[nodiscard]] int indexForIdentity(quint64 identity) const;
    void refreshTabLayout();
    void updateCloseButtonVisibility(int index);

    quint64 m_nextTabIdentity = 0;
    quint64 m_draggedTabIdentity = 0;
    QHash<quint64, QVariantAnimation *> m_openingAnimations;
};
