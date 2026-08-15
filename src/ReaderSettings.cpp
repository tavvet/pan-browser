#include "ReaderSettings.h"

#include "PrivateData.h"

#include <QCoreApplication>
#include <QSettings>

#include <algorithm>

namespace {

constexpr auto organization = "PanBrowser";
constexpr auto application = "PanBrowser";

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

ReaderTheme themeFromName(const QString &name)
{
    if (name == QStringLiteral("light"))
        return ReaderTheme::Light;
    if (name == QStringLiteral("sepia"))
        return ReaderTheme::Sepia;
    if (name == QStringLiteral("dark"))
        return ReaderTheme::Dark;
    return ReaderTheme::System;
}

ReaderTypeface typefaceFromName(const QString &name)
{
    return name == QStringLiteral("sans")
        ? ReaderTypeface::SansSerif
        : ReaderTypeface::Serif;
}

} // namespace

ReaderSettings ReaderSettings::load(QString *error)
{
    QSettings settings(QString::fromLatin1(organization), QString::fromLatin1(application));
    ReaderSettings result;
    result.m_theme = themeFromName(
        settings.value(QStringLiteral("Reader/theme"), QStringLiteral("system")).toString()
    );
    result.m_typeface = typefaceFromName(
        settings.value(QStringLiteral("Reader/typeface"), QStringLiteral("serif")).toString()
    );
    result.m_textSize = std::clamp(
        settings.value(QStringLiteral("Reader/textSize"), 20).toInt(),
        minimumTextSize,
        maximumTextSize
    );
    result.m_contentWidth = std::clamp(
        settings.value(QStringLiteral("Reader/contentWidth"), 720).toInt(),
        minimumContentWidth,
        maximumContentWidth
    );
    if (settings.status() != QSettings::NoError && error) {
        *error = QCoreApplication::translate(
            "ReaderSettings",
            "Cannot read reader mode settings"
        );
    }
    return result;
}

bool ReaderSettings::save(QString *error) const
{
    if (!validate(error))
        return false;

    QSettings settings(QString::fromLatin1(organization), QString::fromLatin1(application));
    settings.setValue(QStringLiteral("Reader/theme"), themeName());
    settings.setValue(QStringLiteral("Reader/typeface"), typefaceName());
    settings.setValue(QStringLiteral("Reader/textSize"), m_textSize);
    settings.setValue(QStringLiteral("Reader/contentWidth"), m_contentWidth);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return fail(error, QCoreApplication::translate(
            "ReaderSettings",
            "Cannot write reader mode settings"
        ));
    }
    return PrivateData::restrictFile(settings.fileName(), error);
}

bool ReaderSettings::validate(QString *error) const
{
    if (m_textSize < minimumTextSize || m_textSize > maximumTextSize) {
        return fail(error, QCoreApplication::translate(
            "ReaderSettings",
            "Reader text size is outside the supported range"
        ));
    }
    if (m_contentWidth < minimumContentWidth || m_contentWidth > maximumContentWidth) {
        return fail(error, QCoreApplication::translate(
            "ReaderSettings",
            "Reader content width is outside the supported range"
        ));
    }
    return true;
}

ReaderTheme ReaderSettings::theme() const
{
    return m_theme;
}

void ReaderSettings::setTheme(ReaderTheme theme)
{
    m_theme = theme;
}

ReaderTypeface ReaderSettings::typeface() const
{
    return m_typeface;
}

void ReaderSettings::setTypeface(ReaderTypeface typeface)
{
    m_typeface = typeface;
}

int ReaderSettings::textSize() const
{
    return m_textSize;
}

void ReaderSettings::setTextSize(int size)
{
    m_textSize = std::clamp(size, minimumTextSize, maximumTextSize);
}

int ReaderSettings::contentWidth() const
{
    return m_contentWidth;
}

void ReaderSettings::setContentWidth(int width)
{
    m_contentWidth = std::clamp(width, minimumContentWidth, maximumContentWidth);
}

QString ReaderSettings::themeName() const
{
    switch (m_theme) {
    case ReaderTheme::System:
        return QStringLiteral("system");
    case ReaderTheme::Light:
        return QStringLiteral("light");
    case ReaderTheme::Sepia:
        return QStringLiteral("sepia");
    case ReaderTheme::Dark:
        return QStringLiteral("dark");
    }
    return QStringLiteral("system");
}

QString ReaderSettings::typefaceName() const
{
    return m_typeface == ReaderTypeface::SansSerif
        ? QStringLiteral("sans")
        : QStringLiteral("serif");
}
