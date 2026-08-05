#include "TrustSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QSslCertificate>

namespace {

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

bool certificateFileIsValid(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = file.readAll();
    QList<QSslCertificate> certificates = QSslCertificate::fromData(data, QSsl::Pem);
    if (certificates.isEmpty())
        certificates = QSslCertificate::fromData(data, QSsl::Der);
    return !certificates.isEmpty();
}

} // namespace

QString trustModeToString(TrustMode mode)
{
    switch (mode) {
    case TrustMode::SystemOnly:
        return QStringLiteral("system-only");
    case TrustMode::SystemPlusCustom:
        return QStringLiteral("system-plus-custom");
    case TrustMode::CustomOnly:
        return QStringLiteral("custom-only");
    }
    return QString();
}

bool trustModeFromString(const QString &source, TrustMode *mode)
{
    if (source == QStringLiteral("system-only")) {
        *mode = TrustMode::SystemOnly;
        return true;
    }
    if (source == QStringLiteral("system-plus-custom")) {
        *mode = TrustMode::SystemPlusCustom;
        return true;
    }
    if (source == QStringLiteral("custom-only")) {
        *mode = TrustMode::CustomOnly;
        return true;
    }
    return false;
}

bool TrustSettings::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, QStringLiteral("Cannot open %1: %2").arg(path, file.errorString()));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("Invalid JSON: %1").arg(parseError.errorString()));

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1)
        return fail(error, QStringLiteral("Unsupported rules version"));

    const QUrl startPage(root.value(QStringLiteral("startPage")).toString(
        QStringLiteral("https://example.com")
    ));
    if (!startPage.isValid()
        || (startPage.scheme() != QStringLiteral("http")
            && startPage.scheme() != QStringLiteral("https"))) {
        return fail(error, QStringLiteral("Invalid startPage"));
    }

    QList<TrustRuleSettings> rules;
    const QJsonValue rulesValue = root.value(QStringLiteral("rules"));
    if (!rulesValue.isArray())
        return fail(error, QStringLiteral("Rules must be an array"));

    for (const QJsonValue &value : rulesValue.toArray()) {
        if (!value.isObject())
            return fail(error, QStringLiteral("Every rule must be an object"));

        const QJsonObject object = value.toObject();
        TrustRuleSettings rule;
        rule.name = object.value(QStringLiteral("name")).toString().trimmed();
        rule.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        if (!trustModeFromString(object.value(QStringLiteral("mode")).toString(), &rule.mode)) {
            return fail(
                error,
                QStringLiteral("Rule %1 has an invalid mode").arg(
                    rule.name.isEmpty() ? QStringLiteral("<unnamed>") : rule.name
                )
            );
        }

        const QJsonValue domainsValue = object.value(QStringLiteral("domains"));
        if (!domainsValue.isArray())
            return fail(error, QStringLiteral("Rule domains must be an array"));
        for (const QJsonValue &domain : domainsValue.toArray())
            rule.domains.append(domain.toString());

        const QJsonValue anchorsValue = object.value(QStringLiteral("anchors"));
        if (!anchorsValue.isArray())
            return fail(error, QStringLiteral("Rule anchors must be an array"));
        for (const QJsonValue &anchor : anchorsValue.toArray())
            rule.anchors.append(anchor.toString());

        rules.append(rule);
    }

    m_startPage = startPage;
    m_rules = rules;
    return true;
}

bool TrustSettings::save(const QString &path, QString *error) const
{
    if (!validate(path, error))
        return false;

    QJsonArray jsonRules;
    for (const TrustRuleSettings &rule : m_rules) {
        QJsonArray domains;
        for (const QString &domain : rule.domains)
            domains.append(domain);

        QJsonArray anchors;
        for (const QString &anchor : rule.anchors)
            anchors.append(anchor);

        QJsonObject object;
        object.insert(QStringLiteral("name"), rule.name.trimmed());
        object.insert(QStringLiteral("enabled"), rule.enabled);
        object.insert(QStringLiteral("domains"), domains);
        object.insert(QStringLiteral("mode"), trustModeToString(rule.mode));
        object.insert(QStringLiteral("anchors"), anchors);
        jsonRules.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("startPage"), m_startPage.toString());
    root.insert(QStringLiteral("rules"), jsonRules);

    if (QFile::exists(path)) {
        const QString backupPath = path + QStringLiteral(".backup");
        if (QFile::exists(backupPath) && !QFile::remove(backupPath))
            return fail(error, QStringLiteral("Cannot replace backup %1").arg(backupPath));
        if (!QFile::copy(path, backupPath))
            return fail(error, QStringLiteral("Cannot create backup %1").arg(backupPath));
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QStringLiteral("Cannot write %1: %2").arg(path, file.errorString()));

    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size())
        return fail(error, QStringLiteral("Cannot write %1: %2").arg(path, file.errorString()));
    if (!file.commit())
        return fail(error, QStringLiteral("Cannot commit %1: %2").arg(path, file.errorString()));
    return true;
}

bool TrustSettings::validate(const QString &path, QString *error) const
{
    if (!m_startPage.isValid()
        || (m_startPage.scheme() != QStringLiteral("http")
            && m_startPage.scheme() != QStringLiteral("https"))) {
        return fail(error, QStringLiteral("Invalid start page"));
    }

    const QDir baseDirectory = QFileInfo(path).absoluteDir();
    QList<QPair<QString, QString>> configuredDomains;
    QSet<QString> configuredNames;

    for (const TrustRuleSettings &rule : m_rules) {
        const QString name = rule.name.trimmed();
        if (name.isEmpty())
            return fail(error, QStringLiteral("Rule name cannot be empty"));

        const QString normalizedName = name.toCaseFolded();
        if (configuredNames.contains(normalizedName))
            return fail(error, QStringLiteral("Duplicate rule name: %1").arg(name));
        configuredNames.insert(normalizedName);

        if (!rule.enabled)
            continue;

        if (rule.domains.isEmpty())
            return fail(error, QStringLiteral("Rule %1 has no domains").arg(name));

        for (const QString &domain : rule.domains) {
            QString domainError;
            if (!DomainPattern::parse(domain, &domainError).isValid())
                return fail(error, domainError);

            for (const auto &[configuredDomain, configuredRule] : configuredDomains) {
                if (domainPatternsOverlap(domain, configuredDomain)) {
                    return fail(
                        error,
                        QStringLiteral("Domain %1 overlaps rule %2").arg(domain, configuredRule)
                    );
                }
            }
            configuredDomains.append({domain, name});
        }

        if (rule.mode != TrustMode::SystemOnly && rule.anchors.isEmpty()) {
            return fail(
                error,
                QStringLiteral("Rule %1 requires at least one certificate").arg(name)
            );
        }

        for (const QString &anchor : rule.anchors) {
            const QString absolutePath = QFileInfo(anchor).isAbsolute()
                ? anchor
                : baseDirectory.filePath(anchor);
            if (!certificateFileIsValid(absolutePath)) {
                return fail(
                    error,
                    QStringLiteral("Cannot decode certificate %1").arg(absolutePath)
                );
            }
        }
    }

    return true;
}

QUrl TrustSettings::startPage() const
{
    return m_startPage;
}

void TrustSettings::setStartPage(const QUrl &startPage)
{
    m_startPage = startPage;
}

const QList<TrustRuleSettings> &TrustSettings::rules() const
{
    return m_rules;
}

QList<TrustRuleSettings> &TrustSettings::rules()
{
    return m_rules;
}
