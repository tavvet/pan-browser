#include "HistoryStore.h"

#include "AddressSuggestion.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>

namespace {

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString databaseError(const QString &action, const QSqlQuery &query)
{
    return QStringLiteral("%1: %2").arg(action, query.lastError().text());
}

QString normalizedText(const QString &text)
{
    return text.trimmed().toCaseFolded();
}

QString escapedLikePattern(QString text)
{
    text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    text.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    text.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    return QLatin1Char('%') + text + QLatin1Char('%');
}

int transitionValue(HistoryTransition transition)
{
    return static_cast<int>(transition);
}

HistoryTransition transitionFromValue(int value)
{
    if (value < static_cast<int>(HistoryTransition::Other)
        || value > static_cast<int>(HistoryTransition::Reload)) {
        return HistoryTransition::Other;
    }
    return static_cast<HistoryTransition>(value);
}

struct RankedHistorySuggestion {
    HistorySuggestion suggestion;
    int matchClass = 0;
};

} // namespace

HistoryStore::HistoryStore(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path)
    , m_connectionName(
        QStringLiteral("panbrowser-history-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
        )
    )
{
}

HistoryStore::~HistoryStore()
{
    if (!m_database.isValid())
        return;
    const QString connectionName = m_connectionName;
    m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

bool HistoryStore::open(QString *error)
{
    if (isOpen())
        return true;
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return fail(error, tr("Cannot create history directory"));

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_path);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=3000"));
    if (!m_database.open())
        return fail(error, tr("Cannot open history: %1").arg(m_database.lastError().text()));

    QSqlQuery pragma(m_database);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        const QString message = databaseError(tr("Cannot enable foreign keys"), pragma);
        m_database.close();
        return fail(error, message);
    }
    if (!pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        const QString message = databaseError(tr("Cannot enable history journal"), pragma);
        m_database.close();
        return fail(error, message);
    }
    if (!pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"))) {
        const QString message = databaseError(tr("Cannot configure history journal"), pragma);
        m_database.close();
        return fail(error, message);
    }
    if (!createSchema(error)) {
        m_database.close();
        return false;
    }
    return true;
}

bool HistoryStore::isOpen() const
{
    return m_database.isValid() && m_database.isOpen();
}

QString HistoryStore::path() const
{
    return m_path;
}

bool HistoryStore::recordVisit(
    const QUrl &sourceUrl,
    const QString &sourceTitle,
    HistoryTransition transition,
    const QDateTime &visitedAt,
    QString *error
)
{
    if (!isOpen())
        return fail(error, tr("History is unavailable"));
    const QUrl url = sanitizedUrl(sourceUrl);
    if (!url.isValid())
        return fail(error, tr("Only HTTP and HTTPS visits can be stored"));
    const QString encodedUrl = url.toString(QUrl::FullyEncoded);
    const QString title = sourceTitle.trimmed();
    const qint64 timestamp = visitedAt.toUTC().toMSecsSinceEpoch();
    if (timestamp <= 0)
        return fail(error, tr("Visit date is invalid"));

    if (!m_database.transaction())
        return fail(error, tr("Cannot start history transaction: %1").arg(m_database.lastError().text()));

    QSqlQuery upsert(m_database);
    upsert.prepare(QStringLiteral(
        "INSERT INTO pages(url, normalized_url, title, normalized_title, visit_count, typed_count, last_visited_at) "
        "VALUES(?, ?, ?, ?, 1, ?, ?) "
        "ON CONFLICT(url) DO UPDATE SET "
        "title = CASE WHEN excluded.title = '' THEN pages.title ELSE excluded.title END, "
        "normalized_title = CASE WHEN excluded.title = '' THEN pages.normalized_title ELSE excluded.normalized_title END, "
        "visit_count = pages.visit_count + 1, "
        "typed_count = pages.typed_count + excluded.typed_count, "
        "last_visited_at = MAX(pages.last_visited_at, excluded.last_visited_at)"
    ));
    upsert.addBindValue(encodedUrl);
    upsert.addBindValue(normalizedText(url.toDisplayString(QUrl::PrettyDecoded)));
    upsert.addBindValue(title);
    upsert.addBindValue(normalizedText(title));
    upsert.addBindValue(transition == HistoryTransition::Typed ? 1 : 0);
    upsert.addBindValue(timestamp);
    if (!upsert.exec()) {
        m_database.rollback();
        return fail(error, databaseError(tr("Cannot save history page"), upsert));
    }

    QSqlQuery pageId(m_database);
    pageId.prepare(QStringLiteral("SELECT id FROM pages WHERE url = ?"));
    pageId.addBindValue(encodedUrl);
    if (!pageId.exec() || !pageId.next()) {
        m_database.rollback();
        return fail(error, databaseError(tr("Cannot find history page"), pageId));
    }

    QSqlQuery insertVisit(m_database);
    insertVisit.prepare(QStringLiteral(
        "INSERT INTO visits(page_id, visited_at, transition) VALUES(?, ?, ?)"
    ));
    insertVisit.addBindValue(pageId.value(0));
    insertVisit.addBindValue(timestamp);
    insertVisit.addBindValue(transitionValue(transition));
    if (!insertVisit.exec()) {
        m_database.rollback();
        return fail(error, databaseError(tr("Cannot save history visit"), insertVisit));
    }
    if (!m_database.commit())
        return fail(error, tr("Cannot commit history: %1").arg(m_database.lastError().text()));
    if (!pruneIfNeeded(error))
        return false;
    emit historyChanged();
    return true;
}

bool HistoryStore::updateTitle(const QUrl &sourceUrl, const QString &sourceTitle, QString *error)
{
    if (!isOpen())
        return fail(error, tr("History is unavailable"));
    const QUrl url = sanitizedUrl(sourceUrl);
    const QString title = sourceTitle.trimmed();
    if (!url.isValid() || title.isEmpty())
        return true;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE pages SET title = ?, normalized_title = ? WHERE url = ?"
    ));
    query.addBindValue(title);
    query.addBindValue(normalizedText(title));
    query.addBindValue(url.toString(QUrl::FullyEncoded));
    if (!query.exec())
        return fail(error, databaseError(tr("Cannot update history title"), query));
    if (query.numRowsAffected() > 0)
        emit historyChanged();
    return true;
}

QList<HistoryVisit> HistoryStore::visits(
    const QString &sourceFilter,
    int limit,
    QString *error
) const
{
    QList<HistoryVisit> result;
    if (!isOpen()) {
        fail(error, tr("History is unavailable"));
        return result;
    }
    QSqlQuery query(m_database);
    QString sql = QStringLiteral(
        "SELECT visits.id, pages.url, pages.title, visits.visited_at, visits.transition "
        "FROM visits JOIN pages ON pages.id = visits.page_id "
    );
    const QString filter = normalizedText(sourceFilter);
    if (!filter.isEmpty())
        sql += QStringLiteral("WHERE pages.normalized_url LIKE ? ESCAPE '\\' OR pages.normalized_title LIKE ? ESCAPE '\\' ");
    sql += QStringLiteral("ORDER BY visits.visited_at DESC, visits.id DESC LIMIT ?");
    query.prepare(sql);
    if (!filter.isEmpty()) {
        const QString pattern = escapedLikePattern(filter);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    query.addBindValue(std::clamp(limit, 1, 5000));
    if (!query.exec()) {
        fail(error, databaseError(tr("Cannot read history"), query));
        return result;
    }
    while (query.next()) {
        HistoryVisit visit;
        visit.id = query.value(0).toLongLong();
        visit.url = QUrl(query.value(1).toString(), QUrl::StrictMode);
        visit.title = query.value(2).toString();
        visit.visitedAt = QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong(), QTimeZone::UTC);
        visit.transition = transitionFromValue(query.value(4).toInt());
        result.append(visit);
    }
    return result;
}

QList<HistorySuggestion> HistoryStore::suggestions(
    const QString &sourceInput,
    int limit,
    QString *error
) const
{
    QList<HistorySuggestion> result;
    if (!isOpen()) {
        fail(error, tr("History is unavailable"));
        return result;
    }
    if (limit <= 0)
        return result;
    QString input = normalizedText(sourceInput);
    if (input.startsWith(QStringLiteral("http://")))
        input.remove(0, 7);
    else if (input.startsWith(QStringLiteral("https://")))
        input.remove(0, 8);
    if (input.isEmpty())
        return result;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, url, title, last_visited_at, visit_count, typed_count "
        "FROM pages "
        "WHERE normalized_url LIKE ? ESCAPE '\\' OR normalized_title LIKE ? ESCAPE '\\' "
        "ORDER BY last_visited_at DESC"
    ));
    const QString pattern = escapedLikePattern(input);
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    if (!query.exec()) {
        fail(error, databaseError(tr("Cannot search history"), query));
        return result;
    }
    QList<RankedHistorySuggestion> candidates;
    while (query.next()) {
        HistorySuggestion suggestion;
        suggestion.pageId = query.value(0).toLongLong();
        suggestion.url = QUrl(query.value(1).toString(), QUrl::StrictMode);
        suggestion.title = query.value(2).toString();
        suggestion.lastVisitedAt = QDateTime::fromMSecsSinceEpoch(
            query.value(3).toLongLong(),
            QTimeZone::UTC
        );
        suggestion.visitCount = query.value(4).toInt();
        suggestion.typedCount = query.value(5).toInt();
        candidates.append({
            suggestion,
            addressMatchClass(suggestion.url, suggestion.title, input),
        });
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](
        const RankedHistorySuggestion &left,
        const RankedHistorySuggestion &right
    ) {
        if (left.matchClass != right.matchClass)
            return left.matchClass > right.matchClass;
        if (left.suggestion.lastVisitedAt != right.suggestion.lastVisitedAt)
            return left.suggestion.lastVisitedAt > right.suggestion.lastVisitedAt;
        if (left.suggestion.typedCount != right.suggestion.typedCount)
            return left.suggestion.typedCount > right.suggestion.typedCount;
        return left.suggestion.visitCount > right.suggestion.visitCount;
    });
    const qsizetype resultCount = std::min(candidates.size(), static_cast<qsizetype>(limit));
    result.reserve(resultCount);
    for (qsizetype index = 0; index < resultCount; ++index)
        result.append(candidates.at(index).suggestion);
    return result;
}

bool HistoryStore::removeVisits(const QList<qint64> &visitIds, QString *error)
{
    if (visitIds.isEmpty())
        return true;
    if (!isOpen())
        return fail(error, tr("History is unavailable"));
    if (!m_database.transaction())
        return fail(error, tr("Cannot start history transaction"));

    QSet<qint64> pageIds;
    QSqlQuery findPage(m_database);
    findPage.prepare(QStringLiteral("SELECT page_id FROM visits WHERE id = ?"));
    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM visits WHERE id = ?"));
    for (qint64 visitId : visitIds) {
        findPage.bindValue(0, visitId);
        if (!findPage.exec()) {
            m_database.rollback();
            return fail(error, databaseError(tr("Cannot find history visit"), findPage));
        }
        if (findPage.next())
            pageIds.insert(findPage.value(0).toLongLong());
        remove.bindValue(0, visitId);
        if (!remove.exec()) {
            m_database.rollback();
            return fail(error, databaseError(tr("Cannot remove history visit"), remove));
        }
    }
    if (!rebuildPageAggregates(pageIds.values(), error)) {
        m_database.rollback();
        return false;
    }
    if (!m_database.commit())
        return fail(error, tr("Cannot commit history deletion"));
    emit historyChanged();
    return true;
}

bool HistoryStore::clear(QString *error)
{
    if (!isOpen())
        return fail(error, tr("History is unavailable"));
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("DELETE FROM pages")))
        return fail(error, databaseError(tr("Cannot clear history"), query));
    emit historyChanged();
    return true;
}

QUrl HistoryStore::sanitizedUrl(const QUrl &source)
{
    QUrl url(source);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid()
        || url.host().isEmpty()
        || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        return {};
    }
    url.setScheme(scheme);
    url.setUserName(QString());
    url.setPassword(QString());
    url.setFragment(QString());
    return url;
}

bool HistoryStore::createSchema(QString *error)
{
    QSqlQuery version(m_database);
    if (!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next())
        return fail(error, databaseError(tr("Cannot read history version"), version));
    const int schemaVersion = version.value(0).toInt();
    if (schemaVersion != 0 && schemaVersion != 1)
        return fail(error, tr("Unsupported history version: %1").arg(schemaVersion));
    if (!m_database.transaction())
        return fail(error, tr("Cannot start history schema transaction"));

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS pages("
            "id INTEGER PRIMARY KEY, "
            "url TEXT NOT NULL UNIQUE, "
            "normalized_url TEXT NOT NULL, "
            "title TEXT NOT NULL DEFAULT '', "
            "normalized_title TEXT NOT NULL DEFAULT '', "
            "visit_count INTEGER NOT NULL DEFAULT 0, "
            "typed_count INTEGER NOT NULL DEFAULT 0, "
            "last_visited_at INTEGER NOT NULL)"
        ),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS visits("
            "id INTEGER PRIMARY KEY, "
            "page_id INTEGER NOT NULL REFERENCES pages(id) ON DELETE CASCADE, "
            "visited_at INTEGER NOT NULL, "
            "transition INTEGER NOT NULL DEFAULT 0)"
        ),
        QStringLiteral("CREATE INDEX IF NOT EXISTS pages_last_visit ON pages(last_visited_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS visits_time ON visits(visited_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS visits_page ON visits(page_id)"),
        QStringLiteral("PRAGMA user_version = 1"),
    };
    for (const QString &statement : statements) {
        QSqlQuery query(m_database);
        if (!query.exec(statement)) {
            m_database.rollback();
            return fail(error, databaseError(tr("Cannot create history schema"), query));
        }
    }
    if (!m_database.commit())
        return fail(error, tr("Cannot commit history schema"));
    return true;
}

bool HistoryStore::pruneIfNeeded(QString *error)
{
    QSqlQuery count(m_database);
    if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM visits")) || !count.next())
        return fail(error, databaseError(tr("Cannot count history"), count));
    if (count.value(0).toInt() <= maximumVisits + 100)
        return true;
    if (!m_database.transaction())
        return fail(error, tr("Cannot start history cleanup"));

    const int removalCount = count.value(0).toInt() - maximumVisits;
    QSqlQuery affectedPages(m_database);
    affectedPages.prepare(QStringLiteral(
        "SELECT DISTINCT page_id FROM visits WHERE id IN ("
        "SELECT id FROM visits ORDER BY visited_at ASC, id ASC LIMIT ?"
        ")"
    ));
    affectedPages.addBindValue(removalCount);
    if (!affectedPages.exec()) {
        m_database.rollback();
        return fail(
            error,
            databaseError(tr("Cannot find history pages to prune"), affectedPages)
        );
    }
    QList<qint64> pageIds;
    while (affectedPages.next())
        pageIds.append(affectedPages.value(0).toLongLong());

    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral(
        "DELETE FROM visits WHERE id IN ("
        "SELECT id FROM visits ORDER BY visited_at ASC, id ASC LIMIT ?"
        ")"
    ));
    remove.addBindValue(removalCount);
    if (!remove.exec()) {
        m_database.rollback();
        return fail(error, databaseError(tr("Cannot prune history"), remove));
    }
    if (!rebuildPageAggregates(pageIds, error)) {
        m_database.rollback();
        return false;
    }
    if (!m_database.commit())
        return fail(error, tr("Cannot commit history cleanup"));
    return true;
}

bool HistoryStore::rebuildPageAggregates(const QList<qint64> &pageIds, QString *error)
{
    QSqlQuery count(m_database);
    count.prepare(QStringLiteral(
        "SELECT COUNT(*), "
        "SUM(CASE WHEN transition = ? THEN 1 ELSE 0 END), "
        "MAX(visited_at) FROM visits WHERE page_id = ?"
    ));
    QSqlQuery update(m_database);
    update.prepare(QStringLiteral(
        "UPDATE pages SET visit_count = ?, typed_count = ?, last_visited_at = ? WHERE id = ?"
    ));
    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM pages WHERE id = ?"));
    for (qint64 pageId : pageIds) {
        count.bindValue(0, transitionValue(HistoryTransition::Typed));
        count.bindValue(1, pageId);
        if (!count.exec() || !count.next())
            return fail(error, databaseError(tr("Cannot rebuild history page"), count));
        const int visits = count.value(0).toInt();
        if (visits == 0) {
            remove.bindValue(0, pageId);
            if (!remove.exec())
                return fail(error, databaseError(tr("Cannot remove empty history page"), remove));
            continue;
        }
        update.bindValue(0, visits);
        update.bindValue(1, count.value(1).toInt());
        update.bindValue(2, count.value(2).toLongLong());
        update.bindValue(3, pageId);
        if (!update.exec())
            return fail(error, databaseError(tr("Cannot update history page"), update));
    }
    return true;
}
