#include "PermissionPolicy.h"

#include <QCoreApplication>

namespace {

QString permissionText(const char *source)
{
    return QCoreApplication::translate("PermissionPolicy", source);
}

} // namespace

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
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "Camera access requested"));
    case BrowserPermissionKind::Microphone:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "Microphone access requested"));
    case BrowserPermissionKind::CameraAndMicrophone:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "Camera and microphone access requested"));
    case BrowserPermissionKind::Location:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "Location access requested"));
    case BrowserPermissionKind::Notifications:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "Notification access requested"));
    case BrowserPermissionKind::Other:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "Permission requested"));
    }
    return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "Permission requested"));
}

QString permissionDescription(BrowserPermissionKind kind)
{
    switch (kind) {
    case BrowserPermissionKind::Camera:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "This site wants to use your camera."));
    case BrowserPermissionKind::Microphone:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "This site wants to use your microphone."));
    case BrowserPermissionKind::CameraAndMicrophone:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "This site wants to use your camera and microphone."));
    case BrowserPermissionKind::Location:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "This site wants to access your location."));
    case BrowserPermissionKind::Notifications:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "This site wants to send notifications."));
    case BrowserPermissionKind::Other:
        return permissionText(QT_TRANSLATE_NOOP("PermissionPolicy", "This permission is not supported by PanBrowser."));
    }
    return QString();
}
