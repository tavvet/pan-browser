#include "BrowserFullScreenController.h"

#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QWebEngineView>

#include <utility>

BrowserFullScreenController::BrowserFullScreenController(QMainWindow *window)
    : m_window(window)
{
}

bool BrowserFullScreenController::enter(QWebEngineView *webView)
{
    if (!m_window || !webView || m_active)
        return false;

    m_active = true;
    m_webView = webView;
    m_previousWindowState = m_window->windowState();
    m_chromeVisibility.clear();

    const QList<QToolBar *> toolBars = m_window->findChildren<QToolBar *>(
        QString(),
        Qt::FindDirectChildrenOnly
    );
    for (QToolBar *toolBar : toolBars)
        m_chromeVisibility.append({toolBar, toolBar->isVisible()});
    if (QStatusBar *statusBar = m_window->findChild<QStatusBar *>(
            QString(),
            Qt::FindDirectChildrenOnly
        )) {
        m_chromeVisibility.append({statusBar, statusBar->isVisible()});
    }
    if (QMenuBar *menuBar = m_window->findChild<QMenuBar *>(
            QString(),
            Qt::FindDirectChildrenOnly
        )) {
        m_chromeVisibility.append({menuBar, menuBar->isVisible()});
    }

    for (const auto &[widget, visible] : std::as_const(m_chromeVisibility)) {
        Q_UNUSED(visible);
        if (widget)
            widget->hide();
    }
    m_window->showFullScreen();
    webView->setFocus(Qt::OtherFocusReason);
    return true;
}

void BrowserFullScreenController::exit()
{
    if (!m_window || !m_active)
        return;

    m_active = false;
    m_webView.clear();
    if (m_previousWindowState.testFlag(Qt::WindowFullScreen))
        m_window->showFullScreen();
    else if (m_previousWindowState.testFlag(Qt::WindowMaximized))
        m_window->showMaximized();
    else
        m_window->showNormal();

    for (const auto &[widget, visible] : std::as_const(m_chromeVisibility)) {
        if (widget)
            widget->setVisible(visible);
    }
    m_chromeVisibility.clear();
    m_previousWindowState = Qt::WindowNoState;
}

bool BrowserFullScreenController::isActive() const
{
    return m_active;
}

QWebEngineView *BrowserFullScreenController::webView() const
{
    return m_webView;
}
