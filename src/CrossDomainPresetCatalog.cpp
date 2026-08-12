#include "CrossDomainPresetCatalog.h"

#include "SiteDomain.h"

#include <QDebug>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace {

constexpr qsizetype maximumCatalogSize = 512 * 1024;
constexpr qsizetype maximumPresets = 32;
constexpr qsizetype maximumPatternsPerPreset = 4096;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

bool parseDecision(const QString &value, CrossDomainPresetDecision *decision)
{
    if (value == QStringLiteral("allow")) {
        *decision = CrossDomainPresetDecision::Allow;
        return true;
    }
    if (value == QStringLiteral("block")) {
        *decision = CrossDomainPresetDecision::Block;
        return true;
    }
    return false;
}

bool isSafePresetPattern(const QString &pattern)
{
    QHostAddress address;
    if (address.setAddress(pattern) || !pattern.contains(QLatin1Char('.')))
        return false;
    const QString probe = QStringLiteral("panbrowser-probe.") + pattern;
    return SiteDomain::registrableDomain(probe) != probe;
}

} // namespace

const CrossDomainPresetCatalog &CrossDomainPresetCatalog::bundled()
{
    static const CrossDomainPresetCatalog catalog = [] {
        CrossDomainPresetCatalog loaded;
        QString error;
        if (!loaded.load(QStringLiteral(":/assets/site_connection_presets.json"), &error))
            qWarning().noquote() << "[PanBrowser connection presets]" << error;
        return loaded;
    }();
    return catalog;
}

bool CrossDomainPresetCatalog::load(const QString &path, QString *error)
{
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, QStringLiteral("Cannot open %1: %2").arg(path, file.errorString()));
    if (file.size() > maximumCatalogSize)
        return fail(error, QStringLiteral("Connection preset catalog is too large"));
    return loadJson(file.readAll(), error);
}

bool CrossDomainPresetCatalog::loadJson(const QByteArray &contents, QString *error)
{
    if (error)
        error->clear();
    if (contents.size() > maximumCatalogSize)
        return fail(error, QStringLiteral("Connection preset catalog is too large"));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(contents, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(
            error,
            QStringLiteral("Invalid connection preset catalog JSON: %1")
                .arg(parseError.errorString())
        );
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1
        || !root.value(QStringLiteral("revision")).isString()
        || !root.value(QStringLiteral("presets")).isArray()) {
        return fail(error, QStringLiteral("Connection preset catalog has invalid fields"));
    }
    const QString revision = root.value(QStringLiteral("revision")).toString().trimmed();
    if (revision.isEmpty() || revision.size() > 64)
        return fail(error, QStringLiteral("Connection preset catalog revision is invalid"));

    const QJsonArray presetValues = root.value(QStringLiteral("presets")).toArray();
    if (presetValues.size() > maximumPresets)
        return fail(error, QStringLiteral("Connection preset catalog has too many presets"));

    static const QRegularExpression validId(QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$"));
    QList<CrossDomainPreset> parsedPresets;
    QSet<QString> seenIds;
    for (const QJsonValue &value : presetValues) {
        if (!value.isObject())
            return fail(error, QStringLiteral("Connection presets must be objects"));
        const QJsonObject object = value.toObject();
        if (!object.value(QStringLiteral("id")).isString()
            || !object.value(QStringLiteral("decision")).isString()
            || !object.value(QStringLiteral("recommended")).isBool()
            || !object.value(QStringLiteral("patterns")).isArray()) {
            return fail(error, QStringLiteral("A connection preset has invalid fields"));
        }

        CrossDomainPreset preset;
        preset.id = object.value(QStringLiteral("id")).toString();
        if (!validId.match(preset.id).hasMatch() || seenIds.contains(preset.id))
            return fail(error, QStringLiteral("A connection preset ID is invalid or duplicated"));
        seenIds.insert(preset.id);
        if (!parseDecision(
                object.value(QStringLiteral("decision")).toString(),
                &preset.decision
            )) {
            return fail(error, QStringLiteral("A connection preset decision is invalid"));
        }
        preset.recommended = object.value(QStringLiteral("recommended")).toBool();

        const QJsonArray patternValues = object.value(QStringLiteral("patterns")).toArray();
        if (patternValues.isEmpty() || patternValues.size() > maximumPatternsPerPreset) {
            return fail(error, QStringLiteral("A connection preset has an invalid pattern count"));
        }
        QSet<QString> seenPatterns;
        for (const QJsonValue &patternValue : patternValues) {
            if (!patternValue.isString())
                return fail(error, QStringLiteral("Connection preset patterns must be strings"));
            const QString pattern = SiteDomain::normalizeHostPattern(
                patternValue.toString()
            );
            if (!isSafePresetPattern(pattern))
                return fail(error, QStringLiteral("A connection preset hostname is invalid"));
            if (!seenPatterns.contains(pattern)) {
                seenPatterns.insert(pattern);
                preset.patterns.append(pattern);
            }
        }
        preset.patterns.sort(Qt::CaseInsensitive);
        preset.matcher.setPatterns(preset.patterns);
        parsedPresets.append(preset);
    }

    m_revision = revision;
    m_presets = parsedPresets;
    return true;
}

QString CrossDomainPresetCatalog::revision() const
{
    return m_revision;
}

QList<CrossDomainPreset> CrossDomainPresetCatalog::presets() const
{
    return m_presets;
}

const CrossDomainPreset *CrossDomainPresetCatalog::preset(const QString &id) const
{
    for (const CrossDomainPreset &candidate : m_presets) {
        if (candidate.id == id)
            return &candidate;
    }
    return nullptr;
}

QStringList CrossDomainPresetCatalog::knownPresetIds() const
{
    QStringList result;
    result.reserve(m_presets.size());
    for (const CrossDomainPreset &preset : m_presets)
        result.append(preset.id);
    return result;
}

std::optional<CrossDomainPresetDecision> CrossDomainPresetCatalog::evaluate(
    const QStringList &enabledPresetIds,
    const QString &targetHost
) const
{
    const QSet<QString> enabled(enabledPresetIds.cbegin(), enabledPresetIds.cend());
    bool allowed = false;
    for (const CrossDomainPreset &candidate : m_presets) {
        if (!enabled.contains(candidate.id) || !candidate.matcher.matches(targetHost))
            continue;
        if (candidate.decision == CrossDomainPresetDecision::Block)
            return CrossDomainPresetDecision::Block;
        allowed = true;
    }
    if (allowed)
        return CrossDomainPresetDecision::Allow;
    return std::nullopt;
}
