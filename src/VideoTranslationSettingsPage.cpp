#include "VideoTranslationSettingsPage.h"

#include "VotUserscriptManager.h"
#include "VotUserscriptPackage.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

VideoTranslationSettingsPage::VideoTranslationSettingsPage(
    const VideoTranslationSettings &settings,
    VotUserscriptManager *manager,
    QWidget *parent
)
    : QWidget(parent)
    , m_settings(settings)
    , m_manager(manager)
{
    setObjectName(QStringLiteral("videoTranslationSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("Video Translation"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Experimentally add voice-over video translation using the third-party VOT userscript."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *statusLabel = new QLabel(tr("STATUS"), this);
    statusLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(statusLabel);

    auto *statusCard = new QFrame(this);
    statusCard->setObjectName(QStringLiteral("settingsCard"));
    auto *statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(18, 16, 18, 16);
    statusLayout->setSpacing(8);
    m_status = new QLabel(statusCard);
    m_status->setObjectName(QStringLiteral("votUserscriptStatus"));
    m_status->setWordWrap(true);
    statusLayout->addWidget(m_status);
    auto *restartHint = new QLabel(
        tr("After changing this setting or the userscript, reload already open video pages."),
        statusCard
    );
    restartHint->setObjectName(QStringLiteral("fieldHint"));
    restartHint->setWordWrap(true);
    statusLayout->addWidget(restartHint);
    layout->addWidget(statusCard);

    auto *extensionLabel = new QLabel(tr("VOT USERSCRIPT"), this);
    extensionLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(extensionLabel);

    auto *extensionCard = new QFrame(this);
    extensionCard->setObjectName(QStringLiteral("settingsCard"));
    auto *extensionLayout = new QVBoxLayout(extensionCard);
    extensionLayout->setContentsMargins(18, 16, 18, 16);
    extensionLayout->setSpacing(10);

    m_enabled = new QCheckBox(tr("Enable VOT video translation"), extensionCard);
    m_enabled->setObjectName(QStringLiteral("enableVotUserscript"));
    m_enabled->setChecked(settings.enabled());
    extensionLayout->addWidget(m_enabled);

    auto *sourceRow = new QWidget(extensionCard);
    auto *sourceLayout = new QHBoxLayout(sourceRow);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    sourceLayout->setSpacing(8);
    m_sourcePath = new QLineEdit(sourceRow);
    m_sourcePath->setObjectName(QStringLiteral("votUserscriptSourcePath"));
    m_sourcePath->setText(settings.sourcePath());
    m_sourcePath->setPlaceholderText(tr("Official vot.user.js"));
    auto *chooseButton = new QPushButton(tr("Choose script…"), sourceRow);
    chooseButton->setObjectName(QStringLiteral("chooseVotUserscript"));
    sourceLayout->addWidget(m_sourcePath, 1);
    sourceLayout->addWidget(chooseButton);
    extensionLayout->addWidget(sourceRow);

    auto *sourceHint = new QLabel(
        tr("Select vot.user.js from the official VOT %1 release. PanBrowser accepts only the file with the verified SHA-256 hash.\n%2")
            .arg(
                VotUserscriptPackage::supportedVersion(),
                VotUserscriptPackage::officialDownloadUrl()
            ),
        extensionCard
    );
    sourceHint->setObjectName(QStringLiteral("fieldHint"));
    sourceHint->setWordWrap(true);
    extensionLayout->addWidget(sourceHint);

    auto *privacyHint = new QLabel(
        tr("VOT can read supported video pages and contact the HTTPS hosts declared by its verified @connect metadata. Its code and network behavior are maintained outside PanBrowser. Video translation currently requires System DNS because Qt Network cannot use Chromium Secure DNS."),
        extensionCard
    );
    privacyHint->setObjectName(QStringLiteral("fieldHint"));
    privacyHint->setWordWrap(true);
    extensionLayout->addWidget(privacyHint);
    layout->addWidget(extensionCard);
    layout->addStretch();

    connect(
        chooseButton,
        &QPushButton::clicked,
        this,
        &VideoTranslationSettingsPage::chooseUserscript
    );
    if (m_manager) {
        connect(
            m_manager,
            &VotUserscriptManager::stateChanged,
            this,
            &VideoTranslationSettingsPage::updateStatus
        );
    }
    updateStatus();
}

VideoTranslationSettings VideoTranslationSettingsPage::settings() const
{
    VideoTranslationSettings result = m_settings;
    result.setEnabled(m_enabled->isChecked());
    result.setSourcePath(m_sourcePath->text());
    return result;
}

bool VideoTranslationSettingsPage::validate(QString *error) const
{
    const VideoTranslationSettings candidate = settings();
    if (!candidate.validate(error))
        return false;
    if (!candidate.enabled())
        return true;
    VotUserscript userscript;
    return VotUserscriptPackage::load(candidate.sourcePath(), &userscript, error);
}

void VideoTranslationSettingsPage::chooseUserscript()
{
    const QString initialDirectory = QFileInfo(m_sourcePath->text()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Choose the official VOT userscript"),
        initialDirectory,
        tr("JavaScript files (*.user.js *.js)")
    );
    if (!path.isEmpty())
        m_sourcePath->setText(path);
}

void VideoTranslationSettingsPage::updateStatus()
{
    if (!m_manager) {
        m_status->setText(tr("Userscript manager unavailable"));
        return;
    }
    m_status->setText(m_manager->statusText());
}
