#include "TabNavigation.h"

#include <QtGlobal>

namespace {

void appendUnique(QList<QKeySequence> &shortcuts, const QKeySequence &shortcut)
{
    if (!shortcut.isEmpty() && !shortcuts.contains(shortcut))
        shortcuts.append(shortcut);
}

QList<QKeySequence> standardKeyBindings(QKeySequence::StandardKey key)
{
    QList<QKeySequence> shortcuts = QKeySequence::keyBindings(key);
    if (shortcuts.isEmpty())
        appendUnique(shortcuts, QKeySequence(key));
    return shortcuts;
}

} // namespace

namespace TabNavigation {

QList<QKeySequence> newTabShortcuts()
{
    return {QKeySequence(Qt::CTRL | Qt::Key_T)};
}

QList<QKeySequence> closeTabShortcuts()
{
    return standardKeyBindings(QKeySequence::Close);
}

QList<QKeySequence> reopenClosedTabShortcuts()
{
    return {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T)};
}

QList<QKeySequence> nextTabShortcuts()
{
    QList<QKeySequence> shortcuts;
#if defined(Q_OS_MACOS)
    appendUnique(shortcuts, QKeySequence(Qt::META | Qt::Key_Tab));
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right));
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
#else
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::Key_Tab));
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::Key_PageDown));
#endif
    return shortcuts;
}

QList<QKeySequence> previousTabShortcuts()
{
    QList<QKeySequence> shortcuts;
#if defined(Q_OS_MACOS)
    appendUnique(shortcuts, QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Tab));
    appendUnique(shortcuts, QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Backtab));
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft));
#else
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab));
    appendUnique(shortcuts, QKeySequence(Qt::CTRL | Qt::Key_PageUp));
#endif
    return shortcuts;
}

QKeySequence numberedTabShortcut(int position)
{
    if (position < 1 || position > numberedShortcutCount)
        return {};
    return QKeySequence(Qt::CTRL | (Qt::Key_0 + position));
}

int adjacentTabIndex(int currentIndex, int tabCount, int offset)
{
    if (tabCount <= 0 || currentIndex < 0 || currentIndex >= tabCount)
        return -1;
    const int normalizedOffset = offset % tabCount;
    return (currentIndex + normalizedOffset + tabCount) % tabCount;
}

int numberedTabIndex(int position, int tabCount)
{
    if (tabCount <= 0 || position < 1 || position > numberedShortcutCount)
        return -1;
    if (position == numberedShortcutCount)
        return tabCount - 1;
    const int index = position - 1;
    return index < tabCount ? index : -1;
}

} // namespace TabNavigation
