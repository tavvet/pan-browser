#pragma once

#include "BrowserPreferences.h"
#include "CrossDomainSettings.h"
#include "DnsSettings.h"
#include "ProxySettings.h"
#include "SearchSettings.h"
#include "VideoTranslationSettings.h"

#include <QDialog>
#include <QHash>

class QIcon;
class QListWidget;
class QStackedWidget;
class BrowserProfile;
class DiagnosticsPage;
class CrossDomainSettingsPage;
class DnsSettingsPage;
class GeneralSettingsPage;
class HistorySettingsPage;
class HistoryStore;
class ProxySettingsPage;
class PrivacyDataSettingsPage;
class SearchSettingsPage;
class TrustRulesSettingsPage;
class VideoTranslationSettingsPage;
class VotUserscriptManager;
class WebAppsSettingsPage;
class WebAppStore;

struct SettingsDialogContext {
    QString trustConfigurationPath;
    QString searchConfigurationPath;
    QString dnsConfigurationPath;
    QString proxyConfigurationPath;
    QString crossDomainConfigurationPath;
    QString videoTranslationConfigurationPath;
    BrowserPreferences preferences;
    SearchSettings searchSettings;
    DnsSettings dnsSettings;
    ProxySettings proxySettings;
    ProxySettings activeProxySettings;
    CrossDomainSettings crossDomainSettings;
    VideoTranslationSettings videoTranslationSettings;
    bool networkBlockedByProxyError = false;
    BrowserProfile *profile = nullptr;
    HistoryStore *historyStore = nullptr;
    WebAppStore *webAppStore = nullptr;
    VotUserscriptManager *votUserscriptManager = nullptr;
};

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Page {
        General,
        Search,
        History,
        WebApps,
        VideoTranslation,
        PrivacyData,
        Dns,
        Proxy,
        SiteConnections,
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
    [[nodiscard]] CrossDomainSettings crossDomainSettings() const;
    [[nodiscard]] VideoTranslationSettings videoTranslationSettings() const;

public slots:
    void reject() override;

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
    QString m_crossDomainConfigurationPath;
    QString m_videoTranslationConfigurationPath;
    BrowserPreferences m_preferences;
    SearchSettings m_searchSettings;
    DnsSettings m_dnsSettings;
    ProxySettings m_proxySettings;
    ProxySettings m_activeProxySettings;
    CrossDomainSettings m_crossDomainSettings;
    VideoTranslationSettings m_videoTranslationSettings;
    bool m_networkBlockedByProxyError = false;
    BrowserProfile *m_profile = nullptr;
    VotUserscriptManager *m_votUserscriptManager = nullptr;
    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_pages = nullptr;
    QHash<int, QWidget *> m_pageWidgets;
    GeneralSettingsPage *m_generalPage = nullptr;
    PrivacyDataSettingsPage *m_privacyDataPage = nullptr;
    SearchSettingsPage *m_searchPage = nullptr;
    DnsSettingsPage *m_dnsPage = nullptr;
    ProxySettingsPage *m_proxyPage = nullptr;
    CrossDomainSettingsPage *m_crossDomainPage = nullptr;
    HistorySettingsPage *m_historyPage = nullptr;
    WebAppsSettingsPage *m_webAppsPage = nullptr;
    VideoTranslationSettingsPage *m_videoTranslationPage = nullptr;
    DiagnosticsPage *m_diagnosticsPage = nullptr;
    TrustRulesSettingsPage *m_trustRules = nullptr;
};
