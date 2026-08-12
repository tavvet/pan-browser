#pragma once

#include "CrossDomainSettings.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <optional>

class BrowserProfile;
class CrossDomainPrompt;
class QWebEngineView;

struct CrossDomainAffectedView {
    QPointer<QWebEngineView> webView;
    QUrl expectedUrl;
};

struct CrossDomainPromptSource {
    QUrl url;
    bool originOnly = false;
};

[[nodiscard]] QList<qsizetype> crossDomainPromptCandidateIndexes(
    const QUrl &sourceUrl,
    const QString &sourceSite,
    bool sourceUrlIsOriginOnly,
    const QList<QUrl> &candidateUrls
);

class CrossDomainPromptController final : public QObject {
    Q_OBJECT

public:
    explicit CrossDomainPromptController(
        BrowserProfile *profile,
        CrossDomainPrompt *prompt,
        QObject *parent = nullptr
    );
    ~CrossDomainPromptController() override;

    void request(
        QWebEngineView *webView,
        const QList<CrossDomainAffectedView> &affectedViews,
        const QList<CrossDomainPromptSource> &sources,
        const QString &sourceSite,
        const QString &targetHost,
        int resourceType
    );
    void currentViewChanged(QWebEngineView *webView);
    void cancelForView(QWebEngineView *webView);
    void reset();

signals:
    void errorOccurred(const QString &error);
    void requestFinished(const QString &sourceSite, const QString &targetHost);
    void requestReanchorRequested(
        QWebEngineView *webView,
        const QList<CrossDomainAffectedView> &affectedViews,
        const QList<CrossDomainPromptSource> &sources,
        const QString &sourceSite,
        const QString &targetHost,
        int resourceType
    );
    void requestDismissed(
        const QString &sourceSite,
        const QString &targetHost,
        const QList<CrossDomainPromptSource> &sources
    );

private:
    static constexpr qsizetype maximumAffectedViews = 64;

    struct PendingRequest {
        QPointer<QWebEngineView> webView;
        QList<CrossDomainAffectedView> affectedViews;
        QList<CrossDomainPromptSource> sources;
        QString sourceSite;
        QString targetHost;
        int resourceType = 0;
    };

    void resolveCurrent(CrossDomainRuleDecision decision, bool persist);
    void showNext();
    bool removeViewAndChooseAnchor(PendingRequest *request, QWebEngineView *webView);
    void dismiss(const PendingRequest &request);
    void dismissAll();

    BrowserProfile *m_profile = nullptr;
    CrossDomainPrompt *m_prompt = nullptr;
    QPointer<QWebEngineView> m_currentView;
    QList<PendingRequest> m_queue;
    std::optional<PendingRequest> m_active;
};
