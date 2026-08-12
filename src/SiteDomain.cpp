#include "SiteDomain.h"

#include <QFile>
#include <QHostAddress>
#include <QSet>
#include <QUrl>

namespace {

struct PublicSuffixRules {
    QSet<QString> exact;
    QSet<QString> wildcardBases;
    QSet<QString> exceptions;
};

QString normalizedDomain(QString domain)
{
    domain = domain.trimmed();
    while (domain.endsWith(QLatin1Char('.')))
        domain.chop(1);
    if (domain.isEmpty())
        return {};
    QHostAddress address;
    if (address.setAddress(domain))
        return address.toString().toLower();
    return QString::fromLatin1(QUrl::toAce(domain)).toLower();
}

PublicSuffixRules loadPublicSuffixRules()
{
    PublicSuffixRules rules;
    QFile file(QStringLiteral(":/assets/public_suffix_list.dat"));
    if (!file.open(QIODevice::ReadOnly))
        return rules;

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QStringLiteral("//")))
            continue;

        QSet<QString> *destination = &rules.exact;
        if (line.startsWith(QLatin1Char('!'))) {
            line.remove(0, 1);
            destination = &rules.exceptions;
        } else if (line.startsWith(QStringLiteral("*."))) {
            line.remove(0, 2);
            destination = &rules.wildcardBases;
        }

        const QString normalized = normalizedDomain(line);
        if (!normalized.isEmpty())
            destination->insert(normalized);
    }
    return rules;
}

const PublicSuffixRules &publicSuffixRules()
{
    static const PublicSuffixRules rules = loadPublicSuffixRules();
    return rules;
}

QString suffixFrom(const QStringList &labels, qsizetype first)
{
    return labels.mid(first).join(QLatin1Char('.'));
}

qsizetype publicSuffixLabelCount(const QStringList &labels)
{
    const PublicSuffixRules &rules = publicSuffixRules();

    qsizetype exceptionLength = 0;
    qsizetype matchLength = 1; // The prevailing default rule is "*".
    for (qsizetype index = 0; index < labels.size(); ++index) {
        const QString suffix = suffixFrom(labels, index);
        const qsizetype suffixLength = labels.size() - index;
        if (rules.exceptions.contains(suffix))
            exceptionLength = qMax(exceptionLength, suffixLength);
        if (rules.exact.contains(suffix))
            matchLength = qMax(matchLength, suffixLength);
        if (index > 0 && rules.wildcardBases.contains(suffix))
            matchLength = qMax(matchLength, suffixLength + 1);
    }

    if (exceptionLength > 0)
        return exceptionLength - 1;
    return qMin(matchLength, labels.size());
}

} // namespace

QString SiteDomain::normalizeHost(const QString &host)
{
    return normalizedDomain(host);
}

QString SiteDomain::registrableDomain(const QString &host)
{
    const QString normalized = normalizeHost(host);
    if (normalized.isEmpty())
        return {};

    QHostAddress address;
    if (address.setAddress(normalized) || !normalized.contains(QLatin1Char('.')))
        return normalized;

    const QStringList labels = normalized.split(
        QLatin1Char('.'),
        Qt::SkipEmptyParts
    );
    const qsizetype suffixLabels = publicSuffixLabelCount(labels);
    if (labels.size() <= suffixLabels)
        return normalized;
    return suffixFrom(labels, labels.size() - suffixLabels - 1);
}

QString SiteDomain::siteForUrl(const QUrl &url)
{
    return registrableDomain(url.host());
}

bool SiteDomain::sameSite(const QUrl &left, const QUrl &right)
{
    const QString leftSite = siteForUrl(left);
    return !leftSite.isEmpty() && leftSite == siteForUrl(right);
}

QUrl SiteDomain::normalizedPageUrl(const QUrl &url)
{
    QUrl normalized = url.adjusted(
        QUrl::RemoveFragment | QUrl::NormalizePathSegments
    );
    normalized.setUserInfo(QString());
    if (normalized.path().isEmpty())
        normalized.setPath(QStringLiteral("/"));
    const bool defaultPort = (normalized.scheme() == QStringLiteral("https")
                              && normalized.port() == 443)
        || (normalized.scheme() == QStringLiteral("http") && normalized.port() == 80);
    if (defaultPort)
        normalized.setPort(-1);
    return normalized;
}

QString SiteDomain::normalizeHostPattern(const QString &pattern)
{
    QString normalized = pattern.trimmed();
    if (normalized.startsWith(QStringLiteral("*.")))
        normalized.remove(0, 2);
    while (normalized.endsWith(QLatin1Char('.')))
        normalized.chop(1);
    if (normalized.isEmpty())
        return {};

    QHostAddress address;
    if (address.setAddress(normalized))
        return address.toString().toLower();
    for (const QChar character : normalized) {
        if (character.isSpace()
            || character.category() == QChar::Other_Control
            || QStringLiteral("/:\\@?#[]").contains(character)) {
            return {};
        }
    }

    const QString ace = normalizeHost(normalized);
    if (ace.isEmpty() || ace.size() > 253)
        return {};
    const QStringList labels = ace.split(QLatin1Char('.'), Qt::KeepEmptyParts);
    for (const QString &label : labels) {
        if (label.isEmpty()
            || label.size() > 63
            || label.startsWith(QLatin1Char('-'))
            || label.endsWith(QLatin1Char('-'))) {
            return {};
        }
        for (const QChar character : label) {
            if (!character.isLetterOrNumber() && character != QLatin1Char('-'))
                return {};
        }
    }
    return ace;
}

bool SiteDomain::hostMatchesPattern(const QString &host, const QString &pattern)
{
    const QString normalizedHost = normalizeHost(host);
    const QString normalizedPattern = normalizeHostPattern(pattern);
    return !normalizedHost.isEmpty()
        && !normalizedPattern.isEmpty()
        && (normalizedHost == normalizedPattern
            || normalizedHost.endsWith(QLatin1Char('.') + normalizedPattern));
}
