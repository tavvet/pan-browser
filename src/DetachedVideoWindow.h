#pragma once

#include <QFrame>
#include <QMainWindow>
#include <QPointer>
#include <QString>

#include <functional>

class QCloseEvent;
class QEvent;
class QLabel;
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
    using CopyTextHandler = std::function<void(const QString &)>;

    DetachedVideoWindow(
        QWebEngineView *sourceView,
        const QString &windowTitle,
        const QString &sourceCaption,
        const QString &sourceOrigin,
        QWidget *parent = nullptr,
        CopyTextHandler copyTextHandler = {}
    );
    ~DetachedVideoWindow() override;

    [[nodiscard]] QWebEngineView *webView() const;
    [[nodiscard]] QString sourceOriginText() const;
    void restorePage();

signals:
    void returnRequested();
    void contextMenuRequested(const QPoint &position);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<QWebEngineView> m_sourceView;
    QWebEngineView *m_webView = nullptr;
    QLabel *m_originLabel = nullptr;
    QString m_sourceOriginText;
    bool m_pageRestored = false;
};
