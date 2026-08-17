#include "CrossDomainRequestInterceptor.h"

#include "BrowserProfile.h"
#include "SiteDomain.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QReadLocker>
#include <QUrl>
#include <QUrlQuery>
#include <QWebEngineUrlRequestInfo>
#include <QWriteLocker>

#include <utility>

namespace {

constexpr qsizetype maximumFingerprintComponentCharacters = 2048;
constexpr qsizetype maximumNormalizedFingerprintComponentCharacters =
    2 * maximumFingerprintComponentCharacters;
constexpr qsizetype maximumVotTransportRequests = 256;
constexpr qsizetype maximumVotTransportRedirects = 5;
constexpr qsizetype maximumVotTransportCountedRequestAttempts =
    maximumVotTransportRedirects + 1;

struct PromptSourceIdentity {
    QUrl url;
    bool originOnly = false;
};

bool isTopLevelRequest(QWebEngineUrlRequestInfo::ResourceType type)
{
    return type == QWebEngineUrlRequestInfo::ResourceTypeMainFrame
        || type == QWebEngineUrlRequestInfo::ResourceTypeNavigationPreloadMainFrame;
}

bool isAttributableWebSource(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    return url.isValid()
        && (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
        && !SiteDomain::siteForUrl(url).isEmpty();
}

bool isExtensionOrigin(const QUrl &url, const QString &extensionId)
{
    return !extensionId.isEmpty()
        && url.scheme().compare(
            QStringLiteral("chrome-extension"),
            Qt::CaseInsensitive
        ) == 0
        && url.host().compare(extensionId, Qt::CaseInsensitive) == 0;
}

void addBoundedFingerprintComponent(
    QCryptographicHash *hash,
    const QString &value
)
{
    hash->addData(QByteArray::number(value.size()));
    hash->addData(QByteArrayLiteral(":"));
    if (value.size() <= 2 * maximumFingerprintComponentCharacters) {
        hash->addData(value.toUtf8());
    } else {
        hash->addData(value.left(maximumFingerprintComponentCharacters).toUtf8());
        hash->addData(value.right(maximumFingerprintComponentCharacters).toUtf8());
    }
    hash->addData(QByteArrayLiteral(";"));
}

PromptSourceIdentity promptSourceIdentity(
    const QUrl &sourceUrl,
    bool forceOriginOnly = false
)
{
    const QString sourcePath = sourceUrl.path();
    const QString sourceQuery = sourceUrl.query();
    const QString sourceFragment = sourceUrl.fragment();
    const QString sourceUserInfo = sourceUrl.userInfo();
    const bool oversized =
        sourcePath.size() > maximumNormalizedFingerprintComponentCharacters
        || sourceQuery.size() > maximumNormalizedFingerprintComponentCharacters
        || sourceFragment.size() > maximumNormalizedFingerprintComponentCharacters
        || sourceUserInfo.size() > maximumNormalizedFingerprintComponentCharacters;
    if (!forceOriginOnly && !oversized)
        return {SiteDomain::normalizedPageUrl(sourceUrl), false};

    QUrl bounded;
    bounded.setScheme(sourceUrl.scheme().toLower());
    bounded.setHost(sourceUrl.host());
    int port = sourceUrl.port();
    if ((bounded.scheme() == QStringLiteral("https") && port == 443)
        || (bounded.scheme() == QStringLiteral("http") && port == 80)) {
        port = -1;
    }
    bounded.setPort(port);
    bounded.setPath(QStringLiteral("/"));
    return {bounded, true};
}

QByteArray boundedSourceFingerprint(const PromptSourceIdentity &identity)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    addBoundedFingerprintComponent(
        &hash,
        identity.originOnly ? QStringLiteral("origin") : QStringLiteral("page")
    );
    const QUrl &normalized = identity.url;
    addBoundedFingerprintComponent(&hash, normalized.scheme());
    addBoundedFingerprintComponent(&hash, normalized.host());
    addBoundedFingerprintComponent(&hash, QString::number(normalized.port()));
    addBoundedFingerprintComponent(
        &hash,
        normalized.path(QUrl::FullyEncoded)
    );
    addBoundedFingerprintComponent(
        &hash,
        normalized.query(QUrl::FullyEncoded)
    );
    return hash.result();
}

QByteArray sourceFingerprint(const QUrl &sourceUrl, bool sourceUrlIsOriginOnly)
{
    return boundedSourceFingerprint(
        promptSourceIdentity(sourceUrl, sourceUrlIsOriginOnly)
    );
}

} // namespace

CrossDomainResolvedSource crossDomainRequestSource(
    const QUrl &firstPartyUrl,
    const QUrl &initiator
)
{
    if (isAttributableWebSource(firstPartyUrl))
        return {firstPartyUrl, false};
    if (isAttributableWebSource(initiator))
        return {initiator, true};
    return {};
}

bool CrossDomainPendingRequestTracker::record(
    const QString &requestKey,
    const QUrl &sourceUrl,
    QUrl *promptSourceUrl,
    bool *sourceUrlIsOriginOnly,
    bool forceOriginOnly
)
{
    if (promptSourceUrl)
        promptSourceUrl->clear();
    if (sourceUrlIsOriginOnly)
        *sourceUrlIsOriginOnly = false;
    auto sources = m_sourcesByRequest.find(requestKey);
    if (m_size >= maximumPendingSources)
        return false;
    if (sources != m_sourcesByRequest.end()
        && sources->size() >= maximumSourcesPerRequest) {
        return false;
    }
    const PromptSourceIdentity identity = promptSourceIdentity(
        sourceUrl,
        forceOriginOnly
    );
    const QByteArray fingerprint = boundedSourceFingerprint(identity);
    if (sources != m_sourcesByRequest.end() && sources->contains(fingerprint))
        return false;
    if (sources == m_sourcesByRequest.end())
        sources = m_sourcesByRequest.insert(requestKey, {});
    sources->insert(fingerprint);
    ++m_size;
    if (promptSourceUrl)
        *promptSourceUrl = identity.url;
    if (sourceUrlIsOriginOnly)
        *sourceUrlIsOriginOnly = identity.originOnly;
    return true;
}

bool CrossDomainPendingRequestTracker::contains(
    const QString &requestKey,
    const QUrl &sourceUrl,
    bool sourceUrlIsOriginOnly
) const
{
    const auto sources = m_sourcesByRequest.constFind(requestKey);
    return sources != m_sourcesByRequest.cend()
        && sources->contains(sourceFingerprint(sourceUrl, sourceUrlIsOriginOnly));
}

void CrossDomainPendingRequestTracker::removeSource(
    const QString &requestKey,
    const QUrl &sourceUrl,
    bool sourceUrlIsOriginOnly
)
{
    const auto found = m_sourcesByRequest.find(requestKey);
    if (found == m_sourcesByRequest.end()
        || !found->remove(sourceFingerprint(sourceUrl, sourceUrlIsOriginOnly))) {
        return;
    }
    --m_size;
    if (found->isEmpty())
        m_sourcesByRequest.erase(found);
}

void CrossDomainPendingRequestTracker::remove(const QString &requestKey)
{
    const auto found = m_sourcesByRequest.find(requestKey);
    if (found == m_sourcesByRequest.end())
        return;
    m_size -= found->size();
    m_sourcesByRequest.erase(found);
}

void CrossDomainPendingRequestTracker::clear()
{
    m_sourcesByRequest.clear();
    m_size = 0;
}

qsizetype CrossDomainPendingRequestTracker::size() const
{
    return m_size;
}

CrossDomainRequestInterceptor::CrossDomainRequestInterceptor(
    bool blockNetwork,
    const CrossDomainSettings &settings,
    QObject *parent
)
    : QWebEngineUrlRequestInterceptor(parent)
    , m_blockNetwork(blockNetwork)
    , m_policy(settings)
{
}

void CrossDomainRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (m_blockNetwork
        && BrowserProfile::shouldBlockForProxyConfigurationError(info.requestUrl())) {
        info.block(true);
        return;
    }
    bool votTransportRequest = false;
    const bool votTransportAllowed = interceptVotTransportRequest(
        info,
        &votTransportRequest
    );
    if (votTransportRequest) {
        if (!votTransportAllowed)
            info.block(true);
        return;
    }
    if (isTopLevelRequest(info.resourceType()) || info.isDownload())
        return;

    const CrossDomainResolvedSource source = crossDomainRequestSource(
        info.firstPartyUrl(),
        info.initiator()
    );
    if (!allowRequest(
            source.url,
            info.requestUrl(),
            static_cast<int>(info.resourceType()),
            source.originOnly
        )) {
        info.block(true);
    }
}

QString CrossDomainRequestInterceptor::votTransportRequestId(
    const QUrl &firstPartyUrl,
    const QString &extensionId
)
{
    if (!isExtensionOrigin(firstPartyUrl, extensionId)
        || firstPartyUrl.path() != QStringLiteral("/transport.html")) {
        return {};
    }
    const QStringList requestIds = QUrlQuery(firstPartyUrl).allQueryItemValues(
        QStringLiteral("request"),
        QUrl::FullyDecoded
    );
    if (requestIds.size() != 1
        || requestIds.constFirst().isEmpty()
        || requestIds.constFirst().size() > 128) {
        return {};
    }
    return requestIds.constFirst();
}

void CrossDomainRequestInterceptor::setVotTransportExtensionId(
    const QString &extensionId
)
{
    QWriteLocker locker(&m_lock);
    if (m_votTransportExtensionId == extensionId)
        return;
    const bool replacingActiveExtension = !m_votTransportExtensionId.isEmpty();
    m_votTransportExtensionId = extensionId;
    if (extensionId.isEmpty() || replacingActiveExtension)
        m_votTransportRequests.clear();
}

bool CrossDomainRequestInterceptor::registerVotTransportRequest(
    const QString &requestId,
    VotTransportAuthorizer authorizer
)
{
    if (requestId.isEmpty() || requestId.size() > 128 || !authorizer)
        return false;
    QWriteLocker locker(&m_lock);
    if (m_votTransportRequests.contains(requestId)
        || m_votTransportRequests.size() >= maximumVotTransportRequests) {
        return false;
    }
    m_votTransportRequests.insert(
        requestId,
        {std::move(authorizer), 0}
    );
    return true;
}

void CrossDomainRequestInterceptor::unregisterVotTransportRequest(
    const QString &requestId
)
{
    QWriteLocker locker(&m_lock);
    m_votTransportRequests.remove(requestId);
}

bool CrossDomainRequestInterceptor::interceptVotTransportRequest(
    QWebEngineUrlRequestInfo &info,
    bool *handled
)
{
    if (handled)
        *handled = false;

    const QString requestScheme = info.requestUrl().scheme().toLower();
    if (requestScheme != QStringLiteral("http")
        && requestScheme != QStringLiteral("https")) {
        return false;
    }

    QString extensionId;
    {
        QReadLocker locker(&m_lock);
        extensionId = m_votTransportExtensionId;
    }
    const bool fromVotExtension = isExtensionOrigin(
        info.firstPartyUrl(),
        extensionId
    ) || isExtensionOrigin(info.initiator(), extensionId);
    if (!fromVotExtension)
        return false;
    if (handled)
        *handled = true;
    const QString requestId = votTransportRequestId(
        info.firstPartyUrl(),
        extensionId
    );
    if (requestId.isEmpty())
        return false;

    VotTransportAuthorizer authorizer;
    {
        QWriteLocker locker(&m_lock);
        auto authorization = m_votTransportRequests.find(requestId);
        if (authorization == m_votTransportRequests.end())
            return false;
        const bool isPreflight = info.requestMethod().compare(
            QByteArrayLiteral("OPTIONS"),
            Qt::CaseInsensitive
        ) == 0;
        if (!isPreflight) {
            if (authorization->countedRequestAttempts
                >= maximumVotTransportCountedRequestAttempts) {
                return false;
            }
            ++authorization->countedRequestAttempts;
        }
        authorizer = authorization->authorizer;
    }
    return authorizer
        && authorizer(
            info.requestUrl(),
            static_cast<int>(info.resourceType())
        );
}

bool CrossDomainRequestInterceptor::allowRequest(
    const QUrl &sourceUrl,
    const QUrl &targetUrl,
    int resourceType,
    bool sourceUrlIsOriginOnly
)
{
    if (m_blockNetwork
        && BrowserProfile::shouldBlockForProxyConfigurationError(targetUrl)) {
        return false;
    }

    CrossDomainPolicySnapshot policy;
    {
        QReadLocker locker(&m_lock);
        policy = m_policy;
    }
    const CrossDomainRequestPolicyResult configured = policy.evaluateUserPolicy(
        sourceUrl,
        targetUrl
    );
    if (configured.decision == CrossDomainEvaluation::Allow)
        return true;
    if (configured.decision == CrossDomainEvaluation::Block)
        return false;

    const QString &sourceSite = configured.sourceSite;
    const QString &targetHost = configured.targetHost;
    if (sourceSite.isEmpty() || targetHost.isEmpty())
        return true;
    const QString key = requestKey(sourceSite, targetHost);

    {
        QReadLocker locker(&m_lock);
        const auto session = m_sessionDecisions.constFind(key);
        if (session != m_sessionDecisions.cend()) {
            return *session == CrossDomainRuleDecision::Allow;
        }
    }

    const CrossDomainEvaluation preset = policy.evaluatePresetPolicy(targetHost);
    if (preset == CrossDomainEvaluation::Allow)
        return true;
    if (preset == CrossDomainEvaluation::Block)
        return false;

    CrossDomainEvaluation finalDecision = CrossDomainEvaluation::Ask;
    QUrl pendingSourceUrl;
    bool pendingSourceUrlIsOriginOnly = false;
    QString pendingSourceSite;
    QString pendingTargetHost;
    bool notify = false;
    {
        QWriteLocker locker(&m_lock);
        const CrossDomainRequestPolicyResult latest = m_policy.evaluateUserPolicy(
            sourceUrl,
            targetUrl
        );
        finalDecision = latest.decision;
        if (finalDecision == CrossDomainEvaluation::Ask) {
            pendingSourceSite = latest.sourceSite;
            pendingTargetHost = latest.targetHost;
            if (pendingSourceSite.isEmpty() || pendingTargetHost.isEmpty()) {
                finalDecision = CrossDomainEvaluation::Allow;
            } else {
                const QString latestKey = requestKey(
                    pendingSourceSite,
                    pendingTargetHost
                );
                const auto latestSession = m_sessionDecisions.constFind(latestKey);
                if (latestSession != m_sessionDecisions.cend()) {
                    finalDecision = *latestSession == CrossDomainRuleDecision::Allow
                        ? CrossDomainEvaluation::Allow
                        : CrossDomainEvaluation::Block;
                } else {
                    finalDecision = m_policy.evaluatePresetPolicy(pendingTargetHost);
                    if (finalDecision == CrossDomainEvaluation::Ask) {
                        notify = m_pendingRequests.record(
                            latestKey,
                            sourceUrl,
                            &pendingSourceUrl,
                            &pendingSourceUrlIsOriginOnly,
                            sourceUrlIsOriginOnly
                        );
                    }
                }
            }
        }
    }
    if (finalDecision == CrossDomainEvaluation::Allow)
        return true;

    if (notify) {
        emit requestBlocked(
            pendingSourceUrl,
            pendingSourceSite,
            pendingTargetHost,
            resourceType,
            pendingSourceUrlIsOriginOnly
        );
    }
    return false;
}

void CrossDomainRequestInterceptor::setSettings(const CrossDomainSettings &settings)
{
    CrossDomainPolicySnapshot policy(settings);
    QWriteLocker locker(&m_lock);
    m_policy = std::move(policy);
    m_pendingRequests.clear();
    if (!settings.enabled())
        m_sessionDecisions.clear();
}

void CrossDomainRequestInterceptor::setSettingsResolvingPendingRequest(
    const CrossDomainSettings &settings,
    const QString &sourceSite,
    const QString &targetHost
)
{
    CrossDomainPolicySnapshot policy(settings);
    const QString key = requestKey(sourceSite, targetHost);
    QWriteLocker locker(&m_lock);
    m_policy = std::move(policy);
    if (settings.enabled()) {
        m_pendingRequests.remove(key);
    } else {
        m_pendingRequests.clear();
        m_sessionDecisions.clear();
    }
}

void CrossDomainRequestInterceptor::resolveForSession(
    const QString &sourceSite,
    const QString &targetHost,
    CrossDomainRuleDecision decision
)
{
    const QString key = requestKey(sourceSite, targetHost);
    QWriteLocker locker(&m_lock);
    m_pendingRequests.remove(key);
    m_sessionDecisions.insert(key, decision);
}

void CrossDomainRequestInterceptor::dismiss(
    const QString &sourceSite,
    const QString &targetHost
)
{
    QWriteLocker locker(&m_lock);
    m_pendingRequests.remove(requestKey(sourceSite, targetHost));
}

void CrossDomainRequestInterceptor::dismissSource(
    const QString &sourceSite,
    const QString &targetHost,
    const QUrl &sourceUrl,
    bool sourceUrlIsOriginOnly
)
{
    QWriteLocker locker(&m_lock);
    m_pendingRequests.removeSource(
        requestKey(sourceSite, targetHost),
        sourceUrl,
        sourceUrlIsOriginOnly
    );
}

bool CrossDomainRequestInterceptor::isPending(
    const QString &sourceSite,
    const QString &targetHost,
    const QUrl &sourceUrl,
    bool sourceUrlIsOriginOnly
) const
{
    const QString key = requestKey(sourceSite, targetHost);
    QReadLocker locker(&m_lock);
    return m_pendingRequests.contains(key, sourceUrl, sourceUrlIsOriginOnly);
}

QString CrossDomainRequestInterceptor::requestKey(
    const QString &sourceSite,
    const QString &targetHost
)
{
    return SiteDomain::registrableDomain(sourceSite)
        + QLatin1Char('\n')
        + SiteDomain::normalizeHost(targetHost);
}

QString crossDomainResourceTypeDisplayName(int resourceType)
{
    using ResourceType = QWebEngineUrlRequestInfo::ResourceType;
    switch (static_cast<ResourceType>(resourceType)) {
    case ResourceType::ResourceTypeSubFrame:
        return QCoreApplication::translate("CrossDomainPrompt", "embedded frame");
    case ResourceType::ResourceTypeStylesheet:
        return QCoreApplication::translate("CrossDomainPrompt", "stylesheet");
    case ResourceType::ResourceTypeScript:
        return QCoreApplication::translate("CrossDomainPrompt", "script");
    case ResourceType::ResourceTypeImage:
    case ResourceType::ResourceTypeFavicon:
        return QCoreApplication::translate("CrossDomainPrompt", "image");
    case ResourceType::ResourceTypeFontResource:
        return QCoreApplication::translate("CrossDomainPrompt", "font");
    case ResourceType::ResourceTypeMedia:
        return QCoreApplication::translate("CrossDomainPrompt", "media");
    case ResourceType::ResourceTypeWorker:
    case ResourceType::ResourceTypeSharedWorker:
    case ResourceType::ResourceTypeServiceWorker:
        return QCoreApplication::translate("CrossDomainPrompt", "web worker");
    case ResourceType::ResourceTypeXhr:
    case ResourceType::ResourceTypeJson:
        return QCoreApplication::translate("CrossDomainPrompt", "data request");
    case ResourceType::ResourceTypeWebSocket:
        return QCoreApplication::translate("CrossDomainPrompt", "WebSocket");
    case ResourceType::ResourceTypePing:
        return QCoreApplication::translate("CrossDomainPrompt", "tracking ping");
    case ResourceType::ResourceTypePrefetch:
        return QCoreApplication::translate("CrossDomainPrompt", "prefetch request");
    case ResourceType::ResourceTypeCspReport:
        return QCoreApplication::translate("CrossDomainPrompt", "security report");
    case ResourceType::ResourceTypeMainFrame:
    case ResourceType::ResourceTypeSubResource:
    case ResourceType::ResourceTypeObject:
    case ResourceType::ResourceTypePluginResource:
    case ResourceType::ResourceTypeNavigationPreloadMainFrame:
    case ResourceType::ResourceTypeNavigationPreloadSubFrame:
    case ResourceType::ResourceTypeUnknown:
        return QCoreApplication::translate("CrossDomainPrompt", "network request");
    }
    return QCoreApplication::translate("CrossDomainPrompt", "network request");
}
