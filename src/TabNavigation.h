#pragma once

#include <QKeySequence>
#include <QList>

namespace TabNavigation {

constexpr int numberedShortcutCount = 9;

[[nodiscard]] QList<QKeySequence> newTabShortcuts();
[[nodiscard]] QList<QKeySequence> closeTabShortcuts();
[[nodiscard]] QList<QKeySequence> reopenClosedTabShortcuts();
[[nodiscard]] QList<QKeySequence> nextTabShortcuts();
[[nodiscard]] QList<QKeySequence> previousTabShortcuts();
[[nodiscard]] QKeySequence numberedTabShortcut(int position);

[[nodiscard]] int adjacentTabIndex(int currentIndex, int tabCount, int offset);
[[nodiscard]] int numberedTabIndex(int position, int tabCount);

} // namespace TabNavigation
