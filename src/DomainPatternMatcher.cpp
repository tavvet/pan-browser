#include "DomainPatternMatcher.h"

#include "SiteDomain.h"

#include <QHostAddress>

void DomainPatternMatcher::clear()
{
    m_suffixes.clear();
}

void DomainPatternMatcher::setPatterns(const QStringList &patterns)
{
    m_suffixes.clear();
    for (const QString &pattern : patterns) {
        const QString normalized = SiteDomain::normalizeHostPattern(pattern);
        if (!normalized.isEmpty())
            m_suffixes.insert(normalized);
    }
}

bool DomainPatternMatcher::matches(const QString &host) const
{
    const QString normalized = SiteDomain::normalizeHost(host);
    if (normalized.isEmpty())
        return false;
    if (m_suffixes.contains(normalized))
        return true;

    QHostAddress address;
    if (address.setAddress(normalized))
        return false;

    qsizetype separator = normalized.indexOf(QLatin1Char('.'));
    while (separator >= 0) {
        const QString suffix = normalized.sliced(separator + 1);
        if (m_suffixes.contains(suffix))
            return true;
        separator = normalized.indexOf(QLatin1Char('.'), separator + 1);
    }
    return false;
}

QStringList DomainPatternMatcher::patterns() const
{
    QStringList result(m_suffixes.cbegin(), m_suffixes.cend());
    result.sort(Qt::CaseInsensitive);
    return result;
}

bool DomainPatternMatcher::isEmpty() const
{
    return m_suffixes.isEmpty();
}
