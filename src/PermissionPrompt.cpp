#include "PermissionPrompt.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

PermissionPrompt::PermissionPrompt(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("permissionPrompt"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(11);

    auto *icon = new QLabel(this);
    icon->setPixmap(
        QIcon(QStringLiteral(":/assets/icons/shield-check.svg")).pixmap(22, 22)
    );
    layout->addWidget(icon);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("permissionTitle"));
    textLayout->addWidget(m_title);
    m_description = new QLabel(this);
    m_description->setObjectName(QStringLiteral("permissionDescription"));
    m_description->setWordWrap(true);
    textLayout->addWidget(m_description);
    layout->addLayout(textLayout, 1);

    auto *block = new QPushButton(tr("Block"), this);
    block->setObjectName(QStringLiteral("permissionBlock"));
    auto *allow = new QPushButton(tr("Allow once"), this);
    allow->setObjectName(QStringLiteral("permissionAllow"));
    allow->setDefault(true);
    layout->addWidget(block);
    layout->addWidget(allow);

    connect(block, &QPushButton::clicked, this, &PermissionPrompt::blockRequested);
    connect(allow, &QPushButton::clicked, this, &PermissionPrompt::allowRequested);
}

void PermissionPrompt::showRequest(
    const QString &origin,
    const QString &title,
    const QString &description
)
{
    m_title->setText(title);
    m_description->setText(QStringLiteral("%1 — %2").arg(origin, description));
    show();
    if (parentWidget())
        parentWidget()->show();
}

void PermissionPrompt::hideRequest()
{
    hide();
    if (parentWidget())
        parentWidget()->hide();
}
