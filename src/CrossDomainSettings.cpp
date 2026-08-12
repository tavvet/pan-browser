#include "CrossDomainSettings.h"

#include "CrossDomainPresetCatalog.h"
#include "PrivateData.h"
#include "SiteDomain.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QRegularExpression>
#include <QUrl>

#include <utility>

namespace {

constexpr qsizetype maximumSettingsFileSize = 256 * 1024;
constexpr qsizetype maximumRules = 4096;
constexpr qsizetype maximumPatterns = 1024;
constexpr qsizetype maximumEnabledPresets = 32;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString text(const char *source)
{
    return QCoreApplication::translate("CrossDomainSettings", source);
}

QString decisionKey(CrossDomainRuleDecision decision)
{
    return decision == CrossDomainRuleDecision::Allow
        ? QStringLiteral("allow")
        : QStringLiteral("block");
}

bool parseDecision(const QString &value, CrossDomainRuleDecision *decision)
{
    if (value == QStringLiteral("allow")) {
        *decision = CrossDomainRuleDecision::Allow;
        return true;
    }
    if (value == QStringLiteral("block")) {
        *decision = CrossDomainRuleDecision::Block;
        return true;
    }
    return false;
}

bool isWebRequestScheme(const QString &scheme)
{
    const QString normalized = scheme.toLower();
    return normalized == QStringLiteral("http")
        || normalized == QStringLiteral("https")
        || normalized == QStringLiteral("ws")
        || normalized == QStringLiteral("wss");
}

QString normalizedPattern(const QString &pattern)
{
    return SiteDomain::normalizeHostPattern(pattern);
}

QString normalizedExactHost(const QString &host)
{
    const QString trimmed = host.trimmed();
    if (trimmed.startsWith(QStringLiteral("*.")))
        return {};
    return SiteDomain::normalizeHostPattern(trimmed);
}

bool patternsOverlap(const QString &left, const QString &right)
{
    return SiteDomain::hostMatchesPattern(left, right)
        || SiteDomain::hostMatchesPattern(right, left);
}

CrossDomainRule normalizedRule(const CrossDomainRule &rule)
{
    const QString sourceHost = SiteDomain::normalizeHostPattern(rule.sourceSite);
    return {
        SiteDomain::registrableDomain(sourceHost),
        normalizedExactHost(rule.targetHost),
        rule.decision,
    };
}

QByteArray serializedSettings(const CrossDomainSettings &settings)
{
    QJsonArray patterns;
    for (const QString &pattern : settings.globalAllowPatterns())
        patterns.append(normalizedPattern(pattern));

    QJsonArray blockedPatterns;
    for (const QString &pattern : settings.globalBlockPatterns())
        blockedPatterns.append(normalizedPattern(pattern));

    QJsonArray rules;
    for (const CrossDomainRule &sourceRule : settings.rules()) {
        const CrossDomainRule rule = normalizedRule(sourceRule);
        QJsonObject object;
        object.insert(QStringLiteral("sourceSite"), rule.sourceSite);
        object.insert(QStringLiteral("targetHost"), rule.targetHost);
        object.insert(QStringLiteral("decision"), decisionKey(rule.decision));
        rules.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("enabled"), settings.enabled());
    root.insert(QStringLiteral("globalAllowPatterns"), patterns);
    root.insert(QStringLiteral("globalBlockPatterns"), blockedPatterns);
    root.insert(
        QStringLiteral("enabledPresets"),
        QJsonArray::fromStringList(settings.enabledPresetIds())
    );
    root.insert(QStringLiteral("rules"), rules);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace

CrossDomainSettings CrossDomainSettings::defaults()
{
    return {};
}

bool CrossDomainSettings::load(const QString &path, QString *error)
{
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Cannot open %1: %2"))
                .arg(path, file.errorString())
        );
    }
    if (file.size() > maximumSettingsFileSize) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Site connection settings file is too large"))
        );
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Invalid site connection settings JSON: %1"))
                .arg(parseError.errorString())
        );
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Unsupported site connection settings version"))
        );
    }
    if (!root.value(QStringLiteral("enabled")).isBool()
        || !root.value(QStringLiteral("globalAllowPatterns")).isArray()
        || (root.contains(QStringLiteral("globalBlockPatterns"))
            && !root.value(QStringLiteral("globalBlockPatterns")).isArray())
        || (root.contains(QStringLiteral("enabledPresets"))
            && !root.value(QStringLiteral("enabledPresets")).isArray())
        || !root.value(QStringLiteral("rules")).isArray()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Site connection settings have invalid field types"))
        );
    }

    CrossDomainSettings loaded;
    loaded.m_enabled = root.value(QStringLiteral("enabled")).toBool();
    const QJsonArray patterns = root.value(QStringLiteral("globalAllowPatterns")).toArray();
    if (patterns.size() > maximumPatterns) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Too many global site exceptions"))
        );
    }
    for (const QJsonValue &value : patterns) {
        if (!value.isString()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Global site exceptions must be strings"))
            );
        }
        loaded.m_globalAllowPatterns.append(value.toString());
    }

    const QJsonArray blockedPatterns = root.value(
        QStringLiteral("globalBlockPatterns")
    ).toArray();
    if (blockedPatterns.size() > maximumPatterns) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Too many globally blocked hosts"))
        );
    }
    for (const QJsonValue &value : blockedPatterns) {
        if (!value.isString()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Globally blocked hosts must be strings"))
            );
        }
        loaded.m_globalBlockPatterns.append(value.toString());
    }

    const QJsonArray enabledPresets = root.value(QStringLiteral("enabledPresets")).toArray();
    if (enabledPresets.size() > maximumEnabledPresets) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Too many enabled PanBrowser presets"))
        );
    }
    for (const QJsonValue &value : enabledPresets) {
        if (!value.isString()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Enabled PanBrowser preset IDs must be strings"))
            );
        }
        loaded.m_enabledPresetIds.append(value.toString());
    }

    const QJsonArray rules = root.value(QStringLiteral("rules")).toArray();
    if (rules.size() > maximumRules) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Too many site connection rules"))
        );
    }
    for (const QJsonValue &value : rules) {
        if (!value.isObject()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Site connection rules must be objects"))
            );
        }
        const QJsonObject object = value.toObject();
        const bool hasTargetHost = object.contains(QStringLiteral("targetHost"));
        const bool hasLegacyTargetPattern = object.contains(
            QStringLiteral("targetPattern")
        );
        if (hasTargetHost == hasLegacyTargetPattern) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "A site connection rule has invalid fields"))
            );
        }
        const QJsonValue targetHost = hasTargetHost
            ? object.value(QStringLiteral("targetHost"))
            : object.value(QStringLiteral("targetPattern"));
        if (!object.value(QStringLiteral("sourceSite")).isString()
            || !targetHost.isString()
            || !object.value(QStringLiteral("decision")).isString()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "A site connection rule has invalid fields"))
            );
        }
        CrossDomainRule rule;
        rule.sourceSite = object.value(QStringLiteral("sourceSite")).toString();
        rule.targetHost = targetHost.toString();
        if (!parseDecision(object.value(QStringLiteral("decision")).toString(), &rule.decision)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Unknown site connection rule decision"))
            );
        }
        loaded.m_rules.append(rule);
    }

    if (!loaded.validate(error))
        return false;
    loaded.setGlobalAllowPatterns(loaded.m_globalAllowPatterns);
    loaded.setGlobalBlockPatterns(loaded.m_globalBlockPatterns);
    loaded.setEnabledPresetIds(loaded.m_enabledPresetIds);
    loaded.setRules(loaded.m_rules);
    *this = loaded;
    return true;
}

bool CrossDomainSettings::loadRecoveringBackup(
    const QString &path,
    bool *recoveredFromBackup,
    QString *error
)
{
    if (recoveredFromBackup)
        *recoveredFromBackup = false;
    QString primaryError;
    if (load(path, &primaryError)) {
        if (error)
            error->clear();
        return true;
    }

    CrossDomainSettings backup;
    QString backupError;
    const QString backupPath = path + QStringLiteral(".backup");
    if (QFile::exists(backupPath) && backup.load(backupPath, &backupError)) {
        *this = backup;
        if (recoveredFromBackup)
            *recoveredFromBackup = true;
        if (error)
            *error = primaryError;
        return true;
    }

    if (error) {
        *error = primaryError;
        if (!backupError.isEmpty())
            *error += QStringLiteral("\n") + backupError;
    }
    return false;
}

bool CrossDomainSettings::save(const QString &path, QString *error) const
{
    if (error)
        error->clear();
    if (!validate(error))
        return false;

    const QByteArray contents = serializedSettings(*this);
    if (contents.size() > maximumSettingsFileSize) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Site connection settings file is too large"))
        );
    }

    CrossDomainSettings previousSettings;
    if (QFile::exists(path) && previousSettings.load(path)) {
        const QString backupPath = path + QStringLiteral(".backup");
        const QByteArray previousContents = serializedSettings(previousSettings);

        QSaveFile backupFile(backupPath);
        if (!backupFile.open(QIODevice::WriteOnly)
            || backupFile.write(previousContents) != previousContents.size()
            || !backupFile.commit()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Cannot create backup %1"))
                    .arg(backupPath)
            );
        }
        if (!PrivateData::restrictFile(backupPath, error))
            return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Cannot write %1: %2"))
                .arg(path, file.errorString())
        );
    }
    if (file.write(contents) != contents.size() || !file.commit()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Cannot commit %1: %2"))
                .arg(path, file.errorString())
        );
    }
    return PrivateData::restrictFile(path, error);
}

bool CrossDomainSettings::validate(QString *error) const
{
    if (m_globalAllowPatterns.size() > maximumPatterns) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Too many global site exceptions"))
        );
    }
    if (m_rules.size() > maximumRules) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Too many site connection rules"))
        );
    }
    if (m_globalBlockPatterns.size() > maximumPatterns) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Too many globally blocked hosts"))
        );
    }
    if (m_enabledPresetIds.size() > maximumEnabledPresets) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Too many enabled PanBrowser presets"))
        );
    }

    static const QRegularExpression validPresetId(
        QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$")
    );
    QSet<QString> seenPresetIds;
    for (const QString &id : m_enabledPresetIds) {
        if (!validPresetId.match(id).hasMatch() || seenPresetIds.contains(id)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "An enabled PanBrowser preset ID is invalid or duplicated"))
            );
        }
        seenPresetIds.insert(id);
    }

    QSet<QString> seenPatterns;
    for (const QString &pattern : m_globalAllowPatterns) {
        const QString normalized = normalizedPattern(pattern);
        if (normalized.isEmpty()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Enter valid hostnames in global site exceptions"))
            );
        }
        seenPatterns.insert(normalized);
    }

    for (const QString &pattern : m_globalBlockPatterns) {
        const QString normalized = normalizedPattern(pattern);
        if (normalized.isEmpty()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Enter valid hostnames in globally blocked hosts"))
            );
        }
        for (const QString &allowedPattern : std::as_const(seenPatterns)) {
            if (patternsOverlap(normalized, allowedPattern)) {
                return fail(
                    error,
                    text(QT_TRANSLATE_NOOP(
                        "CrossDomainSettings",
                        "A hostname cannot be both globally allowed and globally blocked: %1"
                    )).arg(normalized)
                );
            }
        }
    }

    QSet<QString> seenRules;
    for (const CrossDomainRule &sourceRule : m_rules) {
        const CrossDomainRule rule = normalizedRule(sourceRule);
        if (rule.sourceSite.isEmpty() || rule.targetHost.isEmpty()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "A site connection rule contains an invalid hostname"))
            );
        }
        if (rule.sourceSite == SiteDomain::registrableDomain(rule.targetHost)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Same-site connections do not need an exception"))
            );
        }
        const QString key = rule.sourceSite + QLatin1Char('\n') + rule.targetHost;
        if (seenRules.contains(key)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP("CrossDomainSettings", "Duplicate site connection rule"))
            );
        }
        seenRules.insert(key);
    }
    return true;
}

bool CrossDomainSettings::enabled() const
{
    return m_enabled;
}

void CrossDomainSettings::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

QStringList CrossDomainSettings::globalAllowPatterns() const
{
    return m_globalAllowPatterns;
}

void CrossDomainSettings::setGlobalAllowPatterns(const QStringList &patterns)
{
    const QStringList input = patterns;
    QSet<QString> seen;
    m_globalAllowPatterns.clear();
    for (const QString &pattern : input) {
        const QString normalized = normalizedPattern(pattern);
        const QString stored = normalized.isEmpty() ? pattern.trimmed() : normalized;
        if (!stored.isEmpty() && !seen.contains(stored)) {
            seen.insert(stored);
            m_globalAllowPatterns.append(stored);
        }
    }
    m_globalAllowPatterns.sort(Qt::CaseInsensitive);
}

QStringList CrossDomainSettings::globalBlockPatterns() const
{
    return m_globalBlockPatterns;
}

void CrossDomainSettings::setGlobalBlockPatterns(const QStringList &patterns)
{
    const QStringList input = patterns;
    QSet<QString> seen;
    m_globalBlockPatterns.clear();
    for (const QString &pattern : input) {
        const QString normalized = normalizedPattern(pattern);
        const QString stored = normalized.isEmpty() ? pattern.trimmed() : normalized;
        if (!stored.isEmpty() && !seen.contains(stored)) {
            seen.insert(stored);
            m_globalBlockPatterns.append(stored);
        }
    }
    m_globalBlockPatterns.sort(Qt::CaseInsensitive);
}

QStringList CrossDomainSettings::enabledPresetIds() const
{
    return m_enabledPresetIds;
}

void CrossDomainSettings::setEnabledPresetIds(const QStringList &ids)
{
    const QStringList input = ids;
    QSet<QString> seen;
    m_enabledPresetIds.clear();
    for (const QString &sourceId : input) {
        const QString id = sourceId.trimmed().toLower();
        if (!id.isEmpty() && !seen.contains(id)) {
            seen.insert(id);
            m_enabledPresetIds.append(id);
        }
    }
    m_enabledPresetIds.sort(Qt::CaseInsensitive);
}

QList<CrossDomainRule> CrossDomainSettings::rules() const
{
    return m_rules;
}

void CrossDomainSettings::setRules(const QList<CrossDomainRule> &rules)
{
    const QList<CrossDomainRule> input = rules;
    m_rules.clear();
    QSet<QString> seen;
    for (const CrossDomainRule &sourceRule : input) {
        const CrossDomainRule rule = normalizedRule(sourceRule);
        if (rule.sourceSite.isEmpty() || rule.targetHost.isEmpty())
            continue;
        const QString key = rule.sourceSite + QLatin1Char('\n') + rule.targetHost;
        if (seen.contains(key))
            continue;
        seen.insert(key);
        m_rules.append(rule);
    }
}

void CrossDomainSettings::setRule(
    const QString &sourceSite,
    const QString &targetHost,
    CrossDomainRuleDecision decision
)
{
    const CrossDomainRule normalized = normalizedRule({sourceSite, targetHost, decision});
    for (CrossDomainRule &rule : m_rules) {
        if (rule.sourceSite == normalized.sourceSite
            && rule.targetHost == normalized.targetHost) {
            rule.decision = decision;
            return;
        }
    }
    m_rules.append(normalized);
}

CrossDomainEvaluation CrossDomainSettings::evaluate(
    const QUrl &sourceUrl,
    const QUrl &targetUrl
) const
{
    const CrossDomainEvaluation userDecision = evaluateUserPolicy(sourceUrl, targetUrl);
    if (userDecision != CrossDomainEvaluation::Ask)
        return userDecision;
    return evaluatePresetPolicy(targetUrl.host());
}

CrossDomainEvaluation CrossDomainSettings::evaluate(
    const QString &sourceSite,
    const QString &targetHost
) const
{
    const CrossDomainEvaluation userDecision = evaluateUserPolicy(sourceSite, targetHost);
    if (userDecision != CrossDomainEvaluation::Ask)
        return userDecision;
    return evaluatePresetPolicy(targetHost);
}

CrossDomainEvaluation CrossDomainSettings::evaluateUserPolicy(
    const QUrl &sourceUrl,
    const QUrl &targetUrl
) const
{
    if (!m_enabled || !isWebRequestScheme(targetUrl.scheme()))
        return CrossDomainEvaluation::Allow;
    if (sourceUrl.scheme() != QStringLiteral("http")
        && sourceUrl.scheme() != QStringLiteral("https")) {
        return CrossDomainEvaluation::Block;
    }

    const QString sourceSite = SiteDomain::siteForUrl(sourceUrl);
    const QString targetSite = SiteDomain::siteForUrl(targetUrl);
    if (sourceSite.isEmpty())
        return CrossDomainEvaluation::Block;
    if (targetSite.isEmpty() || sourceSite == targetSite)
        return CrossDomainEvaluation::Allow;
    return evaluateUserPolicy(sourceSite, targetUrl.host());
}

CrossDomainEvaluation CrossDomainSettings::evaluateUserPolicy(
    const QString &sourceSite,
    const QString &targetHost
) const
{
    if (!m_enabled)
        return CrossDomainEvaluation::Allow;
    const QString normalizedSource = SiteDomain::registrableDomain(sourceSite);
    const QString normalizedTarget = SiteDomain::normalizeHost(targetHost);
    if (normalizedSource.isEmpty() || normalizedTarget.isEmpty())
        return CrossDomainEvaluation::Allow;
    if (normalizedSource == SiteDomain::registrableDomain(normalizedTarget))
        return CrossDomainEvaluation::Allow;

    for (const QString &pattern : m_globalBlockPatterns) {
        if (SiteDomain::hostMatchesPattern(normalizedTarget, pattern))
            return CrossDomainEvaluation::Block;
    }
    for (const CrossDomainRule &rule : m_rules) {
        if (rule.sourceSite == normalizedSource
            && normalizedTarget == rule.targetHost) {
            return rule.decision == CrossDomainRuleDecision::Allow
                ? CrossDomainEvaluation::Allow
                : CrossDomainEvaluation::Block;
        }
    }
    for (const QString &pattern : m_globalAllowPatterns) {
        if (SiteDomain::hostMatchesPattern(normalizedTarget, pattern))
            return CrossDomainEvaluation::Allow;
    }
    return CrossDomainEvaluation::Ask;
}

CrossDomainEvaluation CrossDomainSettings::evaluatePresetPolicy(
    const QString &targetHost
) const
{
    if (!m_enabled)
        return CrossDomainEvaluation::Allow;
    const std::optional<CrossDomainPresetDecision> decision =
        CrossDomainPresetCatalog::bundled().evaluate(m_enabledPresetIds, targetHost);
    if (!decision.has_value())
        return CrossDomainEvaluation::Ask;
    return *decision == CrossDomainPresetDecision::Allow
        ? CrossDomainEvaluation::Allow
        : CrossDomainEvaluation::Block;
}

QString crossDomainDecisionDisplayName(CrossDomainRuleDecision decision)
{
    return decision == CrossDomainRuleDecision::Allow
        ? QCoreApplication::translate("CrossDomainSettings", "Allow")
        : QCoreApplication::translate("CrossDomainSettings", "Block");
}
