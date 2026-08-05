#pragma once

#include <QUrl>

class QSettings;

enum class StartupMode {
    StartPage,
    RestoreTabs,
};

class BrowserPreferences {
public:
    static BrowserPreferences load(const QUrl &legacyStartPage = QUrl());

    bool save(QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;

    [[nodiscard]] QUrl startPage() const;
    void setStartPage(const QUrl &startPage);

    [[nodiscard]] StartupMode startupMode() const;
    void setStartupMode(StartupMode mode);

    [[nodiscard]] bool persistSessionCookies() const;
    void setPersistSessionCookies(bool persist);

    [[nodiscard]] bool saveBrowsingHistory() const;
    void setSaveBrowsingHistory(bool save);

private:
    QUrl m_startPage = QUrl(QStringLiteral("https://example.com"));
    StartupMode m_startupMode = StartupMode::StartPage;
    bool m_persistSessionCookies = false;
    bool m_saveBrowsingHistory = true;
};
