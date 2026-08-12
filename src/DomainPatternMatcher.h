#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

class DomainPatternMatcher {
public:
    void clear();
    void setPatterns(const QStringList &patterns);

    [[nodiscard]] bool matches(const QString &host) const;
    [[nodiscard]] QStringList patterns() const;
    [[nodiscard]] bool isEmpty() const;

private:
    QSet<QString> m_suffixes;
};
