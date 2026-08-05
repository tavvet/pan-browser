#include "PermissionController.h"

#include "PermissionPrompt.h"

#include <QWebEngineView>

#include <utility>

PermissionController::PermissionController(PermissionPrompt *prompt, QObject *parent)
    : QObject(parent)
    , m_prompt(prompt)
{
    connect(m_prompt, &PermissionPrompt::allowRequested, this, &PermissionController::allowCurrent);
    connect(m_prompt, &PermissionPrompt::blockRequested, this, &PermissionController::denyCurrent);
}

PermissionController::~PermissionController()
{
    denyAll();
}

void PermissionController::request(
    QWebEngineView *webView,
    const QWebEnginePermission &permission
)
{
    if (!webView || !permission.isValid())
        return;

    const BrowserPermissionKind kind = kindForPermission(permission.permissionType());
    if (permission.state() != QWebEnginePermission::State::Ask
        || permissionDisposition(permission.origin(), kind) == PermissionDisposition::Deny
        || webView != m_currentView) {
        permission.deny();
        return;
    }

    m_queue.append({webView, permission, kind});
    showNext();
}

void PermissionController::currentViewChanged(QWebEngineView *webView)
{
    m_currentView = webView;
    if (m_active && m_active->webView != m_currentView)
        denyCurrent();
    showNext();
}

void PermissionController::cancelForView(QWebEngineView *webView)
{
    if (!webView)
        return;
    if (m_active && m_active->webView == webView) {
        if (m_active->permission.isValid()
            && m_active->permission.state() == QWebEnginePermission::State::Ask) {
            m_active->permission.deny();
        }
        m_active.reset();
        m_prompt->hideRequest();
    }

    for (qsizetype index = m_queue.size() - 1; index >= 0; --index) {
        if (m_queue.at(index).webView == webView) {
            m_queue.at(index).permission.deny();
            m_queue.removeAt(index);
        }
    }
    showNext();
}

BrowserPermissionKind PermissionController::kindForPermission(
    QWebEnginePermission::PermissionType type
)
{
    switch (type) {
    case QWebEnginePermission::PermissionType::MediaVideoCapture:
        return BrowserPermissionKind::Camera;
    case QWebEnginePermission::PermissionType::MediaAudioCapture:
        return BrowserPermissionKind::Microphone;
    case QWebEnginePermission::PermissionType::MediaAudioVideoCapture:
        return BrowserPermissionKind::CameraAndMicrophone;
    case QWebEnginePermission::PermissionType::Geolocation:
        return BrowserPermissionKind::Location;
    case QWebEnginePermission::PermissionType::Notifications:
        return BrowserPermissionKind::Notifications;
    case QWebEnginePermission::PermissionType::Unsupported:
    case QWebEnginePermission::PermissionType::DesktopVideoCapture:
    case QWebEnginePermission::PermissionType::DesktopAudioVideoCapture:
    case QWebEnginePermission::PermissionType::MouseLock:
    case QWebEnginePermission::PermissionType::ClipboardReadWrite:
    case QWebEnginePermission::PermissionType::LocalFontsAccess:
        return BrowserPermissionKind::Other;
    }
    return BrowserPermissionKind::Other;
}

void PermissionController::allowCurrent()
{
    if (m_active
        && m_active->webView == m_currentView
        && m_active->permission.isValid()
        && m_active->permission.state() == QWebEnginePermission::State::Ask) {
        m_active->permission.grant();
    }
    m_active.reset();
    m_prompt->hideRequest();
    showNext();
}

void PermissionController::denyCurrent()
{
    if (m_active
        && m_active->permission.isValid()
        && m_active->permission.state() == QWebEnginePermission::State::Ask) {
        m_active->permission.deny();
    }
    m_active.reset();
    m_prompt->hideRequest();
    showNext();
}

void PermissionController::showNext()
{
    if (m_active)
        return;
    while (!m_queue.isEmpty()) {
        PendingPermission pending = m_queue.takeFirst();
        if (!pending.webView
            || pending.webView != m_currentView
            || !pending.permission.isValid()
            || pending.permission.state() != QWebEnginePermission::State::Ask) {
            if (pending.permission.isValid()
                && pending.permission.state() == QWebEnginePermission::State::Ask) {
                pending.permission.deny();
            }
            continue;
        }

        m_active = pending;
        const QUrl origin = pending.permission.origin();
        const QString originText = origin.port() > 0
            ? QStringLiteral("%1://%2:%3").arg(origin.scheme(), origin.host()).arg(origin.port())
            : QStringLiteral("%1://%2").arg(origin.scheme(), origin.host());
        m_prompt->showRequest(
            originText,
            permissionTitle(pending.kind),
            permissionDescription(pending.kind)
        );
        return;
    }
    m_prompt->hideRequest();
}

void PermissionController::denyAll()
{
    if (m_active
        && m_active->permission.isValid()
        && m_active->permission.state() == QWebEnginePermission::State::Ask) {
        m_active->permission.deny();
    }
    m_active.reset();
    for (const PendingPermission &pending : std::as_const(m_queue)) {
        if (pending.permission.isValid()
            && pending.permission.state() == QWebEnginePermission::State::Ask) {
            pending.permission.deny();
        }
    }
    m_queue.clear();
    if (m_prompt)
        m_prompt->hideRequest();
}
