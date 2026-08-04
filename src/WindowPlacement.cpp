#include "WindowPlacement.h"

#include <QtGlobal>

namespace {

constexpr int minimumVisibleTitleWidth = 160;
constexpr int minimumVisibleTitleHeight = 24;
constexpr int titleAreaHeight = 56;

QRect fittedGeometry(const QRect &geometry, const QRect &screen, bool center)
{
    if (!screen.isValid())
        return geometry;

    const QSize size(
        qMin(qMax(geometry.width(), 1), screen.width()),
        qMin(qMax(geometry.height(), 1), screen.height())
    );
    if (center) {
        QRect centered(QPoint(), size);
        centered.moveCenter(screen.center());
        return centered;
    }

    const int maximumX = screen.right() - size.width() + 1;
    const int maximumY = screen.bottom() - size.height() + 1;
    return QRect(
        QPoint(
            qBound(screen.left(), geometry.x(), maximumX),
            qBound(screen.top(), geometry.y(), maximumY)
        ),
        size
    );
}

} // namespace

QRect adjustedWindowGeometry(
    const QRect &restoredGeometry,
    const QList<QRect> &availableScreens,
    const QRect &fallbackScreen
)
{
    if (!restoredGeometry.isValid() || availableScreens.isEmpty())
        return restoredGeometry;

    const QRect titleArea(
        restoredGeometry.x(),
        restoredGeometry.y(),
        restoredGeometry.width(),
        qMin(titleAreaHeight, restoredGeometry.height())
    );

    QRect bestScreen;
    qint64 bestIntersectionArea = 0;
    for (const QRect &screen : availableScreens) {
        if (!screen.isValid())
            continue;

        const QRect titleIntersection = titleArea.intersected(screen);
        if (titleIntersection.width() >= minimumVisibleTitleWidth
            && titleIntersection.height() >= minimumVisibleTitleHeight) {
            if (restoredGeometry.width() <= screen.width()
                && restoredGeometry.height() <= screen.height()) {
                return restoredGeometry;
            }
            return fittedGeometry(restoredGeometry, screen, false);
        }

        const QRect intersection = restoredGeometry.intersected(screen);
        const qint64 area = static_cast<qint64>(intersection.width()) * intersection.height();
        if (area > bestIntersectionArea) {
            bestIntersectionArea = area;
            bestScreen = screen;
        }
    }

    QRect target = bestIntersectionArea > 0 ? bestScreen : fallbackScreen;
    if (!target.isValid())
        target = availableScreens.first();
    return fittedGeometry(restoredGeometry, target, true);
}
