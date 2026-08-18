#include "CrossDomainWindowRouter.h"

#include "BrowserProfile.h"
#include "CrossDomainPromptController.h"
#include "MainWindow.h"
#include "SiteDomain.h"

#include <QApplication>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QWebEngineView>

#include <algorithm>
#include <memory>
#include <utility>

namespace {

QString requestKey(const QString &sourceSite, const QString &targetHost)
{
    return SiteDomain::registrableDomain(sourceSite)
        + QLatin1Char('\n')
        + SiteDomain::normalizeHost(targetHost);
}

} // namespace

class CrossDomainWindowRouter::Impl {
public:
    struct Registration {
        QPointer<MainWindow> window;
        QPointer<CrossDomainPromptController> controller;
    };

    struct Route {
        QPointer<MainWindow> window;
        QPointer<QWebEngineView> anchor;
    };

    BrowserProfile *profile = nullptr;
    QList<Registration> registrations;
    QHash<QString, Route> routes;
};

CrossDomainWindowRouter::CrossDomainWindowRouter(
    BrowserProfile *profile,
    QObject *parent
)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->profile = profile;
    if (profile) {
        connect(
            profile,
            &BrowserProfile::crossDomainRequestBlocked,
            this,
            [this](
                const QUrl &sourceUrl,
                const QString &sourceSite,
                const QString &targetHost,
                int resourceType,
                bool sourceUrlIsOriginOnly
            ) {
                route(
                    sourceUrl,
                    sourceSite,
                    targetHost,
                    resourceType,
                    sourceUrlIsOriginOnly
                );
            }
        );
    }
}

CrossDomainWindowRouter::~CrossDomainWindowRouter() = default;

void CrossDomainWindowRouter::registerWindow(
    MainWindow *window,
    CrossDomainPromptController *controller
)
{
    if (!window || !controller)
        return;

    unregisterWindow(window);
    m_impl->registrations.append({window, controller});

    connect(
        controller,
        &CrossDomainPromptController::requestFinished,
        this,
        [this, owner = QPointer<MainWindow>(window)](
            const QString &sourceSite,
            const QString &targetHost
        ) {
            const auto route = m_impl->routes.find(
                requestKey(sourceSite, targetHost)
            );
            if (route != m_impl->routes.end() && route->window == owner)
                m_impl->routes.erase(route);
        }
    );
    connect(
        controller,
        &CrossDomainPromptController::requestReanchorRequested,
        this,
        [this, owner = QPointer<MainWindow>(window)](
            QWebEngineView *webView,
            const QList<CrossDomainAffectedView> &affectedViews,
            const QList<CrossDomainPromptSource> &sources,
            const QString &sourceSite,
            const QString &targetHost,
            int resourceType
        ) {
            const QString key = requestKey(sourceSite, targetHost);
            MainWindow *newPromptWindow = nullptr;
            CrossDomainPromptController *newController = nullptr;
            for (const Impl::Registration &registration
                 : std::as_const(m_impl->registrations)) {
                if (registration.window
                    && registration.controller
                    && registration.window->m_tabStates.contains(webView)) {
                    newPromptWindow = registration.window;
                    newController = registration.controller;
                    break;
                }
            }

            auto route = m_impl->routes.find(key);
            if (!newPromptWindow || !newController) {
                if (route != m_impl->routes.end() && route->window == owner)
                    m_impl->routes.erase(route);
                if (m_impl->profile) {
                    for (const CrossDomainPromptSource &source : sources) {
                        m_impl->profile->dismissCrossDomainRequestSource(
                            sourceSite,
                            targetHost,
                            source.url,
                            source.originOnly
                        );
                    }
                }
                return;
            }

            if (route == m_impl->routes.end()) {
                m_impl->routes.insert(key, {newPromptWindow, webView});
            } else {
                route->window = newPromptWindow;
                route->anchor = webView;
            }
            newController->request(
                webView,
                affectedViews,
                sources,
                sourceSite,
                targetHost,
                resourceType
            );
        }
    );
}

void CrossDomainWindowRouter::unregisterWindow(MainWindow *window)
{
    if (!window)
        return;

    for (qsizetype index = m_impl->registrations.size() - 1; index >= 0; --index) {
        const Impl::Registration &registration = m_impl->registrations.at(index);
        if (!registration.window || registration.window == window) {
            if (registration.controller)
                disconnect(registration.controller, nullptr, this, nullptr);
            m_impl->registrations.removeAt(index);
        }
    }
    for (auto iterator = m_impl->routes.begin();
         iterator != m_impl->routes.end();) {
        if (!iterator->window || iterator->window == window || !iterator->anchor)
            iterator = m_impl->routes.erase(iterator);
        else
            ++iterator;
    }
}

void CrossDomainWindowRouter::cancelForView(QWebEngineView *webView)
{
    if (!webView)
        return;
    for (const Impl::Registration &registration
         : std::as_const(m_impl->registrations)) {
        if (registration.controller)
            registration.controller->cancelForView(webView);
    }
}

void CrossDomainWindowRouter::clearRoutes()
{
    m_impl->routes.clear();
}

void CrossDomainWindowRouter::route(
    const QUrl &sourceUrl,
    const QString &sourceSite,
    const QString &targetHost,
    int resourceType,
    bool sourceUrlIsOriginOnly,
    int attempt
)
{
    if (!m_impl->profile
        || !m_impl->profile->isCrossDomainRequestPending(
            sourceSite,
            targetHost,
            sourceUrl,
            sourceUrlIsOriginOnly
        )) {
        return;
    }

    if (m_impl->profile->crossDomainSettings().evaluate(sourceSite, targetHost)
        != CrossDomainEvaluation::Ask) {
        m_impl->profile->dismissCrossDomainRequest(sourceSite, targetHost);
        return;
    }

    const QString key = requestKey(sourceSite, targetHost);
    auto existingRoute = m_impl->routes.find(key);
    const auto controllerFor = [this](MainWindow *window) {
        for (const Impl::Registration &registration
             : std::as_const(m_impl->registrations)) {
            if (registration.window == window)
                return registration.controller.data();
        }
        return static_cast<CrossDomainPromptController *>(nullptr);
    };
    if (existingRoute != m_impl->routes.end()
        && (!existingRoute->window
            || !existingRoute->anchor
            || !controllerFor(existingRoute->window)
            || !existingRoute->window->m_tabStates.contains(
                existingRoute->anchor
            ))) {
        existingRoute = m_impl->routes.erase(existingRoute);
    }
    const bool hasPromptOwner = existingRoute != m_impl->routes.end();

    QList<MainWindow *> windows;
    for (const Impl::Registration &registration
         : std::as_const(m_impl->registrations)) {
        if (registration.window && registration.controller)
            windows.append(registration.window);
    }
    if (auto *active = qobject_cast<MainWindow *>(QApplication::activeWindow())) {
        if (windows.removeOne(active))
            windows.prepend(active);
    }

    struct Candidate {
        MainWindow *window = nullptr;
        QWebEngineView *view = nullptr;
    };
    QList<Candidate> candidates;
    QList<QUrl> candidateUrls;
    for (MainWindow *window : std::as_const(windows)) {
        QList<QWebEngineView *> views = window->m_tabStates.keys();
        if (QWebEngineView *current = window->currentWebView()) {
            if (views.removeOne(current))
                views.prepend(current);
        }
        for (QWebEngineView *view : std::as_const(views)) {
            candidates.append({window, view});
            candidateUrls.append(window->urlForTab(view));
        }
    }

    const QList<qsizetype> matchingIndexes = crossDomainPromptCandidateIndexes(
        sourceUrl,
        sourceSite,
        sourceUrlIsOriginOnly,
        candidateUrls
    );
    const Candidate candidate = matchingIndexes.isEmpty()
        ? Candidate()
        : candidates.at(matchingIndexes.constFirst());
    if (candidate.window && candidate.view) {
        QList<CrossDomainAffectedView> affectedViews;
        affectedViews.reserve(matchingIndexes.size());
        for (const qsizetype index : matchingIndexes) {
            const Candidate &matchingCandidate = candidates.at(index);
            affectedViews.append({
                matchingCandidate.view,
                SiteDomain::normalizedPageUrl(
                    matchingCandidate.window->urlForTab(matchingCandidate.view)
                ),
            });
        }

        MainWindow *promptWindow = candidate.window;
        QWebEngineView *promptAnchor = candidate.view;
        if (hasPromptOwner) {
            promptWindow = existingRoute->window;
            promptAnchor = existingRoute->anchor;
        } else {
            m_impl->routes.insert(key, {promptWindow, promptAnchor});
        }
        if (CrossDomainPromptController *controller = controllerFor(promptWindow)) {
            controller->request(
                promptAnchor,
                affectedViews,
                QList<CrossDomainPromptSource>{
                    {sourceUrl, sourceUrlIsOriginOnly},
                },
                sourceSite,
                targetHost,
                resourceType
            );
        }
        return;
    }

    if (attempt < 3) {
        QTimer::singleShot(50 * (attempt + 1), this, [
            this,
            sourceUrl,
            sourceSite,
            targetHost,
            resourceType,
            sourceUrlIsOriginOnly,
            attempt
        ] {
            route(
                sourceUrl,
                sourceSite,
                targetHost,
                resourceType,
                sourceUrlIsOriginOnly,
                attempt + 1
            );
        });
        return;
    }

    m_impl->profile->dismissCrossDomainRequestSource(
        sourceSite,
        targetHost,
        sourceUrl,
        sourceUrlIsOriginOnly
    );
}
