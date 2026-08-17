#include "VotUserscriptManager.h"

#include "BrowserPage.h"
#include "BrowserProfile.h"
#include "VotChromiumNetworkTransport.h"
#include "VotUserscriptBridge.h"

#include <QWebEngineScriptCollection>

#include <utility>

VotUserscriptManager::VotUserscriptManager(
    const QString &storagePath,
    BrowserProfile *profile,
    QObject *parent
)
    : QObject(parent)
    , m_store(storagePath)
    , m_transport(new VotChromiumNetworkTransport(profile, this))
{
    Q_ASSERT(profile);
    m_store.load(&m_storageError);
    connect(
        m_transport,
        &VotChromiumNetworkTransport::stateChanged,
        this,
        &VotUserscriptManager::handleTransportStateChanged
    );
    connect(
        m_transport,
        &VotChromiumNetworkTransport::proxyAuthenticationRequired,
        this,
        &VotUserscriptManager::proxyAuthenticationRequired
    );
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
    m_transport->ensureReady();
    handleTransportStateChanged();
}

void VotUserscriptManager::configurePage(BrowserPage *page)
{
    if (!page)
        return;
    m_pages.removeIf([](const QPointer<BrowserPage> &candidate) {
        return candidate.isNull();
    });
    if (!m_pages.contains(page))
        m_pages.append(page);
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

    if ((m_state == VotUserscriptState::Preparing
            || m_state == VotUserscriptState::Ready)
        && m_userscript) {
        new VotUserscriptBridge(
            page,
            *m_userscript,
            &m_store,
            m_transport,
            page
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
    case VotUserscriptState::Preparing:
        return tr("Preparing Chromium network transport…");
    case VotUserscriptState::Ready:
        return tr("Ready — reload open video pages to apply");
    case VotUserscriptState::Error:
        return tr("Userscript error");
    }
    return {};
}

void VotUserscriptManager::handleTransportStateChanged()
{
    if (!m_settings.enabled() || !m_userscript)
        return;
    switch (m_transport->state()) {
    case VotChromiumTransportState::Uninitialized:
    case VotChromiumTransportState::Loading:
        setState(VotUserscriptState::Preparing);
        reconfigurePages();
        break;
    case VotChromiumTransportState::Ready:
        setState(VotUserscriptState::Ready);
        reconfigurePages();
        break;
    case VotChromiumTransportState::Error:
        setState(VotUserscriptState::Error, m_transport->errorString());
        reconfigurePages();
        break;
    }
}

void VotUserscriptManager::reconfigurePages()
{
    const QList<QPointer<BrowserPage>> pages = m_pages;
    for (const QPointer<BrowserPage> &page : pages) {
        if (page)
            configurePage(page);
    }
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
