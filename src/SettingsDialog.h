#pragma once

#include "BrowserPreferences.h"
#include "DnsSettings.h"
#include "ProxySettings.h"
#include "SearchSettings.h"

#include <QDialog>
#include <QHash>

class QIcon;
class QListWidget;
class QStackedWidget;
class BrowserProfile;
class DiagnosticsPage;
class DnsSettingsPage;
class GeneralSettingsPage;
class HistorySettingsPage;
class HistoryStore;
class ProxySettingsPage;
class PrivacyDataSettingsPage;
class SearchSettingsPage;
class TrustRulesDialog;
class WebAppsSettingsPage;
class WebAppStore;

struct SettingsDialogContext {
    QString trustConfigurationPath;
    QString searchConfigurationPath;
    QString dnsConfigurationPath;
    QString proxyConfigurationPath;
    BrowserPreferences preferences;
    SearchSettings searchSettings;
    DnsSettings dnsSettings;
    ProxySettings proxySettings;
    ProxySettings activeProxySettings;
    bool networkBlockedByProxyError = false;
    BrowserProfile *profile = nullptr;
    HistoryStore *historyStore = nullptr;
    WebAppStore *webAppStore = nullptr;
};

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
        const SettingsDialogContext &context,
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
    void createInterface(
        const QUrl &currentUrl,
        Page initialPage,
        HistoryStore *historyStore,
        WebAppStore *webAppStore
    );
    void registerPage(Page page, const QIcon &icon, const QString &title, QWidget *widget);
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
    QHash<int, QWidget *> m_pageWidgets;
    GeneralSettingsPage *m_generalPage = nullptr;
    PrivacyDataSettingsPage *m_privacyDataPage = nullptr;
    SearchSettingsPage *m_searchPage = nullptr;
    DnsSettingsPage *m_dnsPage = nullptr;
    ProxySettingsPage *m_proxyPage = nullptr;
    HistorySettingsPage *m_historyPage = nullptr;
    WebAppsSettingsPage *m_webAppsPage = nullptr;
    DiagnosticsPage *m_diagnosticsPage = nullptr;
    TrustRulesDialog *m_trustRules = nullptr;
};
