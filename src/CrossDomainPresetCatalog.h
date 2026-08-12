#pragma once

#include "DomainPatternMatcher.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

enum class CrossDomainPresetDecision {
    Allow,
    Block,
};

struct CrossDomainPreset {
    QString id;
    CrossDomainPresetDecision decision = CrossDomainPresetDecision::Block;
    bool recommended = false;
    QStringList patterns;
    DomainPatternMatcher matcher;
};

class CrossDomainPresetCatalog {
public:
    static const CrossDomainPresetCatalog &bundled();

    bool load(const QString &path, QString *error = nullptr);
    bool loadJson(const QByteArray &contents, QString *error = nullptr);

    [[nodiscard]] QString revision() const;
    [[nodiscard]] QList<CrossDomainPreset> presets() const;
    [[nodiscard]] const CrossDomainPreset *preset(const QString &id) const;
    [[nodiscard]] QStringList knownPresetIds() const;
    [[nodiscard]] std::optional<CrossDomainPresetDecision> evaluate(
        const QStringList &enabledPresetIds,
        const QString &targetHost
    ) const;

private:
    QString m_revision;
    QList<CrossDomainPreset> m_presets;
};
