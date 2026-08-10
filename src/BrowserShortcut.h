#pragma once

#include <QList>
#include <QKeySequence>

class QKeyEvent;

namespace BrowserShortcut {

[[nodiscard]] bool matches(
    const QKeyEvent &event,
    const QList<QKeySequence> &shortcuts
);

} // namespace BrowserShortcut
