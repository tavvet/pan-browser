#include "Localization.h"

#include <QCoreApplication>
#include <QLocale>
#include <QLoggingCategory>

namespace {

// QDialogButtonBox and QMessageBox obtain these labels from QPlatformTheme.
// Keeping them in the app catalog avoids a runtime dependency on qttranslations.
[[maybe_unused]] constexpr const char *standardButtonTranslations[] = {
    QT_TRANSLATE_NOOP("QPlatformTheme", "OK"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Save"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Close"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Yes"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "No"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "&Yes"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "&No"),
};

} // namespace

QString LocalizationManager::resolveLanguage(
    InterfaceLanguage preference,
    const QStringList &systemUiLanguages
)
{
    if (preference == InterfaceLanguage::English)
        return QStringLiteral("en");
    if (preference == InterfaceLanguage::Russian)
        return QStringLiteral("ru");

    for (const QString &tag : systemUiLanguages) {
        if (QLocale(tag).language() == QLocale::Russian)
            return QStringLiteral("ru");
        if (QLocale(tag).language() == QLocale::English)
            return QStringLiteral("en");
    }
    return QStringLiteral("en");
}

QString LocalizationManager::install(
    QCoreApplication &application,
    InterfaceLanguage preference,
    const QStringList &systemUiLanguages
)
{
    const QString language = resolveLanguage(preference, systemUiLanguages);
    const QString catalog = QStringLiteral("panbrowser_%1").arg(language);
    if (m_applicationTranslator.load(catalog, QStringLiteral(":/i18n"))) {
        application.installTranslator(&m_applicationTranslator);
    } else if (language != QStringLiteral("en")) {
        qWarning().noquote() << "[PanBrowser localization] Cannot load" << catalog;
        return QStringLiteral("en");
    }

    return language;
}
