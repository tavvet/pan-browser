#pragma once

#include "CrossDomainPolicySnapshot.h"

#include <QByteArray>
#include <QHash>
#include <QReadWriteLock>
#include <QSet>
#include <QWebEngineUrlRequestInterceptor>

#include <functional>

struct CrossDomainResolvedSource {
    QUrl url;
    bool originOnly = false;
};

[[nodiscard]] CrossDomainResolvedSource crossDomainRequestSource(
    const QUrl &firstPartyUrl,
    const QUrl &initiator
);

class CrossDomainPendingRequestTracker final {
public:
    static constexpr qsizetype maximumPendingSources = 128;
    static constexpr qsizetype maximumSourcesPerRequest = 16;

    [[nodiscard]] bool record(
        const QString &requestKey,
        const QUrl &sourceUrl,
        QUrl *promptSourceUrl = nullptr,
        bool *sourceUrlIsOriginOnly = nullptr,
        bool forceOriginOnly = false
    );
    [[nodiscard]] bool contains(
        const QString &requestKey,
        const QUrl &sourceUrl,
        bool sourceUrlIsOriginOnly = false
    ) const;
    void removeSource(
        const QString &requestKey,
        const QUrl &sourceUrl,
        bool sourceUrlIsOriginOnly
    );
    void remove(const QString &requestKey);
    void clear();
    [[nodiscard]] qsizetype size() const;

private:
    QHash<QString, QSet<QByteArray>> m_sourcesByRequest;
    qsizetype m_size = 0;
};

class CrossDomainRequestInterceptor final : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

public:
    using VotTransportAuthorizer = std::function<bool(const QUrl &, int)>;

    explicit CrossDomainRequestInterceptor(
        bool blockNetwork,
        const CrossDomainSettings &settings,
        QObject *parent = nullptr
    );

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    static QString votTransportRequestId(
        const QUrl &firstPartyUrl,
        const QString &extensionId
    );
    void setVotTransportExtensionId(const QString &extensionId);
    bool registerVotTransportRequest(
        const QString &requestId,
        VotTransportAuthorizer authorizer
    );
    void unregisterVotTransportRequest(const QString &requestId);
    [[nodiscard]] bool allowRequest(
        const QUrl &sourceUrl,
        const QUrl &targetUrl,
        int resourceType,
        bool sourceUrlIsOriginOnly = false
    );
    void setSettings(const CrossDomainSettings &settings);
    void setSettingsResolvingPendingRequest(
        const CrossDomainSettings &settings,
        const QString &sourceSite,
        const QString &targetHost
    );
    void resolveForSession(
        const QString &sourceSite,
        const QString &targetHost,
        CrossDomainRuleDecision decision
    );
    void dismiss(const QString &sourceSite, const QString &targetHost);
    void dismissSource(
        const QString &sourceSite,
        const QString &targetHost,
        const QUrl &sourceUrl,
        bool sourceUrlIsOriginOnly
    );
    [[nodiscard]] bool isPending(
        const QString &sourceSite,
        const QString &targetHost,
        const QUrl &sourceUrl,
        bool sourceUrlIsOriginOnly
    ) const;

signals:
    void requestBlocked(
        const QUrl &sourceUrl,
        const QString &sourceSite,
        const QString &targetHost,
        int resourceType,
        bool sourceUrlIsOriginOnly
    );

private:
    struct VotTransportRequestAuthorization final {
        VotTransportAuthorizer authorizer;
        qsizetype countedRequestAttempts = 0;
    };

    static QString requestKey(const QString &sourceSite, const QString &targetHost);
    [[nodiscard]] bool interceptVotTransportRequest(
        QWebEngineUrlRequestInfo &info,
        bool *handled
    );

    const bool m_blockNetwork = false;
    mutable QReadWriteLock m_lock;
    CrossDomainPolicySnapshot m_policy;
    QHash<QString, CrossDomainRuleDecision> m_sessionDecisions;
    CrossDomainPendingRequestTracker m_pendingRequests;
    QString m_votTransportExtensionId;
    QHash<QString, VotTransportRequestAuthorization> m_votTransportRequests;
};

[[nodiscard]] QString crossDomainResourceTypeDisplayName(int resourceType);
