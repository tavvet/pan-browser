#pragma once

#include <QFrame>
#include <QMainWindow>
#include <QPointer>
#include <QSize>

class QCloseEvent;
class QEvent;
class QResizeEvent;
class QToolButton;
class QWebEngineView;

class DetachedVideoPlaceholder final : public QFrame {
    Q_OBJECT

public:
    DetachedVideoPlaceholder(
        QWebEngineView *sourceView,
        const QString &message,
        const QString &returnButtonText
    );
    ~DetachedVideoPlaceholder() override;

signals:
    void returnRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<QWebEngineView> m_sourceView;
};

class DetachedVideoWindow final : public QMainWindow {
    Q_OBJECT

public:
    DetachedVideoWindow(
        QWebEngineView *sourceView,
        const QString &windowTitle,
        const QSize &videoSize = QSize(16, 9),
        QWidget *parent = nullptr
    );
    ~DetachedVideoWindow() override;

    [[nodiscard]] QWebEngineView *webView() const;
    void restorePage();

signals:
    void returnRequested();
    void contextMenuRequested(const QPoint &position);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateCloseButtonGeometry();
    void setCloseButtonVisible(bool visible);
    [[nodiscard]] QRect constrainedResizeGeometry(const QPoint &globalPosition) const;
    [[nodiscard]] QSize constrainedSize(int width, int height, bool widthDriven) const;

    QPointer<QWebEngineView> m_sourceView;
    QWebEngineView *m_webView = nullptr;
    QToolButton *m_closeButton = nullptr;
    QPoint m_dragPressGlobal;
    QPoint m_dragStartPosition;
    QRect m_resizeStartGeometry;
    Qt::Edges m_resizeEdges;
    QSize m_videoAspectSize = QSize(16, 9);
    double m_videoAspectRatio = 16.0 / 9.0;
    bool m_pageRestored = false;
    bool m_dragCandidate = false;
    bool m_dragging = false;
    bool m_systemMoving = false;
    bool m_resizing = false;
};
