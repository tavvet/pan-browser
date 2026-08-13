#pragma once

#include <QList>
#include <QPair>
#include <QPointer>
#include <QWidget>

class QMainWindow;
class QWebEngineView;

class BrowserFullScreenController final {
public:
    explicit BrowserFullScreenController(QMainWindow *window);

    [[nodiscard]] bool enter(QWebEngineView *webView);
    void exit();

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] QWebEngineView *webView() const;

private:
    QPointer<QMainWindow> m_window;
    QPointer<QWebEngineView> m_webView;
    QList<QPair<QPointer<QWidget>, bool>> m_chromeVisibility;
    Qt::WindowStates m_previousWindowState = Qt::WindowNoState;
    bool m_active = false;
};
