#include "CrossDomainPrompt.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

CrossDomainPrompt::CrossDomainPrompt(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("crossDomainPrompt"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(11);

    auto *icon = new QLabel(this);
    icon->setPixmap(
        QIcon(QStringLiteral(":/assets/icons/shield-ban.svg")).pixmap(22, 22)
    );
    layout->addWidget(icon);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("crossDomainTitle"));
    textLayout->addWidget(m_title);
    m_description = new QLabel(this);
    m_description->setObjectName(QStringLiteral("crossDomainDescription"));
    m_description->setWordWrap(true);
    textLayout->addWidget(m_description);
    layout->addLayout(textLayout, 1);

    auto *blockSession = new QPushButton(tr("Block this session"), this);
    auto *allowSession = new QPushButton(tr("Allow this session"), this);
    auto *blockAlways = new QPushButton(tr("Always block for this site"), this);
    auto *allowAlways = new QPushButton(tr("Always allow for this site"), this);
    allowSession->setObjectName(QStringLiteral("crossDomainAllow"));
    allowAlways->setObjectName(QStringLiteral("crossDomainAllow"));
    layout->addWidget(blockSession);
    layout->addWidget(allowSession);
    layout->addWidget(blockAlways);
    layout->addWidget(allowAlways);

    connect(blockSession, &QPushButton::clicked, this, [this] {
        emit decisionRequested(CrossDomainRuleDecision::Block, false);
    });
    connect(allowSession, &QPushButton::clicked, this, [this] {
        emit decisionRequested(CrossDomainRuleDecision::Allow, false);
    });
    connect(blockAlways, &QPushButton::clicked, this, [this] {
        emit decisionRequested(CrossDomainRuleDecision::Block, true);
    });
    connect(allowAlways, &QPushButton::clicked, this, [this] {
        emit decisionRequested(CrossDomainRuleDecision::Allow, true);
    });
}

void CrossDomainPrompt::showRequest(
    const QString &sourceSite,
    const QString &targetHost,
    const QString &resourceType
)
{
    m_title->setText(tr("%1 wants to connect to %2").arg(sourceSite, targetHost));
    m_description->setText(
        tr("Blocked %1. Allowing the connection reloads the page.").arg(resourceType)
    );
    show();
    if (parentWidget())
        parentWidget()->show();
}

void CrossDomainPrompt::hideRequest()
{
    hide();
    if (parentWidget())
        parentWidget()->hide();
}
