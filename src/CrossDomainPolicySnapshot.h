#pragma once

#include "CrossDomainSettings.h"
#include "DomainPatternMatcher.h"

#include <QHash>
#include <QString>
#include <QUrl>

struct CrossDomainRequestPolicyResult {
    CrossDomainEvaluation decision = CrossDomainEvaluation::Allow;
    QString sourceSite;
    QString targetHost;
};

class CrossDomainPolicySnapshot {
public:
    CrossDomainPolicySnapshot() = default;
    explicit CrossDomainPolicySnapshot(const CrossDomainSettings &settings);

    [[nodiscard]] bool enabled() const;
    [[nodiscard]] CrossDomainRequestPolicyResult evaluateUserPolicy(
        const QUrl &sourceUrl,
        const QUrl &targetUrl
    ) const;
    [[nodiscard]] CrossDomainEvaluation evaluatePresetPolicy(
        const QString &targetHost
    ) const;

private:
    bool m_enabled = false;
    DomainPatternMatcher m_globalBlockMatcher;
    DomainPatternMatcher m_globalAllowMatcher;
    DomainPatternMatcher m_presetBlockMatcher;
    DomainPatternMatcher m_presetAllowMatcher;
    QHash<QString, QHash<QString, CrossDomainRuleDecision>> m_rulesBySource;
};
