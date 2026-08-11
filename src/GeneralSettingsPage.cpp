#include "GeneralSettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

GeneralSettingsPage::GeneralSettingsPage(
    const BrowserPreferences &preferences,
    const QUrl &currentUrl,
    QWidget *parent
)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("generalSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("General"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Choose how PanBrowser starts and what it keeps between launches."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    layout->addWidget(subtitle);

    auto *languageLabel = new QLabel(tr("LANGUAGE"), this);
    languageLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(languageLabel);

    auto *languageCard = new QFrame(this);
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
        static_cast<int>(preferences.interfaceLanguage())
    ));
    languageLayout->addRow(tr("Interface language"), m_interfaceLanguage);
    auto *languageHint = new QLabel(
        tr("Language changes take effect after PanBrowser restarts."),
        languageCard
    );
    languageHint->setObjectName(QStringLiteral("fieldHint"));
    languageHint->setWordWrap(true);
    languageLayout->addRow(languageHint);
    layout->addWidget(languageCard);

    auto *startupLabel = new QLabel(tr("STARTUP"), this);
    startupLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(startupLabel);

    auto *startupCard = new QFrame(this);
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
    m_startPage->setText(preferences.startPage().toString());
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
        static_cast<int>(preferences.startupMode())
    ));
    startupLayout->addRow(tr("On launch"), m_startupMode);

    auto *restoreSignInHint = new QLabel(
        tr("Some restored tabs may ask you to sign in again unless “Keep sign-ins between launches” is enabled."),
        startupCard
    );
    restoreSignInHint->setObjectName(QStringLiteral("fieldHint"));
    restoreSignInHint->setWordWrap(true);
    m_restoreSignInHint = restoreSignInHint;
    startupLayout->addRow(restoreSignInHint);
    layout->addWidget(startupCard);

    auto *privacyLabel = new QLabel(tr("PRIVACY"), this);
    privacyLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(privacyLabel);

    auto *privacyCard = new QFrame(this);
    privacyCard->setObjectName(QStringLiteral("settingsCard"));
    auto *privacyLayout = new QVBoxLayout(privacyCard);
    privacyLayout->setContentsMargins(18, 16, 18, 16);
    privacyLayout->setSpacing(8);
    m_persistSessionCookies = new QCheckBox(
        tr("Keep sign-ins between launches"),
        privacyCard
    );
    m_persistSessionCookies->setChecked(preferences.persistSessionCookies());
    privacyLayout->addWidget(m_persistSessionCookies);
    auto *privacyHint = new QLabel(
        tr("Session cookies will be stored in the PanBrowser profile. Avoid this on a shared computer."),
        privacyCard
    );
    privacyHint->setObjectName(QStringLiteral("fieldHint"));
    privacyHint->setWordWrap(true);
    privacyLayout->addWidget(privacyHint);
    layout->addWidget(privacyCard);

    auto *developerLabel = new QLabel(tr("DEVELOPER TOOLS"), this);
    developerLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(developerLabel);

    auto *developerCard = new QFrame(this);
    developerCard->setObjectName(QStringLiteral("settingsCard"));
    auto *developerLayout = new QVBoxLayout(developerCard);
    developerLayout->setContentsMargins(18, 16, 18, 16);
    developerLayout->setSpacing(8);
    m_developerToolsEnabled = new QCheckBox(
        tr("Enable developer tools"),
        developerCard
    );
    m_developerToolsEnabled->setChecked(preferences.developerToolsEnabled());
    developerLayout->addWidget(m_developerToolsEnabled);
    auto *developerHint = new QLabel(
        tr("Developer tools can read and modify page content and browser data for the inspected site."),
        developerCard
    );
    developerHint->setObjectName(QStringLiteral("fieldHint"));
    developerHint->setWordWrap(true);
    developerLayout->addWidget(developerHint);
    layout->addWidget(developerCard);
    layout->addStretch();

    connect(useCurrent, &QPushButton::clicked, this, [this, currentUrl] {
        m_startPage->setText(currentUrl.toString());
    });
    connect(m_startupMode, &QComboBox::currentIndexChanged, this, [this](int) {
        updateRestoreHint();
    });
    connect(m_persistSessionCookies, &QCheckBox::toggled, this, [this](bool) {
        updateRestoreHint();
    });
    updateRestoreHint();
}

BrowserPreferences GeneralSettingsPage::applyTo(BrowserPreferences preferences) const
{
    preferences.setStartPage(QUrl::fromUserInput(m_startPage->text().trimmed()));
    preferences.setStartupMode(
        static_cast<StartupMode>(m_startupMode->currentData().toInt())
    );
    preferences.setPersistSessionCookies(m_persistSessionCookies->isChecked());
    preferences.setDeveloperToolsEnabled(m_developerToolsEnabled->isChecked());
    preferences.setInterfaceLanguage(
        static_cast<InterfaceLanguage>(m_interfaceLanguage->currentData().toInt())
    );
    return preferences;
}

void GeneralSettingsPage::updateRestoreHint()
{
    const bool restoringTabs = static_cast<StartupMode>(
        m_startupMode->currentData().toInt()
    ) == StartupMode::RestoreTabs;
    m_restoreSignInHint->setVisible(
        restoringTabs && !m_persistSessionCookies->isChecked()
    );
}
