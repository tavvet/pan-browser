#pragma once

#include "BrowserPreferences.h"

#include <QCoreApplication>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QUrl;

class GeneralSettingsPage final : public QWidget {
    Q_DECLARE_TR_FUNCTIONS(SettingsDialog)

public:
    explicit GeneralSettingsPage(
        const BrowserPreferences &preferences,
        const QUrl &currentUrl,
        QWidget *parent = nullptr
    );

    [[nodiscard]] BrowserPreferences applyTo(BrowserPreferences preferences) const;

private:
    void updateRestoreHint();

    QLineEdit *m_startPage = nullptr;
    QComboBox *m_startupMode = nullptr;
    QComboBox *m_interfaceLanguage = nullptr;
    QCheckBox *m_persistSessionCookies = nullptr;
    QCheckBox *m_developerToolsEnabled = nullptr;
    QWidget *m_restoreSignInHint = nullptr;
};
