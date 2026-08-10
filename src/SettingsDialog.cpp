#include "SettingsDialog.h"

#include "BrowserProfile.h"
#include "DiagnosticsPage.h"
#include "DnsSettingsPage.h"
#include "HistorySettingsPage.h"
#include "PrivateData.h"
#include "ProxySettingsPage.h"
#include "SearchSettingsPage.h"
#include "TrustRulesDialog.h"
#include "WebAppsSettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QStackedWidget>
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

struct FileSnapshot {
    QString path;
    QByteArray contents;
    bool existed = false;
};

bool captureFile(const QString &path, FileSnapshot *snapshot, QString *error)
{
    snapshot->path = path;
    snapshot->existed = QFile::exists(path);
    snapshot->contents.clear();
    if (!snapshot->existed)
        return true;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QCoreApplication::translate(
                "SettingsDialog",
                "Cannot snapshot %1: %2"
            ).arg(path, file.errorString());
        return false;
    }
    snapshot->contents = file.readAll();
    return true;
}

bool restoreFile(const FileSnapshot &snapshot, QString *error)
{
    if (!snapshot.existed) {
        if (!QFile::exists(snapshot.path) || QFile::remove(snapshot.path))
            return true;
        if (error)
            *error = QCoreApplication::translate(
                "SettingsDialog",
                "Cannot remove %1 during rollback"
            ).arg(snapshot.path);
        return false;
    }

    QSaveFile file(snapshot.path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QCoreApplication::translate(
                "SettingsDialog",
                "Cannot restore %1: %2"
            ).arg(snapshot.path, file.errorString());
        return false;
    }
    if (file.write(snapshot.contents) != snapshot.contents.size() || !file.commit()) {
        if (error)
            *error = QCoreApplication::translate(
                "SettingsDialog",
                "Cannot restore %1: %2"
            ).arg(snapshot.path, file.errorString());
        return false;
    }
    return PrivateData::restrictFile(snapshot.path, error);
}

QString rollbackSettings(
    const BrowserPreferences &preferences,
    const QList<FileSnapshot> &snapshots
)
{
    QStringList failures;
    for (const FileSnapshot &snapshot : snapshots) {
        QString error;
        if (!restoreFile(snapshot, &error))
            failures.append(error);
    }
    QString error;
    if (!preferences.save(&error))
        failures.append(error);
    return failures.join(QLatin1Char('\n'));
}

} // namespace

SettingsDialog::SettingsDialog(
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
    QWidget *parent
)
    : QDialog(parent)
    , m_configurationPath(configurationPath)
    , m_searchConfigurationPath(searchConfigurationPath)
    , m_dnsConfigurationPath(dnsConfigurationPath)
    , m_proxyConfigurationPath(proxyConfigurationPath)
    , m_preferences(preferences)
    , m_searchSettings(searchSettings)
    , m_dnsSettings(dnsSettings)
    , m_proxySettings(proxySettings)
    , m_activeProxySettings(activeProxySettings)
    , m_networkBlockedByProxyError(networkBlockedByProxyError)
    , m_profile(profile)
{
    createInterface(currentUrl, initialPage);
    m_historyPage = new HistorySettingsPage(
        historyStore,
        m_preferences.saveBrowsingHistory(),
        m_pages
    );
    m_pages->insertWidget(static_cast<int>(Page::History), m_historyPage);
    m_webAppsPage = new WebAppsSettingsPage(webAppStore, m_pages);
    m_pages->insertWidget(static_cast<int>(Page::WebApps), m_webAppsPage);
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
    m_diagnosticsPage = new DiagnosticsPage(
        m_profile,
        m_dnsSettings,
        m_activeProxySettings,
        m_proxySettings,
        m_networkBlockedByProxyError,
        m_pages
    );
    m_pages->addWidget(m_diagnosticsPage);
    m_pages->setCurrentIndex(static_cast<int>(initialPage));
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

void SettingsDialog::createInterface(const QUrl &currentUrl, Page initialPage)
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
    auto *generalItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/settings.svg")),
        tr("General"),
        m_sidebar
    );
    generalItem->setData(Qt::UserRole, static_cast<int>(Page::General));
    auto *searchItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/search.svg")),
        tr("Search"),
        m_sidebar
    );
    searchItem->setData(Qt::UserRole, static_cast<int>(Page::Search));
    auto *historyItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/history.svg")),
        tr("History"),
        m_sidebar
    );
    historyItem->setData(Qt::UserRole, static_cast<int>(Page::History));
    auto *webAppsItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/layout-grid.svg")),
        tr("Web Apps"),
        m_sidebar
    );
    webAppsItem->setData(Qt::UserRole, static_cast<int>(Page::WebApps));
    auto *privacyDataItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/database.svg")),
        tr("Privacy & Data"),
        m_sidebar
    );
    privacyDataItem->setData(Qt::UserRole, static_cast<int>(Page::PrivacyData));
    auto *dnsItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/network.svg")),
        tr("DNS"),
        m_sidebar
    );
    dnsItem->setData(Qt::UserRole, static_cast<int>(Page::Dns));
    auto *proxyItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/proxy.svg")),
        tr("Proxy"),
        m_sidebar
    );
    proxyItem->setData(Qt::UserRole, static_cast<int>(Page::Proxy));
    auto *trustItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/shield-check.svg")),
        tr("Trust Rules"),
        m_sidebar
    );
    trustItem->setData(Qt::UserRole, static_cast<int>(Page::TrustRules));
    auto *diagnosticsItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/info.svg")),
        tr("Diagnostics"),
        m_sidebar
    );
    diagnosticsItem->setData(Qt::UserRole, static_cast<int>(Page::Diagnostics));

    m_sidebar->setFixedWidth(SettingsSidebarMetrics::preferredWidth(m_sidebar));
    bodyLayout->addWidget(m_sidebar);

    m_pages = new QStackedWidget(body);
    m_pages->setObjectName(QStringLiteral("settingsPages"));
    bodyLayout->addWidget(m_pages, 1);

    auto *generalPage = new QWidget(m_pages);
    generalPage->setObjectName(QStringLiteral("generalSettingsPage"));
    auto *generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setContentsMargins(30, 24, 30, 24);
    generalLayout->setSpacing(14);

    auto *title = new QLabel(tr("General"), generalPage);
    title->setObjectName(QStringLiteral("dialogTitle"));
    generalLayout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Choose how PanBrowser starts and what it keeps between launches."),
        generalPage
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    generalLayout->addWidget(subtitle);

    auto *languageLabel = new QLabel(tr("LANGUAGE"), generalPage);
    languageLabel->setObjectName(QStringLiteral("sectionLabel"));
    generalLayout->addWidget(languageLabel);

    auto *languageCard = new QFrame(generalPage);
    languageCard->setObjectName(QStringLiteral("settingsCard"));
    auto *languageLayout = new QFormLayout(languageCard);
    languageLayout->setContentsMargins(18, 16, 18, 16);
    languageLayout->setHorizontalSpacing(18);
    languageLayout->setVerticalSpacing(8);
    languageLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_interfaceLanguage = new QComboBox(languageCard);
    m_interfaceLanguage->addItem(
        tr("System default"),
        static_cast<int>(InterfaceLanguage::System)
    );
    m_interfaceLanguage->addItem(
        tr("English"),
        static_cast<int>(InterfaceLanguage::English)
    );
    m_interfaceLanguage->addItem(
        QStringLiteral("Русский"),
        static_cast<int>(InterfaceLanguage::Russian)
    );
    m_interfaceLanguage->setCurrentIndex(m_interfaceLanguage->findData(
        static_cast<int>(m_preferences.interfaceLanguage())
    ));
    languageLayout->addRow(tr("Interface language"), m_interfaceLanguage);
    auto *languageHint = new QLabel(
        tr("Language changes take effect after PanBrowser restarts."),
        languageCard
    );
    languageHint->setObjectName(QStringLiteral("fieldHint"));
    languageHint->setWordWrap(true);
    languageLayout->addRow(languageHint);
    generalLayout->addWidget(languageCard);

    auto *startupLabel = new QLabel(tr("STARTUP"), generalPage);
    startupLabel->setObjectName(QStringLiteral("sectionLabel"));
    generalLayout->addWidget(startupLabel);

    auto *startupCard = new QFrame(generalPage);
    startupCard->setObjectName(QStringLiteral("settingsCard"));
    auto *startupLayout = new QFormLayout(startupCard);
    startupLayout->setContentsMargins(18, 16, 18, 16);
    startupLayout->setHorizontalSpacing(18);
    startupLayout->setVerticalSpacing(14);
    startupLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto *startPageRow = new QWidget(startupCard);
    auto *startPageLayout = new QHBoxLayout(startPageRow);
    startPageLayout->setContentsMargins(0, 0, 0, 0);
    startPageLayout->setSpacing(8);
    m_startPage = new QLineEdit(startPageRow);
    m_startPage->setText(m_preferences.startPage().toString());
    m_startPage->setPlaceholderText(QStringLiteral("https://example.com"));
    auto *useCurrent = new QPushButton(tr("Use current tab"), startPageRow);
    useCurrent->setEnabled(
        currentUrl.isValid()
        && (currentUrl.scheme() == QStringLiteral("http")
            || currentUrl.scheme() == QStringLiteral("https"))
    );
    startPageLayout->addWidget(m_startPage, 1);
    startPageLayout->addWidget(useCurrent);
    startupLayout->addRow(tr("Start page"), startPageRow);

    m_startupMode = new QComboBox(startupCard);
    m_startupMode->addItem(
        tr("Open the start page"),
        static_cast<int>(StartupMode::StartPage)
    );
    m_startupMode->addItem(
        tr("Continue with previous tabs"),
        static_cast<int>(StartupMode::RestoreTabs)
    );
    m_startupMode->setCurrentIndex(m_startupMode->findData(
        static_cast<int>(m_preferences.startupMode())
    ));
    startupLayout->addRow(tr("On launch"), m_startupMode);

    auto *restoreSignInHint = new QLabel(
        tr("Some restored tabs may ask you to sign in again unless “Keep sign-ins between launches” is enabled."),
        startupCard
    );
    restoreSignInHint->setObjectName(QStringLiteral("fieldHint"));
    restoreSignInHint->setWordWrap(true);
    startupLayout->addRow(restoreSignInHint);
    generalLayout->addWidget(startupCard);

    auto *privacyLabel = new QLabel(tr("PRIVACY"), generalPage);
    privacyLabel->setObjectName(QStringLiteral("sectionLabel"));
    generalLayout->addWidget(privacyLabel);

    auto *privacyCard = new QFrame(generalPage);
    privacyCard->setObjectName(QStringLiteral("settingsCard"));
    auto *privacyLayout = new QVBoxLayout(privacyCard);
    privacyLayout->setContentsMargins(18, 16, 18, 16);
    privacyLayout->setSpacing(8);
    m_persistSessionCookies = new QCheckBox(
        tr("Keep sign-ins between launches"),
        privacyCard
    );
    m_persistSessionCookies->setChecked(m_preferences.persistSessionCookies());
    privacyLayout->addWidget(m_persistSessionCookies);
    auto *privacyHint = new QLabel(
        tr("Session cookies will be stored in the PanBrowser profile. Avoid this on a shared computer."),
        privacyCard
    );
    privacyHint->setObjectName(QStringLiteral("fieldHint"));
    privacyHint->setWordWrap(true);
    privacyLayout->addWidget(privacyHint);
    generalLayout->addWidget(privacyCard);

    auto *developerLabel = new QLabel(tr("DEVELOPER TOOLS"), generalPage);
    developerLabel->setObjectName(QStringLiteral("sectionLabel"));
    generalLayout->addWidget(developerLabel);

    auto *developerCard = new QFrame(generalPage);
    developerCard->setObjectName(QStringLiteral("settingsCard"));
    auto *developerLayout = new QVBoxLayout(developerCard);
    developerLayout->setContentsMargins(18, 16, 18, 16);
    developerLayout->setSpacing(8);
    m_developerToolsEnabled = new QCheckBox(
        tr("Enable developer tools"),
        developerCard
    );
    m_developerToolsEnabled->setChecked(m_preferences.developerToolsEnabled());
    developerLayout->addWidget(m_developerToolsEnabled);
    auto *developerHint = new QLabel(
        tr("Developer tools can read and modify page content and browser data for the inspected site."),
        developerCard
    );
    developerHint->setObjectName(QStringLiteral("fieldHint"));
    developerHint->setWordWrap(true);
    developerLayout->addWidget(developerHint);
    generalLayout->addWidget(developerCard);
    generalLayout->addStretch();
    m_pages->addWidget(generalPage);

    m_searchPage = new SearchSettingsPage(m_searchSettings, m_pages);
    m_pages->addWidget(m_searchPage);

    auto *privacyDataPage = new QWidget(m_pages);
    privacyDataPage->setObjectName(QStringLiteral("privacyDataSettingsPage"));
    auto *dataLayout = new QVBoxLayout(privacyDataPage);
    dataLayout->setContentsMargins(30, 24, 30, 24);
    dataLayout->setSpacing(14);

    auto *dataTitle = new QLabel(tr("Privacy & Data"), privacyDataPage);
    dataTitle->setObjectName(QStringLiteral("dialogTitle"));
    dataLayout->addWidget(dataTitle);
    auto *dataSubtitle = new QLabel(
        tr("Remove browsing data kept in PanBrowser’s isolated WebEngine profile."),
        privacyDataPage
    );
    dataSubtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    dataLayout->addWidget(dataSubtitle);

    auto *storedDataLabel = new QLabel(tr("STORED DATA"), privacyDataPage);
    storedDataLabel->setObjectName(QStringLiteral("sectionLabel"));
    dataLayout->addWidget(storedDataLabel);

    auto *cacheCard = new QFrame(privacyDataPage);
    cacheCard->setObjectName(QStringLiteral("settingsCard"));
    auto *cacheLayout = new QHBoxLayout(cacheCard);
    cacheLayout->setContentsMargins(18, 16, 18, 16);
    cacheLayout->setSpacing(18);
    auto *cacheTextLayout = new QVBoxLayout();
    cacheTextLayout->setSpacing(5);
    auto *cacheTitle = new QLabel(tr("HTTP cache"), cacheCard);
    cacheTitle->setObjectName(QStringLiteral("settingsCardTitle"));
    cacheTextLayout->addWidget(cacheTitle);
    auto *cacheDescription = new QLabel(
        tr("Remove temporary page resources. This does not sign you out."),
        cacheCard
    );
    cacheDescription->setObjectName(QStringLiteral("fieldHint"));
    cacheDescription->setWordWrap(true);
    cacheTextLayout->addWidget(cacheDescription);
    auto *cacheStatus = new QLabel(cacheCard);
    cacheStatus->setObjectName(QStringLiteral("dataActionStatus"));
    cacheStatus->hide();
    cacheTextLayout->addWidget(cacheStatus);
    cacheLayout->addLayout(cacheTextLayout, 1);
    auto *clearCache = new QPushButton(tr("Clear cache"), cacheCard);
    cacheLayout->addWidget(clearCache);
    dataLayout->addWidget(cacheCard);

    auto *cookiesCard = new QFrame(privacyDataPage);
    cookiesCard->setObjectName(QStringLiteral("settingsCard"));
    auto *cookiesLayout = new QHBoxLayout(cookiesCard);
    cookiesLayout->setContentsMargins(18, 16, 18, 16);
    cookiesLayout->setSpacing(18);
    auto *cookiesTextLayout = new QVBoxLayout();
    cookiesTextLayout->setSpacing(5);
    auto *cookiesTitle = new QLabel(tr("Cookies"), cookiesCard);
    cookiesTitle->setObjectName(QStringLiteral("settingsCardTitle"));
    cookiesTextLayout->addWidget(cookiesTitle);
    auto *cookiesDescription = new QLabel(
        tr("Remove persistent and session cookies. Most sites will ask you to sign in again."),
        cookiesCard
    );
    cookiesDescription->setObjectName(QStringLiteral("fieldHint"));
    cookiesDescription->setWordWrap(true);
    cookiesTextLayout->addWidget(cookiesDescription);
    auto *cookiesStatus = new QLabel(cookiesCard);
    cookiesStatus->setObjectName(QStringLiteral("dataActionStatus"));
    cookiesStatus->hide();
    cookiesTextLayout->addWidget(cookiesStatus);
    cookiesLayout->addLayout(cookiesTextLayout, 1);
    auto *clearCookies = new QPushButton(tr("Clear cookies…"), cookiesCard);
    cookiesLayout->addWidget(clearCookies);
    dataLayout->addWidget(cookiesCard);

    auto *allDataCard = new QFrame(privacyDataPage);
    allDataCard->setObjectName(QStringLiteral("settingsCard"));
    auto *allDataLayout = new QHBoxLayout(allDataCard);
    allDataLayout->setContentsMargins(18, 16, 18, 16);
    allDataLayout->setSpacing(18);
    auto *allDataTextLayout = new QVBoxLayout();
    allDataTextLayout->setSpacing(5);
    auto *allDataTitle = new QLabel(tr("All site data"), allDataCard);
    allDataTitle->setObjectName(QStringLiteral("settingsCardTitle"));
    allDataTextLayout->addWidget(allDataTitle);
    auto *allDataDescription = new QLabel(
        tr("Remove cookies, local storage, IndexedDB, service workers, and cache on the next launch. Settings, history, trust rules, certificates, and saved tabs are kept."),
        allDataCard
    );
    allDataDescription->setObjectName(QStringLiteral("fieldHint"));
    allDataDescription->setWordWrap(true);
    allDataTextLayout->addWidget(allDataDescription);
    auto *allDataStatus = new QLabel(allDataCard);
    allDataStatus->setObjectName(QStringLiteral("dataActionStatus"));
    allDataTextLayout->addWidget(allDataStatus);
    allDataLayout->addLayout(allDataTextLayout, 1);
    auto *resetAllData = new QPushButton(allDataCard);
    resetAllData->setObjectName(QStringLiteral("dangerButton"));
    allDataLayout->addWidget(resetAllData);
    dataLayout->addWidget(allDataCard);
    dataLayout->addStretch();
    m_pages->addWidget(privacyDataPage);

    m_dnsPage = new DnsSettingsPage(m_dnsSettings, m_pages);
    m_pages->addWidget(m_dnsPage);

    m_proxyPage = new ProxySettingsPage(m_proxySettings, m_pages);
    m_pages->addWidget(m_proxyPage);

    m_trustRules = new TrustRulesDialog(m_configurationPath, m_pages, true);
    m_pages->addWidget(m_trustRules);

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
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save settings"));
    rootLayout->addWidget(buttons);

    connect(m_sidebar, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(useCurrent, &QPushButton::clicked, this, [this, currentUrl] {
        m_startPage->setText(currentUrl.toString());
    });
    connect(clearCache, &QPushButton::clicked, this, [this, clearCache, cacheStatus] {
        clearCache->setEnabled(false);
        cacheStatus->setText(tr("Clearing cache…"));
        cacheStatus->show();
        m_profile->clearHttpCache();
    });
    connect(
        m_profile,
        &QWebEngineProfile::clearHttpCacheCompleted,
        this,
        [clearCache, cacheStatus] {
            clearCache->setEnabled(true);
            cacheStatus->setText(QCoreApplication::translate("SettingsDialog", "Cache cleared."));
            cacheStatus->show();
        }
    );
    connect(clearCookies, &QPushButton::clicked, this, [this, cookiesStatus] {
        if (QMessageBox::question(
                this,
                tr("Clear cookies"),
                tr("Clear all cookies? Most sites will ask you to sign in again."),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel
            ) != QMessageBox::Yes) {
            return;
        }
        m_profile->clearAllCookies();
        cookiesStatus->setText(
            tr("Cookie deletion requested. Reload open tabs to update their sign-in state.")
        );
        cookiesStatus->show();
    });
    const auto updateDataResetUi = [resetAllData, allDataStatus] {
        const bool scheduled = BrowserProfile::dataResetScheduled();
        resetAllData->setText(
            scheduled
                ? QCoreApplication::translate("SettingsDialog", "Cancel scheduled reset")
                : QCoreApplication::translate("SettingsDialog", "Reset on next launch…")
        );
        allDataStatus->setText(
            scheduled ? QCoreApplication::translate(
                            "SettingsDialog",
                            "Full site-data reset is scheduled for the next launch."
                        )
                      : QString()
        );
        allDataStatus->setVisible(scheduled);
    };
    connect(resetAllData, &QPushButton::clicked, this, [this, updateDataResetUi] {
        QString error;
        if (BrowserProfile::dataResetScheduled()) {
            if (!BrowserProfile::cancelDataReset(&error))
                QMessageBox::warning(this, tr("Cannot cancel reset"), error);
            updateDataResetUi();
            return;
        }

        if (QMessageBox::question(
                this,
                tr("Reset all site data"),
                tr("Schedule deletion of all cookies and site storage the next time PanBrowser starts? History, trust rules, certificates, settings, and saved tabs will be kept."),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel
            ) != QMessageBox::Yes) {
            return;
        }
        if (!BrowserProfile::scheduleDataReset(&error))
            QMessageBox::warning(this, tr("Cannot schedule reset"), error);
        updateDataResetUi();
    });
    const auto updateRestoreHint = [this, restoreSignInHint] {
        const bool restoringTabs = static_cast<StartupMode>(
            m_startupMode->currentData().toInt()
        ) == StartupMode::RestoreTabs;
        restoreSignInHint->setVisible(
            restoringTabs && !m_persistSessionCookies->isChecked()
        );
    };
    connect(m_startupMode, &QComboBox::currentIndexChanged, this, [updateRestoreHint](int) {
        updateRestoreHint();
    });
    connect(m_persistSessionCookies, &QCheckBox::toggled, this, [updateRestoreHint](bool) {
        updateRestoreHint();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::saveAndClose);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateRestoreHint();
    updateDataResetUi();
    selectPage(initialPage);
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

BrowserPreferences SettingsDialog::preferencesFromControls() const
{
    BrowserPreferences preferences = m_preferences;
    preferences.setStartPage(QUrl::fromUserInput(m_startPage->text().trimmed()));
    preferences.setStartupMode(
        static_cast<StartupMode>(m_startupMode->currentData().toInt())
    );
    preferences.setPersistSessionCookies(m_persistSessionCookies->isChecked());
    preferences.setSaveBrowsingHistory(m_historyPage->saveHistoryEnabled());
    preferences.setDeveloperToolsEnabled(m_developerToolsEnabled->isChecked());
    preferences.setInterfaceLanguage(
        static_cast<InterfaceLanguage>(m_interfaceLanguage->currentData().toInt())
    );
    return preferences;
}

void SettingsDialog::saveAndClose()
{
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
    QList<FileSnapshot> snapshots;
    for (const QString &path : {
             m_searchConfigurationPath,
             m_searchConfigurationPath + QStringLiteral(".backup"),
             m_dnsConfigurationPath,
             m_dnsConfigurationPath + QStringLiteral(".backup"),
             m_proxyConfigurationPath,
             m_proxyConfigurationPath + QStringLiteral(".backup"),
             m_configurationPath,
             m_configurationPath + QStringLiteral(".backup"),
         }) {
        FileSnapshot snapshot;
        if (!captureFile(path, &snapshot, &error)) {
            QMessageBox::warning(this, tr("Cannot save settings"), error);
            return;
        }
        snapshots.append(snapshot);
    }

    SearchSettings searchSettings = m_searchPage->settings();
    DnsSettings dnsSettings = m_dnsPage->settings();
    ProxySettings proxySettings = m_proxyPage->settings();
    if (!preferences.save(&error)) {
        selectPage(Page::General);
        QMessageBox::warning(this, tr("Cannot save settings"), error);
        return;
    }
    if (!searchSettings.save(m_searchConfigurationPath, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, snapshots);
        selectPage(Page::Search);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save search settings"), error);
        return;
    }
    if (!dnsSettings.save(m_dnsConfigurationPath, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, snapshots);
        selectPage(Page::Dns);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save DNS settings"), error);
        return;
    }
    if (!proxySettings.save(m_proxyConfigurationPath, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, snapshots);
        selectPage(Page::Proxy);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save proxy settings"), error);
        return;
    }
    if (!m_trustRules->save(&error)) {
        const QString rollbackError = rollbackSettings(m_preferences, snapshots);
        selectPage(Page::TrustRules);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot save trust rules"), error);
        return;
    }
    if (!applyDnsSettings(dnsSettings, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, snapshots);
        selectPage(Page::Dns);
        if (!rollbackError.isEmpty())
            error += tr("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, tr("Cannot apply DNS settings"), error);
        return;
    }

    m_trustRules->finalizeSave();
    m_preferences = preferences;
    m_searchSettings = searchSettings;
    m_dnsSettings = dnsSettings;
    m_proxySettings = proxySettings;
    accept();
}
