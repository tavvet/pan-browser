#pragma once

#include "DnsSettings.h"
#include "VideoTranslationSettings.h"
#include "VotUserscriptPackage.h"
#include "VotUserscriptStore.h"

#include <QObject>

#include <optional>

class BrowserPage;
class QAuthenticator;
class QUrl;

enum class VotUserscriptState {
    Disabled,
    NotConfigured,
    Ready,
    Error,
};

class VotUserscriptManager final : public QObject {
    Q_OBJECT

public:
    explicit VotUserscriptManager(
        const QString &storagePath,
        DnsResolutionMode dnsResolutionMode,
        QObject *parent = nullptr
    );

    static bool supportsDnsResolutionMode(DnsResolutionMode mode);
    void setDnsResolutionMode(DnsResolutionMode mode);
    void applySettings(const VideoTranslationSettings &settings);
    void configurePage(BrowserPage *page);

    [[nodiscard]] VideoTranslationSettings settings() const;
    [[nodiscard]] VotUserscriptState state() const;
    [[nodiscard]] QString statusText() const;

signals:
    void stateChanged();
    void settingsChanged(const VideoTranslationSettings &settings);
    void proxyAuthenticationRequired(
        BrowserPage *page,
        const QUrl &requestUrl,
        QAuthenticator *authenticator,
        const QString &proxyHost
    );

private:
    void setState(VotUserscriptState state, const QString &detail = QString());

    VideoTranslationSettings m_settings;
    VotUserscriptStore m_store;
    QString m_storageError;
    DnsResolutionMode m_dnsResolutionMode = DnsResolutionMode::System;
    std::optional<VotUserscript> m_userscript;
    VotUserscriptState m_state = VotUserscriptState::Disabled;
    QString m_stateDetail;
};
