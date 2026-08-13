#include "VotUserscriptManager.h"

#include "BrowserPage.h"
#include "VotUserscriptBridge.h"

#include <QWebEngineScriptCollection>

#include <utility>

VotUserscriptManager::VotUserscriptManager(
    const QString &storagePath,
    DnsResolutionMode dnsResolutionMode,
    QObject *parent
)
    : QObject(parent)
    , m_store(storagePath)
    , m_dnsResolutionMode(dnsResolutionMode)
{
    m_store.load(&m_storageError);
}

bool VotUserscriptManager::supportsDnsResolutionMode(DnsResolutionMode mode)
{
    return mode == DnsResolutionMode::System;
}

void VotUserscriptManager::setDnsResolutionMode(DnsResolutionMode mode)
{
    m_dnsResolutionMode = mode;
}

void VotUserscriptManager::applySettings(
    const VideoTranslationSettings &settings
)
{
    m_settings = settings;
    m_userscript.reset();
    emit settingsChanged(m_settings);

    if (!settings.enabled()) {
        setState(VotUserscriptState::Disabled);
        return;
    }
    if (!m_storageError.isEmpty()) {
        setState(VotUserscriptState::Error, m_storageError);
        return;
    }
    if (!supportsDnsResolutionMode(m_dnsResolutionMode)) {
        setState(
            VotUserscriptState::Error,
            tr("Video translation is unavailable while Secure DNS is enabled")
        );
        return;
    }
    QString validationError;
    if (!settings.validate(&validationError)) {
        setState(VotUserscriptState::Error, validationError);
        return;
    }
    VotUserscript userscript;
    if (!VotUserscriptPackage::load(
            settings.sourcePath(),
            &userscript,
            &validationError
        )) {
        setState(VotUserscriptState::Error, validationError);
        return;
    }
    m_userscript = std::move(userscript);
    setState(VotUserscriptState::Ready);
}

void VotUserscriptManager::configurePage(BrowserPage *page)
{
    if (!page)
        return;
    const QList<QWebEngineScript> oldScripts = page->scripts().find(
        VotUserscriptBridge::scriptName()
    );
    for (const QWebEngineScript &script : oldScripts)
        page->scripts().remove(script);
    const QList<VotUserscriptBridge *> oldBridges = page->findChildren<VotUserscriptBridge *>(
        QString(),
        Qt::FindDirectChildrenOnly
    );
    for (VotUserscriptBridge *bridge : oldBridges)
        delete bridge;

    if (m_state == VotUserscriptState::Ready && m_userscript) {
        auto *bridge = new VotUserscriptBridge(page, *m_userscript, &m_store, page);
        connect(
            bridge,
            &VotUserscriptBridge::proxyAuthenticationRequired,
            this,
            &VotUserscriptManager::proxyAuthenticationRequired
        );
    }
}

VideoTranslationSettings VotUserscriptManager::settings() const
{
    return m_settings;
}

VotUserscriptState VotUserscriptManager::state() const
{
    return m_state;
}

QString VotUserscriptManager::statusText() const
{
    if (!m_stateDetail.isEmpty())
        return m_stateDetail;
    switch (m_state) {
    case VotUserscriptState::Disabled:
        return tr("Disabled");
    case VotUserscriptState::NotConfigured:
        return tr("Choose the verified VOT userscript");
    case VotUserscriptState::Ready:
        return tr("Ready — reload open video pages to apply");
    case VotUserscriptState::Error:
        return tr("Userscript error");
    }
    return {};
}

void VotUserscriptManager::setState(
    VotUserscriptState state,
    const QString &detail
)
{
    if (m_state == state && m_stateDetail == detail)
        return;
    m_state = state;
    m_stateDetail = detail;
    emit stateChanged();
}
