#pragma once

#include <QList>
#include <QString>

class QWebEngineProfile;

enum class UserAgentPlatform {
    System,
    Windows,
    MacOS,
    Linux,
    Android,
    IOS,
};

struct UserAgentProfile {
    QString id;
    QString name;
    QString userAgent;
    UserAgentPlatform platform = UserAgentPlatform::System;
    bool mobile = false;
    bool builtIn = false;
};

class UserAgentSettings {
public:
    static UserAgentSettings defaults();

    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;

    [[nodiscard]] QString selectedProfileId() const;
    void setSelectedProfileId(const QString &id);
    [[nodiscard]] const QList<UserAgentProfile> &profiles() const;
    [[nodiscard]] QList<UserAgentProfile> &profiles();
    [[nodiscard]] const UserAgentProfile *profileById(const QString &id) const;
    [[nodiscard]] UserAgentProfile *profileById(const QString &id);
    [[nodiscard]] const UserAgentProfile *selectedProfile() const;

private:
    QList<UserAgentProfile> m_profiles;
    QString m_selectedProfileId;
};

[[nodiscard]] QString defaultUserAgentProfileId();
[[nodiscard]] QString userAgentPlatformKey(UserAgentPlatform platform);
[[nodiscard]] QString userAgentPlatformDisplayName(UserAgentPlatform platform);
[[nodiscard]] bool hasSameEffectiveUserAgentConfiguration(
    const UserAgentSettings &left,
    const UserAgentSettings &right
);
[[nodiscard]] bool applyUserAgentSettings(
    QWebEngineProfile *profile,
    const UserAgentSettings &settings,
    QString *error = nullptr
);
