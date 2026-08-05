#include "AddressLineEdit.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPalette>

AddressLineEdit::AddressLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
}

void AddressLineEdit::setGhostCompletion(const QString &completionText, const QUrl &url)
{
    const QString currentText = text();
    if (currentText.isEmpty()
        || completionText.size() <= currentText.size()
        || !completionText.startsWith(currentText, Qt::CaseInsensitive)
        || !url.isValid()) {
        clearGhostCompletion();
        return;
    }

    m_ghostText = completionText;
    m_ghostSuffix = completionText.sliced(currentText.size());
    m_ghostUrl = url;
    update();
}

void AddressLineEdit::clearGhostCompletion()
{
    if (m_ghostText.isEmpty() && m_ghostUrl.isEmpty())
        return;
    m_ghostText.clear();
    m_ghostSuffix.clear();
    m_ghostUrl.clear();
    update();
}

bool AddressLineEdit::hasGhostCompletion() const
{
    return !m_ghostSuffix.isEmpty() && m_ghostUrl.isValid();
}

QUrl AddressLineEdit::ghostCompletionUrl() const
{
    return m_ghostUrl;
}

bool AddressLineEdit::acceptGhostCompletion()
{
    if (!hasGhostCompletion())
        return false;
    const QString completion = m_ghostText;
    clearGhostCompletion();
    setText(completion);
    setCursorPosition(completion.size());
    return true;
}

void AddressLineEdit::paintEvent(QPaintEvent *event)
{
    QLineEdit::paintEvent(event);
    if (!hasGhostCompletion()
        || !hasFocus()
        || hasSelectedText()
        || cursorPosition() != text().size()) {
        return;
    }

    QPainter painter(this);
    const QFontMetrics metrics(font());
    const QRect caret = cursorRect();
    const int x = caret.left() + 1;
    const int baseline = (height() - metrics.height()) / 2 + metrics.ascent();
    const QRect clip = rect().adjusted(x, 1, -27, -1);
    if (clip.width() <= 0)
        return;
    painter.setClipRect(clip);
    QColor color = palette().color(QPalette::PlaceholderText);
    color.setAlpha(190);
    painter.setPen(color);
    painter.drawText(QPoint(x, baseline), m_ghostSuffix);
}
