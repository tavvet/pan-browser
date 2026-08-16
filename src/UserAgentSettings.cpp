#include "UserAgentSettings.h"

#include "PrivateData.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QWebEngineClientHints>
#include <QWebEngineProfile>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

#include <utility>

namespace {

constexpr qint64 maximumSettingsFileSize = 256 * 1024;
constexpr qsizetype maximumCustomProfiles = 64;
constexpr qsizetype maximumProfileNameLength = 80;
constexpr qsizetype maximumUserAgentLength = 1024;

QString text(const char *source)
{
    return QCoreApplication::translate("UserAgentSettings", source);
}

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString chromiumVersion()
{
    const QString version = QString::fromLatin1(qWebEngineChromiumVersion()).trimmed();
    return version.isEmpty() ? QStringLiteral("0.0.0.0") : version;
}

QString chromiumUserAgent(const QString &platformToken, bool mobile)
{
    return QStringLiteral(
        "Mozilla/5.0 (%1) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/%2 %3Safari/537.36"
    ).arg(
        platformToken,
        chromiumVersion(),
        mobile ? QStringLiteral("Mobile ") : QString()
    );
}

QList<UserAgentProfile> builtInProfiles()
{
    return {
        {
            defaultUserAgentProfileId(),
            text(QT_TRANSLATE_NOOP("UserAgentSettings", "Chromium default")),
            {},
            UserAgentPlatform::System,
            false,
            true,
        },
        {
            QStringLiteral("builtin-chromium-windows"),
            text(QT_TRANSLATE_NOOP("UserAgentSettings", "Chromium — Windows")),
            chromiumUserAgent(QStringLiteral("Windows NT 10.0; Win64; x64"), false),
            UserAgentPlatform::Windows,
            false,
            true,
        },
        {
            QStringLiteral("builtin-chromium-macos"),
            text(QT_TRANSLATE_NOOP("UserAgentSettings", "Chromium — macOS")),
            chromiumUserAgent(
                QStringLiteral("Macintosh; Intel Mac OS X 10_15_7"),
                false
            ),
            UserAgentPlatform::MacOS,
            false,
            true,
        },
        {
            QStringLiteral("builtin-chromium-linux"),
            text(QT_TRANSLATE_NOOP("UserAgentSettings", "Chromium — Linux")),
            chromiumUserAgent(QStringLiteral("X11; Linux x86_64"), false),
            UserAgentPlatform::Linux,
            false,
            true,
        },
        {
            QStringLiteral("builtin-chromium-android"),
            text(QT_TRANSLATE_NOOP("UserAgentSettings", "Chromium — Android mobile")),
            chromiumUserAgent(QStringLiteral("Linux; Android 10; K"), true),
            UserAgentPlatform::Android,
            true,
            true,
        },
    };
}

bool parsePlatform(const QString &key, UserAgentPlatform *platform)
{
    if (!platform)
        return false;
    if (key == QStringLiteral("system"))
        *platform = UserAgentPlatform::System;
    else if (key == QStringLiteral("windows"))
        *platform = UserAgentPlatform::Windows;
    else if (key == QStringLiteral("macos"))
        *platform = UserAgentPlatform::MacOS;
    else if (key == QStringLiteral("linux"))
        *platform = UserAgentPlatform::Linux;
    else if (key == QStringLiteral("android"))
        *platform = UserAgentPlatform::Android;
    else if (key == QStringLiteral("ios"))
        *platform = UserAgentPlatform::IOS;
    else
        return false;
    return true;
}

QString clientHintsPlatform(UserAgentPlatform platform)
{
    switch (platform) {
    case UserAgentPlatform::Windows:
        return QStringLiteral("Windows");
    case UserAgentPlatform::MacOS:
        return QStringLiteral("macOS");
    case UserAgentPlatform::Linux:
        return QStringLiteral("Linux");
    case UserAgentPlatform::Android:
        return QStringLiteral("Android");
    case UserAgentPlatform::IOS:
        return QStringLiteral("iOS");
    case UserAgentPlatform::System:
        return {};
    }
    return {};
}

bool isPrintableName(const QString &name)
{
    for (const QChar character : name) {
        if (!character.isPrint())
            return false;
    }
    return true;
}

bool isSafeUserAgent(const QString &userAgent)
{
    if (userAgent.isEmpty() || userAgent.size() > maximumUserAgentLength)
        return false;
    for (const QChar character : userAgent) {
        const ushort value = character.unicode();
        if (value < 0x20 || value > 0x7e)
            return false;
    }
    return true;
}

} // namespace

UserAgentSettings UserAgentSettings::defaults()
{
    UserAgentSettings settings;
    settings.m_profiles = builtInProfiles();
    settings.m_selectedProfileId = defaultUserAgentProfileId();
    return settings;
}

bool UserAgentSettings::load(const QString &path, QString *error)
{
    if (error)
        error->clear();
    if (!PrivateData::restrictFile(path, error))
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("UserAgentSettings", "Cannot open %1: %2"))
                .arg(path, file.errorString())
        );
    }
    if (file.size() > maximumSettingsFileSize) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "User-Agent settings file is too large"
            ))
        );
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "Invalid User-Agent settings JSON: %1"
            )).arg(parseError.errorString())
        );
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "Unsupported User-Agent settings version"
            ))
        );
    }
    if (!root.value(QStringLiteral("selectedProfile")).isString()
        || !root.value(QStringLiteral("customProfiles")).isArray()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "User-Agent settings have an invalid structure"
            ))
        );
    }

    UserAgentSettings loaded = defaults();
    loaded.m_selectedProfileId =
        root.value(QStringLiteral("selectedProfile")).toString().trimmed();
    const QJsonArray customProfiles = root.value(QStringLiteral("customProfiles")).toArray();
    if (customProfiles.size() > maximumCustomProfiles) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "Too many custom User-Agent profiles"
            ))
        );
    }
    for (const QJsonValue &value : customProfiles) {
        if (!value.isObject()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "Every custom User-Agent profile must be an object"
                ))
            );
        }
        const QJsonObject object = value.toObject();
        if (!object.value(QStringLiteral("id")).isString()
            || !object.value(QStringLiteral("name")).isString()
            || !object.value(QStringLiteral("userAgent")).isString()
            || !object.value(QStringLiteral("platform")).isString()
            || !object.value(QStringLiteral("mobile")).isBool()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "Custom User-Agent profile has an invalid structure"
                ))
            );
        }

        UserAgentProfile profile;
        profile.id = object.value(QStringLiteral("id")).toString().trimmed();
        profile.name = object.value(QStringLiteral("name")).toString().trimmed();
        profile.userAgent = object.value(QStringLiteral("userAgent")).toString().trimmed();
        if (!parsePlatform(
                object.value(QStringLiteral("platform")).toString(),
                &profile.platform
            )) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "Custom User-Agent profile has an unknown platform"
                ))
            );
        }
        profile.mobile = object.value(QStringLiteral("mobile")).toBool();
        loaded.m_profiles.append(std::move(profile));
    }

    if (!loaded.validate(error))
        return false;
    *this = std::move(loaded);
    return true;
}

bool UserAgentSettings::save(const QString &path, QString *error) const
{
    if (error)
        error->clear();
    if (!validate(error))
        return false;

    QJsonArray customProfiles;
    for (const UserAgentProfile &profile : m_profiles) {
        if (profile.builtIn)
            continue;
        QJsonObject object;
        object.insert(QStringLiteral("id"), profile.id.trimmed());
        object.insert(QStringLiteral("name"), profile.name.trimmed());
        object.insert(QStringLiteral("userAgent"), profile.userAgent.trimmed());
        object.insert(QStringLiteral("platform"), userAgentPlatformKey(profile.platform));
        object.insert(QStringLiteral("mobile"), profile.mobile);
        customProfiles.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("selectedProfile"), m_selectedProfileId);
    root.insert(QStringLiteral("customProfiles"), customProfiles);
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (contents.size() > maximumSettingsFileSize) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "User-Agent settings file is too large"
            ))
        );
    }

    if (QFile::exists(path)) {
        const QString backupPath = path + QStringLiteral(".backup");
        if (QFile::exists(backupPath) && !QFile::remove(backupPath)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "Cannot replace backup %1"
                )).arg(backupPath)
            );
        }
        if (!QFile::copy(path, backupPath)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "Cannot create backup %1"
                )).arg(backupPath)
            );
        }
        if (!PrivateData::restrictFile(backupPath, error))
            return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("UserAgentSettings", "Cannot write %1: %2"))
                .arg(path, file.errorString())
        );
    }
    if (file.write(contents) != contents.size() || !file.commit()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP("UserAgentSettings", "Cannot commit %1: %2"))
                .arg(path, file.errorString())
        );
    }
    return PrivateData::restrictFile(path, error);
}

bool UserAgentSettings::validate(QString *error) const
{
    if (error)
        error->clear();
    if (m_profiles.isEmpty()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "At least the Chromium default profile is required"
            ))
        );
    }

    static const QRegularExpression idPattern(
        QStringLiteral("^[a-z0-9][a-z0-9._-]{0,127}$")
    );
    QSet<QString> ids;
    QSet<QString> customNames;
    qsizetype customCount = 0;
    for (const UserAgentProfile &profile : m_profiles) {
        const QString id = profile.id.trimmed();
        const QString name = profile.name.trimmed();
        if (!idPattern.match(id).hasMatch()) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "User-Agent profile has an invalid id"
                ))
            );
        }
        if (ids.contains(id)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "Duplicate User-Agent profile id: %1"
                )).arg(id)
            );
        }
        ids.insert(id);
        if (!profile.builtIn) {
            ++customCount;
            if (id.startsWith(QStringLiteral("builtin-"))) {
                return fail(
                    error,
                    text(QT_TRANSLATE_NOOP(
                        "UserAgentSettings",
                        "Custom User-Agent profile id must not use the built-in prefix"
                    ))
                );
            }
            const QString foldedName = name.toCaseFolded();
            if (customNames.contains(foldedName)) {
                return fail(
                    error,
                    text(QT_TRANSLATE_NOOP(
                        "UserAgentSettings",
                        "Duplicate User-Agent profile name: %1"
                    )).arg(name)
                );
            }
            customNames.insert(foldedName);
        }
        if (name.isEmpty() || name.size() > maximumProfileNameLength
            || !isPrintableName(name)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "User-Agent profile name must contain between 1 and 80 printable characters"
                ))
            );
        }
        const bool isDefault = id == defaultUserAgentProfileId();
        if (isDefault) {
            if (!profile.builtIn || !profile.userAgent.isEmpty()
                || profile.platform != UserAgentPlatform::System || profile.mobile) {
                return fail(
                    error,
                    text(QT_TRANSLATE_NOOP(
                        "UserAgentSettings",
                        "Chromium default profile is invalid"
                    ))
                );
            }
        } else if (!isSafeUserAgent(profile.userAgent)) {
            return fail(
                error,
                text(QT_TRANSLATE_NOOP(
                    "UserAgentSettings",
                    "User-Agent must contain between 1 and 1024 printable ASCII characters"
                ))
            );
        }
    }
    if (customCount > maximumCustomProfiles) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "Too many custom User-Agent profiles"
            ))
        );
    }
    if (!ids.contains(defaultUserAgentProfileId())) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "Chromium default profile is missing"
            ))
        );
    }
    if (!ids.contains(m_selectedProfileId)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "Selected User-Agent profile does not exist"
            ))
        );
    }
    return true;
}

QString UserAgentSettings::selectedProfileId() const
{
    return m_selectedProfileId;
}

void UserAgentSettings::setSelectedProfileId(const QString &id)
{
    m_selectedProfileId = id.trimmed();
}

const QList<UserAgentProfile> &UserAgentSettings::profiles() const
{
    return m_profiles;
}

QList<UserAgentProfile> &UserAgentSettings::profiles()
{
    return m_profiles;
}

const UserAgentProfile *UserAgentSettings::profileById(const QString &id) const
{
    for (const UserAgentProfile &profile : m_profiles) {
        if (profile.id == id)
            return &profile;
    }
    return nullptr;
}

UserAgentProfile *UserAgentSettings::profileById(const QString &id)
{
    for (UserAgentProfile &profile : m_profiles) {
        if (profile.id == id)
            return &profile;
    }
    return nullptr;
}

const UserAgentProfile *UserAgentSettings::selectedProfile() const
{
    return profileById(m_selectedProfileId);
}

QString defaultUserAgentProfileId()
{
    return QStringLiteral("builtin-default");
}

QString userAgentPlatformKey(UserAgentPlatform platform)
{
    switch (platform) {
    case UserAgentPlatform::System:
        return QStringLiteral("system");
    case UserAgentPlatform::Windows:
        return QStringLiteral("windows");
    case UserAgentPlatform::MacOS:
        return QStringLiteral("macos");
    case UserAgentPlatform::Linux:
        return QStringLiteral("linux");
    case UserAgentPlatform::Android:
        return QStringLiteral("android");
    case UserAgentPlatform::IOS:
        return QStringLiteral("ios");
    }
    return {};
}

QString userAgentPlatformDisplayName(UserAgentPlatform platform)
{
    switch (platform) {
    case UserAgentPlatform::System:
        return text(QT_TRANSLATE_NOOP("UserAgentSettings", "System"));
    case UserAgentPlatform::Windows:
        return QStringLiteral("Windows");
    case UserAgentPlatform::MacOS:
        return QStringLiteral("macOS");
    case UserAgentPlatform::Linux:
        return QStringLiteral("Linux");
    case UserAgentPlatform::Android:
        return QStringLiteral("Android");
    case UserAgentPlatform::IOS:
        return QStringLiteral("iOS");
    }
    return {};
}

bool hasSameEffectiveUserAgentConfiguration(
    const UserAgentSettings &left,
    const UserAgentSettings &right
)
{
    const UserAgentProfile *leftProfile = left.selectedProfile();
    const UserAgentProfile *rightProfile = right.selectedProfile();
    if (!leftProfile || !rightProfile)
        return false;
    const bool leftDefault = leftProfile->id == defaultUserAgentProfileId();
    const bool rightDefault = rightProfile->id == defaultUserAgentProfileId();
    if (leftDefault || rightDefault)
        return leftDefault && rightDefault;
    return leftProfile->userAgent == rightProfile->userAgent
        && leftProfile->platform == rightProfile->platform
        && leftProfile->mobile == rightProfile->mobile;
}

bool applyUserAgentSettings(
    QWebEngineProfile *profile,
    const UserAgentSettings &settings,
    QString *error
)
{
    if (error)
        error->clear();
    if (!profile) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "WebEngine profile is unavailable"
            ))
        );
    }
    if (!settings.validate(error))
        return false;
    const UserAgentProfile *selected = settings.selectedProfile();
    if (!selected) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "UserAgentSettings",
                "Selected User-Agent profile does not exist"
            ))
        );
    }
    if (selected->id == defaultUserAgentProfileId())
        return true;

    profile->setHttpUserAgent(selected->userAgent);
    if (QWebEngineClientHints *hints = profile->clientHints()) {
        hints->resetAll();
        hints->setAllClientHintsEnabled(false);
        const QString platform = clientHintsPlatform(selected->platform);
        if (!platform.isEmpty())
            hints->setPlatform(platform);
        hints->setIsMobile(selected->mobile);
    }
    return true;
}
