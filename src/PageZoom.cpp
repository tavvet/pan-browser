#include "PageZoom.h"

#include "PrivateData.h"

#include <QSettings>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr std::array zoomLevels = {
    0.25,
    0.33,
    0.50,
    0.67,
    0.75,
    0.80,
    0.90,
    1.00,
    1.10,
    1.25,
    1.50,
    1.75,
    2.00,
    2.50,
    3.00,
    4.00,
    5.00,
};

constexpr double comparisonEpsilon = 0.0001;
constexpr auto settingsGroup = "PageZoom";

bool approximatelyEqual(double left, double right)
{
    return std::abs(left - right) < comparisonEpsilon;
}

} // namespace

QString pageZoomSiteKey(const QUrl &sourceUrl)
{
    const QString scheme = sourceUrl.scheme().toLower();
    const QString host = sourceUrl.host().toLower();
    if (!sourceUrl.isValid()
        || host.isEmpty()
        || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        return {};
    }

    QUrl origin;
    origin.setScheme(scheme);
    origin.setHost(host);
    const int port = sourceUrl.port(-1);
    const bool defaultPort = (scheme == QStringLiteral("http") && port == 80)
        || (scheme == QStringLiteral("https") && port == 443);
    if (port >= 0 && !defaultPort)
        origin.setPort(port);
    return QString::fromLatin1(QUrl::toPercentEncoding(origin.toString(QUrl::FullyEncoded)));
}

double normalizedPageZoomFactor(double factor)
{
    if (!std::isfinite(factor)
        || factor < minimumPageZoomFactor
        || factor > maximumPageZoomFactor) {
        return defaultPageZoomFactor;
    }
    const auto closest = std::min_element(zoomLevels.cbegin(), zoomLevels.cend(), [factor](
        double left,
        double right
    ) {
        return std::abs(left - factor) < std::abs(right - factor);
    });
    return *closest;
}

double nextPageZoomFactor(double currentFactor, bool zoomIn)
{
    const double current = normalizedPageZoomFactor(currentFactor);
    if (zoomIn) {
        const auto next = std::find_if(zoomLevels.cbegin(), zoomLevels.cend(), [current](
            double factor
        ) {
            return factor > current + comparisonEpsilon;
        });
        return next == zoomLevels.cend() ? maximumPageZoomFactor : *next;
    }

    const auto previous = std::find_if(zoomLevels.crbegin(), zoomLevels.crend(), [current](
        double factor
    ) {
        return factor < current - comparisonEpsilon;
    });
    return previous == zoomLevels.crend() ? minimumPageZoomFactor : *previous;
}

int pageZoomPercentage(double factor)
{
    return qRound(normalizedPageZoomFactor(factor) * 100.0);
}

double storedPageZoomFactor(QSettings &settings, const QUrl &url)
{
    const QString key = pageZoomSiteKey(url);
    if (key.isEmpty())
        return defaultPageZoomFactor;
    settings.beginGroup(QString::fromLatin1(settingsGroup));
    bool valid = false;
    const double stored = settings.value(key, defaultPageZoomFactor).toDouble(&valid);
    settings.endGroup();
    return valid ? normalizedPageZoomFactor(stored) : defaultPageZoomFactor;
}

bool persistPageZoomFactor(QSettings &settings, const QUrl &url, double factor)
{
    const QString key = pageZoomSiteKey(url);
    if (key.isEmpty())
        return true;
    const double normalized = normalizedPageZoomFactor(factor);
    settings.beginGroup(QString::fromLatin1(settingsGroup));
    if (approximatelyEqual(normalized, defaultPageZoomFactor))
        settings.remove(key);
    else
        settings.setValue(key, normalized);
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError
        && PrivateData::restrictFile(settings.fileName());
}

QList<QKeySequence> pageZoomInShortcuts()
{
    return {
        QKeySequence(Qt::CTRL | Qt::Key_Plus),
        QKeySequence(Qt::CTRL | Qt::Key_Equal),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Equal),
    };
}

QList<QKeySequence> pageZoomOutShortcuts()
{
    return {QKeySequence(Qt::CTRL | Qt::Key_Minus)};
}

QList<QKeySequence> pageZoomResetShortcuts()
{
    return {QKeySequence(Qt::CTRL | Qt::Key_0)};
}

int takePageZoomSteps(int delta, int threshold, int &remainder)
{
    if (threshold <= 0 || delta == 0)
        return 0;
    if ((remainder > 0 && delta < 0) || (remainder < 0 && delta > 0))
        remainder = 0;
    remainder += delta;
    const int steps = remainder / threshold;
    remainder -= steps * threshold;
    return steps;
}
