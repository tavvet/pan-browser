#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>
#include <QUrl>

class QSettings;

inline constexpr double defaultPageZoomFactor = 1.0;
inline constexpr double minimumPageZoomFactor = 0.25;
inline constexpr double maximumPageZoomFactor = 5.0;

[[nodiscard]] QString pageZoomSiteKey(const QUrl &url);
[[nodiscard]] double normalizedPageZoomFactor(double factor);
[[nodiscard]] double nextPageZoomFactor(double currentFactor, bool zoomIn);
[[nodiscard]] int pageZoomPercentage(double factor);
[[nodiscard]] double storedPageZoomFactor(QSettings &settings, const QUrl &url);
bool persistPageZoomFactor(QSettings &settings, const QUrl &url, double factor);

[[nodiscard]] QList<QKeySequence> pageZoomInShortcuts();
[[nodiscard]] QList<QKeySequence> pageZoomOutShortcuts();
[[nodiscard]] QList<QKeySequence> pageZoomResetShortcuts();

int takePageZoomSteps(int delta, int threshold, int &remainder);
