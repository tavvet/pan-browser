#pragma once

#include <QList>
#include <QRect>

[[nodiscard]] QRect adjustedWindowGeometry(
    const QRect &restoredGeometry,
    const QList<QRect> &availableScreens,
    const QRect &fallbackScreen
);
