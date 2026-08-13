#include "BrowserPage.h"

#include "ExternalNavigationPolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QWebEngineScript>
#include <QWebEngineSettings>

namespace {

const QString fetchMessagePrefix = QStringLiteral("__PANBROWSER_WEB_APP_FETCH__");
const QString videoPopoutMessagePrefix = QStringLiteral(
    "__PANBROWSER_VIDEO_POPOUT_REQUEST__"
);
constexpr qsizetype maximumVideoPopoutMessageLength = 2048;
constexpr int maximumVideoDimension = 32768;

QString javaScriptString(const QString &value)
{
    const QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(array.mid(1, array.size() - 2));
}

} // namespace

BrowserPage::BrowserPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
    , m_videoPopoutToken(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    settings()->setUnknownUrlSchemePolicy(
        QWebEngineSettings::UnknownUrlSchemePolicy::AllowUnknownUrlSchemesFromUserInteraction
    );
}

QString BrowserPage::videoPopoutToken() const
{
    return m_videoPopoutToken;
}

void BrowserPage::setWebApp(const WebApp &app)
{
    m_webApp = app;
}

void BrowserPage::fetchWebAppManifest(
    const QString &requestId,
    const QUrl &manifestUrl,
    qsizetype maximumBytes
)
{
    const QString script = QStringLiteral(R"JS(
(() => {
    const requestId = %1;
    const report = value => console.info(%2 + JSON.stringify(value));
    const requests = globalThis.__panBrowserWebAppFetches ||= new Map();
    const controller = new AbortController();
    requests.set(requestId, controller);
    fetch(%3, {
        credentials: "include",
        cache: "no-cache",
        signal: controller.signal
    })
        .then(async response => {
            if (!response.ok)
                throw new Error("HTTP " + response.status);
            const declaredLength = Number(response.headers.get("content-length"));
            if (Number.isFinite(declaredLength) && declaredLength > %4)
                throw new Error("resource is too large");
            if (!response.body)
                throw new Error("response body is unavailable");

            const reader = response.body.getReader();
            const chunks = [];
            let total = 0;
            try {
                while (true) {
                    const { done, value } = await reader.read();
                    if (done)
                        break;
                    total += value.byteLength;
                    if (total > %4) {
                        try {
                            await reader.cancel();
                        } catch (_) {
                        }
                        throw new Error("resource is too large");
                    }
                    chunks.push(value);
                }
            } finally {
                reader.releaseLock();
            }

            const bytes = new Uint8Array(total);
            let destination = 0;
            for (const chunk of chunks) {
                bytes.set(chunk, destination);
                destination += chunk.byteLength;
            }
            let binary = "";
            const chunkSize = 32768;
            for (let offset = 0; offset < bytes.length; offset += chunkSize)
                binary += String.fromCharCode(...bytes.subarray(offset, offset + chunkSize));
            report({ id: requestId, data: btoa(binary) });
        })
        .catch(error => report({
            id: requestId,
            error: String(error && error.message ? error.message : error).slice(0, 300)
        }))
        .finally(() => {
            if (requests.get(requestId) === controller)
                requests.delete(requestId);
        });
    return true;
})()
)JS").arg(
        javaScriptString(requestId),
        javaScriptString(fetchMessagePrefix),
        javaScriptString(manifestUrl.toString(QUrl::FullyEncoded)),
        QString::number(maximumBytes)
    );
    runJavaScript(script, QWebEngineScript::ApplicationWorld);
}

void BrowserPage::cancelWebAppManifestFetch(const QString &requestId)
{
    const QString script = QStringLiteral(R"JS(
(() => {
    const requests = globalThis.__panBrowserWebAppFetches;
    const requestId = %1;
    const controller = requests && requests.get(requestId);
    if (!controller)
        return false;
    requests.delete(requestId);
    controller.abort();
    return true;
})()
)JS").arg(javaScriptString(requestId));
    runJavaScript(script, QWebEngineScript::ApplicationWorld);
}

bool BrowserPage::acceptNavigationRequest(
    const QUrl &url,
    NavigationType type,
    bool isMainFrame
)
{
    switch (externalNavigationDisposition(url, isMainFrame)) {
    case ExternalNavigationDisposition::Browse:
        if (isMainFrame
            && m_webApp
            && (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
                || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
            && !WebAppStore::containsUrl(*m_webApp, url)) {
            emit outOfScopeNavigationRequested(url, static_cast<int>(type));
            return false;
        }
        if (isMainFrame)
            emit mainFrameNavigationRequested(url, static_cast<int>(type));
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    case ExternalNavigationDisposition::Prompt:
        if (type != NavigationTypeLinkClicked
            && type != NavigationTypeTyped
            && type != NavigationTypeFormSubmitted) {
            return false;
        }
        emit externalUrlRequested(url);
        return false;
    case ExternalNavigationDisposition::Block:
        return false;
    }
    return false;
}

void BrowserPage::javaScriptConsoleMessage(
    JavaScriptConsoleMessageLevel level,
    const QString &message,
    int lineNumber,
    const QString &sourceId
)
{
    if (message.startsWith(videoPopoutMessagePrefix)) {
        if (message.size() > maximumVideoPopoutMessageLength)
            return;
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            message.mid(videoPopoutMessagePrefix.size()).toUtf8(),
            &parseError
        );
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return;
        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("token")).toString() != m_videoPopoutToken)
            return;
        const QUrl frameUrl(object.value(QStringLiteral("url")).toString());
        const QString scheme = frameUrl.scheme().toLower();
        if (!frameUrl.isValid()
            || frameUrl.host().isEmpty()
            || !frameUrl.userInfo().isEmpty()
            || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
            return;
        }
        const int videoWidth = object.value(QStringLiteral("videoWidth")).toInt();
        const int videoHeight = object.value(QStringLiteral("videoHeight")).toInt();
        const QSize videoSize(
            qBound(1, videoWidth, maximumVideoDimension),
            qBound(1, videoHeight, maximumVideoDimension)
        );
        emit videoPopoutRequested(
            frameUrl,
            videoWidth > 0 && videoHeight > 0 ? videoSize : QSize(16, 9)
        );
        return;
    }

    if (!message.startsWith(fetchMessagePrefix)) {
        QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceId);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        message.mid(fetchMessagePrefix.size()).toUtf8(),
        &parseError
    );
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;
    const QJsonObject object = document.object();
    const QString requestId = object.value(QStringLiteral("id")).toString();
    if (requestId.isEmpty())
        return;
    const QString encoded = object.value(QStringLiteral("data")).toString();
    const QByteArray contents = QByteArray::fromBase64(
        encoded.toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors
    );
    QString error = object.value(QStringLiteral("error")).toString();
    if (error.isEmpty() && !encoded.isEmpty() && contents.isEmpty())
        error = QStringLiteral("invalid response encoding");
    emit webAppManifestFetched(requestId, contents, error);
}
