#pragma once

#include <QString>
#include <QUrl>

enum class BrowserPermissionKind {
    Camera,
    Microphone,
    CameraAndMicrophone,
    Location,
    Notifications,
    Other,
};

enum class PermissionDisposition {
    Prompt,
    Deny,
};

[[nodiscard]] PermissionDisposition permissionDisposition(
    const QUrl &origin,
    BrowserPermissionKind kind
);
[[nodiscard]] QString permissionTitle(BrowserPermissionKind kind);
[[nodiscard]] QString permissionDescription(BrowserPermissionKind kind);
