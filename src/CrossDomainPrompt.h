#pragma once

#include "CrossDomainSettings.h"

#include <QWidget>

class QLabel;

class CrossDomainPrompt final : public QWidget {
    Q_OBJECT

public:
    explicit CrossDomainPrompt(QWidget *parent = nullptr);

    void showRequest(
        const QString &sourceSite,
        const QString &targetHost,
        const QString &resourceType
    );
    void hideRequest();

signals:
    void decisionRequested(CrossDomainRuleDecision decision, bool persist);

private:
    QLabel *m_title = nullptr;
    QLabel *m_description = nullptr;
};
