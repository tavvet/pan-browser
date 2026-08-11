#pragma once

#include <QCoreApplication>
#include <QWidget>

class BrowserProfile;
class QLabel;
class QPushButton;

class PrivacyDataSettingsPage final : public QWidget {
    Q_DECLARE_TR_FUNCTIONS(SettingsDialog)

public:
    explicit PrivacyDataSettingsPage(
        BrowserProfile *profile,
        QWidget *parent = nullptr
    );

private:
    void updateDataResetUi();

    BrowserProfile *m_profile = nullptr;
    QPushButton *m_resetAllData = nullptr;
    QLabel *m_allDataStatus = nullptr;
};
