#pragma once

#include <QList>
#include <QSslCertificate>
#include <QString>
#include <QUrl>

enum class TrustMode {
    SystemOnly,
    SystemPlusCustom,
    CustomOnly,
};

class DomainPattern {
public:
    static DomainPattern parse(const QString &source, QString *error = nullptr);

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool matches(const QString &host) const;
    [[nodiscard]] QString source() const;

private:
    QString m_value;
    QString m_source;
    bool m_wildcard = false;
    bool m_valid = false;
};

struct TrustRule {
    QString name;
    TrustMode mode = TrustMode::SystemOnly;
    QList<DomainPattern> domains;
    QList<QSslCertificate> anchors;
    QStringList anchorFingerprints;

    [[nodiscard]] bool matches(const QString &host) const;
};

class TrustPolicy {
public:
    bool load(const QString &path, QString *error = nullptr);

    [[nodiscard]] const TrustRule *ruleForHost(const QString &host) const;
    [[nodiscard]] QUrl startPage() const;
    [[nodiscard]] QString sourcePath() const;
    [[nodiscard]] qsizetype ruleCount() const;

private:
    QList<TrustRule> m_rules;
    QUrl m_startPage = QUrl(QStringLiteral("https://example.com"));
    QString m_sourcePath;
};
