#include "CrossDomainPromptController.h"

#include "BrowserProfile.h"
#include "CrossDomainPrompt.h"
#include "CrossDomainRequestInterceptor.h"
#include "SiteDomain.h"

#include <QWebEnginePage>
#include <QWebEngineView>

#include <algorithm>
#include <utility>

namespace {

QUrl normalizedWebOrigin(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    const QString host = SiteDomain::normalizeHost(url.host());
    if ((scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
        || host.isEmpty()) {
        return {};
    }

    int port = url.port();
    if ((scheme == QStringLiteral("https") && port == 443)
        || (scheme == QStringLiteral("http") && port == 80)) {
        port = -1;
    }
    QUrl origin;
    origin.setScheme(scheme);
    origin.setHost(host);
    origin.setPort(port);
    origin.setPath(QStringLiteral("/"));
    return origin;
}

QList<CrossDomainPromptSource> normalizedPromptSources(
    const QList<CrossDomainPromptSource> &sources
)
{
    QList<CrossDomainPromptSource> normalized;
    for (const CrossDomainPromptSource &source : sources) {
        CrossDomainPromptSource candidate = source;
        candidate.url = source.originOnly
            ? normalizedWebOrigin(source.url)
            : SiteDomain::normalizedPageUrl(source.url);
        if (candidate.url.isEmpty() || normalizedWebOrigin(candidate.url).isEmpty())
            continue;
        const auto duplicate = std::find_if(
            normalized.cbegin(),
            normalized.cend(),
            [&candidate](const CrossDomainPromptSource &existing) {
                return existing.originOnly == candidate.originOnly
                    && existing.url == candidate.url;
            }
        );
        if (duplicate == normalized.cend()
            && normalized.size()
                < CrossDomainPendingRequestTracker::maximumSourcesPerRequest) {
            normalized.append(std::move(candidate));
        }
    }
    return normalized;
}

} // namespace

QList<qsizetype> crossDomainPromptCandidateIndexes(
    const QUrl &sourceUrl,
    const QString &sourceSite,
    bool sourceUrlIsOriginOnly,
    const QList<QUrl> &candidateUrls
)
{
    QList<qsizetype> matchingIndexes;
    const QString normalizedSourceSite = SiteDomain::registrableDomain(sourceSite);
    if (normalizedSourceSite.isEmpty())
        return matchingIndexes;

    const QUrl normalizedSourceUrl = SiteDomain::normalizedPageUrl(sourceUrl);
    const QUrl sourceOrigin = sourceUrlIsOriginOnly
        ? normalizedWebOrigin(sourceUrl)
        : QUrl();
    if (sourceUrlIsOriginOnly && sourceOrigin.isEmpty())
        return matchingIndexes;
    for (qsizetype index = 0; index < candidateUrls.size(); ++index) {
        const QUrl &candidateUrl = candidateUrls.at(index);
        if (SiteDomain::siteForUrl(candidateUrl) != normalizedSourceSite)
            continue;
        if ((sourceUrlIsOriginOnly
             && normalizedWebOrigin(candidateUrl) == sourceOrigin)
            || (!sourceUrlIsOriginOnly
                && SiteDomain::normalizedPageUrl(candidateUrl) == normalizedSourceUrl)) {
            matchingIndexes.append(index);
        }
    }

    if (sourceUrlIsOriginOnly && matchingIndexes.size() != 1)
        matchingIndexes.clear();
    return matchingIndexes;
}

CrossDomainPromptController::CrossDomainPromptController(
    BrowserProfile *profile,
    CrossDomainPrompt *prompt,
    QObject *parent
)
    : QObject(parent)
    , m_profile(profile)
    , m_prompt(prompt)
{
    Q_ASSERT(m_prompt || !m_profile);
    if (m_prompt) {
        connect(
            m_prompt,
            &CrossDomainPrompt::decisionRequested,
            this,
            &CrossDomainPromptController::resolveCurrent
        );
    }
}

CrossDomainPromptController::~CrossDomainPromptController()
{
    dismissAll();
}

void CrossDomainPromptController::request(
    QWebEngineView *webView,
    const QList<CrossDomainAffectedView> &affectedViews,
    const QList<CrossDomainPromptSource> &sources,
    const QString &sourceSite,
    const QString &targetHost,
    int resourceType
)
{
    const QList<CrossDomainPromptSource> normalizedSources = normalizedPromptSources(
        sources
    );
    if (!webView
        || normalizedSources.isEmpty()
        || sourceSite.isEmpty()
        || targetHost.isEmpty()) {
        return;
    }
    const auto matches = [&](const PendingRequest &request) {
        return request.sourceSite == sourceSite
            && request.targetHost == targetHost;
    };
    const auto mergeAffectedViews = [&](PendingRequest &request) {
        QList<CrossDomainAffectedView> candidates = affectedViews;
        const auto isAnchor = [webView](const CrossDomainAffectedView &candidate) {
            return candidate.webView == webView;
        };
        if (std::none_of(candidates.cbegin(), candidates.cend(), isAnchor)) {
            candidates.prepend({
                webView,
                webView->page()
                    ? SiteDomain::normalizedPageUrl(webView->page()->url())
                    : QUrl(),
            });
        }
        for (CrossDomainAffectedView candidate : std::as_const(candidates)) {
            if (!candidate.webView || candidate.expectedUrl.isEmpty())
                continue;
            candidate.expectedUrl = SiteDomain::normalizedPageUrl(candidate.expectedUrl);
            const auto alreadyTracked = std::find_if(
                request.affectedViews.begin(),
                request.affectedViews.end(),
                [&candidate](const CrossDomainAffectedView &existing) {
                    return existing.webView == candidate.webView;
                }
            );
            if (alreadyTracked != request.affectedViews.end()) {
                alreadyTracked->expectedUrl = candidate.expectedUrl;
            } else if (request.affectedViews.size() < maximumAffectedViews) {
                request.affectedViews.append(std::move(candidate));
            }
        }
    };
    const auto mergeSources = [&](PendingRequest &request) {
        for (const CrossDomainPromptSource &candidate : normalizedSources) {
            const auto duplicate = std::find_if(
                request.sources.cbegin(),
                request.sources.cend(),
                [&candidate](const CrossDomainPromptSource &existing) {
                    return existing.originOnly == candidate.originOnly
                        && existing.url == candidate.url;
                }
            );
            if (duplicate == request.sources.cend()
                && request.sources.size()
                    < CrossDomainPendingRequestTracker::maximumSourcesPerRequest) {
                request.sources.append(candidate);
            }
        }
    };
    if (m_active && matches(*m_active)) {
        mergeAffectedViews(*m_active);
        mergeSources(*m_active);
        return;
    }
    const auto queued = std::find_if(m_queue.begin(), m_queue.end(), matches);
    if (queued != m_queue.end()) {
        mergeAffectedViews(*queued);
        mergeSources(*queued);
        return;
    }

    PendingRequest request;
    request.webView = webView;
    request.sourceSite = sourceSite;
    request.targetHost = targetHost;
    request.resourceType = resourceType;
    mergeAffectedViews(request);
    mergeSources(request);
    m_queue.append(request);
    showNext();
}

void CrossDomainPromptController::currentViewChanged(QWebEngineView *webView)
{
    m_currentView = webView;
    if (m_active && m_active->webView != m_currentView) {
        m_queue.prepend(*m_active);
        m_active.reset();
        if (m_prompt)
            m_prompt->hideRequest();
    }
    showNext();
}

void CrossDomainPromptController::cancelForView(QWebEngineView *webView)
{
    if (!webView)
        return;
    QList<PendingRequest> reanchorRequests;
    if (m_active) {
        const bool anchorChanged = m_active->webView == webView
            || !m_active->webView;
        if (!removeViewAndChooseAnchor(&*m_active, webView)) {
            dismiss(*m_active);
            m_active.reset();
            if (m_prompt)
                m_prompt->hideRequest();
        } else if (anchorChanged) {
            reanchorRequests.append(*m_active);
            m_active.reset();
            if (m_prompt)
                m_prompt->hideRequest();
        }
    }
    for (qsizetype index = m_queue.size() - 1; index >= 0; --index) {
        const bool anchorChanged = m_queue.at(index).webView == webView
            || !m_queue.at(index).webView;
        if (!removeViewAndChooseAnchor(&m_queue[index], webView)) {
            dismiss(m_queue.at(index));
            m_queue.removeAt(index);
        } else if (anchorChanged) {
            reanchorRequests.prepend(m_queue.takeAt(index));
        }
    }
    for (const PendingRequest &request : std::as_const(reanchorRequests)) {
        emit requestReanchorRequested(
            request.webView,
            request.affectedViews,
            request.sources,
            request.sourceSite,
            request.targetHost,
            request.resourceType
        );
    }
    showNext();
}

void CrossDomainPromptController::reset()
{
    dismissAll();
}

void CrossDomainPromptController::resolveCurrent(
    CrossDomainRuleDecision decision,
    bool persist
)
{
    if (!m_active || !m_profile)
        return;

    QString error;
    if (!m_profile->resolveCrossDomainRequest(
            m_active->sourceSite,
            m_active->targetHost,
            decision,
            persist,
            &error
        )) {
        emit errorOccurred(error);
        return;
    }

    const QString sourceSite = m_active->sourceSite;
    const QString targetHost = m_active->targetHost;
    const QList<CrossDomainAffectedView> affectedViews = m_active->affectedViews;
    m_active.reset();
    if (m_prompt)
        m_prompt->hideRequest();
    emit requestFinished(sourceSite, targetHost);
    if (decision == CrossDomainRuleDecision::Allow) {
        for (const CrossDomainAffectedView &affected : affectedViews) {
            if (affected.webView
                && affected.webView->page()
                && SiteDomain::normalizedPageUrl(affected.webView->page()->url())
                    == affected.expectedUrl) {
                affected.webView->page()->triggerAction(QWebEnginePage::Reload);
            }
        }
    }
    showNext();
}

void CrossDomainPromptController::showNext()
{
    if (m_active)
        return;
    for (qsizetype index = 0; index < m_queue.size();) {
        const PendingRequest request = m_queue.at(index);
        if (!request.webView) {
            dismiss(request);
            m_queue.removeAt(index);
            continue;
        }
        if (request.webView != m_currentView) {
            ++index;
            continue;
        }

        m_active = request;
        m_queue.removeAt(index);
        if (m_prompt) {
            m_prompt->showRequest(
                request.sourceSite,
                request.targetHost,
                crossDomainResourceTypeDisplayName(request.resourceType)
            );
        }
        return;
    }
    if (m_prompt)
        m_prompt->hideRequest();
}

bool CrossDomainPromptController::removeViewAndChooseAnchor(
    PendingRequest *request,
    QWebEngineView *webView
)
{
    request->affectedViews.removeIf([webView](const auto &affected) {
        return !affected.webView || affected.webView == webView;
    });
    if (request->webView && request->webView != webView)
        return true;
    if (request->affectedViews.isEmpty())
        return false;

    request->webView = request->affectedViews.constFirst().webView;
    return true;
}

void CrossDomainPromptController::dismiss(const PendingRequest &request)
{
    if (m_profile) {
        for (const CrossDomainPromptSource &source : request.sources) {
            m_profile->dismissCrossDomainRequestSource(
                request.sourceSite,
                request.targetHost,
                source.url,
                source.originOnly
            );
        }
    }
    emit requestDismissed(request.sourceSite, request.targetHost, request.sources);
    emit requestFinished(request.sourceSite, request.targetHost);
}

void CrossDomainPromptController::dismissAll()
{
    if (m_active)
        dismiss(*m_active);
    m_active.reset();
    for (const PendingRequest &request : std::as_const(m_queue))
        dismiss(request);
    m_queue.clear();
    if (m_prompt)
        m_prompt->hideRequest();
}
