#pragma once

#include "TrustConfiguration.h"

#include <QList>
#include <QStringList>
#include <QUrl>

struct TrustRuleSettings {
    QString name;
    bool enabled = true;
    TrustMode mode = TrustMode::SystemPlusCustom;
    QStringList domains;
    QStringList anchors;
};

class TrustSettings {
public:
    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    bool validate(const QString &path, QString *error = nullptr) const;

    [[nodiscard]] QUrl startPage() const;
    void setStartPage(const QUrl &startPage);

    [[nodiscard]] const QList<TrustRuleSettings> &rules() const;
    QList<TrustRuleSettings> &rules();

private:
    QUrl m_startPage = QUrl(QStringLiteral("https://example.com"));
    QList<TrustRuleSettings> m_rules;
};

[[nodiscard]] QString trustModeToString(TrustMode mode);
bool trustModeFromString(const QString &source, TrustMode *mode);
