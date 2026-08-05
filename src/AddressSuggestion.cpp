#include "AddressSuggestion.h"

#include <QHash>

#include <algorithm>
#include <utility>

namespace {

bool beginsWord(const QString &text, const QString &query)
{
    if (text.startsWith(query))
        return true;
    qsizetype position = text.indexOf(query);
    while (position > 0) {
        if (!text.at(position - 1).isLetterOrNumber())
            return true;
        position = text.indexOf(query, position + 1);
    }
    return false;
}

QString suggestionKey(const QUrl &url)
{
    return url.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
}

struct RankedSuggestion {
    AddressSuggestion suggestion;
    int matchClass = 0;
};

} // namespace

int addressMatchClass(
    const QUrl &sourceUrl,
    const QString &sourceTitle,
    const QString &normalizedQuery
)
{
    const QString query = normalizedQuery.trimmed().toCaseFolded();
    if (query.isEmpty())
        return 0;
    const QString host = sourceUrl.host().toCaseFolded();
    const QString url = sourceUrl.toDisplayString(QUrl::PrettyDecoded).toCaseFolded();
    QString withoutScheme = url;
    const qsizetype schemeEnd = withoutScheme.indexOf(QStringLiteral("://"));
    if (schemeEnd >= 0)
        withoutScheme.remove(0, schemeEnd + 3);
    const QString title = sourceTitle.toCaseFolded();

    if (host == query)
        return 7;
    if (host.startsWith(query))
        return 6;
    if (withoutScheme.startsWith(query) || url.startsWith(query))
        return 5;
    if (beginsWord(title, query))
        return 4;
    if (host.contains(query))
        return 3;
    if (url.contains(query))
        return 2;
    if (title.contains(query))
        return 1;
    return 0;
}

QList<AddressSuggestion> rankedAddressSuggestions(
    const QList<AddressSuggestion> &candidates,
    const QString &query,
    int limit
)
{
    QList<AddressSuggestion> deduplicated;
    QHash<QString, qsizetype> indexByUrl;
    for (const AddressSuggestion &candidate : candidates) {
        if (!candidate.url.isValid())
            continue;
        const QString key = suggestionKey(candidate.url);
        const auto existing = indexByUrl.constFind(key);
        if (existing == indexByUrl.cend()) {
            indexByUrl.insert(key, deduplicated.size());
            deduplicated.append(candidate);
            continue;
        }
        AddressSuggestion &saved = deduplicated[*existing];
        const bool candidatePreferred = candidate.source == AddressSuggestionSource::Bookmark
            && saved.source != AddressSuggestionSource::Bookmark;
        const bool candidateNewerSameSource = candidate.source == saved.source
            && candidate.lastUsedAt > saved.lastUsedAt;
        if (candidatePreferred || candidateNewerSameSource)
            saved = candidate;
    }

    QList<RankedSuggestion> ranked;
    ranked.reserve(deduplicated.size());
    const QString normalizedQuery = query.trimmed().toCaseFolded();
    for (const AddressSuggestion &suggestion : std::as_const(deduplicated)) {
        const int matchClass = addressMatchClass(
            suggestion.url,
            suggestion.title,
            normalizedQuery
        );
        if (matchClass > 0)
            ranked.append({suggestion, matchClass});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](
        const RankedSuggestion &left,
        const RankedSuggestion &right
    ) {
        if (left.matchClass != right.matchClass)
            return left.matchClass > right.matchClass;
        if (left.suggestion.source != right.suggestion.source) {
            return left.suggestion.source == AddressSuggestionSource::Bookmark;
        }
        if (left.suggestion.lastUsedAt != right.suggestion.lastUsedAt)
            return left.suggestion.lastUsedAt > right.suggestion.lastUsedAt;
        if (left.suggestion.typedCount != right.suggestion.typedCount)
            return left.suggestion.typedCount > right.suggestion.typedCount;
        if (left.suggestion.visitCount != right.suggestion.visitCount)
            return left.suggestion.visitCount > right.suggestion.visitCount;
        return left.suggestion.title.localeAwareCompare(right.suggestion.title) < 0;
    });

    QList<AddressSuggestion> result;
    if (limit <= 0)
        return result;
    const qsizetype count = std::min(ranked.size(), static_cast<qsizetype>(limit));
    result.reserve(count);
    for (qsizetype index = 0; index < count; ++index)
        result.append(ranked.at(index).suggestion);
    return result;
}
