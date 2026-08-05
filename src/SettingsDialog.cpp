#include "SettingsDialog.h"

#include "TrustRulesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(
    const QString &configurationPath,
    const BrowserPreferences &preferences,
    const QUrl &currentUrl,
    Page initialPage,
    QWidget *parent
)
    : QDialog(parent)
    , m_configurationPath(configurationPath)
    , m_preferences(preferences)
{
    createInterface(currentUrl, initialPage);
}

bool SettingsDialog::load(QString *error)
{
    return m_trustRules->load(error);
}

BrowserPreferences SettingsDialog::preferences() const
{
    return m_preferences;
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
    auto *trustItem = new QListWidgetItem(
        QIcon(QStringLiteral(":/assets/icons/shield-check.svg")),
        QStringLiteral("Trust Rules"),
        m_sidebar
    );
    trustItem->setData(Qt::UserRole, static_cast<int>(Page::TrustRules));
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
    if (!m_trustRules->save(&error)) {
        selectPage(Page::TrustRules);
        QMessageBox::warning(this, QStringLiteral("Cannot save trust rules"), error);
        return;
    }
    if (!preferences.save(&error)) {
        selectPage(Page::General);
        QMessageBox::warning(this, QStringLiteral("Cannot save settings"), error);
        return;
    }

    m_preferences = preferences;
    accept();
}
