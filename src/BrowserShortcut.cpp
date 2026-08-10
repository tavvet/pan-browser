#include "BrowserShortcut.h"

#include <QEvent>
#include <QKeyEvent>

namespace BrowserShortcut {

bool matches(const QKeyEvent &event, const QList<QKeySequence> &shortcuts)
{
    if (event.type() != QEvent::KeyPress)
        return false;

    const QKeyCombination pressed = event.keyCombination();
    for (const QKeySequence &shortcut : shortcuts) {
        if (shortcut.count() == 1 && shortcut[0] == pressed)
            return true;
    }
    return false;
}

} // namespace BrowserShortcut
