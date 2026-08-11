#include "PrivacyDataSettingsPage.h"

#include "BrowserProfile.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PrivacyDataSettingsPage::PrivacyDataSettingsPage(
    BrowserProfile *profile,
    QWidget *parent
)
    : QWidget(parent)
    , m_profile(profile)
{
    setObjectName(QStringLiteral("privacyDataSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("Privacy & Data"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Remove browsing data kept in PanBrowser’s isolated WebEngine profile."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    layout->addWidget(subtitle);

    auto *storedDataLabel = new QLabel(tr("STORED DATA"), this);
    storedDataLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(storedDataLabel);

    auto *cacheCard = new QFrame(this);
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
    layout->addWidget(cacheCard);

    auto *cookiesCard = new QFrame(this);
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
    layout->addWidget(cookiesCard);

    auto *allDataCard = new QFrame(this);
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
    m_allDataStatus = new QLabel(allDataCard);
    m_allDataStatus->setObjectName(QStringLiteral("dataActionStatus"));
    allDataTextLayout->addWidget(m_allDataStatus);
    allDataLayout->addLayout(allDataTextLayout, 1);
    m_resetAllData = new QPushButton(allDataCard);
    m_resetAllData->setObjectName(QStringLiteral("dangerButton"));
    allDataLayout->addWidget(m_resetAllData);
    layout->addWidget(allDataCard);
    layout->addStretch();

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
    connect(m_resetAllData, &QPushButton::clicked, this, [this] {
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
    updateDataResetUi();
}

void PrivacyDataSettingsPage::updateDataResetUi()
{
    const bool scheduled = BrowserProfile::dataResetScheduled();
    m_resetAllData->setText(
        scheduled ? tr("Cancel scheduled reset") : tr("Reset on next launch…")
    );
    m_allDataStatus->setText(
        scheduled ? tr("Full site-data reset is scheduled for the next launch.") : QString()
    );
    m_allDataStatus->setVisible(scheduled);
}
