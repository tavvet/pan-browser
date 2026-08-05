#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

enum class AddressSuggestionSource {
    History,
    Bookmark,
};

struct AddressSuggestion {
    QUrl url;
    QString title;
    QDateTime lastUsedAt;
    AddressSuggestionSource source = AddressSuggestionSource::History;
    int typedCount = 0;
    int visitCount = 0;
};

[[nodiscard]] int addressMatchClass(
    const QUrl &url,
    const QString &title,
    const QString &normalizedQuery
);

[[nodiscard]] QList<AddressSuggestion> rankedAddressSuggestions(
    const QList<AddressSuggestion> &candidates,
    const QString &query,
    int limit
);
