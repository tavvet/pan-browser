#include "VotUserscriptBridge.h"

#include "BrowserPage.h"
#include "BrowserProfile.h"
#include "CrossDomainRequestInterceptor.h"
#include "VotUserscriptStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineUrlRequestInfo>

#include <utility>

namespace {

constexpr qsizetype maximumRequestBodySize = 32 * 1024 * 1024;
constexpr qsizetype maximumResponseBodySize = 32 * 1024 * 1024;
constexpr int maximumRedirects = 5;
constexpr int defaultTimeoutMilliseconds = 30'000;
constexpr int maximumTimeoutMilliseconds = 180'000;
constexpr qsizetype maximumConcurrentRequests = 16;
constexpr qsizetype maximumCachedFrames = 128;
constexpr qsizetype maximumFrameSearchCount = 256;

QString javaScriptValue(const QJsonValue &value)
{
    QJsonArray wrapper;
    wrapper.append(value);
    const QByteArray json = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

bool isBlockedRequestHeader(const QByteArray &name)
{
    const QByteArray lower = name.trimmed().toLower();
    return lower.isEmpty()
        || lower == QByteArrayLiteral("host")
        || lower == QByteArrayLiteral("content-length")
        || lower == QByteArrayLiteral("connection")
        || lower == QByteArrayLiteral("cookie")
        || lower == QByteArrayLiteral("proxy-authorization")
        || lower == QByteArrayLiteral("proxy-connection")
        || lower == QByteArrayLiteral("transfer-encoding")
        || lower == QByteArrayLiteral("upgrade");
}

QString responseHeaders(QNetworkReply *reply)
{
    QStringList lines;
    for (const auto &[name, value] : reply->rawHeaderPairs()) {
        if (name.compare(QByteArrayLiteral("set-cookie"), Qt::CaseInsensitive) == 0)
            continue;
        lines.append(QString::fromLatin1(name + QByteArrayLiteral(": ") + value));
    }
    return lines.join(QStringLiteral("\r\n"));
}

} // namespace

namespace VotNetworkPolicy {

bool mayForwardHeaderAcrossRedirect(
    const QByteArray &name,
    const QUrl &source,
    const QUrl &target
)
{
    const auto effectivePort = [](const QUrl &url) {
        if (url.port() >= 0)
            return url.port();
        return url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
            ? 443
            : 80;
    };
    const bool sameOrigin = source.scheme().compare(
        target.scheme(),
        Qt::CaseInsensitive
    ) == 0
        && source.host().compare(target.host(), Qt::CaseInsensitive) == 0
        && effectivePort(source) == effectivePort(target);
    if (sameOrigin)
        return true;
    const QByteArray lower = name.trimmed().toLower();
    return lower != QByteArrayLiteral("authorization")
        && lower != QByteArrayLiteral("cookie")
        && lower != QByteArrayLiteral("cookie2");
}

} // namespace VotNetworkPolicy

struct VotUserscriptBridge::RequestState final {
    QString id;
    QString frameToken;
    QUrl sourceUrl;
    bool sourceUrlIsOriginOnly = false;
    QByteArray method;
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;
    int timeoutMilliseconds = defaultTimeoutMilliseconds;
    int redirectCount = 0;
    bool aborted = false;
    bool responseTooLarge = false;
    QPointer<QNetworkReply> reply;
};

VotUserscriptBridge::VotUserscriptBridge(
    BrowserPage *page,
    const VotUserscript &userscript,
    VotUserscriptStore *store,
    QObject *parent
)
    : QObject(parent ? parent : page)
    , m_page(page)
    , m_userscript(userscript)
    , m_store(store)
{
    setObjectName(QStringLiteral("panBrowserVotUserscriptBridge"));
    Q_ASSERT(page);
    connect(
        page,
        &BrowserPage::votNetworkMessage,
        this,
        &VotUserscriptBridge::handleMessage
    );
    connect(page, &QWebEnginePage::loadStarted, this, [this] {
        m_frames.clear();
    });
    connect(
        &m_network,
        &QNetworkAccessManager::proxyAuthenticationRequired,
        this,
        [this](const QNetworkProxy &proxy, QAuthenticator *authenticator) {
            emit proxyAuthenticationRequired(
                m_page,
                m_page ? m_page->url() : QUrl(),
                authenticator,
                proxy.hostName()
            );
        }
    );

    if (m_store) {
        connect(
            m_store,
            &VotUserscriptStore::valueChanged,
            this,
            [this](const QString &name, const QJsonValue &value, bool removed) {
                installScript();
                broadcastStorageUpdate(name, value, removed);
            }
        );
    }
    installScript();
}

QString VotUserscriptBridge::scriptName()
{
    return QStringLiteral("PanBrowserVotUserscript");
}

QString VotUserscriptBridge::messagePrefix()
{
    return QStringLiteral("__PANBROWSER_VOT_NETWORK__");
}

qsizetype VotUserscriptBridge::maximumMessageLength()
{
    return 48 * 1024 * 1024;
}

QString VotUserscriptBridge::injectedSource(
    const VotUserscript &userscript,
    const QString &token,
    const QJsonObject &storedValues
)
{
    const QJsonArray matches = QJsonArray::fromStringList(userscript.matchPatterns);
    const QJsonArray excludes = QJsonArray::fromStringList(userscript.excludePatterns);
    return QStringLiteral(R"JS(
(() => {
    const matches = %1;
    const excludes = %2;
    const matchesPattern = (pattern, url) => {
        const separator = pattern.indexOf("://");
        if (separator <= 0)
            return false;
        const schemePattern = pattern.slice(0, separator).toLowerCase();
        if (schemePattern === "*") {
            if (url.protocol !== "http:" && url.protocol !== "https:")
                return false;
        } else if (`${schemePattern}:` !== url.protocol.toLowerCase()) {
            return false;
        }
        const hostStart = separator + 3;
        const pathStart = pattern.indexOf("/", hostStart);
        if (pathStart < 0)
            return false;
        const hostPattern = pattern.slice(hostStart, pathStart).toLowerCase();
        const host = url.hostname.toLowerCase();
        if (hostPattern !== "*") {
            if (hostPattern.startsWith("*.")) {
                const suffix = hostPattern.slice(2);
                if (host !== suffix && !host.endsWith(`.${suffix}`))
                    return false;
            } else if (host !== hostPattern) {
                return false;
            }
        }
        const escaped = pattern.slice(pathStart)
            .replace(/[.+?^${}()|[\]\\]/g, "\\$&")
            .replace(/\*/g, ".*");
        return new RegExp(`^${escaped}$`).test(url.pathname + url.search);
    };
    let pageUrl;
    try {
        pageUrl = new URL(location.href);
    } catch (_) {
        return;
    }
    if (!matches.some(pattern => matchesPattern(pattern, pageUrl))
        || excludes.some(pattern => matchesPattern(pattern, pageUrl))) {
        return;
    }
    if (globalThis.__panBrowserVotUserscriptInstalled)
        return;
    Object.defineProperty(globalThis, "__panBrowserVotUserscriptInstalled", {
        value: true,
        configurable: false,
        enumerable: false
    });

    const token = %3;
    const prefix = %4;
    const frameToken = crypto.randomUUID?.()
        ?? `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`;
    Object.defineProperty(globalThis, "__panBrowserVotFrameToken", {
        value: frameToken,
        configurable: false,
        enumerable: false
    });
    const pending = new Map();
    let requestSequence = 0;
    const storage = new Map(Object.entries(%6));
    const cloneStoredValue = value => {
        const serialized = JSON.stringify(value);
        if (serialized === undefined)
            throw new TypeError("VOT storage values must be JSON-serializable");
        return JSON.parse(serialized);
    };

    const readValue = (name, fallback) => {
        const key = String(name);
        return storage.has(key) ? cloneStoredValue(storage.get(key)) : fallback;
    };
    const writeValue = (name, value) => {
        const key = String(name);
        const storedValue = cloneStoredValue(value);
        storage.set(key, storedValue);
        send({ token, action: "storage-set", key, value: storedValue });
    };
    const deleteValue = name => {
        const key = String(name);
        storage.delete(key);
        send({ token, action: "storage-delete", key });
    };
    const listValues = () => Array.from(storage.keys());
    Object.defineProperty(globalThis, "__panBrowserVotApplyStorageUpdate", {
        value(name, value, removed) {
            const key = String(name);
            if (removed)
                storage.delete(key);
            else
                storage.set(key, cloneStoredValue(value));
        },
        configurable: false,
        enumerable: false
    });
    const addStyle = css => {
        const style = document.createElement("style");
        style.textContent = String(css);
        (document.head || document.documentElement).append(style);
        return style;
    };
    const bodyToBase64 = async body => {
        if (body == null)
            return "";
        let blob;
        if (body instanceof Blob)
            blob = body;
        else if (body instanceof ArrayBuffer)
            blob = new Blob([body]);
        else if (ArrayBuffer.isView(body))
            blob = new Blob([body.buffer.slice(body.byteOffset, body.byteOffset + body.byteLength)]);
        else
            blob = new Blob([String(body)]);
        const bytes = new Uint8Array(await blob.arrayBuffer());
        let binary = "";
        const chunkSize = 32768;
        for (let offset = 0; offset < bytes.length; offset += chunkSize)
            binary += String.fromCharCode(...bytes.subarray(offset, offset + chunkSize));
        return btoa(binary);
    };
    const normalizedHeaders = headers => {
        if (!headers)
            return {};
        try {
            return Object.fromEntries(new Headers(headers).entries());
        } catch (_) {
            return typeof headers === "object" ? { ...headers } : {};
        }
    };
    function send(value) {
        console.info(prefix + JSON.stringify({
            ...value,
            frameToken,
            sourceUrl: location.href
        }));
    }

    globalThis.GM_info = Object.freeze({
        script: Object.freeze({
            name: "[VOT] - Voice Over Translation",
            version: %5
        }),
        scriptHandler: "PanBrowser",
        version: "0.2"
    });
    globalThis.GM_addStyle = addStyle;
    globalThis.GM_getValue = readValue;
    globalThis.GM_setValue = writeValue;
    globalThis.GM_deleteValue = deleteValue;
    globalThis.GM_listValues = listValues;
    globalThis.GM_notification = () => undefined;

    globalThis.GM_xmlhttpRequest = details => {
        const id = `${frameToken}-${Date.now().toString(36)}-${(++requestSequence).toString(36)}`;
        const entry = { details, aborted: false };
        pending.set(id, entry);
        Promise.resolve()
            .then(() => bodyToBase64(details.data))
            .then(body => {
                if (entry.aborted)
                    return;
                send({
                    token,
                    action: "request",
                    id,
                    url: String(details.url || ""),
                    method: String(details.method || "GET"),
                    headers: normalizedHeaders(details.headers),
                    body,
                    timeout: Number(details.timeout) || 0
                });
            })
            .catch(error => {
                pending.delete(id);
                details.onerror?.({ error: String(error) });
            });
        return Object.freeze({
            abort() {
                if (entry.aborted)
                    return;
                entry.aborted = true;
                pending.delete(id);
                send({ token, action: "abort", id });
                details.onabort?.({ status: 0, statusText: "Aborted" });
            }
        });
    };
    globalThis.__panBrowserVotDeliver = message => {
        const entry = pending.get(message.id);
        if (!entry)
            return;
        pending.delete(message.id);
        const details = entry.details;
        if (message.type === "abort") {
            details.onabort?.({ status: 0, statusText: "Aborted" });
            return;
        }
        if (message.type === "error") {
            details.onerror?.({
                status: Number(message.status) || 0,
                statusText: String(message.error || "Network request failed"),
                error: String(message.error || "Network request failed")
            });
            return;
        }
        let bytes;
        try {
            const binary = atob(String(message.body || ""));
            bytes = new Uint8Array(binary.length);
            for (let index = 0; index < binary.length; ++index)
                bytes[index] = binary.charCodeAt(index);
        } catch (error) {
            details.onerror?.({ error: String(error) });
            return;
        }
        details.onload?.({
            status: Number(message.status) || 0,
            statusText: String(message.statusText || ""),
            finalUrl: String(message.finalUrl || details.url || ""),
            responseHeaders: String(message.responseHeaders || ""),
            response: new Blob([bytes])
        });
    };
    globalThis.GM = Object.freeze({
        getValue: async (name, fallback) => readValue(name, fallback),
        getValues: async defaults => Object.fromEntries(
            Object.entries(defaults || {}).map(([name, fallback]) => [name, readValue(name, fallback)])
        ),
        setValue: async (name, value) => writeValue(name, value),
        deleteValue: async name => deleteValue(name),
        listValues: async () => listValues(),
        notification: async () => undefined,
        xmlHttpRequest(details) {
            let handle;
            const promise = new Promise((resolve, reject) => {
                handle = globalThis.GM_xmlhttpRequest({
                    ...details,
                    onload: resolve,
                    onerror: reject,
                    onabort: reject,
                    ontimeout: reject
                });
            });
            promise.abort = () => handle?.abort();
            return promise;
        }
    });

%7
})()
)JS").arg(
        javaScriptValue(matches),
        javaScriptValue(excludes),
        javaScriptValue(token),
        javaScriptValue(messagePrefix()),
        javaScriptValue(userscript.version),
        javaScriptValue(storedValues),
        userscript.sourceCode
    );
}

void VotUserscriptBridge::handleMessage(const QJsonObject &message)
{
    const QString action = message.value(QStringLiteral("action")).toString();
    if (action == QStringLiteral("storage-set")) {
        if (!m_store)
            return;
        QString error;
        if (!m_store->setValue(
                message.value(QStringLiteral("key")).toString(),
                message.value(QStringLiteral("value")),
                &error
            )) {
            qWarning().noquote() << "[PanBrowser VOT storage]" << error;
        }
        return;
    }
    if (action == QStringLiteral("storage-delete")) {
        if (!m_store)
            return;
        QString error;
        if (!m_store->removeValue(
                message.value(QStringLiteral("key")).toString(),
                &error
            )) {
            qWarning().noquote() << "[PanBrowser VOT storage]" << error;
        }
        return;
    }
    const QString id = message.value(QStringLiteral("id")).toString();
    const QString frameToken = message.value(QStringLiteral("frameToken")).toString();
    const QUrl sourceUrl(message.value(QStringLiteral("sourceUrl")).toString());
    if (id.isEmpty() || id.size() > 128)
        return;
    if (frameToken.isEmpty() || frameToken.size() > 128
        || !sourceUrl.isValid()
        || (sourceUrl.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0
            && sourceUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0)) {
        return;
    }
    if (action == QStringLiteral("abort")) {
        abortRequest(id);
        return;
    }
    if (action != QStringLiteral("request") || m_requests.contains(id))
        return;
    if (m_requests.size() >= maximumConcurrentRequests) {
        deliver({
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), QStringLiteral("Too many active VOT requests")},
        }, frameToken);
        return;
    }

    const QUrl url(message.value(QStringLiteral("url")).toString());
    if (!VotUserscriptPackage::isAllowedConnectUrl(m_userscript.connectHosts, url)) {
        deliver({
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), QStringLiteral("Destination is not permitted by VOT @connect")},
        }, frameToken);
        return;
    }
    const CrossDomainResolvedSource policySource = crossDomainRequestSource(
        m_page ? m_page->url() : QUrl(),
        sourceUrl
    );
    auto *profile = qobject_cast<BrowserProfile *>(m_page ? m_page->profile() : nullptr);
    if (policySource.url.isEmpty()
        || !profile
        || !profile->allowCrossDomainRequest(
            policySource.url,
            url,
            static_cast<int>(QWebEngineUrlRequestInfo::ResourceTypeXhr),
            policySource.originOnly
        )) {
        deliver({
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), QStringLiteral("Request blocked by PanBrowser network policy")},
        }, frameToken);
        return;
    }

    const QByteArray method = message.value(QStringLiteral("method"))
        .toString(QStringLiteral("GET")).trimmed().toUpper().toLatin1();
    static const QList<QByteArray> allowedMethods{
        QByteArrayLiteral("GET"),
        QByteArrayLiteral("HEAD"),
        QByteArrayLiteral("POST"),
        QByteArrayLiteral("PUT"),
        QByteArrayLiteral("DELETE"),
        QByteArrayLiteral("PATCH"),
    };
    if (!allowedMethods.contains(method)) {
        deliver({
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), QStringLiteral("HTTP method is not permitted")},
        }, frameToken);
        return;
    }

    const QByteArray body = QByteArray::fromBase64(
        message.value(QStringLiteral("body")).toString().toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors
    );
    if (body.size() > maximumRequestBodySize) {
        deliver({
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), QStringLiteral("Request body is too large")},
        }, frameToken);
        return;
    }

    auto state = QSharedPointer<RequestState>::create();
    state->id = id;
    state->frameToken = frameToken;
    state->sourceUrl = policySource.url;
    state->sourceUrlIsOriginOnly = policySource.originOnly;
    state->method = method;
    state->body = body;
    const int requestedTimeout = message.value(QStringLiteral("timeout")).toInt();
    state->timeoutMilliseconds = qBound(
        1,
        requestedTimeout > 0 ? requestedTimeout : defaultTimeoutMilliseconds,
        maximumTimeoutMilliseconds
    );
    const QJsonObject headers = message.value(QStringLiteral("headers")).toObject();
    if (headers.size() > 64) {
        deliver({
            {QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), QStringLiteral("Too many request headers")},
        }, frameToken);
        return;
    }
    for (auto iterator = headers.constBegin(); iterator != headers.constEnd(); ++iterator) {
        const QByteArray name = iterator.key().trimmed().toLatin1();
        const QByteArray value = iterator.value().toString().toUtf8();
        if (name.size() > 128 || value.size() > 8192 || isBlockedRequestHeader(name))
            continue;
        state->headers.append({name, value});
    }

    m_requests.insert(id, state);
    beginRequest(state, url);
}

void VotUserscriptBridge::beginRequest(
    const QSharedPointer<RequestState> &state,
    const QUrl &url
)
{
    if (!state || state->aborted || !m_requests.contains(state->id))
        return;
    QNetworkRequest request(url);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::ManualRedirectPolicy
    );
    request.setTransferTimeout(state->timeoutMilliseconds);
    for (const auto &[name, value] : std::as_const(state->headers))
        request.setRawHeader(name, value);

    QNetworkReply *reply = m_network.sendCustomRequest(
        request,
        state->method,
        state->body
    );
    state->reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [state, reply] {
        if (reply->bytesAvailable() <= maximumResponseBodySize)
            return;
        state->responseTooLarge = true;
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, state, reply] {
        finishRequest(state, reply);
    });
}

void VotUserscriptBridge::finishRequest(
    const QSharedPointer<RequestState> &state,
    QNetworkReply *reply
)
{
    if (!state || !reply)
        return;
    const bool currentReply = state->reply == reply;
    if (!currentReply) {
        reply->deleteLater();
        return;
    }

    if (state->aborted) {
        m_requests.remove(state->id);
        deliver({
            {QStringLiteral("id"), state->id},
            {QStringLiteral("type"), QStringLiteral("abort")},
        }, state->frameToken);
        reply->deleteLater();
        return;
    }
    if (state->responseTooLarge) {
        m_requests.remove(state->id);
        deliver({
            {QStringLiteral("id"), state->id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("error"), QStringLiteral("Response body is too large")},
        }, state->frameToken);
        reply->deleteLater();
        return;
    }

    const QUrl redirect = reply->attribute(
        QNetworkRequest::RedirectionTargetAttribute
    ).toUrl();
    if (redirect.isValid()) {
        const QUrl target = reply->url().resolved(redirect);
        if (state->redirectCount >= maximumRedirects
            || !VotUserscriptPackage::isAllowedConnectUrl(
                m_userscript.connectHosts,
                target
            )
            || !m_page
            || !qobject_cast<BrowserProfile *>(m_page->profile())
            || !qobject_cast<BrowserProfile *>(m_page->profile())
                    ->allowCrossDomainRequest(
                        state->sourceUrl,
                        target,
                        static_cast<int>(QWebEngineUrlRequestInfo::ResourceTypeXhr),
                        state->sourceUrlIsOriginOnly
                    )) {
            m_requests.remove(state->id);
            deliver({
                {QStringLiteral("id"), state->id},
                {QStringLiteral("type"), QStringLiteral("error")},
                {QStringLiteral("error"), QStringLiteral("Redirect destination is not permitted")},
            }, state->frameToken);
            reply->deleteLater();
            return;
        }
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
        ).toInt();
        const QUrl redirectSource = reply->url();
        state->headers.removeIf([&redirectSource, &target](const auto &header) {
            return !VotNetworkPolicy::mayForwardHeaderAcrossRedirect(
                header.first,
                redirectSource,
                target
            );
        });
        if (status == 303
            || ((status == 301 || status == 302)
                && state->method != QByteArrayLiteral("GET")
                && state->method != QByteArrayLiteral("HEAD"))) {
            state->method = QByteArrayLiteral("GET");
            state->body.clear();
            state->headers.removeIf([](const auto &header) {
                const QByteArray name = header.first.toLower();
                return name == QByteArrayLiteral("content-type")
                    || name == QByteArrayLiteral("content-encoding");
            });
        }
        ++state->redirectCount;
        state->reply.clear();
        reply->deleteLater();
        beginRequest(state, target);
        return;
    }

    const QVariant statusAttribute = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute
    );
    const int status = statusAttribute.toInt();
    if (reply->error() != QNetworkReply::NoError && !statusAttribute.isValid()) {
        m_requests.remove(state->id);
        deliver({
            {QStringLiteral("id"), state->id},
            {QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("status"), status},
            {QStringLiteral("error"), reply->errorString()},
        }, state->frameToken);
        reply->deleteLater();
        return;
    }

    const QByteArray body = reply->readAll();
    m_requests.remove(state->id);
    deliver({
        {QStringLiteral("id"), state->id},
        {QStringLiteral("type"), QStringLiteral("load")},
        {QStringLiteral("status"), status},
        {
            QStringLiteral("statusText"),
            reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()
        },
        {QStringLiteral("finalUrl"), reply->url().toString(QUrl::FullyEncoded)},
        {QStringLiteral("responseHeaders"), responseHeaders(reply)},
        {QStringLiteral("body"), QString::fromLatin1(body.toBase64())},
    }, state->frameToken);
    reply->deleteLater();
}

void VotUserscriptBridge::abortRequest(const QString &id)
{
    const auto state = m_requests.value(id);
    if (!state)
        return;
    state->aborted = true;
    if (state->reply)
        state->reply->abort();
}

void VotUserscriptBridge::installScript()
{
    if (!m_page)
        return;
    const QList<QWebEngineScript> oldScripts = m_page->scripts().find(scriptName());
    for (const QWebEngineScript &script : oldScripts)
        m_page->scripts().remove(script);

    QWebEngineScript script;
    script.setName(scriptName());
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode(injectedSource(
        m_userscript,
        m_page->votNetworkToken(),
        m_store ? m_store->values() : QJsonObject{}
    ));
    m_page->scripts().insert(script);
}

void VotUserscriptBridge::broadcastStorageUpdate(
    const QString &name,
    const QJsonValue &value,
    bool removed
)
{
    if (!m_page)
        return;
    const QString script = QStringLiteral(
        "globalThis.__panBrowserVotApplyStorageUpdate?.(%1, %2, %3);"
    ).arg(
        javaScriptValue(name),
        javaScriptValue(value),
        removed ? QStringLiteral("true") : QStringLiteral("false")
    );
    QList<QWebEngineFrame> pending{m_page->mainFrame()};
    qsizetype visited = 0;
    while (!pending.isEmpty() && visited < maximumFrameSearchCount) {
        QWebEngineFrame frame = pending.takeLast();
        if (!frame.isValid())
            continue;
        ++visited;
        pending.append(frame.children());
        frame.runJavaScript(script, QWebEngineScript::ApplicationWorld);
    }
}

void VotUserscriptBridge::deliver(
    const QJsonObject &message,
    const QString &frameToken
)
{
    if (!m_page || frameToken.isEmpty())
        return;
    const QString payload = QString::fromUtf8(
        QJsonDocument(message).toJson(QJsonDocument::Compact)
    );
    const auto cached = m_frames.constFind(frameToken);
    if (cached != m_frames.cend() && cached->isValid()) {
        deliverToFrame(*cached, frameToken, payload);
        return;
    }

    QList<QWebEngineFrame> pending{m_page->mainFrame()};
    qsizetype searched = 0;
    while (!pending.isEmpty() && searched < maximumFrameSearchCount) {
        QWebEngineFrame frame = pending.takeLast();
        if (!frame.isValid())
            continue;
        ++searched;
        pending.append(frame.children());
        const QString probe = QStringLiteral(
            "globalThis.__panBrowserVotFrameToken === %1"
        ).arg(javaScriptValue(frameToken));
        QPointer<VotUserscriptBridge> self(this);
        frame.runJavaScript(
            probe,
            QWebEngineScript::ApplicationWorld,
            [self, frame, frameToken, payload](const QVariant &matches) mutable {
                if (!self || !matches.toBool() || !frame.isValid())
                    return;
                if (self->m_frames.size() >= maximumCachedFrames
                    && !self->m_frames.contains(frameToken)) {
                    self->m_frames.clear();
                }
                self->m_frames.insert(frameToken, frame);
                self->deliverToFrame(frame, frameToken, payload);
            }
        );
    }
}

void VotUserscriptBridge::deliverToFrame(
    const QWebEngineFrame &targetFrame,
    const QString &frameToken,
    const QString &payload
)
{
    if (!targetFrame.isValid())
        return;
    QWebEngineFrame frame = targetFrame;
    frame.runJavaScript(
        QStringLiteral(R"JS(
if (globalThis.__panBrowserVotFrameToken === %1)
    globalThis.__panBrowserVotDeliver?.(%2);
)JS").arg(javaScriptValue(frameToken), payload),
        QWebEngineScript::ApplicationWorld
    );
}
