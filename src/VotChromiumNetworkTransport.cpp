#include "VotChromiumNetworkTransport.h"

#include "BrowserPage.h"
#include "CrossDomainRequestInterceptor.h"
#include "PrivateData.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QWebEngineExtensionInfo>
#include <QWebEngineExtensionManager>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineProfileBuilder>
#include <QWebEngineScript>

#include <functional>
#include <utility>

namespace {

constexpr qsizetype maximumTransportMessageLength = 48 * 1024 * 1024;
constexpr int initializationTimeoutMilliseconds = 15'000;

QString javaScriptValue(const QJsonValue &value)
{
    QJsonArray wrapper;
    wrapper.append(value);
    const QByteArray json = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

QString normalizedPath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool writeResourceFile(
    const QString &resourcePath,
    const QString &destinationPath,
    QString *error
)
{
    QFile source(resourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QCoreApplication::translate(
                "VotChromiumNetworkTransport",
                "Cannot read the built-in VOT network transport"
            );
        }
        return false;
    }
    const QByteArray contents = source.readAll();

    QFile existing(destinationPath);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == contents)
        return PrivateData::restrictFile(destinationPath, error);

    QSaveFile destination(destinationPath);
    if (!destination.open(QIODevice::WriteOnly)
        || destination.write(contents) != contents.size()
        || !destination.commit()) {
        if (error) {
            *error = QCoreApplication::translate(
                "VotChromiumNetworkTransport",
                "Cannot prepare the built-in VOT network transport"
            );
        }
        return false;
    }
    return PrivateData::restrictFile(destinationPath, error);
}

class TransportPage final : public QWebEnginePage {
public:
    explicit TransportPage(QWebEngineProfile *profile, QObject *parent)
        : QWebEnginePage(profile, parent)
    {
    }

    std::function<void(const QString &)> consoleHandler;

protected:
    void javaScriptConsoleMessage(
        JavaScriptConsoleMessageLevel level,
        const QString &message,
        int lineNumber,
        const QString &sourceId
    ) override
    {
        if (consoleHandler
            && message.startsWith(
                VotChromiumNetworkTransport::responseMessagePrefix()
            )) {
            consoleHandler(message);
            return;
        }
        QWebEnginePage::javaScriptConsoleMessage(
            level,
            message,
            lineNumber,
            sourceId
        );
    }
};

} // namespace

VotChromiumNetworkTransport::VotChromiumNetworkTransport(
    QWebEngineProfile *profile,
    QObject *parent
)
    : VotChromiumNetworkTransport(profile, QString(), parent)
{
}

VotChromiumNetworkTransport::VotChromiumNetworkTransport(
    QWebEngineProfile *profile,
    const QString &cacheRoot,
    QObject *parent
)
    : VotChromiumNetworkTransport(profile, cacheRoot, {}, parent)
{
}

VotChromiumNetworkTransport::VotChromiumNetworkTransport(
    QWebEngineProfile *profile,
    const QString &cacheRoot,
    const QList<QSslCertificate> &additionalTrustedCertificates,
    QObject *parent
)
    : QObject(parent ? parent : profile)
    , m_profile(profile)
    , m_cacheRoot(cacheRoot)
    , m_additionalTrustedCertificates(additionalTrustedCertificates)
    , m_token(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    Q_ASSERT(profile);
    m_initializationTimer = new QTimer(this);
    m_initializationTimer->setSingleShot(true);
    connect(m_initializationTimer, &QTimer::timeout, this, [this] {
        if (m_state == VotChromiumTransportState::Loading) {
            fail(tr("Timed out while preparing the Chromium VOT network transport"));
        }
    });
}

VotChromiumNetworkTransport::~VotChromiumNetworkTransport()
{
    for (const ActiveRequest &request : std::as_const(m_activeRequests)) {
        if (auto *page = static_cast<TransportPage *>(request.transportPage.data()))
            page->consoleHandler = {};
        delete request.timeoutTimer;
        delete request.transportPage;
    }
    m_activeRequests.clear();
    resetTransportProfile();
}

QString VotChromiumNetworkTransport::responseMessagePrefix()
{
    return QStringLiteral("__PANBROWSER_VOT_CHROMIUM_RESPONSE__");
}

bool VotChromiumNetworkTransport::prepareExtensionDirectory(
    const QString &directoryPath,
    QString *error
)
{
    if (error)
        error->clear();
    if (!PrivateData::ensureDirectory(directoryPath, error))
        return false;

    const QList<QPair<QString, QString>> files{
        {
            QStringLiteral(":/assets/vot-network-extension/manifest.json"),
            QStringLiteral("manifest.json"),
        },
        {
            QStringLiteral(":/assets/vot-network-extension/transport.html"),
            QStringLiteral("transport.html"),
        },
        {
            QStringLiteral(":/assets/vot-network-extension/transport.js"),
            QStringLiteral("transport.js"),
        },
    };
    const QDir directory(directoryPath);
    for (const auto &[resourcePath, fileName] : files) {
        if (!writeResourceFile(
                resourcePath,
                directory.filePath(fileName),
                error
            )) {
            return false;
        }
    }
    return true;
}

void VotChromiumNetworkTransport::ensureReady()
{
    if (m_state == VotChromiumTransportState::Ready
        || m_state == VotChromiumTransportState::Loading) {
        return;
    }
    if (m_state == VotChromiumTransportState::Error) {
        resetTransportProfile();
        m_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_state = VotChromiumTransportState::Uninitialized;
        m_error.clear();
    }
    if (!m_profile) {
        fail(tr("Chromium profile is unavailable"));
        return;
    }

    const QString cacheRoot = m_cacheRoot.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        : m_cacheRoot;
    m_extensionDirectory = QDir(cacheRoot).filePath(
        QStringLiteral("InternalExtensions/vot-network-v1")
    );
    QString preparationError;
    if (!prepareExtensionDirectory(m_extensionDirectory, &preparationError)) {
        fail(preparationError);
        return;
    }
    if (!createTransportProfile(&preparationError)) {
        fail(preparationError);
        return;
    }

    setState(VotChromiumTransportState::Loading);
    m_initializationTimer->start(initializationTimeoutMilliseconds);

    m_profileBootstrapPage = new QWebEnginePage(m_transportProfile, this);
    connect(
        m_profileBootstrapPage,
        &QWebEnginePage::loadFinished,
        this,
        [this](bool ok) {
            if (!ok) {
                fail(tr("Cannot initialize the Chromium profile for VOT"));
                return;
            }
            beginExtensionLoad();
        }
    );
    m_profileBootstrapPage->setUrl(QUrl(QStringLiteral("about:blank")));
}

bool VotChromiumNetworkTransport::createTransportProfile(QString *error)
{
    if (error)
        error->clear();
    if (m_transportProfile)
        return true;

    const QString cacheRoot = m_cacheRoot.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        : m_cacheRoot;
    const QString profileRoot = QDir(cacheRoot).filePath(
        QStringLiteral("InternalProfiles/VotNetwork")
    );
    const QString dataPath = QDir(profileRoot).filePath(QStringLiteral("Profile"));
    const QString cachePath = QDir(profileRoot).filePath(QStringLiteral("Cache"));
    if (!PrivateData::ensureDirectory(dataPath, error)
        || !PrivateData::ensureDirectory(cachePath, error)) {
        return false;
    }

    QWebEngineProfileBuilder builder;
    builder.setPersistentStoragePath(dataPath)
        .setCachePath(cachePath)
        .setHttpCacheType(QWebEngineProfile::NoCache)
        .setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies)
        .setPersistentPermissionsPolicy(
            QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime
        );
    if (!m_additionalTrustedCertificates.isEmpty()) {
        builder.setAdditionalTrustedCertificates(
            m_additionalTrustedCertificates
        );
    }
    m_transportProfile = builder.createProfile(
        QStringLiteral("PanBrowserVotNetwork"),
        this
    );
    if (!m_transportProfile) {
        if (error) {
            *error = tr("Cannot create the isolated Chromium profile for VOT");
        }
        return false;
    }

    m_transportProfile->setHttpUserAgent(m_profile->httpUserAgent());
    m_transportProfile->setHttpAcceptLanguage(m_profile->httpAcceptLanguage());
    m_requestInterceptor = new CrossDomainRequestInterceptor(
        false,
        CrossDomainSettings::defaults(),
        m_transportProfile
    );
    m_transportProfile->setUrlRequestInterceptor(m_requestInterceptor);
    return true;
}

void VotChromiumNetworkTransport::beginExtensionLoad()
{
    if (!m_profile || m_state != VotChromiumTransportState::Loading)
        return;
    QWebEngineExtensionManager *manager = m_transportProfile
        ? m_transportProfile->extensionManager()
        : nullptr;
    if (!manager) {
        fail(tr("Chromium extension manager is unavailable"));
        return;
    }

    const QString expectedPath = normalizedPath(m_extensionDirectory);
    for (const QWebEngineExtensionInfo &extension : manager->extensions()) {
        if (normalizedPath(extension.path()) != expectedPath)
            continue;
        if (!extension.isLoaded()) {
            fail(extension.error().isEmpty()
                ? tr("Cannot load the built-in VOT network transport")
                : extension.error());
            return;
        }
        enableExtension(manager, extension);
        return;
    }

    QPointer<QWebEngineExtensionManager> guardedManager(manager);
    connect(
        manager,
        &QWebEngineExtensionManager::loadFinished,
        this,
        [this, guardedManager, expectedPath](
            const QWebEngineExtensionInfo &extension
        ) {
            if (!guardedManager
                || m_state != VotChromiumTransportState::Loading
                || normalizedPath(extension.path()) != expectedPath) {
                return;
            }
            if (!extension.isLoaded()) {
                fail(extension.error().isEmpty()
                    ? tr("Cannot load the built-in VOT network transport")
                    : extension.error());
                return;
            }
            enableExtension(guardedManager, extension);
        }
    );
    manager->loadExtension(m_extensionDirectory);
}

void VotChromiumNetworkTransport::enableExtension(
    QWebEngineExtensionManager *manager,
    const QWebEngineExtensionInfo &extension
)
{
    if (m_state != VotChromiumTransportState::Loading)
        return;
    if (extension.isEnabled()) {
        activateExtension(extension.id());
        return;
    }

    QPointer<VotChromiumNetworkTransport> self(this);
    QPointer<QWebEngineExtensionManager> guardedManager(manager);
    QTimer::singleShot(0, this, [self, guardedManager, extension] {
        if (!self || !guardedManager
            || self->m_state != VotChromiumTransportState::Loading) {
            return;
        }
        guardedManager->setExtensionEnabled(extension, true);
        self->activateExtension(extension.id());
    });
}

void VotChromiumNetworkTransport::sendRequest(
    const VotChromiumRequest &request
)
{
    if (request.id.isEmpty() || m_activeRequests.contains(request.id))
        return;
    auto *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [this, id = request.id] {
        const auto found = m_activeRequests.constFind(id);
        if (found == m_activeRequests.cend())
            return;
        if (found->started && found->transportPage) {
            found->transportPage->runJavaScript(
                QStringLiteral("globalThis.panBrowserVotTransport?.abort(%1)")
                    .arg(javaScriptValue(id)),
                QWebEngineScript::MainWorld
            );
        }
        emitTerminalResponse({
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("timeout")},
            {QStringLiteral("error"), tr("Request timed out")},
        });
    });
    ActiveRequest active;
    active.request = request;
    active.timeoutTimer = timeoutTimer;
    m_activeRequests.insert(request.id, active);
    timeoutTimer->start(qMax(1, request.timeoutMilliseconds));
    if (m_state == VotChromiumTransportState::Uninitialized)
        ensureReady();
    if (m_state == VotChromiumTransportState::Error) {
        emitTerminalResponse({
            {QStringLiteral("id"), request.id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), m_error},
        });
        return;
    }
    if (m_state != VotChromiumTransportState::Ready) {
        m_pendingRequests.insert(request.id, request);
        return;
    }
    startRequest(request);
}

void VotChromiumNetworkTransport::abortRequest(const QString &id)
{
    const auto found = m_activeRequests.constFind(id);
    if (found == m_activeRequests.cend())
        return;
    if (found->started && found->transportPage) {
        found->transportPage->runJavaScript(
            QStringLiteral("globalThis.panBrowserVotTransport?.abort(%1)")
                .arg(javaScriptValue(id)),
            QWebEngineScript::MainWorld
        );
    }
    emitTerminalResponse({
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), QStringLiteral("abort")},
    });
}

bool VotChromiumNetworkTransport::registerRequestAuthorizer(
    const QString &requestId,
    std::function<bool(const QUrl &, int)> authorizer
)
{
    return m_requestInterceptor
        && m_requestInterceptor->registerVotTransportRequest(
            requestId,
            std::move(authorizer)
        );
}

void VotChromiumNetworkTransport::unregisterRequestAuthorizer(
    const QString &requestId
)
{
    if (m_requestInterceptor)
        m_requestInterceptor->unregisterVotTransportRequest(requestId);
}

VotChromiumTransportState VotChromiumNetworkTransport::state() const
{
    return m_state;
}

QString VotChromiumNetworkTransport::errorString() const
{
    return m_error;
}

void VotChromiumNetworkTransport::activateExtension(
    const QString &extensionId
)
{
    if (m_state != VotChromiumTransportState::Loading)
        return;
    if (extensionId.isEmpty()) {
        fail(tr("The built-in VOT network transport has no extension ID"));
        return;
    }
    if (!m_transportProfile || !m_requestInterceptor) {
        fail(tr("Chromium profile is unavailable"));
        return;
    }
    m_extensionId = extensionId;
    m_requestInterceptor->setVotTransportExtensionId(extensionId);
    createControlPage(extensionId);
    if (m_profileBootstrapPage) {
        delete m_profileBootstrapPage;
        m_profileBootstrapPage = nullptr;
    }
}

void VotChromiumNetworkTransport::createControlPage(
    const QString &extensionId
)
{
    if (!m_transportProfile)
        return;
    if (m_controlPage)
        m_controlPage->deleteLater();

    auto *page = new TransportPage(m_transportProfile, this);
    QPointer<VotChromiumNetworkTransport> self(this);
    m_controlPage = page;
    connect(
        page,
        &QWebEnginePage::authenticationRequired,
        this,
        [](const QUrl &, QAuthenticator *authenticator) {
            if (authenticator)
                *authenticator = QAuthenticator();
        }
    );
    connect(
        page,
        &QWebEnginePage::proxyAuthenticationRequired,
        this,
        [](const QUrl &, QAuthenticator *authenticator, const QString &) {
            if (authenticator)
                *authenticator = QAuthenticator();
        }
    );
    connect(
        page,
        &QWebEnginePage::renderProcessTerminated,
        this,
        [self, guardedPage = QPointer<QWebEnginePage>(page)](
            QWebEnginePage::RenderProcessTerminationStatus,
            int
        ) {
            if (!self || !guardedPage || guardedPage != self->m_controlPage)
                return;
            self->fail(QCoreApplication::translate(
                "VotChromiumNetworkTransport",
                "The Chromium VOT network process stopped unexpectedly"
            ));
        }
    );
    connect(
        page,
        &QWebEnginePage::loadFinished,
        this,
        [self, guardedPage = QPointer<QWebEnginePage>(page)](bool ok) {
        if (!self || !guardedPage || guardedPage != self->m_controlPage
            || self->m_state != VotChromiumTransportState::Loading) {
            return;
        }
        if (!ok) {
            self->fail(QCoreApplication::translate(
                "VotChromiumNetworkTransport",
                "Cannot open the built-in VOT network transport"
            ));
            return;
        }
        guardedPage->runJavaScript(
            QStringLiteral(
                "typeof globalThis.panBrowserVotTransport?.request === 'function'"
            ),
            QWebEngineScript::MainWorld,
            [self, guardedPage](
                const QVariant &available
            ) {
                if (!self || !guardedPage
                    || guardedPage != self->m_controlPage
                    || self->m_state != VotChromiumTransportState::Loading) {
                    return;
                }
                if (!available.toBool()) {
                    self->fail(QCoreApplication::translate(
                        "VotChromiumNetworkTransport",
                        "The built-in VOT network transport did not initialize"
                    ));
                    return;
                }
                self->setState(VotChromiumTransportState::Ready);
                self->flushPendingRequests();
            }
        );
    });
    page->setUrl(QUrl(QStringLiteral("chrome-extension://%1/transport.html").arg(
        extensionId
    )));
}

void VotChromiumNetworkTransport::flushPendingRequests()
{
    const QList<VotChromiumRequest> pending = m_pendingRequests.values();
    m_pendingRequests.clear();
    for (const VotChromiumRequest &request : pending) {
        if (m_activeRequests.contains(request.id))
            startRequest(request);
    }
}

void VotChromiumNetworkTransport::startRequest(
    const VotChromiumRequest &request
)
{
    if (!m_transportProfile || !m_activeRequests.contains(request.id))
        return;
    auto *page = new TransportPage(m_transportProfile, this);
    QPointer<VotChromiumNetworkTransport> self(this);
    page->consoleHandler = [self](const QString &message) {
        if (self)
            self->handleConsoleMessage(message);
    };
    auto active = m_activeRequests.find(request.id);
    if (active == m_activeRequests.end()) {
        page->deleteLater();
        return;
    }
    active->transportPage = page;
    connect(
        page,
        &QWebEnginePage::authenticationRequired,
        this,
        [](const QUrl &, QAuthenticator *authenticator) {
            if (authenticator)
                *authenticator = QAuthenticator();
        }
    );
    connect(
        page,
        &QWebEnginePage::proxyAuthenticationRequired,
        this,
        [this, id = request.id](
            const QUrl &requestUrl,
            QAuthenticator *authenticator,
            const QString &proxyHost
        ) {
            const auto found = m_activeRequests.constFind(id);
            if (found == m_activeRequests.cend()) {
                if (authenticator)
                    *authenticator = QAuthenticator();
                return;
            }
            bool handled = false;
            emit proxyAuthenticationRequired(
                found->request.sourcePage.data(),
                requestUrl,
                authenticator,
                proxyHost,
                &handled
            );
            if (!handled && authenticator)
                *authenticator = QAuthenticator();
        }
    );
    connect(
        page,
        &QWebEnginePage::renderProcessTerminated,
        this,
        [self, id = request.id, guardedPage = QPointer<QWebEnginePage>(page)](
            QWebEnginePage::RenderProcessTerminationStatus,
            int
        ) {
            if (!self || !guardedPage)
                return;
            self->handleRequestPageFailure(
                id,
                guardedPage,
                QCoreApplication::translate(
                    "VotChromiumNetworkTransport",
                    "The Chromium VOT request process stopped unexpectedly"
                )
            );
        }
    );
    connect(
        page,
        &QWebEnginePage::loadFinished,
        this,
        [self, id = request.id, guardedPage = QPointer<QWebEnginePage>(page)](
            bool ok
        ) {
            if (!self || !guardedPage)
                return;
            if (!ok) {
                self->handleRequestPageFailure(
                    id,
                    guardedPage,
                    QCoreApplication::translate(
                        "VotChromiumNetworkTransport",
                        "Cannot open the Chromium page for a VOT request"
                    )
                );
                return;
            }
            self->startLoadedRequest(id, guardedPage);
        }
    );
    QUrl transportUrl(QStringLiteral(
        "chrome-extension://%1/transport.html"
    ).arg(m_extensionId));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("request"), request.id);
    transportUrl.setQuery(query);
    page->setUrl(transportUrl);
}

void VotChromiumNetworkTransport::startLoadedRequest(
    const QString &requestId,
    QWebEnginePage *page
)
{
    auto active = m_activeRequests.find(requestId);
    if (active == m_activeRequests.end() || !page
        || active->transportPage != page) {
        return;
    }
    active->started = true;
    const VotChromiumRequest &request = active->request;
    QJsonObject payload{
        {QStringLiteral("id"), request.id},
        {QStringLiteral("token"), m_token},
        {QStringLiteral("url"), request.url.toString(QUrl::FullyEncoded)},
        {QStringLiteral("method"), QString::fromLatin1(request.method)},
        {QStringLiteral("headers"), request.headers},
        {QStringLiteral("body"), QString::fromLatin1(request.body.toBase64())},
        {QStringLiteral("redirect"), request.redirectMode},
        {QStringLiteral("timeout"), request.timeoutMilliseconds},
    };
    page->runJavaScript(
        QStringLiteral("globalThis.panBrowserVotTransport?.request(%1)")
            .arg(javaScriptValue(payload)),
        QWebEngineScript::MainWorld,
        [self = QPointer<VotChromiumNetworkTransport>(this),
         id = request.id,
         guardedPage = QPointer<QWebEnginePage>(page)](const QVariant &started) {
            if (!self || !guardedPage || !self->m_activeRequests.contains(id)
                || started.toBool()) {
                return;
            }
            self->emitTerminalResponse({
                {QStringLiteral("id"), id},
                {QStringLiteral("type"), QStringLiteral("error")},
                {
                    QStringLiteral("error"),
                    QCoreApplication::translate(
                        "VotChromiumNetworkTransport",
                        "The Chromium VOT request could not be started"
                    )
                },
            });
        }
    );
}

void VotChromiumNetworkTransport::handleRequestPageFailure(
    const QString &requestId,
    QWebEnginePage *page,
    const QString &error
)
{
    const auto found = m_activeRequests.constFind(requestId);
    if (found == m_activeRequests.cend() || found->transportPage != page)
        return;
    emitTerminalResponse({
        {QStringLiteral("id"), requestId},
        {QStringLiteral("type"), QStringLiteral("error")},
        {QStringLiteral("error"), error},
    });
}

void VotChromiumNetworkTransport::handleConsoleMessage(
    const QString &message
)
{
    const QString prefix = responseMessagePrefix();
    if (!message.startsWith(prefix) || message.size() > maximumTransportMessageLength)
        return;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        message.mid(prefix.size()).toUtf8(),
        &parseError
    );
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;
    QJsonObject response = document.object();
    if (response.take(QStringLiteral("token")).toString() != m_token)
        return;
    const QString id = response.value(QStringLiteral("id")).toString();
    if (!m_activeRequests.contains(id))
        return;
    emitTerminalResponse(response);
}

void VotChromiumNetworkTransport::resetTransportProfile()
{
    if (m_initializationTimer)
        m_initializationTimer->stop();
    const QSet<QWebEnginePage *> retiredRequestPages = m_retiredRequestPages;
    m_retiredRequestPages.clear();
    for (QWebEnginePage *page : retiredRequestPages)
        delete page;
    if (auto *page = static_cast<TransportPage *>(m_controlPage))
        page->consoleHandler = {};
    delete m_controlPage;
    m_controlPage = nullptr;
    delete m_profileBootstrapPage;
    m_profileBootstrapPage = nullptr;
    delete m_transportProfile;
    m_transportProfile = nullptr;
    m_requestInterceptor = nullptr;
    m_extensionId.clear();
}

void VotChromiumNetworkTransport::retireRequestPage(QWebEnginePage *page)
{
    if (!page || m_retiredRequestPages.contains(page))
        return;
    m_retiredRequestPages.insert(page);
    connect(page, &QObject::destroyed, this, [this, page] {
        m_retiredRequestPages.remove(page);
    });
    page->deleteLater();
}

void VotChromiumNetworkTransport::fail(const QString &error)
{
    const QString detail = error.isEmpty()
        ? tr("The built-in VOT network transport failed")
        : error;
    const QStringList activeIds = m_activeRequests.keys();
    m_pendingRequests.clear();
    for (const QString &id : activeIds) {
        emitTerminalResponse({
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), detail},
        });
    }
    setState(VotChromiumTransportState::Error, detail);
}

void VotChromiumNetworkTransport::setState(
    VotChromiumTransportState state,
    const QString &error
)
{
    if (m_state == state && m_error == error)
        return;
    if (state != VotChromiumTransportState::Loading && m_initializationTimer)
        m_initializationTimer->stop();
    m_state = state;
    m_error = error;
    emit stateChanged();
}

void VotChromiumNetworkTransport::emitTerminalResponse(
    const QJsonObject &response
)
{
    const QString id = response.value(QStringLiteral("id")).toString();
    auto found = m_activeRequests.find(id);
    if (id.isEmpty() || found == m_activeRequests.end())
        return;
    const ActiveRequest active = found.value();
    m_activeRequests.erase(found);
    m_pendingRequests.remove(id);
    if (active.timeoutTimer) {
        active.timeoutTimer->stop();
        active.timeoutTimer->deleteLater();
    }
    if (active.transportPage) {
        static_cast<TransportPage *>(active.transportPage.data())
            ->consoleHandler = {};
        retireRequestPage(active.transportPage.data());
    }
    emit responseReady(response);
}
