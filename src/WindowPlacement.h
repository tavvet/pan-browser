#pragma once

#include <QList>
#include <QRect>

[[nodiscard]] QRect adjustedWindowGeometry(
    const QRect &restoredGeometry,
    const QList<QRect> &availableScreens,
    const QRect &fallbackScreen
);

[[nodiscard]] QRect popupWindowGeometry(
    const QRect &requestedGeometry,
    const QRect &ownerGeometry,
    const QList<QRect> &availableScreens,
    const QRect &fallbackScreen,
    const QSize &minimumSize = QSize(720, 520)
);
