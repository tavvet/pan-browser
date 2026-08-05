#pragma once

#include "BrowserPreferences.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class TrustRulesDialog;

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Page {
        General,
        TrustRules,
    };

    SettingsDialog(
        const QString &configurationPath,
        const BrowserPreferences &preferences,
        const QUrl &currentUrl,
        Page initialPage,
        QWidget *parent = nullptr
    );

    bool load(QString *error = nullptr);
    [[nodiscard]] BrowserPreferences preferences() const;

private:
    void createInterface(const QUrl &currentUrl, Page initialPage);
    void selectPage(Page page);
    BrowserPreferences preferencesFromControls() const;
    void saveAndClose();

    QString m_configurationPath;
    BrowserPreferences m_preferences;
    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLineEdit *m_startPage = nullptr;
    QComboBox *m_startupMode = nullptr;
    QCheckBox *m_persistSessionCookies = nullptr;
    TrustRulesDialog *m_trustRules = nullptr;
};
