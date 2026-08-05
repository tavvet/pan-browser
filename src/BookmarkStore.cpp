#include "BookmarkStore.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

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

QString storedUrl(const QUrl &url)
{
    return url.toString(QUrl::FullyEncoded);
}

} // namespace

BookmarkStore::BookmarkStore(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path)
    , m_connectionName(
        QStringLiteral("panbrowser-bookmarks-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
        )
    )
{
}

BookmarkStore::~BookmarkStore()
{
    if (!m_database.isValid())
        return;
    const QString connectionName = m_connectionName;
    m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

bool BookmarkStore::open(QString *error)
{
    if (isOpen())
        return true;
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return fail(error, QStringLiteral("Cannot create bookmarks directory"));

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_path);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=3000"));
    if (!m_database.open()) {
        return fail(
            error,
            QStringLiteral("Cannot open bookmarks: %1").arg(m_database.lastError().text())
        );
    }

    QSqlQuery pragma(m_database);
    if (!pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        const QString message = databaseError(
            QStringLiteral("Cannot enable bookmarks journal"),
            pragma
        );
        m_database.close();
        return fail(error, message);
    }
    if (!pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"))) {
        const QString message = databaseError(
            QStringLiteral("Cannot configure bookmarks journal"),
            pragma
        );
        m_database.close();
        return fail(error, message);
    }
    if (!createSchema(error)) {
        m_database.close();
        return false;
    }
    return true;
}

bool BookmarkStore::isOpen() const
{
    return m_database.isValid() && m_database.isOpen();
}

QString BookmarkStore::path() const
{
    return m_path;
}

std::optional<Bookmark> BookmarkStore::bookmarkForUrl(const QUrl &sourceUrl, QString *error) const
{
    if (!isOpen()) {
        fail(error, QStringLiteral("Bookmarks are unavailable"));
        return std::nullopt;
    }
    const QUrl url = normalizedUrl(sourceUrl);
    if (!url.isValid())
        return std::nullopt;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, url, title, created_at, updated_at FROM bookmarks WHERE url = ?"
    ));
    query.addBindValue(storedUrl(url));
    if (!query.exec()) {
        fail(error, databaseError(QStringLiteral("Cannot find bookmark"), query));
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return bookmarkFromQuery(query);
}

QList<Bookmark> BookmarkStore::bookmarks(const QString &sourceFilter, QString *error) const
{
    QList<Bookmark> result;
    if (!isOpen()) {
        fail(error, QStringLiteral("Bookmarks are unavailable"));
        return result;
    }

    QSqlQuery query(m_database);
    QString sql = QStringLiteral(
        "SELECT id, url, title, created_at, updated_at FROM bookmarks "
    );
    const QString filter = normalizedText(sourceFilter);
    if (!filter.isEmpty()) {
        sql += QStringLiteral(
            "WHERE normalized_url LIKE ? ESCAPE '\\' "
            "OR normalized_title LIKE ? ESCAPE '\\' "
        );
    }
    sql += QStringLiteral("ORDER BY updated_at DESC, id DESC");
    query.prepare(sql);
    if (!filter.isEmpty()) {
        const QString pattern = escapedLikePattern(filter);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    if (!query.exec()) {
        fail(error, databaseError(QStringLiteral("Cannot read bookmarks"), query));
        return result;
    }
    while (query.next())
        result.append(bookmarkFromQuery(query));
    return result;
}

bool BookmarkStore::addOrUpdate(
    const QUrl &sourceUrl,
    const QString &sourceTitle,
    const QDateTime &updatedAt,
    QString *error
)
{
    if (!isOpen())
        return fail(error, QStringLiteral("Bookmarks are unavailable"));
    const QUrl url = normalizedUrl(sourceUrl);
    if (!url.isValid())
        return fail(error, QStringLiteral("Only HTTP and HTTPS pages can be bookmarked"));
    const QString title = sourceTitle.trimmed().isEmpty() ? url.host() : sourceTitle.trimmed();
    const qint64 timestamp = updatedAt.toUTC().toMSecsSinceEpoch();
    if (timestamp <= 0)
        return fail(error, QStringLiteral("Bookmark date is invalid"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO bookmarks(url, normalized_url, title, normalized_title, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(url) DO UPDATE SET "
        "title = excluded.title, "
        "normalized_title = excluded.normalized_title, "
        "updated_at = excluded.updated_at"
    ));
    query.addBindValue(storedUrl(url));
    query.addBindValue(normalizedText(url.toDisplayString(QUrl::PrettyDecoded)));
    query.addBindValue(title);
    query.addBindValue(normalizedText(title));
    query.addBindValue(timestamp);
    query.addBindValue(timestamp);
    if (!query.exec())
        return fail(error, databaseError(QStringLiteral("Cannot save bookmark"), query));
    emit bookmarksChanged();
    return true;
}

bool BookmarkStore::update(
    qint64 id,
    const QUrl &sourceUrl,
    const QString &sourceTitle,
    const QDateTime &updatedAt,
    QString *error
)
{
    if (!isOpen())
        return fail(error, QStringLiteral("Bookmarks are unavailable"));
    if (id <= 0)
        return fail(error, QStringLiteral("Bookmark id is invalid"));
    const QUrl url = normalizedUrl(sourceUrl);
    if (!url.isValid())
        return fail(error, QStringLiteral("Only HTTP and HTTPS pages can be bookmarked"));
    const QString title = sourceTitle.trimmed().isEmpty() ? url.host() : sourceTitle.trimmed();
    const qint64 timestamp = updatedAt.toUTC().toMSecsSinceEpoch();
    if (timestamp <= 0)
        return fail(error, QStringLiteral("Bookmark date is invalid"));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE bookmarks SET url = ?, normalized_url = ?, title = ?, "
        "normalized_title = ?, updated_at = ? WHERE id = ?"
    ));
    query.addBindValue(storedUrl(url));
    query.addBindValue(normalizedText(url.toDisplayString(QUrl::PrettyDecoded)));
    query.addBindValue(title);
    query.addBindValue(normalizedText(title));
    query.addBindValue(timestamp);
    query.addBindValue(id);
    if (!query.exec())
        return fail(error, databaseError(QStringLiteral("Cannot update bookmark"), query));
    if (query.numRowsAffected() == 0)
        return fail(error, QStringLiteral("Bookmark no longer exists"));
    emit bookmarksChanged();
    return true;
}

bool BookmarkStore::remove(qint64 id, QString *error)
{
    return remove(QList<qint64>{id}, error);
}

bool BookmarkStore::remove(const QList<qint64> &ids, QString *error)
{
    if (ids.isEmpty())
        return true;
    if (!isOpen())
        return fail(error, QStringLiteral("Bookmarks are unavailable"));
    if (!m_database.transaction())
        return fail(error, QStringLiteral("Cannot start bookmark deletion"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM bookmarks WHERE id = ?"));
    bool changed = false;
    for (qint64 id : ids) {
        if (id <= 0)
            continue;
        query.bindValue(0, id);
        if (!query.exec()) {
            m_database.rollback();
            return fail(error, databaseError(QStringLiteral("Cannot remove bookmark"), query));
        }
        changed = changed || query.numRowsAffected() > 0;
    }
    if (!m_database.commit())
        return fail(error, QStringLiteral("Cannot commit bookmark deletion"));
    if (changed)
        emit bookmarksChanged();
    return true;
}

bool BookmarkStore::removeUrl(const QUrl &sourceUrl, QString *error)
{
    if (!isOpen())
        return fail(error, QStringLiteral("Bookmarks are unavailable"));
    const QUrl url = normalizedUrl(sourceUrl);
    if (!url.isValid())
        return true;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM bookmarks WHERE url = ?"));
    query.addBindValue(storedUrl(url));
    if (!query.exec())
        return fail(error, databaseError(QStringLiteral("Cannot remove bookmark"), query));
    if (query.numRowsAffected() > 0)
        emit bookmarksChanged();
    return true;
}

bool BookmarkStore::clear(QString *error)
{
    if (!isOpen())
        return fail(error, QStringLiteral("Bookmarks are unavailable"));
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("DELETE FROM bookmarks")))
        return fail(error, databaseError(QStringLiteral("Cannot clear bookmarks"), query));
    emit bookmarksChanged();
    return true;
}

QUrl BookmarkStore::normalizedUrl(const QUrl &source)
{
    QUrl url(source);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid()
        || url.host().isEmpty()
        || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        return {};
    }
    url.setScheme(scheme);
    url.setHost(url.host().toLower());
    url.setUserName(QString());
    url.setPassword(QString());
    if ((scheme == QStringLiteral("http") && url.port() == 80)
        || (scheme == QStringLiteral("https") && url.port() == 443)) {
        url.setPort(-1);
    }
    if (url.path().isEmpty())
        url.setPath(QStringLiteral("/"));
    return url;
}

bool BookmarkStore::createSchema(QString *error)
{
    QSqlQuery version(m_database);
    if (!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next()) {
        return fail(
            error,
            databaseError(QStringLiteral("Cannot read bookmarks version"), version)
        );
    }
    const int schemaVersion = version.value(0).toInt();
    if (schemaVersion != 0 && schemaVersion != 1) {
        return fail(
            error,
            QStringLiteral("Unsupported bookmarks version: %1").arg(schemaVersion)
        );
    }
    if (!m_database.transaction()) {
        return fail(
            error,
            QStringLiteral("Cannot start bookmarks schema transaction")
        );
    }

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS bookmarks("
            "id INTEGER PRIMARY KEY, "
            "url TEXT NOT NULL UNIQUE, "
            "normalized_url TEXT NOT NULL, "
            "title TEXT NOT NULL, "
            "normalized_title TEXT NOT NULL, "
            "created_at INTEGER NOT NULL, "
            "updated_at INTEGER NOT NULL)"
        ),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS bookmarks_updated ON bookmarks(updated_at DESC)"
        ),
        QStringLiteral("PRAGMA user_version = 1"),
    };
    for (const QString &statement : statements) {
        QSqlQuery query(m_database);
        if (!query.exec(statement)) {
            m_database.rollback();
            return fail(
                error,
                databaseError(QStringLiteral("Cannot create bookmarks schema"), query)
            );
        }
    }
    if (!m_database.commit())
        return fail(error, QStringLiteral("Cannot commit bookmarks schema"));
    return true;
}

Bookmark BookmarkStore::bookmarkFromQuery(const QSqlQuery &query)
{
    Bookmark bookmark;
    bookmark.id = query.value(0).toLongLong();
    bookmark.url = QUrl(query.value(1).toString(), QUrl::StrictMode);
    bookmark.title = query.value(2).toString();
    bookmark.createdAt = QDateTime::fromMSecsSinceEpoch(
        query.value(3).toLongLong(),
        QTimeZone::UTC
    );
    bookmark.updatedAt = QDateTime::fromMSecsSinceEpoch(
        query.value(4).toLongLong(),
        QTimeZone::UTC
    );
    return bookmark;
}
