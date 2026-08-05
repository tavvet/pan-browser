#pragma once

#include "BrowserPreferences.h"
#include "SearchSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class BrowserProfile;
class HistorySettingsPage;
class HistoryStore;
class SearchSettingsPage;
class TrustRulesDialog;

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Page {
        General,
        Search,
        History,
        PrivacyData,
        TrustRules,
    };

    SettingsDialog(
        const QString &configurationPath,
        const QString &searchConfigurationPath,
        const BrowserPreferences &preferences,
        const SearchSettings &searchSettings,
        BrowserProfile *profile,
        HistoryStore *historyStore,
        const QUrl &currentUrl,
        Page initialPage,
        QWidget *parent = nullptr
    );

    bool load(QString *error = nullptr);
    [[nodiscard]] BrowserPreferences preferences() const;
    [[nodiscard]] SearchSettings searchSettings() const;

private:
    void createInterface(const QUrl &currentUrl, Page initialPage);
    void selectPage(Page page);
    BrowserPreferences preferencesFromControls() const;
    void saveAndClose();

    QString m_configurationPath;
    QString m_searchConfigurationPath;
    BrowserPreferences m_preferences;
    SearchSettings m_searchSettings;
    BrowserProfile *m_profile = nullptr;
    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLineEdit *m_startPage = nullptr;
    QComboBox *m_startupMode = nullptr;
    QCheckBox *m_persistSessionCookies = nullptr;
    SearchSettingsPage *m_searchPage = nullptr;
    HistorySettingsPage *m_historyPage = nullptr;
    TrustRulesDialog *m_trustRules = nullptr;
};
