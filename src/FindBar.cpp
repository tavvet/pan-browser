#include "FindBar.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("findBarContent"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 10, 6);
    layout->setSpacing(7);

    auto *label = new QLabel(tr("Find in page"), this);
    label->setObjectName(QStringLiteral("findLabel"));
    layout->addWidget(label);

    m_query = new QLineEdit(this);
    m_query->setObjectName(QStringLiteral("findInput"));
    m_query->setPlaceholderText(tr("Search this page"));
    m_query->setClearButtonEnabled(true);
    m_query->installEventFilter(this);
    layout->addWidget(m_query, 1);

    m_result = new QLabel(QStringLiteral("—"), this);
    m_result->setObjectName(QStringLiteral("findResult"));
    m_result->setMinimumWidth(74);
    m_result->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_result);

    m_previous = new QToolButton(this);
    m_previous->setObjectName(QStringLiteral("findPrevious"));
    m_previous->setIcon(QIcon(QStringLiteral(":/assets/icons/chevron-up.svg")));
    m_previous->setToolTip(tr("Previous match (⇧⌘G)"));
    m_previous->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_previous);

    m_next = new QToolButton(this);
    m_next->setObjectName(QStringLiteral("findNext"));
    m_next->setIcon(QIcon(QStringLiteral(":/assets/icons/chevron-down.svg")));
    m_next->setToolTip(tr("Next match (⌘G)"));
    m_next->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_next);

    auto *close = new QToolButton(this);
    close->setObjectName(QStringLiteral("findClose"));
    close->setIcon(QIcon(QStringLiteral(":/assets/icons/x.svg")));
    close->setToolTip(tr("Close find bar (Esc)"));
    close->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(close);

    updateNavigationButtons(false);
    connect(m_query, &QLineEdit::textChanged, this, [this](const QString &text) {
        clearResults();
        emit queryChanged(text);
    });
    connect(m_previous, &QToolButton::clicked, this, [this] {
        emit navigationRequested(true);
    });
    connect(m_next, &QToolButton::clicked, this, [this] {
        emit navigationRequested(false);
    });
    connect(close, &QToolButton::clicked, this, &FindBar::closeRequested);
}

QString FindBar::query() const
{
    return m_query->text();
}

void FindBar::focusInput(const QString &initialText)
{
    if (!initialText.isEmpty() && initialText != m_query->text())
        m_query->setText(initialText);
    m_query->setFocus(Qt::ShortcutFocusReason);
    m_query->selectAll();
}

void FindBar::setSearching()
{
    m_result->setText(QStringLiteral("…"));
    updateNavigationButtons(false);
}

void FindBar::setResults(int activeMatch, int numberOfMatches)
{
    if (numberOfMatches <= 0) {
        m_result->setText(query().isEmpty() ? QStringLiteral("—")
                                            : tr("No matches"));
        updateNavigationButtons(false);
        return;
    }
    m_result->setText(tr("%1 of %2").arg(activeMatch).arg(numberOfMatches));
    updateNavigationButtons(true);
}

void FindBar::clearResults()
{
    m_result->setText(QStringLiteral("—"));
    updateNavigationButtons(false);
}

bool FindBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_query && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            emit closeRequested();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            emit navigationRequested(keyEvent->modifiers().testFlag(Qt::ShiftModifier));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FindBar::updateNavigationButtons(bool enabled)
{
    m_previous->setEnabled(enabled);
    m_next->setEnabled(enabled);
}
