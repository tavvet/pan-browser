#include "CrossDomainPolicySnapshot.h"

#include "CrossDomainPresetCatalog.h"
#include "SiteDomain.h"

namespace {

bool isWebRequestScheme(const QString &scheme)
{
    const QString normalized = scheme.toLower();
    return normalized == QStringLiteral("http")
        || normalized == QStringLiteral("https")
        || normalized == QStringLiteral("ws")
        || normalized == QStringLiteral("wss");
}

} // namespace

CrossDomainPolicySnapshot::CrossDomainPolicySnapshot(
    const CrossDomainSettings &settings
)
    : m_enabled(settings.enabled())
{
    if (!m_enabled)
        return;

    m_globalBlockMatcher.setPatterns(settings.globalBlockPatterns());
    m_globalAllowMatcher.setPatterns(settings.globalAllowPatterns());
    for (const CrossDomainRule &rule : settings.rules())
        m_rulesBySource[rule.sourceSite].insert(rule.targetHost, rule.decision);

    const QStringList enabledPresetIds = settings.enabledPresetIds();
    if (enabledPresetIds.isEmpty())
        return;
    QStringList presetBlockPatterns;
    QStringList presetAllowPatterns;
    const CrossDomainPresetCatalog &catalog = CrossDomainPresetCatalog::bundled();
    for (const QString &id : enabledPresetIds) {
        const CrossDomainPreset *preset = catalog.preset(id);
        if (!preset)
            continue;
        if (preset->decision == CrossDomainPresetDecision::Block)
            presetBlockPatterns.append(preset->patterns);
        else
            presetAllowPatterns.append(preset->patterns);
    }
    m_presetBlockMatcher.setPatterns(presetBlockPatterns);
    m_presetAllowMatcher.setPatterns(presetAllowPatterns);
}

bool CrossDomainPolicySnapshot::enabled() const
{
    return m_enabled;
}

CrossDomainRequestPolicyResult CrossDomainPolicySnapshot::evaluateUserPolicy(
    const QUrl &sourceUrl,
    const QUrl &targetUrl
) const
{
    CrossDomainRequestPolicyResult result;
    if (!m_enabled || !isWebRequestScheme(targetUrl.scheme()))
        return result;
    if (sourceUrl.scheme() != QStringLiteral("http")
        && sourceUrl.scheme() != QStringLiteral("https")) {
        result.decision = CrossDomainEvaluation::Block;
        return result;
    }

    result.sourceSite = SiteDomain::siteForUrl(sourceUrl);
    result.targetHost = SiteDomain::normalizeHost(targetUrl.host());
    const QString targetSite = SiteDomain::registrableDomain(result.targetHost);
    if (result.sourceSite.isEmpty()) {
        result.decision = CrossDomainEvaluation::Block;
        return result;
    }
    if (result.targetHost.isEmpty()
        || targetSite.isEmpty()
        || result.sourceSite == targetSite) {
        return result;
    }

    if (m_globalBlockMatcher.matches(result.targetHost)) {
        result.decision = CrossDomainEvaluation::Block;
        return result;
    }
    const auto sourceRules = m_rulesBySource.constFind(result.sourceSite);
    if (sourceRules != m_rulesBySource.cend()) {
        const auto rule = sourceRules->constFind(result.targetHost);
        if (rule != sourceRules->cend()) {
            result.decision = *rule == CrossDomainRuleDecision::Allow
                ? CrossDomainEvaluation::Allow
                : CrossDomainEvaluation::Block;
            return result;
        }
    }
    if (m_globalAllowMatcher.matches(result.targetHost)) {
        result.decision = CrossDomainEvaluation::Allow;
        return result;
    }
    result.decision = CrossDomainEvaluation::Ask;
    return result;
}

CrossDomainEvaluation CrossDomainPolicySnapshot::evaluatePresetPolicy(
    const QString &targetHost
) const
{
    if (!m_enabled)
        return CrossDomainEvaluation::Allow;
    if (m_presetBlockMatcher.matches(targetHost))
        return CrossDomainEvaluation::Block;
    if (m_presetAllowMatcher.matches(targetHost))
        return CrossDomainEvaluation::Allow;
    return CrossDomainEvaluation::Ask;
}
