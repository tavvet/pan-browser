#include "VotChromiumRequestSession.h"

#include "BrowserPage.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include <QUrlQuery>
#include <QVariant>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>

#include <functional>
#include <utility>

namespace {

constexpr qsizetype maximumTransportMessageLength = 48 * 1024 * 1024;

QString transportText(const char *source)
{
    return QCoreApplication::translate(
        "VotChromiumNetworkTransport",
        source
    );
}

QString javaScriptValue(const QJsonValue &value)
{
    QJsonArray wrapper;
    wrapper.append(value);
    const QByteArray json = QJsonDocument(wrapper).toJson(
        QJsonDocument::Compact
    );
    return QString::fromUtf8(json.mid(1, json.size() - 2));
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
                VotChromiumRequestSession::responseMessagePrefix()
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

VotChromiumRequestSession::VotChromiumRequestSession(
    VotChromiumRequest request,
    QObject *parent
)
    : QObject(parent)
    , m_request(std::move(request))
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this] {
        if (m_started && m_page) {
            m_page->runJavaScript(
                QStringLiteral("globalThis.panBrowserVotTransport?.abort(%1)")
                    .arg(javaScriptValue(m_request.id)),
                QWebEngineScript::MainWorld
            );
        }
        finish({
            {QStringLiteral("id"), m_request.id},
            {QStringLiteral("type"), QStringLiteral("timeout")},
            {
                QStringLiteral("error"),
                transportText(QT_TRANSLATE_NOOP(
                    "VotChromiumNetworkTransport",
                    "Request timed out"
                ))
            },
        });
    });
    m_timeoutTimer->start(qMax(1, m_request.timeoutMilliseconds));
}

VotChromiumRequestSession::~VotChromiumRequestSession()
{
    if (auto *page = static_cast<TransportPage *>(m_page.data()))
        page->consoleHandler = {};
}

QString VotChromiumRequestSession::responseMessagePrefix()
{
    return QStringLiteral("__PANBROWSER_VOT_CHROMIUM_RESPONSE__");
}

QString VotChromiumRequestSession::id() const
{
    return m_request.id;
}

bool VotChromiumRequestSession::isStarted() const
{
    return m_started;
}

void VotChromiumRequestSession::start(
    QWebEngineProfile *profile,
    const QString &extensionId,
    const QString &token
)
{
    if (m_finished || m_page || !profile || extensionId.isEmpty())
        return;

    m_token = token;
    auto *page = new TransportPage(profile, this);
    m_page = page;
    const QPointer<VotChromiumRequestSession> self(this);
    page->consoleHandler = [self](const QString &message) {
        if (self)
            self->handleConsoleMessage(message);
    };
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
        [this, guardedPage = QPointer<QWebEnginePage>(page)](
            const QUrl &requestUrl,
            QAuthenticator *authenticator,
            const QString &proxyHost
        ) {
            if (m_finished || !guardedPage || guardedPage != m_page) {
                if (authenticator)
                    *authenticator = QAuthenticator();
                return;
            }
            bool handled = false;
            emit proxyAuthenticationRequired(
                m_request.sourcePage.data(),
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
        [this, guardedPage = QPointer<QWebEnginePage>(page)](
            QWebEnginePage::RenderProcessTerminationStatus,
            int
        ) {
            if (!guardedPage || guardedPage != m_page)
                return;
            fail(transportText(QT_TRANSLATE_NOOP(
                "VotChromiumNetworkTransport",
                "The Chromium VOT request process stopped unexpectedly"
            )));
        }
    );
    connect(
        page,
        &QWebEnginePage::loadFinished,
        this,
        [this, guardedPage = QPointer<QWebEnginePage>(page)](bool ok) {
            if (!guardedPage || guardedPage != m_page)
                return;
            if (!ok) {
                fail(transportText(QT_TRANSLATE_NOOP(
                    "VotChromiumNetworkTransport",
                    "Cannot open the Chromium page for a VOT request"
                )));
                return;
            }
            startLoadedRequest(guardedPage);
        }
    );

    QUrl transportUrl(QStringLiteral(
        "chrome-extension://%1/transport.html"
    ).arg(extensionId));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("request"), m_request.id);
    transportUrl.setQuery(query);
    page->setUrl(transportUrl);
}

void VotChromiumRequestSession::abort()
{
    if (m_finished)
        return;
    if (m_started && m_page) {
        m_page->runJavaScript(
            QStringLiteral("globalThis.panBrowserVotTransport?.abort(%1)")
                .arg(javaScriptValue(m_request.id)),
            QWebEngineScript::MainWorld
        );
    }
    finish({
        {QStringLiteral("id"), m_request.id},
        {QStringLiteral("type"), QStringLiteral("abort")},
    });
}

void VotChromiumRequestSession::fail(const QString &error)
{
    if (m_finished)
        return;
    finish({
        {QStringLiteral("id"), m_request.id},
        {QStringLiteral("type"), QStringLiteral("error")},
        {QStringLiteral("error"), error},
    });
}

void VotChromiumRequestSession::startLoadedRequest(QWebEnginePage *page)
{
    if (m_finished || !page || page != m_page)
        return;

    m_started = true;
    QJsonObject payload{
        {QStringLiteral("id"), m_request.id},
        {QStringLiteral("token"), m_token},
        {QStringLiteral("url"), m_request.url.toString(QUrl::FullyEncoded)},
        {QStringLiteral("method"), QString::fromLatin1(m_request.method)},
        {QStringLiteral("headers"), m_request.headers},
        {
            QStringLiteral("body"),
            QString::fromLatin1(m_request.body.toBase64())
        },
        {QStringLiteral("redirect"), m_request.redirectMode},
        {QStringLiteral("timeout"), m_request.timeoutMilliseconds},
    };
    page->runJavaScript(
        QStringLiteral("globalThis.panBrowserVotTransport?.request(%1)")
            .arg(javaScriptValue(payload)),
        QWebEngineScript::MainWorld,
        [self = QPointer<VotChromiumRequestSession>(this),
         guardedPage = QPointer<QWebEnginePage>(page)](const QVariant &started) {
            if (!self
                || !guardedPage
                || guardedPage != self->m_page
                || self->m_finished
                || started.toBool()) {
                return;
            }
            self->fail(transportText(QT_TRANSLATE_NOOP(
                "VotChromiumNetworkTransport",
                "The Chromium VOT request could not be started"
            )));
        }
    );
}

void VotChromiumRequestSession::handleConsoleMessage(
    const QString &message
)
{
    const QString prefix = responseMessagePrefix();
    if (m_finished
        || !message.startsWith(prefix)
        || message.size() > maximumTransportMessageLength) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        message.mid(prefix.size()).toUtf8(),
        &parseError
    );
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

    QJsonObject response = document.object();
    if (response.take(QStringLiteral("token")).toString() != m_token
        || response.value(QStringLiteral("id")).toString() != m_request.id) {
        return;
    }
    finish(std::move(response));
}

void VotChromiumRequestSession::finish(QJsonObject response)
{
    if (m_finished)
        return;
    m_finished = true;
    if (m_timeoutTimer)
        m_timeoutTimer->stop();
    if (auto *page = static_cast<TransportPage *>(m_page.data())) {
        page->consoleHandler = {};
        page->deleteLater();
    }
    m_page.clear();
    emit responseReady(response);
}
