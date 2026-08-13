#include "VideoTranslationSettings.h"

#include "PrivateData.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

constexpr qsizetype maximumSettingsFileSize = 64 * 1024;
constexpr qsizetype maximumPathLength = 4096;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString text(const char *source)
{
    return QCoreApplication::translate("VideoTranslationSettings", source);
}

QString normalizedPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return {};
    return QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
}

bool isUsableSource(const QString &path)
{
    const QFileInfo source(path);
    return source.isFile()
        && source.suffix().compare(QStringLiteral("js"), Qt::CaseInsensitive) == 0;
}

} // namespace

VideoTranslationSettings VideoTranslationSettings::defaults()
{
    return {};
}

bool VideoTranslationSettings::load(const QString &path, QString *error)
{
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VideoTranslationSettings",
                "Cannot open %1: %2"
            )).arg(path, file.errorString())
        );
    }
    if (file.size() > maximumSettingsFileSize) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VideoTranslationSettings",
                "Video translation settings file is too large"
            ))
        );
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VideoTranslationSettings",
                "Invalid video translation settings JSON: %1"
            )).arg(parseError.errorString())
        );
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VideoTranslationSettings",
                "Unsupported video translation settings version"
            ))
        );
    }
    if (!root.value(QStringLiteral("enabled")).isBool()
        || !root.value(QStringLiteral("sourcePath")).isString()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VideoTranslationSettings",
                "Video translation settings contain invalid values"
            ))
        );
    }

    VideoTranslationSettings loaded;
    loaded.m_enabled = root.value(QStringLiteral("enabled")).toBool();
    loaded.m_sourcePath = normalizedPath(
        root.value(QStringLiteral("sourcePath")).toString()
    );
    if (!loaded.validate(error))
        return false;
    *this = loaded;
    return PrivateData::restrictFile(path, error);
}

bool VideoTranslationSettings::save(const QString &path, QString *error) const
{
    if (error)
        error->clear();
    if (!validate(error))
        return false;

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("enabled"), m_enabled);
    root.insert(QStringLiteral("sourcePath"), m_sourcePath);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VideoTranslationSettings",
                "Cannot write %1: %2"
            )).arg(path, file.errorString())
        );
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VideoTranslationSettings",
                "Cannot replace %1: %2"
            )).arg(path, file.errorString())
        );
    }
    return PrivateData::restrictFile(path, error);
}

bool VideoTranslationSettings::validate(QString *error) const
{
    if (error)
        error->clear();
    if (m_sourcePath.size() > maximumPathLength) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VideoTranslationSettings",
                "Video translation source path is too long"
            ))
        );
    }
    if (m_enabled) {
        if (m_sourcePath.isEmpty()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "VideoTranslationSettings",
                    "Choose the official VOT userscript"
                ))
            );
        }
        if (!isUsableSource(m_sourcePath)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "VideoTranslationSettings",
                    "The VOT source must be an existing JavaScript file"
                ))
            );
        }
    }
    return true;
}

bool VideoTranslationSettings::enabled() const
{
    return m_enabled;
}

void VideoTranslationSettings::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

QString VideoTranslationSettings::sourcePath() const
{
    return m_sourcePath;
}

void VideoTranslationSettings::setSourcePath(const QString &path)
{
    m_sourcePath = normalizedPath(path);
}
