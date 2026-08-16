#pragma once

#include "BrowserPreferences.h"
#include "CrossDomainSettings.h"
#include "DnsSettings.h"
#include "ProxySettings.h"
#include "SearchSettings.h"
#include "UserAgentSettings.h"
#include "VideoTranslationSettings.h"

#include <QDialog>
#include <QHash>

class QIcon;
class QListWidget;
class QPushButton;
class QStackedWidget;
class BrowserProfile;
class CredentialStore;
class CredentialsSettingsPage;
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
class UserAgentSettingsPage;
class VideoTranslationSettingsPage;
class VotUserscriptManager;
class WebAppsSettingsPage;
class WebAppStore;

struct SettingsDialogContext {
    QString trustConfigurationPath;
    QString searchConfigurationPath;
    QString userAgentConfigurationPath;
    QString dnsConfigurationPath;
    QString proxyConfigurationPath;
    QString crossDomainConfigurationPath;
    QString videoTranslationConfigurationPath;
    BrowserPreferences preferences;
    SearchSettings searchSettings;
    UserAgentSettings userAgentSettings = UserAgentSettings::defaults();
    UserAgentSettings activeUserAgentSettings = UserAgentSettings::defaults();
    DnsSettings dnsSettings;
    ProxySettings proxySettings;
    ProxySettings activeProxySettings;
    CrossDomainSettings crossDomainSettings;
    VideoTranslationSettings videoTranslationSettings;
    bool networkBlockedByProxyError = false;
    QString userAgentConfigurationError;
    BrowserProfile *profile = nullptr;
    HistoryStore *historyStore = nullptr;
    WebAppStore *webAppStore = nullptr;
    VotUserscriptManager *votUserscriptManager = nullptr;
    CredentialStore *credentialStore = nullptr;
};

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Page {
        General,
        Search,
        UserAgent,
        History,
        WebApps,
        VideoTranslation,
        PrivacyData,
        Credentials,
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
    [[nodiscard]] UserAgentSettings userAgentSettings() const;
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
        WebAppStore *webAppStore,
        CredentialStore *credentialStore
    );
    void registerPage(Page page, const QIcon &icon, const QString &title, QWidget *widget);
    void selectPage(Page page);
    void setCredentialOperationActive(bool active);
    BrowserPreferences preferencesFromControls() const;
    void saveAndClose();

    QString m_configurationPath;
    QString m_searchConfigurationPath;
    QString m_userAgentConfigurationPath;
    QString m_dnsConfigurationPath;
    QString m_proxyConfigurationPath;
    QString m_crossDomainConfigurationPath;
    QString m_videoTranslationConfigurationPath;
    BrowserPreferences m_preferences;
    SearchSettings m_searchSettings;
    UserAgentSettings m_userAgentSettings;
    UserAgentSettings m_activeUserAgentSettings;
    DnsSettings m_dnsSettings;
    ProxySettings m_proxySettings;
    ProxySettings m_activeProxySettings;
    CrossDomainSettings m_crossDomainSettings;
    VideoTranslationSettings m_videoTranslationSettings;
    bool m_networkBlockedByProxyError = false;
    QString m_userAgentConfigurationError;
    BrowserProfile *m_profile = nullptr;
    VotUserscriptManager *m_votUserscriptManager = nullptr;
    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_pages = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QHash<int, QWidget *> m_pageWidgets;
    GeneralSettingsPage *m_generalPage = nullptr;
    PrivacyDataSettingsPage *m_privacyDataPage = nullptr;
    CredentialsSettingsPage *m_credentialsPage = nullptr;
    SearchSettingsPage *m_searchPage = nullptr;
    UserAgentSettingsPage *m_userAgentPage = nullptr;
    DnsSettingsPage *m_dnsPage = nullptr;
    ProxySettingsPage *m_proxyPage = nullptr;
    CrossDomainSettingsPage *m_crossDomainPage = nullptr;
    HistorySettingsPage *m_historyPage = nullptr;
    WebAppsSettingsPage *m_webAppsPage = nullptr;
    VideoTranslationSettingsPage *m_videoTranslationPage = nullptr;
    DiagnosticsPage *m_diagnosticsPage = nullptr;
    TrustRulesSettingsPage *m_trustRules = nullptr;
    bool m_credentialOperationActive = false;
};
