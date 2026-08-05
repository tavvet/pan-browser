#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QUrl>

#include <optional>

class QSqlQuery;

struct Bookmark {
    qint64 id = 0;
    QUrl url;
    QString title;
    QDateTime createdAt;
    QDateTime updatedAt;
};

class BookmarkStore final : public QObject {
    Q_OBJECT

public:
    explicit BookmarkStore(const QString &path, QObject *parent = nullptr);
    ~BookmarkStore() override;

    bool open(QString *error = nullptr);
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString path() const;

    [[nodiscard]] std::optional<Bookmark> bookmarkForUrl(
        const QUrl &url,
        QString *error = nullptr
    ) const;
    [[nodiscard]] QList<Bookmark> bookmarks(
        const QString &filter = QString(),
        QString *error = nullptr
    ) const;
    bool addOrUpdate(
        const QUrl &url,
        const QString &title,
        const QDateTime &updatedAt = QDateTime::currentDateTimeUtc(),
        QString *error = nullptr
    );
    bool update(
        qint64 id,
        const QUrl &url,
        const QString &title,
        const QDateTime &updatedAt = QDateTime::currentDateTimeUtc(),
        QString *error = nullptr
    );
    bool remove(qint64 id, QString *error = nullptr);
    bool remove(const QList<qint64> &ids, QString *error = nullptr);
    bool removeUrl(const QUrl &url, QString *error = nullptr);
    bool clear(QString *error = nullptr);

    [[nodiscard]] static QUrl normalizedUrl(const QUrl &url);

signals:
    void bookmarksChanged();

private:
    bool createSchema(QString *error);
    static Bookmark bookmarkFromQuery(const QSqlQuery &query);

    QString m_path;
    QString m_connectionName;
    QSqlDatabase m_database;
};
