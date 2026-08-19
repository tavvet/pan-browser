#pragma once

#include "VideoTranslationSettings.h"
#include "VotUserscriptPackage.h"
#include "VotUserscriptStore.h"

#include <QObject>
#include <QList>
#include <QPointer>

#include <optional>

class BrowserPage;
class BrowserProfile;
class QAuthenticator;
class QUrl;
class VotChromiumNetworkTransport;

enum class VotUserscriptState {
    Disabled,
    NotConfigured,
    Preparing,
    Ready,
    Error,
};

class VotUserscriptManager final : public QObject {
    Q_OBJECT

public:
    explicit VotUserscriptManager(
        const QString &storagePath,
        BrowserProfile *profile,
        QObject *parent = nullptr
    );

    void applySettings(const VideoTranslationSettings &settings);
    void configurePage(BrowserPage *page);
    void forgetPage(BrowserPage *page);

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
        const QString &proxyHost,
        bool *handled
    );

private:
    void handleTransportStateChanged();
    void reconfigurePages();
    void setState(VotUserscriptState state, const QString &detail = QString());

    VideoTranslationSettings m_settings;
    VotUserscriptStore m_store;
    QString m_storageError;
    VotChromiumNetworkTransport *m_transport = nullptr;
    QList<QPointer<BrowserPage>> m_pages;
    std::optional<VotUserscript> m_userscript;
    VotUserscriptState m_state = VotUserscriptState::Disabled;
    QString m_stateDetail;
};
