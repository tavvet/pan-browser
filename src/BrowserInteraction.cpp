#include "BrowserInteraction.h"

#include <QWidget>

namespace {

QUrl normalizedWebOrigin(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty())
        return QUrl();

    const QString scheme = url.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
        return QUrl();

    QUrl origin;
    origin.setScheme(scheme);
    origin.setHost(url.host().toLower());
    const int port = url.port();
    const bool defaultPort = (scheme == QStringLiteral("http") && port == 80)
        || (scheme == QStringLiteral("https") && port == 443);
    if (port >= 0 && !defaultPort)
        origin.setPort(port);
    return origin.isValid() && !origin.host().isEmpty() ? origin : QUrl();
}

} // namespace

DetachedVideoSession::DetachedVideoSession(
    QObject *parent,
    int returnTimeoutMilliseconds
)
    : QObject(parent)
{
    m_returnTimer.setSingleShot(true);
    m_returnTimer.setInterval(qMax(0, returnTimeoutMilliseconds));
    connect(&m_returnTimer, &QTimer::timeout, this, &DetachedVideoSession::completeReturn);
}

DetachedVideoSession::State DetachedVideoSession::state() const
{
    return m_state;
}

bool DetachedVideoSession::isDetached() const
{
    return m_state != State::Attached;
}

bool DetachedVideoSession::beginDetached()
{
    if (m_state != State::Attached)
        return false;
    m_state = State::Detached;
    return true;
}

bool DetachedVideoSession::requestReturn()
{
    if (m_state != State::Detached)
        return false;
    m_state = State::ReturnPending;
    m_returnTimer.start();
    emit exitFullScreenRequested();
    return true;
}

void DetachedVideoSession::browserExitedFullScreen()
{
    completeReturn();
}

void DetachedVideoSession::forceRestore()
{
    completeReturn();
}

void DetachedVideoSession::reset()
{
    m_returnTimer.stop();
    m_state = State::Attached;
}

void DetachedVideoSession::completeReturn()
{
    if (m_state == State::Attached)
        return;
    reset();
    emit restoreRequested();
}

QWebEngineView *resolveActiveBrowserView(
    QWidget *activeWindow,
    const QWidget *mainWindow,
    QWebEngineView *currentView,
    std::span<const BrowserInteractionSurface> detachedSurfaces
)
{
    for (const BrowserInteractionSurface &surface : detachedSurfaces) {
        if (!surface.view || !surface.window)
            continue;
        if (surface.window == activeWindow)
            return surface.view;
    }
    return activeWindow == mainWindow ? currentView : nullptr;
}

QWebEngineView *resolveBrowserCommandTarget(
    QWebEngineView *activeView,
    QWebEngineView *lastActiveView,
    QWebEngineView *currentView
)
{
    if (activeView)
        return activeView;
    return lastActiveView ? lastActiveView : currentView;
}

FullScreenRequestDecision decideFullScreenRequest(
    bool toggleOn,
    bool interactionActive,
    bool alreadyDetached,
    const QUrl &requestOrigin
)
{
    if (!toggleOn)
        return {FullScreenRequestAction::Restore, QUrl()};
    if (!interactionActive || alreadyDetached)
        return {};

    const QUrl origin = normalizedWebOrigin(requestOrigin);
    if (origin.isEmpty())
        return {};
    return {FullScreenRequestAction::Detach, origin};
}

QString fullScreenOriginDisplay(const QUrl &origin)
{
    const QUrl normalized = normalizedWebOrigin(origin);
    if (normalized.isEmpty())
        return QString();
    return QString::fromLatin1(normalized.toEncoded(
        QUrl::RemovePath
        | QUrl::RemoveQuery
        | QUrl::RemoveFragment
        | QUrl::RemoveUserInfo
    ));
}
