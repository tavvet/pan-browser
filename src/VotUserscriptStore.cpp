#include "VotUserscriptStore.h"

#include "PrivateData.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>

#include <utility>

namespace {

constexpr qsizetype maximumStoreSize = 4 * 1024 * 1024;
constexpr qsizetype maximumEntryCount = 1024;
constexpr qsizetype maximumKeyLength = 256;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString text(const char *source)
{
    return QCoreApplication::translate("VotUserscriptStore", source);
}

bool validKey(const QString &name)
{
    return !name.isEmpty() && name.size() <= maximumKeyLength;
}

QByteArray serialized(const QJsonObject &values)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("values"), values);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace

VotUserscriptStore::VotUserscriptStore(QString path, QObject *parent)
    : QObject(parent)
    , m_path(std::move(path))
{
}

bool VotUserscriptStore::load(QString *error)
{
    if (error)
        error->clear();
    m_values = {};
    if (m_path.isEmpty()) {
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptStore",
            "The VOT storage path is unavailable"
        )));
    }
    if (!QFileInfo::exists(m_path))
        return true;

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VotUserscriptStore",
                "Cannot open VOT storage: %1"
            )).arg(file.errorString())
        );
    }
    if (file.size() <= 0 || file.size() > maximumStoreSize) {
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptStore",
            "The VOT storage file has an invalid size"
        )));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VotUserscriptStore",
                "Invalid VOT storage JSON: %1"
            )).arg(parseError.errorString())
        );
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1
        || !root.value(QStringLiteral("values")).isObject()) {
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptStore",
            "Unsupported VOT storage format"
        )));
    }
    const QJsonObject loaded = root.value(QStringLiteral("values")).toObject();
    if (loaded.size() > maximumEntryCount) {
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptStore",
            "The VOT storage contains too many values"
        )));
    }
    for (auto iterator = loaded.constBegin(); iterator != loaded.constEnd(); ++iterator) {
        if (!validKey(iterator.key())) {
            return fail(error, text(QT_TRANSLATE_NOOP(
                "VotUserscriptStore",
                "The VOT storage contains an invalid key"
            )));
        }
    }
    m_values = loaded;
    return PrivateData::restrictFile(m_path, error);
}

bool VotUserscriptStore::setValue(
    const QString &name,
    const QJsonValue &value,
    QString *error
)
{
    if (error)
        error->clear();
    if (!validKey(name) || value.isUndefined()) {
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptStore",
            "The VOT storage update is invalid"
        )));
    }
    QJsonObject updated = m_values;
    updated.insert(name, value);
    if (updated.size() > maximumEntryCount || serialized(updated).size() > maximumStoreSize) {
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptStore",
            "The VOT storage limit has been reached"
        )));
    }
    if (!save(updated, error))
        return false;
    m_values = std::move(updated);
    emit valueChanged(name, value, false);
    return true;
}

bool VotUserscriptStore::removeValue(const QString &name, QString *error)
{
    if (error)
        error->clear();
    if (!validKey(name)) {
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptStore",
            "The VOT storage key is invalid"
        )));
    }
    if (!m_values.contains(name))
        return true;
    QJsonObject updated = m_values;
    updated.remove(name);
    if (!save(updated, error))
        return false;
    m_values = std::move(updated);
    emit valueChanged(name, QJsonValue(), true);
    return true;
}

QJsonObject VotUserscriptStore::values() const
{
    return m_values;
}

QString VotUserscriptStore::path() const
{
    return m_path;
}

bool VotUserscriptStore::save(const QJsonObject &values, QString *error)
{
    const QFileInfo info(m_path);
    if (!PrivateData::ensureDirectory(info.absolutePath(), error))
        return false;
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VotUserscriptStore",
                "Cannot write VOT storage: %1"
            )).arg(file.errorString())
        );
    }
    const QByteArray contents = serialized(values);
    if (file.write(contents) != contents.size() || !file.commit()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VotUserscriptStore",
                "Cannot replace VOT storage: %1"
            )).arg(file.errorString())
        );
    }
    return PrivateData::restrictFile(m_path, error);
}
