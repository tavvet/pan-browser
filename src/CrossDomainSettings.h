#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QUrl;

enum class CrossDomainRuleDecision {
    Allow,
    Block,
};

enum class CrossDomainEvaluation {
    Allow,
    Block,
    Ask,
};

struct CrossDomainRule {
    QString sourceSite;
    QString targetHost;
    CrossDomainRuleDecision decision = CrossDomainRuleDecision::Block;

    friend bool operator==(const CrossDomainRule &, const CrossDomainRule &) = default;
};

class CrossDomainSettings {
public:
    static CrossDomainSettings defaults();

    bool load(const QString &path, QString *error = nullptr);
    bool loadRecoveringBackup(
        const QString &path,
        bool *recoveredFromBackup = nullptr,
        QString *error = nullptr
    );
    bool save(const QString &path, QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;

    [[nodiscard]] bool enabled() const;
    void setEnabled(bool enabled);
    [[nodiscard]] QStringList globalAllowPatterns() const;
    void setGlobalAllowPatterns(const QStringList &patterns);
    [[nodiscard]] QStringList globalBlockPatterns() const;
    void setGlobalBlockPatterns(const QStringList &patterns);
    [[nodiscard]] QStringList enabledPresetIds() const;
    void setEnabledPresetIds(const QStringList &ids);
    [[nodiscard]] QList<CrossDomainRule> rules() const;
    void setRules(const QList<CrossDomainRule> &rules);
    void setRule(
        const QString &sourceSite,
        const QString &targetHost,
        CrossDomainRuleDecision decision
    );

    [[nodiscard]] CrossDomainEvaluation evaluate(
        const QUrl &sourceUrl,
        const QUrl &targetUrl
    ) const;
    [[nodiscard]] CrossDomainEvaluation evaluate(
        const QString &sourceSite,
        const QString &targetHost
    ) const;
    [[nodiscard]] CrossDomainEvaluation evaluateUserPolicy(
        const QUrl &sourceUrl,
        const QUrl &targetUrl
    ) const;
    [[nodiscard]] CrossDomainEvaluation evaluateUserPolicy(
        const QString &sourceSite,
        const QString &targetHost
    ) const;
    [[nodiscard]] CrossDomainEvaluation evaluatePresetPolicy(
        const QString &targetHost
    ) const;

    friend bool operator==(const CrossDomainSettings &, const CrossDomainSettings &) = default;

private:
    bool m_enabled = false;
    QStringList m_globalAllowPatterns;
    QStringList m_globalBlockPatterns;
    QStringList m_enabledPresetIds;
    QList<CrossDomainRule> m_rules;
};

[[nodiscard]] QString crossDomainDecisionDisplayName(CrossDomainRuleDecision decision);
