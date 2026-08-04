#include "TrustConfiguration.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString normalizeHost(QString value)
{
    value = value.trimmed().toLower();
    while (value.startsWith(QLatin1Char('.')))
        value.removeFirst();
    while (value.endsWith(QLatin1Char('.')))
        value.chop(1);
    return value;
}

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString fingerprint(const QSslCertificate &certificate)
{
    const QByteArray digest = QCryptographicHash::hash(
        certificate.toDer(),
        QCryptographicHash::Sha256
    ).toHex(':').toUpper();
    return QString::fromLatin1(digest);
}

bool parseMode(const QString &source, TrustMode *mode)
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

} // namespace

DomainPattern DomainPattern::parse(const QString &source, QString *error)
{
    DomainPattern result;
    result.m_source = source;

    QString normalized = normalizeHost(source);
    if (normalized.startsWith(QStringLiteral("*."))) {
        normalized.remove(0, 2);
        if (!normalized.contains(QLatin1Char('.')) || normalized.contains(QLatin1Char('*'))) {
            fail(error, QStringLiteral("Invalid wildcard domain: %1").arg(source));
            return result;
        }
        result.m_wildcard = true;
    } else if (normalized.isEmpty() || normalized.contains(QLatin1Char('*'))) {
        fail(error, QStringLiteral("Invalid domain: %1").arg(source));
        return result;
    }

    result.m_value = normalized;
    result.m_valid = true;
    return result;
}

bool DomainPattern::isValid() const
{
    return m_valid;
}

bool DomainPattern::matches(const QString &host) const
{
    if (!m_valid)
        return false;

    const QString normalized = normalizeHost(host);
    if (m_wildcard)
        return normalized.endsWith(QLatin1Char('.') + m_value);
    return normalized == m_value;
}

QString DomainPattern::source() const
{
    return m_source;
}

bool TrustRule::matches(const QString &host) const
{
    for (const DomainPattern &domain : domains) {
        if (domain.matches(host))
            return true;
    }
    return false;
}

bool TrustPolicy::load(const QString &path, QString *error)
{
    m_rules.clear();
    m_startPage = QUrl(QStringLiteral("https://example.com"));
    m_sourcePath.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, QStringLiteral("Cannot open %1: %2").arg(path, file.errorString()));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(error, QStringLiteral("Invalid JSON: %1").arg(parseError.errorString()));
    }

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

    const QFileInfo configurationFile(path);
    const QDir baseDirectory = configurationFile.absoluteDir();
    QList<TrustRule> rules;

    const QJsonArray jsonRules = root.value(QStringLiteral("rules")).toArray();
    for (const QJsonValue &value : jsonRules) {
        if (!value.isObject())
            return fail(error, QStringLiteral("Every rule must be an object"));

        const QJsonObject object = value.toObject();
        if (!object.value(QStringLiteral("enabled")).toBool(true))
            continue;

        TrustRule rule;
        rule.name = object.value(QStringLiteral("name")).toString().trimmed();
        if (rule.name.isEmpty())
            return fail(error, QStringLiteral("Rule name cannot be empty"));

        if (!parseMode(object.value(QStringLiteral("mode")).toString(), &rule.mode)) {
            return fail(error, QStringLiteral("Rule %1 has an invalid mode").arg(rule.name));
        }

        const QJsonArray domains = object.value(QStringLiteral("domains")).toArray();
        if (domains.isEmpty())
            return fail(error, QStringLiteral("Rule %1 has no domains").arg(rule.name));

        for (const QJsonValue &domainValue : domains) {
            QString domainError;
            const DomainPattern pattern = DomainPattern::parse(domainValue.toString(), &domainError);
            if (!pattern.isValid())
                return fail(error, domainError);
            rule.domains.append(pattern);
        }

        const QJsonArray anchorPaths = object.value(QStringLiteral("anchors")).toArray();
        for (const QJsonValue &anchorValue : anchorPaths) {
            const QString configuredPath = anchorValue.toString();
            const QString absolutePath = QFileInfo(configuredPath).isAbsolute()
                ? configuredPath
                : baseDirectory.filePath(configuredPath);

            QFile certificateFile(absolutePath);
            if (!certificateFile.open(QIODevice::ReadOnly)) {
                return fail(
                    error,
                    QStringLiteral("Cannot open certificate %1").arg(absolutePath)
                );
            }

            const QByteArray certificateData = certificateFile.readAll();
            QList<QSslCertificate> certificates = QSslCertificate::fromData(
                certificateData,
                QSsl::Pem
            );
            if (certificates.isEmpty())
                certificates = QSslCertificate::fromData(certificateData, QSsl::Der);
            if (certificates.isEmpty()) {
                return fail(
                    error,
                    QStringLiteral("Cannot decode certificate %1").arg(absolutePath)
                );
            }

            for (const QSslCertificate &certificate : certificates) {
                rule.anchors.append(certificate);
                rule.anchorFingerprints.append(fingerprint(certificate));
            }
        }

        if (rule.mode != TrustMode::SystemOnly && rule.anchors.isEmpty()) {
            return fail(
                error,
                QStringLiteral("Rule %1 requires at least one anchor").arg(rule.name)
            );
        }

        rules.append(rule);
    }

    m_rules = rules;
    m_startPage = startPage;
    m_sourcePath = configurationFile.absoluteFilePath();
    return true;
}

const TrustRule *TrustPolicy::ruleForHost(const QString &host) const
{
    for (const TrustRule &rule : m_rules) {
        if (rule.matches(host))
            return &rule;
    }
    return nullptr;
}

QUrl TrustPolicy::startPage() const
{
    return m_startPage;
}

QString TrustPolicy::sourcePath() const
{
    return m_sourcePath;
}

qsizetype TrustPolicy::ruleCount() const
{
    return m_rules.size();
}
