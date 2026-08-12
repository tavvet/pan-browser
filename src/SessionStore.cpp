#include "SessionStore.h"

#include "PrivateData.h"
#include "UrlSanitization.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <limits>
#include <utility>

namespace {

constexpr int maximumRestoredTabs = 30;
constexpr int currentSessionVersion = 2;

struct IndexedSessionTab {
    SessionTab tab;
    int sourceIndex = 0;
};

class BoundedSessionTabs {
public:
    bool append(IndexedSessionTab tab)
    {
        QList<IndexedSessionTab> &target = tab.tab.pinned ? m_pinned : m_regular;
        if (target.size() >= maximumRestoredTabs)
            return false;
        target.append(std::move(tab));
        return true;
    }

    [[nodiscard]] QList<IndexedSessionTab> canonical() const
    {
        QList<IndexedSessionTab> result;
        result.reserve(maximumRestoredTabs);
        for (const IndexedSessionTab &tab : m_pinned)
            result.append(tab);
        for (const IndexedSessionTab &tab : m_regular) {
            if (result.size() >= maximumRestoredTabs)
                break;
            result.append(tab);
        }
        return result;
    }

    [[nodiscard]] qsizetype retainedCount() const
    {
        return m_pinned.size() + m_regular.size();
    }

private:
    QList<IndexedSessionTab> m_pinned;
    QList<IndexedSessionTab> m_regular;
};

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

int mappedActiveIndex(const QList<IndexedSessionTab> &tabs, int sourceIndex)
{
    if (tabs.isEmpty())
        return 0;

    int closestIndex = 0;
    qint64 closestDistance = std::numeric_limits<qint64>::max();
    for (int index = 0; index < tabs.size(); ++index) {
        const qint64 delta = static_cast<qint64>(tabs.at(index).sourceIndex)
            - static_cast<qint64>(sourceIndex);
        const qint64 distance = delta < 0 ? -delta : delta;
        if (distance < closestDistance) {
            closestIndex = index;
            closestDistance = distance;
        }
        if (distance == 0)
            break;
    }
    return closestIndex;
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
    if (!PrivateData::restrictFile(m_path, error))
        return session;
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, QStringLiteral("Cannot open %1: %2").arg(m_path, file.errorString()));
        return session;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, QStringLiteral("Invalid session file: %1").arg(parseError.errorString()));
        return session;
    }

    const QJsonObject root = document.object();
    const int version = root.value(QStringLiteral("version")).toInt(-1);
    if (version != 1 && version != currentSessionVersion) {
        fail(error, QStringLiteral("Unsupported session file version"));
        return session;
    }

    const QJsonArray tabs = root.value(QStringLiteral("tabs")).toArray();
    bool requiresRewrite = version != currentSessionVersion;
    BoundedSessionTabs restoredTabs;
    for (int sourceIndex = 0; sourceIndex < tabs.size(); ++sourceIndex) {
        const QJsonValue value = tabs.at(sourceIndex);
        const QJsonObject object = value.toObject();
        if (version == currentSessionVersion
            && !object.value(QStringLiteral("pinned")).isBool()) {
            fail(error, QStringLiteral("Invalid pinned-tab state in session file"));
            return {};
        }
        const QUrl storedUrl(object.value(QStringLiteral("url")).toString());
        const QUrl url = UrlSanitization::httpUrlForPersistence(
            storedUrl
        );
        if (!url.isValid()) {
            requiresRewrite = true;
            continue;
        }
        requiresRewrite = requiresRewrite || storedUrl != url;
        if (!restoredTabs.append({{
            url,
            object.value(QStringLiteral("title")).toString(),
            version == currentSessionVersion
                && object.value(QStringLiteral("pinned")).toBool(),
        }, sourceIndex})) {
            requiresRewrite = true;
        }
    }

    const int requestedIndex = root.value(QStringLiteral("activeIndex")).toInt(0);
    const QList<IndexedSessionTab> canonical = restoredTabs.canonical();
    requiresRewrite = requiresRewrite
        || canonical.size() != restoredTabs.retainedCount();
    for (int index = 0; index < canonical.size(); ++index) {
        session.tabs.append(canonical.at(index).tab);
        requiresRewrite = requiresRewrite
            || canonical.at(index).sourceIndex != index;
    }
    session.activeIndex = mappedActiveIndex(canonical, requestedIndex);
    if (requiresRewrite && !save(session, error))
        return {};
    return session;
}

bool SessionStore::save(const BrowserSession &session, QString *error) const
{
    if (error)
        error->clear();
    BoundedSessionTabs sanitizedTabs;
    for (qsizetype index = 0; index < session.tabs.size(); ++index) {
        const SessionTab &tab = session.tabs.at(index);
        const QUrl url = UrlSanitization::httpUrlForPersistence(tab.url);
        if (!url.isValid())
            continue;
        sanitizedTabs.append({
            {url, tab.title, tab.pinned},
            static_cast<int>(index),
        });
    }
    const QList<IndexedSessionTab> canonical = sanitizedTabs.canonical();
    const int activeSourceIndex = session.tabs.isEmpty()
        ? 0
        : qBound(0, session.activeIndex, static_cast<int>(session.tabs.size() - 1));
    const int persistedActiveIndex = mappedActiveIndex(canonical, activeSourceIndex);

    QJsonArray tabs;
    for (const IndexedSessionTab &indexedTab : canonical) {
        const SessionTab &tab = indexedTab.tab;
        QJsonObject object;
        object.insert(QStringLiteral("url"), tab.url.toString(QUrl::FullyEncoded));
        object.insert(QStringLiteral("title"), tab.title);
        object.insert(QStringLiteral("pinned"), tab.pinned);
        tabs.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), currentSessionVersion);
    root.insert(QStringLiteral("activeIndex"), persistedActiveIndex);
    root.insert(QStringLiteral("tabs"), tabs);

    if (!PrivateData::ensureDirectory(QFileInfo(m_path).absolutePath(), error))
        return false;

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString()));
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size())
        return fail(error, QStringLiteral("Cannot write %1: %2").arg(m_path, file.errorString()));
    if (!file.commit())
        return fail(error, QStringLiteral("Cannot commit %1: %2").arg(m_path, file.errorString()));
    return PrivateData::restrictFile(m_path, error);
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
