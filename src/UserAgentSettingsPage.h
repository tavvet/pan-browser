#pragma once

#include "UserAgentSettings.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

class UserAgentSettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit UserAgentSettingsPage(
        const UserAgentSettings &settings,
        const QString &defaultUserAgent,
        QWidget *parent = nullptr
    );

    [[nodiscard]] UserAgentSettings settings() const;
    bool validate(QString *error = nullptr) const;

private:
    void rebuildProfiles(const QString &selectedId = QString());
    void rebuildActiveProfiles();
    void updateActions();
    void updateDetails();
    void addProfile();
    void editSelectedProfile();
    void duplicateSelectedProfile();
    void removeSelectedProfile();
    bool editProfile(UserAgentProfile *profile, bool adding);
    [[nodiscard]] UserAgentProfile *selectedProfile();
    [[nodiscard]] const UserAgentProfile *selectedProfile() const;

    UserAgentSettings m_settings;
    QString m_defaultUserAgent;
    QComboBox *m_activeProfile = nullptr;
    QListWidget *m_profileList = nullptr;
    QLabel *m_details = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_duplicateButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    bool m_rebuilding = false;
};
