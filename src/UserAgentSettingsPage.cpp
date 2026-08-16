#include "UserAgentSettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QUuid>
#include <QVBoxLayout>

#include <utility>

namespace {

QString uiText(const char *source)
{
    return QCoreApplication::translate("UserAgentSettingsPage", source);
}

class UserAgentProfileEditor final : public QDialog {
public:
    UserAgentProfileEditor(
        const UserAgentProfile &profile,
        bool adding,
        QWidget *parent
    )
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("userAgentProfileDialog"));
        setWindowTitle(
            adding
                ? uiText(QT_TRANSLATE_NOOP(
                    "UserAgentSettingsPage",
                    "Add User-Agent profile"
                ))
                : uiText(QT_TRANSLATE_NOOP(
                    "UserAgentSettingsPage",
                    "Edit User-Agent profile"
                ))
        );
        setModal(true);
        resize(720, 430);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(22, 20, 22, 18);
        layout->setSpacing(14);

        auto *title = new QLabel(windowTitle(), this);
        title->setObjectName(QStringLiteral("dialogTitle"));
        layout->addWidget(title);

        auto *form = new QFormLayout();
        form->setHorizontalSpacing(18);
        form->setVerticalSpacing(12);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        m_name = new QLineEdit(profile.name, this);
        m_name->setObjectName(QStringLiteral("userAgentProfileName"));
        m_name->setPlaceholderText(uiText(QT_TRANSLATE_NOOP(
            "UserAgentSettingsPage",
            "My compatibility profile"
        )));
        form->addRow(
            uiText(QT_TRANSLATE_NOOP("UserAgentSettingsPage", "Name")),
            m_name
        );

        m_platform = new QComboBox(this);
        m_platform->setObjectName(QStringLiteral("userAgentProfilePlatform"));
        for (const UserAgentPlatform platform : {
                 UserAgentPlatform::System,
                 UserAgentPlatform::Windows,
                 UserAgentPlatform::MacOS,
                 UserAgentPlatform::Linux,
                 UserAgentPlatform::Android,
                 UserAgentPlatform::IOS,
             }) {
            m_platform->addItem(
                userAgentPlatformDisplayName(platform),
                static_cast<int>(platform)
            );
        }
        m_platform->setCurrentIndex(
            m_platform->findData(static_cast<int>(profile.platform))
        );
        form->addRow(
            uiText(QT_TRANSLATE_NOOP("UserAgentSettingsPage", "Platform")),
            m_platform
        );

        m_mobile = new QCheckBox(
            uiText(QT_TRANSLATE_NOOP("UserAgentSettingsPage", "Mobile device")),
            this
        );
        m_mobile->setObjectName(QStringLiteral("userAgentProfileMobile"));
        m_mobile->setChecked(profile.mobile);
        form->addRow(
            uiText(QT_TRANSLATE_NOOP("UserAgentSettingsPage", "Form factor")),
            m_mobile
        );
        layout->addLayout(form);

        auto *userAgentLabel = new QLabel(
            uiText(QT_TRANSLATE_NOOP("UserAgentSettingsPage", "USER-AGENT STRING")),
            this
        );
        userAgentLabel->setObjectName(QStringLiteral("sectionLabel"));
        layout->addWidget(userAgentLabel);
        m_userAgent = new QPlainTextEdit(profile.userAgent, this);
        m_userAgent->setObjectName(QStringLiteral("userAgentProfileValue"));
        m_userAgent->setPlaceholderText(QStringLiteral("Mozilla/5.0 ..."));
        m_userAgent->setFixedHeight(105);
        layout->addWidget(m_userAgent);

        auto *hint = new QLabel(
            uiText(QT_TRANSLATE_NOOP(
                "UserAgentSettingsPage",
                "Use printable ASCII only. PanBrowser also aligns the platform and mobile Client Hints where Qt allows it."
            )),
            this
        );
        hint->setObjectName(QStringLiteral("fieldHint"));
        hint->setWordWrap(true);
        layout->addWidget(hint);

        auto *buttons = new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel,
            Qt::Horizontal,
            this
        );
        buttons->button(QDialogButtonBox::Save)->setText(
            adding
                ? uiText(QT_TRANSLATE_NOOP("UserAgentSettingsPage", "Add profile"))
                : uiText(QT_TRANSLATE_NOOP("UserAgentSettingsPage", "Save profile"))
        );
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void applyTo(UserAgentProfile *profile) const
    {
        if (!profile)
            return;
        profile->name = m_name->text().trimmed();
        profile->platform = static_cast<UserAgentPlatform>(
            m_platform->currentData().toInt()
        );
        profile->mobile = m_mobile->isChecked();
        profile->userAgent = m_userAgent->toPlainText().trimmed();
    }

private:
    QLineEdit *m_name = nullptr;
    QComboBox *m_platform = nullptr;
    QCheckBox *m_mobile = nullptr;
    QPlainTextEdit *m_userAgent = nullptr;
};

} // namespace

UserAgentSettingsPage::UserAgentSettingsPage(
    const UserAgentSettings &settings,
    const QString &defaultUserAgent,
    QWidget *parent
)
    : QWidget(parent)
    , m_settings(settings)
    , m_defaultUserAgent(defaultUserAgent)
{
    setObjectName(QStringLiteral("userAgentSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("User-Agent"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Choose how websites identify this PanBrowser profile."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    layout->addWidget(subtitle);

    auto *activeLabel = new QLabel(tr("ACTIVE PROFILE"), this);
    activeLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(activeLabel);
    auto *activeCard = new QFrame(this);
    activeCard->setObjectName(QStringLiteral("settingsCard"));
    auto *activeLayout = new QFormLayout(activeCard);
    activeLayout->setContentsMargins(18, 16, 18, 16);
    activeLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_activeProfile = new QComboBox(activeCard);
    m_activeProfile->setObjectName(QStringLiteral("activeUserAgentProfile"));
    activeLayout->addRow(tr("Profile"), m_activeProfile);
    layout->addWidget(activeCard);

    auto *restartHint = new QLabel(
        tr("Changes to the active profile take effect after PanBrowser restarts."),
        this
    );
    restartHint->setObjectName(QStringLiteral("fieldHint"));
    restartHint->setWordWrap(true);
    layout->addWidget(restartHint);

    auto *profilesLabel = new QLabel(tr("USER-AGENT PROFILES"), this);
    profilesLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(profilesLabel);
    m_profileList = new QListWidget(this);
    m_profileList->setObjectName(QStringLiteral("userAgentProfilesList"));
    m_profileList->setSpacing(2);
    layout->addWidget(m_profileList, 1);

    m_details = new QLabel(this);
    m_details->setObjectName(QStringLiteral("fieldHint"));
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_details->setWordWrap(true);
    layout->addWidget(m_details);

    auto *warning = new QLabel(
        tr("User-Agent profiles are for website compatibility. They do not fully emulate another browser or device."),
        this
    );
    warning->setObjectName(QStringLiteral("settingsWarning"));
    warning->setWordWrap(true);
    layout->addWidget(warning);

    auto *buttons = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("Add profile"), this);
    addButton->setObjectName(QStringLiteral("addUserAgentProfile"));
    m_editButton = new QPushButton(tr("Edit"), this);
    m_editButton->setObjectName(QStringLiteral("editUserAgentProfile"));
    m_duplicateButton = new QPushButton(tr("Duplicate"), this);
    m_duplicateButton->setObjectName(QStringLiteral("duplicateUserAgentProfile"));
    m_removeButton = new QPushButton(tr("Remove"), this);
    m_removeButton->setObjectName(QStringLiteral("removeUserAgentProfile"));
    m_removeButton->setProperty("dangerAction", true);
    buttons->addWidget(addButton);
    buttons->addWidget(m_editButton);
    buttons->addWidget(m_duplicateButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    connect(m_activeProfile, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_rebuilding && m_activeProfile->currentIndex() >= 0) {
            m_settings.setSelectedProfileId(
                m_activeProfile->currentData().toString()
            );
        }
    });
    connect(m_profileList, &QListWidget::currentRowChanged, this, [this](int) {
        updateActions();
        updateDetails();
    });
    connect(
        m_profileList,
        &QListWidget::itemDoubleClicked,
        this,
        [this](QListWidgetItem *) { editSelectedProfile(); }
    );
    connect(addButton, &QPushButton::clicked, this, &UserAgentSettingsPage::addProfile);
    connect(
        m_editButton,
        &QPushButton::clicked,
        this,
        &UserAgentSettingsPage::editSelectedProfile
    );
    connect(
        m_duplicateButton,
        &QPushButton::clicked,
        this,
        &UserAgentSettingsPage::duplicateSelectedProfile
    );
    connect(
        m_removeButton,
        &QPushButton::clicked,
        this,
        &UserAgentSettingsPage::removeSelectedProfile
    );
    rebuildProfiles();
}

UserAgentSettings UserAgentSettingsPage::settings() const
{
    return m_settings;
}

bool UserAgentSettingsPage::validate(QString *error) const
{
    return m_settings.validate(error);
}

void UserAgentSettingsPage::rebuildProfiles(const QString &selectedId)
{
    QString id = selectedId;
    if (id.isEmpty() && m_profileList->currentItem())
        id = m_profileList->currentItem()->data(Qt::UserRole).toString();
    m_rebuilding = true;
    m_profileList->clear();
    int selectedRow = -1;
    for (const UserAgentProfile &profile : m_settings.profiles()) {
        QString profileText = profile.name;
        if (profile.id != defaultUserAgentProfileId()) {
            profileText += tr("   %1 · %2").arg(
                userAgentPlatformDisplayName(profile.platform),
                profile.mobile ? tr("Mobile") : tr("Desktop")
            );
        }
        if (profile.builtIn)
            profileText += tr("   Built-in");
        auto *item = new QListWidgetItem(profileText, m_profileList);
        item->setData(Qt::UserRole, profile.id);
        item->setToolTip(profile.userAgent.isEmpty() ? m_defaultUserAgent
                                                     : profile.userAgent);
        if (profile.id == id)
            selectedRow = m_profileList->count() - 1;
    }
    m_rebuilding = false;
    rebuildActiveProfiles();
    if (m_profileList->count() > 0)
        m_profileList->setCurrentRow(selectedRow >= 0 ? selectedRow : 0);
    updateActions();
    updateDetails();
}

void UserAgentSettingsPage::rebuildActiveProfiles()
{
    const QSignalBlocker blocker(m_activeProfile);
    const QString selected = m_settings.selectedProfileId();
    m_activeProfile->clear();
    for (const UserAgentProfile &profile : m_settings.profiles())
        m_activeProfile->addItem(profile.name, profile.id);
    int index = m_activeProfile->findData(selected);
    if (index < 0) {
        index = m_activeProfile->findData(defaultUserAgentProfileId());
        m_settings.setSelectedProfileId(defaultUserAgentProfileId());
    }
    m_activeProfile->setCurrentIndex(index);
}

void UserAgentSettingsPage::updateActions()
{
    const UserAgentProfile *profile = selectedProfile();
    m_editButton->setEnabled(profile && !profile->builtIn);
    m_duplicateButton->setEnabled(
        profile && profile->id != defaultUserAgentProfileId()
    );
    m_removeButton->setEnabled(profile && !profile->builtIn);
}

void UserAgentSettingsPage::updateDetails()
{
    const UserAgentProfile *profile = selectedProfile();
    if (!profile) {
        m_details->clear();
        return;
    }
    const QString userAgent = profile->id == defaultUserAgentProfileId()
        ? m_defaultUserAgent
        : profile->userAgent;
    m_details->setText(
        userAgent.isEmpty() ? tr("Provided by Chromium at runtime") : userAgent
    );
}

void UserAgentSettingsPage::addProfile()
{
    UserAgentProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.platform = UserAgentPlatform::System;
    if (!editProfile(&profile, true))
        return;
    m_settings.profiles().append(profile);
    rebuildProfiles(profile.id);
}

void UserAgentSettingsPage::editSelectedProfile()
{
    UserAgentProfile *profile = selectedProfile();
    if (!profile || profile->builtIn)
        return;
    const QString id = profile->id;
    if (editProfile(profile, false))
        rebuildProfiles(id);
}

void UserAgentSettingsPage::duplicateSelectedProfile()
{
    const UserAgentProfile *selected = selectedProfile();
    if (!selected || selected->id == defaultUserAgentProfileId())
        return;
    UserAgentProfile profile = *selected;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = tr("%1 copy").arg(selected->name);
    profile.builtIn = false;
    if (!editProfile(&profile, true))
        return;
    m_settings.profiles().append(profile);
    rebuildProfiles(profile.id);
}

void UserAgentSettingsPage::removeSelectedProfile()
{
    const UserAgentProfile *profile = selectedProfile();
    if (!profile || profile->builtIn)
        return;
    const QString id = profile->id;
    const QString name = profile->name;
    if (QMessageBox::question(
            this,
            tr("Remove User-Agent profile"),
            tr("Remove “%1”? Changes are saved only after you save Settings.")
                .arg(name),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }
    if (m_settings.selectedProfileId() == id)
        m_settings.setSelectedProfileId(defaultUserAgentProfileId());
    for (qsizetype index = 0; index < m_settings.profiles().size(); ++index) {
        if (m_settings.profiles().at(index).id == id) {
            m_settings.profiles().removeAt(index);
            break;
        }
    }
    rebuildProfiles();
}

bool UserAgentSettingsPage::editProfile(UserAgentProfile *profile, bool adding)
{
    if (!profile)
        return false;
    UserAgentProfileEditor dialog(*profile, adding, this);
    while (dialog.exec() == QDialog::Accepted) {
        UserAgentProfile candidate = *profile;
        dialog.applyTo(&candidate);
        candidate.builtIn = false;

        UserAgentSettings validation = m_settings;
        bool replaced = false;
        for (UserAgentProfile &existing : validation.profiles()) {
            if (existing.id == candidate.id) {
                existing = candidate;
                replaced = true;
                break;
            }
        }
        if (!replaced)
            validation.profiles().append(candidate);

        QString error;
        if (validation.validate(&error)) {
            *profile = std::move(candidate);
            return true;
        }
        QMessageBox::warning(
            &dialog,
            tr("Cannot save User-Agent profile"),
            error
        );
    }
    return false;
}

UserAgentProfile *UserAgentSettingsPage::selectedProfile()
{
    if (!m_profileList->currentItem())
        return nullptr;
    return m_settings.profileById(
        m_profileList->currentItem()->data(Qt::UserRole).toString()
    );
}

const UserAgentProfile *UserAgentSettingsPage::selectedProfile() const
{
    if (!m_profileList->currentItem())
        return nullptr;
    return m_settings.profileById(
        m_profileList->currentItem()->data(Qt::UserRole).toString()
    );
}
