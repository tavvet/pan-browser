#pragma once

#include <QWebEnginePage>

class BrowserPage final : public QWebEnginePage {
    Q_OBJECT

public:
    explicit BrowserPage(QWebEngineProfile *profile, QObject *parent = nullptr);

signals:
    void externalUrlRequested(const QUrl &url);

protected:
    bool acceptNavigationRequest(
        const QUrl &url,
        NavigationType type,
        bool isMainFrame
    ) override;
};
