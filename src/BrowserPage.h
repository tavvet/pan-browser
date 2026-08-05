#pragma once

#include <QWebEnginePage>

class BrowserPage final : public QWebEnginePage {
    Q_OBJECT

public:
    explicit BrowserPage(QWebEngineProfile *profile, QObject *parent = nullptr);

signals:
    void externalUrlRequested(const QUrl &url);
    void mainFrameNavigationRequested(const QUrl &url, int navigationType);

protected:
    bool acceptNavigationRequest(
        const QUrl &url,
        NavigationType type,
        bool isMainFrame
    ) override;
};
