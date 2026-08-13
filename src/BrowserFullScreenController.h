#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QWidget>

class QEvent;
class QMainWindow;
class QWebEngineView;

class BrowserFullScreenController final : public QObject {
    Q_OBJECT

public:
    explicit BrowserFullScreenController(QMainWindow *window);
    ~BrowserFullScreenController() override;

    [[nodiscard]] bool enter(QWebEngineView *webView);
    void exit();

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] QWebEngineView *webView() const;

signals:
    void nativeExitRequested(QWebEngineView *webView);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<QMainWindow> m_window;
    QPointer<QWebEngineView> m_webView;
    QList<QPair<QPointer<QWidget>, bool>> m_chromeVisibility;
    Qt::WindowStates m_previousWindowState = Qt::WindowNoState;
    bool m_active = false;
    bool m_nativeExitCheckPending = false;
    bool m_nativeWindowExitObserved = false;
};
