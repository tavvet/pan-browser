#pragma once

#include "WebAppStore.h"

#include <QString>

class WebAppShortcutManager final {
public:
    explicit WebAppShortcutManager(
        QString shortcutRoot = QString(),
        QString hostBundlePath = QString()
    );

    [[nodiscard]] bool isSupported() const;
    [[nodiscard]] bool shortcutExists(const WebApp &app) const;
    [[nodiscard]] QString shortcutPath(const WebApp &app) const;
    [[nodiscard]] QString shortcutRoot() const;
    bool createOrUpdate(const WebApp &app, QString *error = nullptr) const;
    bool remove(const WebApp &app, QString *error = nullptr) const;

    [[nodiscard]] static QString safeShortcutName(const QString &name);

private:
    [[nodiscard]] QStringList matchingShortcutPaths(const QString &appId) const;
    [[nodiscard]] bool pathBelongsToApp(const QString &path, const QString &appId) const;

    QString m_shortcutRoot;
    QString m_hostBundlePath;
};
