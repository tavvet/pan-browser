#include "SettingsDialog.h"

#include "BrowserProfile.h"
#include "CrossDomainSettingsPage.h"
#include "CredentialsSettingsPage.h"
#include "DiagnosticsPage.h"
#include "DnsSettingsPage.h"
#include "GeneralSettingsPage.h"
#include "HistorySettingsPage.h"
#include "PrivacyDataSettingsPage.h"
#include "ProxySettingsPage.h"
#include "SearchSettingsPage.h"
#include "SettingsSaveTransaction.h"
#include "TrustRulesSettingsPage.h"
#include "VideoTranslationSettingsPage.h"
#include "WebAppsSettingsPage.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

namespace {

namespace SettingsSidebarMetrics {

constexpr int minimumWidth = 190;
constexpr int maximumWidth = 280;

// Keep these values aligned with QListWidget#settingsSidebar in Theme.qss.
constexpr int horizontalPadding = 10;
constexpr int rightBorderWidth = 1;
constexpr int roundingAllowance = 1;

int preferredWidth(QListWidget *sidebar)
{
    sidebar->ensurePolished();
    const int surroundingWidth =
        (2 * horizontalPadding) + rightBorderWidth + roundingAllowance;
    const int contentWidth = sidebar->sizeHintForColumn(0) + surroundingWidth;
    return qBound(minimumWidth, contentWidth, maximumWidth);
}

} // namespace SettingsSidebarMetrics

QString rollbackSettings(
    const BrowserPreferences &preferences,
    const SettingsSaveTransaction &transaction
)
{
    QStringList failures;
    const QString fileError = transaction.rollback();
    if (!fileError.isEmpty())
        failures.append(fileError);
    QString preferenceError;
    if (!preferences.save(&preferenceError))
        failures.append(preferenceError);
    return failures.join(QLatin1Char('\n'));
}

} // namespace

SettingsDialog::SettingsDialog(
    const SettingsDialogContext &context,
    const QUrl &currentUrl,
    Page initialPage,
    QWidget *parent
)
    : QDialog(parent)
    , m_configurationPath(context.trustConfigurationPath)
    , m_searchConfigurationPath(context.searchConfigurationPath)
    , m_dnsConfigurationPath(context.dnsConfigurationPath)
    , m_proxyConfigurationPath(context.proxyConfigurationPath)
    , m_crossDomainConfigurationPath(context.crossDomainConfigurationPath)
    , m_videoTranslationConfigurationPath(context.videoTranslationConfigurationPath)
    , m_preferences(context.preferences)
    , m_searchSettings(context.searchSettings)
    , m_dnsSettings(context.dnsSettings)
    , m_proxySettings(context.proxySettings)
    , m_activeProxySettings(context.activeProxySettings)
    , m_crossDomainSettings(context.crossDomainSettings)
    , m_videoTranslationSettings(context.videoTranslationSettings)
    , m_networkBlockedByProxyError(context.networkBlockedByProxyError)
    , m_profile(context.profile)
    , m_votUserscriptManager(context.votUserscriptManager)
{
    createInterface(
        currentUrl,
        initialPage,
        context.historyStore,
        context.webAppStore,
        context.credentialStore
    );
}

bool SettingsDialog::load(QString *error)
{
    return m_trustRules->load(error);
}

BrowserPreferences SettingsDialog::preferences() const
{
    return m_preferences;
}

SearchSettings SettingsDialog::searchSettings() const
{
    return m_searchSettings;
}

DnsSettings SettingsDialog::dnsSettings() const
{
    return m_dnsSettings;
}

ProxySettings SettingsDialog::proxySettings() const
{
    return m_proxySettings;
}

CrossDomainSettings SettingsDialog::crossDomainSettings() const
{
    return m_crossDomainSettings;
}

VideoTranslationSettings SettingsDialog::videoTranslationSettings() const
{
    return m_videoTranslationSettings;
}

void SettingsDialog::reject()
{
    if (m_credentialOperationActive)
        return;

    const QStringList failures = m_trustRules->rollbackPendingCertificates();
    if (!failures.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Certificate cleanup incomplete"),
            tr(
                "Some imported certificate files could not be removed:\n%1"
            ).arg(failures.join(QLatin1Char('\n')))
        );
    }
    QDialog::reject();
}

void SettingsDialog::createInterface(
    const QUrl &currentUrl,
    Page initialPage,
    HistoryStore *historyStore,
    WebAppStore *webAppStore,
    CredentialStore *credentialStore
)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("Settings"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/settings.svg")));
    resize(1180, 760);
    setMinimumSize(980, 640);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 16);
    rootLayout->setSpacing(0);

    auto *body = new QWidget(this);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    rootLayout->addWidget(body, 1);

    m_sidebar = new QListWidget(body);
    m_sidebar->setObjectName(QStringLiteral("settingsSidebar"));
    m_sidebar->setIconSize(QSize(20, 20));
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sidebar->setTextElideMode(Qt::ElideRight);
    bodyLayout->addWidget(m_sidebar);

    m_pages = new QStackedWidget(body);
    m_pages->setObjectName(QStringLiteral("settingsPages"));
    bodyLayout->addWidget(m_pages, 1);

    m_generalPage = new GeneralSettingsPage(m_preferences, currentUrl, m_pages);
    registerPage(
        Page::General,
        QIcon(QStringLiteral(":/assets/icons/settings.svg")),
        tr("General"),
        m_generalPage
    );

    m_searchPage = new SearchSettingsPage(m_searchSettings, m_pages);
    registerPage(
        Page::Search,
        QIcon(QStringLiteral(":/assets/icons/search.svg")),
        tr("Search"),
        m_searchPage
    );

    m_historyPage = new HistorySettingsPage(
        historyStore,
        m_preferences.saveBrowsingHistory(),
        m_pages
    );
    registerPage(
        Page::History,
        QIcon(QStringLiteral(":/assets/icons/history.svg")),
        tr("History"),
        m_historyPage
    );

    m_webAppsPage = new WebAppsSettingsPage(webAppStore, m_pages);
    registerPage(
        Page::WebApps,
        QIcon(QStringLiteral(":/assets/icons/layout-grid.svg")),
        tr("Web Apps"),
        m_webAppsPage
    );
    connect(
        m_webAppsPage,
        &WebAppsSettingsPage::openRequested,
        this,
        [this](const QString &id) {
            saveAndClose();
            if (result() == QDialog::Accepted)
                emit webAppOpenRequested(id);
        }
    );

    m_videoTranslationPage = new VideoTranslationSettingsPage(
        m_videoTranslationSettings,
        m_votUserscriptManager,
        m_pages
    );
    registerPage(
        Page::VideoTranslation,
        QIcon(QStringLiteral(":/assets/icons/languages.svg")),
        tr("Video Translation"),
        m_videoTranslationPage
    );

    m_privacyDataPage = new PrivacyDataSettingsPage(m_profile, m_pages);
    registerPage(
        Page::PrivacyData,
        QIcon(QStringLiteral(":/assets/icons/database.svg")),
        tr("Privacy & Data"),
        m_privacyDataPage
    );

    m_credentialsPage = new CredentialsSettingsPage(
        credentialStore,
        m_pages
    );
    registerPage(
        Page::Credentials,
        QIcon(QStringLiteral(":/assets/icons/key-round.svg")),
        tr("Credentials"),
        m_credentialsPage
    );

    m_dnsPage = new DnsSettingsPage(m_dnsSettings, m_pages);
    registerPage(
        Page::Dns,
        QIcon(QStringLiteral(":/assets/icons/network.svg")),
        tr("DNS"),
        m_dnsPage
    );

    m_proxyPage = new ProxySettingsPage(m_proxySettings, m_pages);
    registerPage(
        Page::Proxy,
        QIcon(QStringLiteral(":/assets/icons/proxy.svg")),
        tr("Proxy"),
        m_proxyPage
    );

    auto *crossDomainScrollArea = new QScrollArea(m_pages);
    crossDomainScrollArea->setObjectName(QStringLiteral("settingsPageScrollArea"));
    crossDomainScrollArea->setFrameShape(QFrame::NoFrame);
    crossDomainScrollArea->setWidgetResizable(true);
    crossDomainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_crossDomainPage = new CrossDomainSettingsPage(
        m_crossDomainSettings,
        crossDomainScrollArea
    );
    crossDomainScrollArea->setWidget(m_crossDomainPage);
    registerPage(
        Page::SiteConnections,
        QIcon(QStringLiteral(":/assets/icons/site-connections.svg")),
        tr("Site Connections"),
        crossDomainScrollArea
    );

    m_trustRules = new TrustRulesSettingsPage(m_configurationPath, m_pages);
    registerPage(
        Page::TrustRules,
        QIcon(QStringLiteral(":/assets/icons/shield-check.svg")),
        tr("Trust Rules"),
        m_trustRules
    );

    m_diagnosticsPage = new DiagnosticsPage(
        m_profile,
        m_dnsSettings,
        m_activeProxySettings,
        m_proxySettings,
        m_networkBlockedByProxyError,
        m_pages
    );
    registerPage(
        Page::Diagnostics,
        QIcon(QStringLiteral(":/assets/icons/info.svg")),
        tr("Diagnostics"),
        m_diagnosticsPage
    );

    m_sidebar->setFixedWidth(SettingsSidebarMetrics::preferredWidth(m_sidebar));

    auto *separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("settingsSeparator"));
    separator->setFrameShape(QFrame::HLine);
    rootLayout->addWidget(separator);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        this
    );
    buttons->setContentsMargins(18, 12, 18, 0);
    m_saveButton = buttons->button(QDialogButtonBox::Save);
    m_cancelButton = buttons->button(QDialogButtonBox::Cancel);
    m_saveButton->setObjectName(QStringLiteral("saveSettingsButton"));
    m_cancelButton->setObjectName(QStringLiteral("cancelSettingsButton"));
    m_saveButton->setText(tr("Save settings"));
    rootLayout->addWidget(buttons);

    connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
        const QListWidgetItem *item = m_sidebar->item(row);
        if (!item)
            return;
        if (QWidget *page = m_pageWidgets.value(item->data(Qt::UserRole).toInt()))
            m_pages->setCurrentWidget(page);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::saveAndClose);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(
        m_credentialsPage,
        &CredentialsSettingsPage::destructiveOperationActiveChanged,
        this,
        &SettingsDialog::setCredentialOperationActive
    );
    selectPage(initialPage);
}

void SettingsDialog::registerPage(
    Page page,
    const QIcon &icon,
    const QString &title,
    QWidget *widget
)
{
    auto *item = new QListWidgetItem(icon, title, m_sidebar);
    const int pageId = static_cast<int>(page);
    item->setData(Qt::UserRole, pageId);
    m_pages->addWidget(widget);
    m_pageWidgets.insert(pageId, widget);
}

void SettingsDialog::selectPage(Page page)
{
    for (int row = 0; row < m_sidebar->count(); ++row) {
        if (m_sidebar->item(row)->data(Qt::UserRole).toInt() == static_cast<int>(page)) {
            m_sidebar->setCurrentRow(row);
            return;
        }
    }
}

void SettingsDialog::setCredentialOperationActive(bool active)
{
    m_credentialOperationActive = active;
    m_saveButton->setEnabled(!active);
    m_cancelButton->setEnabled(!active);
}

BrowserPreferences SettingsDialog::preferencesFromControls() const
{
    BrowserPreferences preferences = m_generalPage->applyTo(m_preferences);
    preferences.setSaveBrowsingHistory(m_historyPage->saveHistoryEnabled());
    return preferences;
}

void SettingsDialog::saveAndClose()
{
    if (m_credentialOperationActive)
        return;

    BrowserPreferences preferences = preferencesFromControls();
    QString error;
    if (!preferences.validate(&error)) {
        selectPage(Page::General);
        QMessageBox::warning(this, tr("Cannot save settings"), error);
        return;
    }
    if (!m_trustRules->validate(&error)) {
        selectPage(Page::TrustRules);
        QMessageBox::warning(this, tr("Cannot save trust rules"), error);
        return;
    }
    if (!m_searchPage->validate(&error)) {
        selectPage(Page::Search);
        QMessageBox::warning(this, tr("Cannot save search settings"), error);
        return;
    }
    if (!m_dnsPage->validate(&error)) {
        selectPage(Page::Dns);
        QMessageBox::warning(this, tr("Cannot save DNS settings"), error);
        return;
    }
    if (!m_proxyPage->validate(&error)) {
        selectPage(Page::Proxy);
        QMessageBox::warning(this, tr("Cannot save proxy settings"), error);
        return;
    }
    if (!m_crossDomainPage->validate(&error)) {
        selectPage(Page::SiteConnections);
        QMessageBox::warning(this, tr("Cannot save site connection settings"), error);
        return;
    }
    if (!m_videoTranslationPage->validate(&error)) {
        selectPage(Page::VideoTranslation);
        QMessageBox::warning(this, tr("Cannot save video translation settings"), error);
        return;
    }
    SettingsSaveTransaction transaction;
    if (!transaction.capture(
            {
                m_searchConfigurationPath,
                m_searchConfigurationPath + QStringLiteral(".backup"),
                m_dnsConfigurationPath,
                m_dnsConfigurationPath + QStringLiteral(".backup"),
                m_proxyConfigurationPath,
                m_proxyConfigurationPath + QStringLiteral(".backup"),
                m_crossDomainConfigurationPath,
                m_crossDomainConfigurationPath + QStringLiteral(".backup"),
                m_videoTranslationConfigurationPath,
                m_videoTranslationConfigurationPath + QStringLiteral(".backup"),
                m_configurationPath,
                m_configurationPath + QStringLiteral(".backup"),
            },
            &error
        )) {
        QMessageBox::warning(this, tr("Cannot save settings"), error);
        return;
    }

    SearchSettings searchSettings = m_searchPage->settings();
    DnsSettings dnsSettings = m_dnsPage->settings();
    ProxySettings proxySettings = m_proxyPage->settings();
    CrossDomainSettings crossDomainSettings = m_crossDomainPage->settings();
    VideoTranslationSettings videoTranslationSettings =
        m_videoTranslationPage->settings();
    if (!preferences.save(&error)) {
        selectPage(Page::General);
        QMessageBox::warning(this, tr("Cannot save settings"), error);
        return;
    }
    if (!searchSettings.save(m_searchConfigurationPath, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, transaction);
        selectPage(Page::Search);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save search settings"), error);
        return;
    }
    if (!dnsSettings.save(m_dnsConfigurationPath, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, transaction);
        selectPage(Page::Dns);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save DNS settings"), error);
        return;
    }
    if (!proxySettings.save(m_proxyConfigurationPath, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, transaction);
        selectPage(Page::Proxy);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save proxy settings"), error);
        return;
    }
    if (!crossDomainSettings.save(m_crossDomainConfigurationPath, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, transaction);
        selectPage(Page::SiteConnections);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save site connection settings"), error);
        return;
    }
    if (!videoTranslationSettings.save(
            m_videoTranslationConfigurationPath,
            &error
        )) {
        const QString rollbackError = rollbackSettings(m_preferences, transaction);
        selectPage(Page::VideoTranslation);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(
            this,
            tr("Cannot save video translation settings"),
            error
        );
        return;
    }
    if (!m_trustRules->save(&error)) {
        const QString rollbackError = rollbackSettings(m_preferences, transaction);
        selectPage(Page::TrustRules);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save trust rules"), error);
        return;
    }
    if (!applyDnsSettings(dnsSettings, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, transaction);
        selectPage(Page::Dns);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot apply DNS settings"), error);
        return;
    }

    const QStringList certificateCleanupFailures = m_trustRules->finalizeSave();
    m_preferences = preferences;
    m_searchSettings = searchSettings;
    m_dnsSettings = dnsSettings;
    m_proxySettings = proxySettings;
    m_crossDomainSettings = crossDomainSettings;
    m_videoTranslationSettings = videoTranslationSettings;
    if (!certificateCleanupFailures.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Settings saved with a warning"),
            tr(
                "Settings were saved, but some unused imported certificate "
                "files could not be removed:\n%1"
            ).arg(certificateCleanupFailures.join(QLatin1Char('\n')))
        );
    }
    accept();
}
