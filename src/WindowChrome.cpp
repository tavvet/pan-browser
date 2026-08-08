#include "WindowChrome.h"

#include "WindowChromePlatform.h"

#include <QEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPlatformSurfaceEvent>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <algorithm>

QMargins integratedChromeContentMargins(
    const QMargins &baseMargins,
    const QMargins &safeAreaMargins,
    const QMargins &platformControlMargins
)
{
    return QMargins(
        std::max({
            baseMargins.left(),
            safeAreaMargins.left(),
            platformControlMargins.left(),
        }),
        baseMargins.top(),
        std::max({
            baseMargins.right(),
            safeAreaMargins.right(),
            platformControlMargins.right(),
        }),
        baseMargins.bottom()
    );
}

bool WindowChromeController::platformSupportsIntegratedTitleBar()
{
#if defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

void WindowChromeController::applyIntegratedTitleBar(QWidget *window)
{
    if (!window || !platformSupportsIntegratedTitleBar())
        return;

    window->setWindowFlag(Qt::ExpandedClientAreaHint, true);
    window->setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
    window->setAttribute(Qt::WA_LayoutOnEntireRect, true);
    window->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
}

WindowChromeController::WindowChromeController(
    QWidget *window,
    QLayout *titleBarLayout,
    const QList<QWidget *> &dragRegions,
    QObject *parent
)
    : QObject(parent)
    , m_window(window)
    , m_titleBarLayout(titleBarLayout)
    , m_baseMargins(titleBarLayout ? titleBarLayout->contentsMargins() : QMargins())
{
    if (m_window)
        m_window->installEventFilter(this);
    for (QWidget *region : dragRegions) {
        if (!region)
            continue;
        m_dragRegions.append(region);
        region->installEventFilter(this);
    }
    bindWindowHandle();
}

bool WindowChromeController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::Show:
            m_surfaceDestroying = false;
            bindWindowHandle();
            scheduleContentMarginsUpdate();
            break;
        case QEvent::WinIdChange:
            if (!m_surfaceDestroying) {
                bindWindowHandle();
                scheduleContentMarginsUpdate();
            }
            break;
        case QEvent::PlatformSurface: {
            const auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
            if (surfaceEvent->surfaceEventType()
                == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                m_surfaceDestroying = true;
                unbindWindowHandle();
                updateContentMargins();
            } else {
                m_surfaceDestroying = false;
                bindWindowHandle();
                scheduleContentMarginsUpdate();
            }
            break;
        }
        default:
            break;
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick && isDragRegion(watched)) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton
            && mouseEvent->modifiers() == Qt::NoModifier
            && m_window
            && !m_window->isFullScreen()) {
            if (m_window->isMaximized())
                m_window->showNormal();
            else
                m_window->showMaximized();
            mouseEvent->accept();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress && isDragRegion(watched)) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton
            && mouseEvent->modifiers() == Qt::NoModifier
            && m_window
            && !m_window->isFullScreen()) {
            bindWindowHandle();
            if (m_windowHandle && m_windowHandle->startSystemMove()) {
                mouseEvent->accept();
                return true;
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

bool WindowChromeController::isDragRegion(const QObject *object) const
{
    return std::any_of(
        m_dragRegions.cbegin(),
        m_dragRegions.cend(),
        [object](const QPointer<QWidget> &region) {
            return region && region == object;
        }
    );
}

void WindowChromeController::bindWindowHandle()
{
    if (m_surfaceDestroying)
        return;
    QWindow *handle = m_window ? m_window->windowHandle() : nullptr;
    if (!handle || handle == m_windowHandle)
        return;

    if (m_safeAreaConnection)
        disconnect(m_safeAreaConnection);
    m_windowHandle = handle;
    configurePlatformIntegratedTitleBar(m_window);
    m_safeAreaConnection = connect(
        handle,
        &QWindow::safeAreaMarginsChanged,
        this,
        [this] {
            updateContentMargins();
        }
    );
    updateContentMargins();
}

void WindowChromeController::unbindWindowHandle()
{
    if (m_safeAreaConnection)
        disconnect(m_safeAreaConnection);
    m_safeAreaConnection = {};
    m_windowHandle.clear();
}

void WindowChromeController::scheduleContentMarginsUpdate()
{
    QTimer::singleShot(0, this, [this] {
        if (m_surfaceDestroying)
            return;
        bindWindowHandle();
        updateContentMargins();
    });
}

void WindowChromeController::updateContentMargins()
{
    if (!m_titleBarLayout)
        return;
    const QMargins safeMargins = m_windowHandle
        ? m_windowHandle->safeAreaMargins()
        : QMargins();
    const QMargins platformMargins = m_windowHandle
        ? platformTitleBarControlMargins(m_window)
        : QMargins();
    m_titleBarLayout->setContentsMargins(
        integratedChromeContentMargins(
            m_baseMargins,
            safeMargins,
            platformMargins
        )
    );
}
