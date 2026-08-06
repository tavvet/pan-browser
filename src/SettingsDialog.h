#pragma once

#include "BrowserPreferences.h"
#include "DnsSettings.h"
#include "ProxySettings.h"
#include "SearchSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class BrowserProfile;
class DiagnosticsPage;
class DnsSettingsPage;
class HistorySettingsPage;
class HistoryStore;
class ProxySettingsPage;
class SearchSettingsPage;
class TrustRulesDialog;
class WebAppsSettingsPage;
class WebAppStore;

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Page {
        General,
        Search,
        History,
        WebApps,
        PrivacyData,
        Dns,
        Proxy,
        TrustRules,
        Diagnostics,
    };

    SettingsDialog(
        const QString &configurationPath,
        const QString &searchConfigurationPath,
        const QString &dnsConfigurationPath,
        const QString &proxyConfigurationPath,
        const BrowserPreferences &preferences,
        const SearchSettings &searchSettings,
        const DnsSettings &dnsSettings,
        const ProxySettings &proxySettings,
        const ProxySettings &activeProxySettings,
        bool networkBlockedByProxyError,
        BrowserProfile *profile,
        HistoryStore *historyStore,
        WebAppStore *webAppStore,
        const QUrl &currentUrl,
        Page initialPage,
        QWidget *parent = nullptr
    );

    bool load(QString *error = nullptr);
    [[nodiscard]] BrowserPreferences preferences() const;
    [[nodiscard]] SearchSettings searchSettings() const;
    [[nodiscard]] DnsSettings dnsSettings() const;
    [[nodiscard]] ProxySettings proxySettings() const;

signals:
    void webAppOpenRequested(const QString &id);

private:
    void createInterface(const QUrl &currentUrl, Page initialPage);
    void selectPage(Page page);
    BrowserPreferences preferencesFromControls() const;
    void saveAndClose();

    QString m_configurationPath;
    QString m_searchConfigurationPath;
    QString m_dnsConfigurationPath;
    QString m_proxyConfigurationPath;
    BrowserPreferences m_preferences;
    SearchSettings m_searchSettings;
    DnsSettings m_dnsSettings;
    ProxySettings m_proxySettings;
    ProxySettings m_activeProxySettings;
    bool m_networkBlockedByProxyError = false;
    BrowserProfile *m_profile = nullptr;
    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLineEdit *m_startPage = nullptr;
    QComboBox *m_startupMode = nullptr;
    QComboBox *m_interfaceLanguage = nullptr;
    QCheckBox *m_persistSessionCookies = nullptr;
    SearchSettingsPage *m_searchPage = nullptr;
    DnsSettingsPage *m_dnsPage = nullptr;
    ProxySettingsPage *m_proxyPage = nullptr;
    HistorySettingsPage *m_historyPage = nullptr;
    WebAppsSettingsPage *m_webAppsPage = nullptr;
    DiagnosticsPage *m_diagnosticsPage = nullptr;
    TrustRulesDialog *m_trustRules = nullptr;
};
