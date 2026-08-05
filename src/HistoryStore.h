#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QUrl>

enum class HistoryTransition {
    Other,
    Typed,
    Link,
    Form,
    BackForward,
    Reload,
};

struct HistoryVisit {
    qint64 id = 0;
    QUrl url;
    QString title;
    QDateTime visitedAt;
    HistoryTransition transition = HistoryTransition::Other;
};

struct HistorySuggestion {
    qint64 pageId = 0;
    QUrl url;
    QString title;
    QDateTime lastVisitedAt;
    int visitCount = 0;
    int typedCount = 0;
};

class HistoryStore final : public QObject {
    Q_OBJECT

public:
    static constexpr int maximumVisits = 50000;

    explicit HistoryStore(const QString &path, QObject *parent = nullptr);
    ~HistoryStore() override;

    bool open(QString *error = nullptr);
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString path() const;

    bool recordVisit(
        const QUrl &url,
        const QString &title,
        HistoryTransition transition,
        const QDateTime &visitedAt = QDateTime::currentDateTimeUtc(),
        QString *error = nullptr
    );
    bool updateTitle(const QUrl &url, const QString &title, QString *error = nullptr);
    [[nodiscard]] QList<HistoryVisit> visits(
        const QString &filter = QString(),
        int limit = 1000,
        QString *error = nullptr
    ) const;
    [[nodiscard]] QList<HistorySuggestion> suggestions(
        const QString &input,
        int limit = 8,
        QString *error = nullptr
    ) const;
    bool removeVisits(const QList<qint64> &visitIds, QString *error = nullptr);
    bool clear(QString *error = nullptr);

    [[nodiscard]] static QUrl sanitizedUrl(const QUrl &url);

signals:
    void historyChanged();

private:
    bool createSchema(QString *error);
    bool pruneIfNeeded(QString *error);
    bool rebuildPageAggregates(const QList<qint64> &pageIds, QString *error);

    QString m_path;
    QString m_connectionName;
    QSqlDatabase m_database;
};
