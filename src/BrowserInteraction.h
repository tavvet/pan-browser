#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>

#include <span>

class QWidget;
class QWebEngineView;

struct BrowserInteractionSurface {
    QWebEngineView *view = nullptr;
    QWidget *window = nullptr;
};

enum class FullScreenRequestAction {
    Reject,
    EnterBrowserFullScreen,
    DetachVideo,
    RestoreBrowserFullScreen,
    RestoreDetachedVideo,
};

struct FullScreenRequestDecision {
    FullScreenRequestAction action = FullScreenRequestAction::Reject;
    QUrl origin;
};

class DetachedVideoSession final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Attached,
        Detached,
        ReturnPending,
    };

    explicit DetachedVideoSession(
        QObject *parent = nullptr,
        int returnTimeoutMilliseconds = 250
    );

    [[nodiscard]] State state() const;
    [[nodiscard]] bool isDetached() const;
    [[nodiscard]] bool beginDetached();
    [[nodiscard]] bool requestReturn();
    void browserExitedFullScreen();
    void forceRestore();
    void reset();

signals:
    void exitFullScreenRequested();
    void restoreRequested();

private:
    void completeReturn();

    State m_state = State::Attached;
    QTimer m_returnTimer;
};

[[nodiscard]] QWebEngineView *resolveActiveBrowserView(
    QWidget *activeWindow,
    const QWidget *mainWindow,
    QWebEngineView *currentView,
    std::span<const BrowserInteractionSurface> detachedSurfaces
);

[[nodiscard]] QWebEngineView *resolveBrowserCommandTarget(
    QWebEngineView *activeView,
    QWebEngineView *lastActiveView,
    QWebEngineView *currentView
);

[[nodiscard]] FullScreenRequestDecision decideFullScreenRequest(
    bool toggleOn,
    bool interactionActive,
    bool videoPopoutRequested,
    bool alreadyDetached,
    bool browserFullScreenActive,
    bool browserFullScreenOwnsRequest,
    const QUrl &requestOrigin
);

[[nodiscard]] QString fullScreenOriginDisplay(const QUrl &origin);
