#include "SessionStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

constexpr int maximumRestoredTabs = 30;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

bool isRestorableUrl(const QUrl &url)
{
    return url.isValid()
        && !url.host().isEmpty()
        && (url.scheme() == QStringLiteral("http")
            || url.scheme() == QStringLiteral("https"));
}

} // namespace

SessionStore::SessionStore(QString path)
    : m_path(std::move(path))
{
}

BrowserSession SessionStore::load(QString *error) const
{
    if (error)
        error->clear();
    BrowserSession session;
    QFile file(m_path);
    if (!file.exists())
        return session;
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, QStringLiteral("Cannot open %1: %2").arg(m_path, file.errorString()));
        return session;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, QStringLiteral("Invalid session file: %1").arg(parseError.errorString()));
        return session;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1) {
        fail(error, QStringLiteral("Unsupported session file version"));
        return session;
    }

    const QJsonArray tabs = root.value(QStringLiteral("tabs")).toArray();
    for (const QJsonValue &value : tabs) {
        if (session.tabs.size() >= maximumRestoredTabs)
            break;
        const QJsonObject object = value.toObject();
        const QUrl url(object.value(QStringLiteral("url")).toString());
        if (!isRestorableUrl(url))
            continue;
        session.tabs.append({url, object.value(QStringLiteral("title")).toString()});
    }

    const int requestedIndex = root.value(QStringLiteral("activeIndex")).toInt(0);
    session.activeIndex = session.tabs.isEmpty()
        ? 0
        : qBound(0, requestedIndex, static_cast<int>(session.tabs.size() - 1));
    return session;
}

bool SessionStore::save(const BrowserSession &session, QString *error) const
{
    if (error)
        error->clear();
    QJsonArray tabs;
    int persistedActiveIndex = 0;
    for (qsizetype index = 0; index < session.tabs.size(); ++index) {
        if (tabs.size() >= maximumRestoredTabs)
            break;
        const SessionTab &tab = session.tabs.at(index);
        if (!isRestorableUrl(tab.url))
            continue;
        if (index <= session.activeIndex)
            persistedActiveIndex = static_cast<int>(tabs.size());
        QJsonObject object;
        object.insert(QStringLiteral("url"), tab.url.toString());
        object.insert(QStringLiteral("title"), tab.title);
        tabs.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("activeIndex"), persistedActiveIndex);
    root.insert(QStringLiteral("tabs"), tabs);

    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return fail(error, QStringLiteral("Cannot create session directory"));

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString()));
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size())
        return fail(error, QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString()));
    if (!file.commit())
        return fail(error, QStringLiteral("Cannot commit %1: %2").arg(m_path, file.errorString()));
    return true;
}

bool SessionStore::clear(QString *error) const
{
    if (error)
        error->clear();
    if (!QFile::exists(m_path) || QFile::remove(m_path))
        return true;
    return fail(error, QStringLiteral("Cannot remove %1").arg(m_path));
}

QString SessionStore::path() const
{
    return m_path;
}
