#pragma once

#include "BrowserPreferences.h"

#include <QString>
#include <QStringList>
#include <QTranslator>

class QCoreApplication;

class LocalizationManager final {
public:
    static QString resolveLanguage(
        InterfaceLanguage preference,
        const QStringList &systemUiLanguages
    );

    QString install(
        QCoreApplication &application,
        InterfaceLanguage preference,
        const QStringList &systemUiLanguages
    );

private:
    QTranslator m_applicationTranslator;
};
