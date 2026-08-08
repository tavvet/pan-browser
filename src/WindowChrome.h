#pragma once

#include <QList>
#include <QMargins>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

class QEvent;
class QLayout;
class QWidget;
class QWindow;

[[nodiscard]] QMargins integratedChromeContentMargins(
    const QMargins &baseMargins,
    const QMargins &safeAreaMargins,
    const QMargins &platformControlMargins = QMargins()
);

class WindowChromeController final : public QObject {
public:
    [[nodiscard]] static bool platformSupportsIntegratedTitleBar();
    [[nodiscard]] static bool platformUsesClientCaptionButtons();
    static void applyIntegratedTitleBar(QWidget *window);

    WindowChromeController(
        QWidget *window,
        QLayout *titleBarLayout,
        const QList<QWidget *> &dragRegions,
        QObject *parent = nullptr
    );

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] bool isDragRegion(const QObject *object) const;
    void bindWindowHandle();
    void unbindWindowHandle();
    void scheduleContentMarginsUpdate();
    void updateContentMargins();

    QPointer<QWidget> m_window;
    QPointer<QWindow> m_windowHandle;
    QPointer<QLayout> m_titleBarLayout;
    QList<QPointer<QWidget>> m_dragRegions;
    QMargins m_baseMargins;
    QMetaObject::Connection m_safeAreaConnection;
    bool m_surfaceDestroying = false;
};
