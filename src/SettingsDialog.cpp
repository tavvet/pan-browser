#include "SettingsDialog.h"

#include "BrowserProfile.h"
#include "DiagnosticsPage.h"
#include "HistorySettingsPage.h"
#include "SearchSettingsPage.h"
#include "TrustRulesDialog.h"

#include <QCheckBox>
#include <QComboBox>
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
            *error = QStringLiteral("Cannot snapshot %1: %2").arg(path, file.errorString());
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
            *error = QStringLiteral("Cannot remove %1 during rollback").arg(snapshot.path);
        return false;
    }

    QSaveFile file(snapshot.path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot restore %1: %2").arg(snapshot.path, file.errorString());
        return false;
    }
    if (file.write(snapshot.contents) != snapshot.contents.size() || !file.commit()) {
        if (error)
            *error = QStringLiteral("Cannot restore %1: %2").arg(snapshot.path, file.errorString());
        return false;
    }
    return true;
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
    const BrowserPreferences &preferences,
    const SearchSettings &searchSettings,
    BrowserProfile *profile,
    HistoryStore *historyStore,
    const QUrl &currentUrl,
    Page initialPage,
    QWidget *parent
)
    : QDialog(parent)
    , m_configurationPath(configurationPath)
    , m_searchConfigurationPath(searchConfigurationPath)
    , m_preferences(preferences)
    , m_searchSettings(searchSettings)
    , m_profile(profile)
{
    createInterface(currentUrl, initialPage);
    m_historyPage = new HistorySettingsPage(
        historyStore,
        m_preferences.saveBrowsingHistory(),
        m_pages
    );
    m_pages->insertWidget(static_cast<int>(Page::History), m_historyPage);
    m_diagnosticsPage = new DiagnosticsPage(m_profile, m_pages);
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

void SettingsDialog::createInterface(const QUrl &currentUrl, Page initialPage)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(QStringLiteral("Settings"));
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
    m_sidebar->setFixedWidth(190);
    auto *generalItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/settings.svg")),
        QStringLiteral("General"),
        m_sidebar
    );
    generalItem->setData(Qt::UserRole, static_cast<int>(Page::General));
    auto *searchItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/search.svg")),
        QStringLiteral("Search"),
        m_sidebar
    );
    searchItem->setData(Qt::UserRole, static_cast<int>(Page::Search));
    auto *historyItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/history.svg")),
        QStringLiteral("History"),
        m_sidebar
    );
    historyItem->setData(Qt::UserRole, static_cast<int>(Page::History));
    auto *privacyDataItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/database.svg")),
        QStringLiteral("Privacy & Data"),
        m_sidebar
    );
    privacyDataItem->setData(Qt::UserRole, static_cast<int>(Page::PrivacyData));
    auto *trustItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/shield-check.svg")),
        QStringLiteral("Trust Rules"),
        m_sidebar
    );
    trustItem->setData(Qt::UserRole, static_cast<int>(Page::TrustRules));
    auto *diagnosticsItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/info.svg")),
        QStringLiteral("Diagnostics"),
        m_sidebar
    );
    diagnosticsItem->setData(Qt::UserRole, static_cast<int>(Page::Diagnostics));
    bodyLayout->addWidget(m_sidebar);

    m_pages = new QStackedWidget(body);
    m_pages->setObjectName(QStringLiteral("settingsPages"));
    bodyLayout->addWidget(m_pages, 1);

    auto *generalPage = new QWidget(m_pages);
    generalPage->setObjectName(QStringLiteral("generalSettingsPage"));
    auto *generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setContentsMargins(30, 24, 30, 24);
    generalLayout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("General"), generalPage);
    title->setObjectName(QStringLiteral("dialogTitle"));
    generalLayout->addWidget(title);
    auto *subtitle = new QLabel(
        QStringLiteral("Choose how PanBrowser starts and what it keeps between launches."),
        generalPage
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    generalLayout->addWidget(subtitle);

    auto *startupLabel = new QLabel(QStringLiteral("STARTUP"), generalPage);
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
    auto *useCurrent = new QPushButton(QStringLiteral("Use current tab"), startPageRow);
    useCurrent->setEnabled(
        currentUrl.isValid()
        && (currentUrl.scheme() == QStringLiteral("http")
            || currentUrl.scheme() == QStringLiteral("https"))
    );
    startPageLayout->addWidget(m_startPage, 1);
    startPageLayout->addWidget(useCurrent);
    startupLayout->addRow(QStringLiteral("Start page"), startPageRow);

    m_startupMode = new QComboBox(startupCard);
    m_startupMode->addItem(
        QStringLiteral("Open the start page"),
        static_cast<int>(StartupMode::StartPage)
    );
    m_startupMode->addItem(
        QStringLiteral("Continue with previous tabs"),
        static_cast<int>(StartupMode::RestoreTabs)
    );
    m_startupMode->setCurrentIndex(m_startupMode->findData(
        static_cast<int>(m_preferences.startupMode())
    ));
    startupLayout->addRow(QStringLiteral("On launch"), m_startupMode);

    auto *restoreSignInHint = new QLabel(
        QStringLiteral("Some restored tabs may ask you to sign in again unless “Keep sign-ins between launches” is enabled."),
        startupCard
    );
    restoreSignInHint->setObjectName(QStringLiteral("fieldHint"));
    restoreSignInHint->setWordWrap(true);
    startupLayout->addRow(restoreSignInHint);
    generalLayout->addWidget(startupCard);

    auto *privacyLabel = new QLabel(QStringLiteral("PRIVACY"), generalPage);
    privacyLabel->setObjectName(QStringLiteral("sectionLabel"));
    generalLayout->addWidget(privacyLabel);

    auto *privacyCard = new QFrame(generalPage);
    privacyCard->setObjectName(QStringLiteral("settingsCard"));
    auto *privacyLayout = new QVBoxLayout(privacyCard);
    privacyLayout->setContentsMargins(18, 16, 18, 16);
    privacyLayout->setSpacing(8);
    m_persistSessionCookies = new QCheckBox(
        QStringLiteral("Keep sign-ins between launches"),
        privacyCard
    );
    m_persistSessionCookies->setChecked(m_preferences.persistSessionCookies());
    privacyLayout->addWidget(m_persistSessionCookies);
    auto *privacyHint = new QLabel(
        QStringLiteral("Session cookies will be stored in the PanBrowser profile. Avoid this on a shared computer."),
        privacyCard
    );
    privacyHint->setObjectName(QStringLiteral("fieldHint"));
    privacyHint->setWordWrap(true);
    privacyLayout->addWidget(privacyHint);
    generalLayout->addWidget(privacyCard);
    generalLayout->addStretch();
    m_pages->addWidget(generalPage);

    m_searchPage = new SearchSettingsPage(m_searchSettings, m_pages);
    m_pages->addWidget(m_searchPage);

    auto *privacyDataPage = new QWidget(m_pages);
    privacyDataPage->setObjectName(QStringLiteral("privacyDataSettingsPage"));
    auto *dataLayout = new QVBoxLayout(privacyDataPage);
    dataLayout->setContentsMargins(30, 24, 30, 24);
    dataLayout->setSpacing(14);

    auto *dataTitle = new QLabel(QStringLiteral("Privacy & Data"), privacyDataPage);
    dataTitle->setObjectName(QStringLiteral("dialogTitle"));
    dataLayout->addWidget(dataTitle);
    auto *dataSubtitle = new QLabel(
        QStringLiteral("Remove browsing data kept in PanBrowser’s isolated WebEngine profile."),
        privacyDataPage
    );
    dataSubtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    dataLayout->addWidget(dataSubtitle);

    auto *storedDataLabel = new QLabel(QStringLiteral("STORED DATA"), privacyDataPage);
    storedDataLabel->setObjectName(QStringLiteral("sectionLabel"));
    dataLayout->addWidget(storedDataLabel);

    auto *cacheCard = new QFrame(privacyDataPage);
    cacheCard->setObjectName(QStringLiteral("settingsCard"));
    auto *cacheLayout = new QHBoxLayout(cacheCard);
    cacheLayout->setContentsMargins(18, 16, 18, 16);
    cacheLayout->setSpacing(18);
    auto *cacheTextLayout = new QVBoxLayout();
    cacheTextLayout->setSpacing(5);
    auto *cacheTitle = new QLabel(QStringLiteral("HTTP cache"), cacheCard);
    cacheTitle->setObjectName(QStringLiteral("settingsCardTitle"));
    cacheTextLayout->addWidget(cacheTitle);
    auto *cacheDescription = new QLabel(
        QStringLiteral("Remove temporary page resources. This does not sign you out."),
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
    auto *clearCache = new QPushButton(QStringLiteral("Clear cache"), cacheCard);
    cacheLayout->addWidget(clearCache);
    dataLayout->addWidget(cacheCard);

    auto *cookiesCard = new QFrame(privacyDataPage);
    cookiesCard->setObjectName(QStringLiteral("settingsCard"));
    auto *cookiesLayout = new QHBoxLayout(cookiesCard);
    cookiesLayout->setContentsMargins(18, 16, 18, 16);
    cookiesLayout->setSpacing(18);
    auto *cookiesTextLayout = new QVBoxLayout();
    cookiesTextLayout->setSpacing(5);
    auto *cookiesTitle = new QLabel(QStringLiteral("Cookies"), cookiesCard);
    cookiesTitle->setObjectName(QStringLiteral("settingsCardTitle"));
    cookiesTextLayout->addWidget(cookiesTitle);
    auto *cookiesDescription = new QLabel(
        QStringLiteral("Remove persistent and session cookies. Most sites will ask you to sign in again."),
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
    auto *clearCookies = new QPushButton(QStringLiteral("Clear cookies…"), cookiesCard);
    cookiesLayout->addWidget(clearCookies);
    dataLayout->addWidget(cookiesCard);

    auto *allDataCard = new QFrame(privacyDataPage);
    allDataCard->setObjectName(QStringLiteral("settingsCard"));
    auto *allDataLayout = new QHBoxLayout(allDataCard);
    allDataLayout->setContentsMargins(18, 16, 18, 16);
    allDataLayout->setSpacing(18);
    auto *allDataTextLayout = new QVBoxLayout();
    allDataTextLayout->setSpacing(5);
    auto *allDataTitle = new QLabel(QStringLiteral("All site data"), allDataCard);
    allDataTitle->setObjectName(QStringLiteral("settingsCardTitle"));
    allDataTextLayout->addWidget(allDataTitle);
    auto *allDataDescription = new QLabel(
        QStringLiteral("Remove cookies, local storage, IndexedDB, service workers, and cache on the next launch. Settings, history, trust rules, certificates, and saved tabs are kept."),
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
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Save settings"));
    rootLayout->addWidget(buttons);

    connect(m_sidebar, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(useCurrent, &QPushButton::clicked, this, [this, currentUrl] {
        m_startPage->setText(currentUrl.toString());
    });
    connect(clearCache, &QPushButton::clicked, this, [this, clearCache, cacheStatus] {
        clearCache->setEnabled(false);
        cacheStatus->setText(QStringLiteral("Clearing cache…"));
        cacheStatus->show();
        m_profile->clearHttpCache();
    });
    connect(
        m_profile,
        &QWebEngineProfile::clearHttpCacheCompleted,
        this,
        [clearCache, cacheStatus] {
            clearCache->setEnabled(true);
            cacheStatus->setText(QStringLiteral("Cache cleared."));
            cacheStatus->show();
        }
    );
    connect(clearCookies, &QPushButton::clicked, this, [this, cookiesStatus] {
        if (QMessageBox::question(
                this,
                QStringLiteral("Clear cookies"),
                QStringLiteral("Clear all cookies? Most sites will ask you to sign in again."),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel
            ) != QMessageBox::Yes) {
            return;
        }
        m_profile->clearAllCookies();
        cookiesStatus->setText(
            QStringLiteral("Cookie deletion requested. Reload open tabs to update their sign-in state.")
        );
        cookiesStatus->show();
    });
    const auto updateDataResetUi = [resetAllData, allDataStatus] {
        const bool scheduled = BrowserProfile::dataResetScheduled();
        resetAllData->setText(
            scheduled ? QStringLiteral("Cancel scheduled reset")
                      : QStringLiteral("Reset on next launch…")
        );
        allDataStatus->setText(
            scheduled ? QStringLiteral("Full site-data reset is scheduled for the next launch.")
                      : QString()
        );
        allDataStatus->setVisible(scheduled);
    };
    connect(resetAllData, &QPushButton::clicked, this, [this, updateDataResetUi] {
        QString error;
        if (BrowserProfile::dataResetScheduled()) {
            if (!BrowserProfile::cancelDataReset(&error))
                QMessageBox::warning(this, QStringLiteral("Cannot cancel reset"), error);
            updateDataResetUi();
            return;
        }

        if (QMessageBox::question(
                this,
                QStringLiteral("Reset all site data"),
                QStringLiteral("Schedule deletion of all cookies and site storage the next time PanBrowser starts? History, trust rules, certificates, settings, and saved tabs will be kept."),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel
            ) != QMessageBox::Yes) {
            return;
        }
        if (!BrowserProfile::scheduleDataReset(&error))
            QMessageBox::warning(this, QStringLiteral("Cannot schedule reset"), error);
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
    return preferences;
}

void SettingsDialog::saveAndClose()
{
    BrowserPreferences preferences = preferencesFromControls();
    QString error;
    if (!preferences.validate(&error)) {
        selectPage(Page::General);
        QMessageBox::warning(this, QStringLiteral("Cannot save settings"), error);
        return;
    }
    if (!m_trustRules->validate(&error)) {
        selectPage(Page::TrustRules);
        QMessageBox::warning(this, QStringLiteral("Cannot save trust rules"), error);
        return;
    }
    if (!m_searchPage->validate(&error)) {
        selectPage(Page::Search);
        QMessageBox::warning(this, QStringLiteral("Cannot save search settings"), error);
        return;
    }
    QList<FileSnapshot> snapshots;
    for (const QString &path : {
             m_searchConfigurationPath,
             m_searchConfigurationPath + QStringLiteral(".backup"),
             m_configurationPath,
             m_configurationPath + QStringLiteral(".backup"),
         }) {
        FileSnapshot snapshot;
        if (!captureFile(path, &snapshot, &error)) {
            QMessageBox::warning(this, QStringLiteral("Cannot save settings"), error);
            return;
        }
        snapshots.append(snapshot);
    }

    SearchSettings searchSettings = m_searchPage->settings();
    if (!preferences.save(&error)) {
        selectPage(Page::General);
        QMessageBox::warning(this, QStringLiteral("Cannot save settings"), error);
        return;
    }
    if (!searchSettings.save(m_searchConfigurationPath, &error)) {
        const QString rollbackError = rollbackSettings(m_preferences, snapshots.mid(0, 2));
        selectPage(Page::Search);
        if (!rollbackError.isEmpty())
            error += QStringLiteral("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, QStringLiteral("Cannot save search settings"), error);
        return;
    }
    if (!m_trustRules->save(&error)) {
        const QString rollbackError = rollbackSettings(m_preferences, snapshots);
        selectPage(Page::TrustRules);
        if (!rollbackError.isEmpty())
            error += QStringLiteral("\n\nRollback was incomplete:\n") + rollbackError;
        QMessageBox::warning(this, QStringLiteral("Cannot save trust rules"), error);
        return;
    }

    m_trustRules->finalizeSave();
    m_preferences = preferences;
    m_searchSettings = searchSettings;
    accept();
}
