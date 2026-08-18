#include "VotChromiumNetworkTransport.h"

#include "BrowserPage.h"
#include "CrossDomainRequestInterceptor.h"
#include "PrivateData.h"
#include "VotChromiumRequestSession.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
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

constexpr int initializationTimeoutMilliseconds = 15'000;

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
    qDeleteAll(m_activeRequests);
    m_activeRequests.clear();
    resetTransportProfile();
}

QString VotChromiumNetworkTransport::responseMessagePrefix()
{
    return VotChromiumRequestSession::responseMessagePrefix();
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

    auto *session = new VotChromiumRequestSession(request, this);
    m_activeRequests.insert(request.id, session);
    connect(
        session,
        &VotChromiumRequestSession::responseReady,
        this,
        &VotChromiumNetworkTransport::handleSessionResponse
    );
    connect(
        session,
        &VotChromiumRequestSession::proxyAuthenticationRequired,
        this,
        &VotChromiumNetworkTransport::proxyAuthenticationRequired
    );

    if (m_state == VotChromiumTransportState::Uninitialized)
        ensureReady();
    if (m_state == VotChromiumTransportState::Error) {
        session->fail(m_error);
        return;
    }
    if (m_state == VotChromiumTransportState::Ready)
        startRequest(request);
}

void VotChromiumNetworkTransport::abortRequest(const QString &id)
{
    if (VotChromiumRequestSession *session = m_activeRequests.value(id))
        session->abort();
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

    auto *page = new QWebEnginePage(m_transportProfile, this);
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
    const QList<QString> requestIds = m_activeRequests.keys();
    for (const QString &requestId : requestIds) {
        VotChromiumRequestSession *session = m_activeRequests.value(requestId);
        if (session && !session->isStarted()) {
            session->start(
                m_transportProfile,
                m_extensionId,
                m_token
            );
        }
    }
}

void VotChromiumNetworkTransport::startRequest(
    const VotChromiumRequest &request
)
{
    VotChromiumRequestSession *session = m_activeRequests.value(request.id);
    if (!session || !m_transportProfile)
        return;
    session->start(
        m_transportProfile,
        m_extensionId,
        m_token
    );
}

void VotChromiumNetworkTransport::handleSessionResponse(
    const QJsonObject &response
)
{
    const QString id = response.value(QStringLiteral("id")).toString();
    VotChromiumRequestSession *session = m_activeRequests.take(id);
    if (id.isEmpty() || !session)
        return;
    session->deleteLater();
    emit responseReady(response);
}

void VotChromiumNetworkTransport::resetTransportProfile()
{
    if (m_initializationTimer)
        m_initializationTimer->stop();
    delete m_controlPage;
    m_controlPage = nullptr;
    delete m_profileBootstrapPage;
    m_profileBootstrapPage = nullptr;
    delete m_transportProfile;
    m_transportProfile = nullptr;
    m_requestInterceptor = nullptr;
    m_extensionId.clear();
}

void VotChromiumNetworkTransport::fail(const QString &error)
{
    const QString detail = error.isEmpty()
        ? tr("The built-in VOT network transport failed")
        : error;
    const QStringList activeIds = m_activeRequests.keys();
    for (const QString &id : activeIds) {
        if (VotChromiumRequestSession *session = m_activeRequests.value(id))
            session->fail(detail);
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
