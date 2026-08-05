#include "PermissionPolicy.h"

PermissionDisposition permissionDisposition(
    const QUrl &origin,
    BrowserPermissionKind kind
)
{
    if (!origin.isValid()
        || origin.scheme() != QStringLiteral("https")
        || origin.host().isEmpty()) {
        return PermissionDisposition::Deny;
    }

    switch (kind) {
    case BrowserPermissionKind::Camera:
    case BrowserPermissionKind::Microphone:
    case BrowserPermissionKind::CameraAndMicrophone:
    case BrowserPermissionKind::Location:
        return PermissionDisposition::Prompt;
    case BrowserPermissionKind::Notifications:
    case BrowserPermissionKind::Other:
        return PermissionDisposition::Deny;
    }
    return PermissionDisposition::Deny;
}

QString permissionTitle(BrowserPermissionKind kind)
{
    switch (kind) {
    case BrowserPermissionKind::Camera:
        return QStringLiteral("Camera access requested");
    case BrowserPermissionKind::Microphone:
        return QStringLiteral("Microphone access requested");
    case BrowserPermissionKind::CameraAndMicrophone:
        return QStringLiteral("Camera and microphone access requested");
    case BrowserPermissionKind::Location:
        return QStringLiteral("Location access requested");
    case BrowserPermissionKind::Notifications:
        return QStringLiteral("Notification access requested");
    case BrowserPermissionKind::Other:
        return QStringLiteral("Permission requested");
    }
    return QStringLiteral("Permission requested");
}

QString permissionDescription(BrowserPermissionKind kind)
{
    switch (kind) {
    case BrowserPermissionKind::Camera:
        return QStringLiteral("This site wants to use your camera.");
    case BrowserPermissionKind::Microphone:
        return QStringLiteral("This site wants to use your microphone.");
    case BrowserPermissionKind::CameraAndMicrophone:
        return QStringLiteral("This site wants to use your camera and microphone.");
    case BrowserPermissionKind::Location:
        return QStringLiteral("This site wants to access your location.");
    case BrowserPermissionKind::Notifications:
        return QStringLiteral("This site wants to send notifications.");
    case BrowserPermissionKind::Other:
        return QStringLiteral("This permission is not supported by PanBrowser.");
    }
    return QString();
}
