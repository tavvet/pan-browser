#include "VideoElementBridge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace {

const QString videoPopoutMessagePrefix = QStringLiteral(
    "__PANBROWSER_VIDEO_POPOUT_REQUEST__"
);

QString javaScriptString(const QString &value)
{
    const QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(array.mid(1, array.size() - 2));
}

QString overlayScript(
    const QString &token,
    const QString &buttonLabel
)
{
    return QStringLiteral(R"JS(
(() => {
    if (globalThis.__panBrowserVideoPopoutInstalled)
        return;
    globalThis.__panBrowserVideoPopoutInstalled = true;

    const token = %1;
    const buttonLabel = %2;
    const messagePrefix = %3;
    const minimumVideoWidth = 120;
    const minimumVideoHeight = 68;
    let currentVideo = null;

    const host = document.createElement("div");
    host.dataset.panbrowserVideoPopout = "";
    host.style.cssText = [
        "all:initial",
        "position:fixed",
        "display:none",
        "width:36px",
        "height:36px",
        "z-index:2147483647",
        "pointer-events:none"
    ].join(";");
    const shadow = host.attachShadow({ mode: "closed" });
    const button = document.createElement("button");
    button.type = "button";
    button.title = buttonLabel;
    button.setAttribute("aria-label", buttonLabel);
    button.style.cssText = [
        "all:initial",
        "box-sizing:border-box",
        "display:flex",
        "align-items:center",
        "justify-content:center",
        "width:36px",
        "height:36px",
        "border:1px solid rgba(255,255,255,.34)",
        "border-radius:9px",
        "background:rgba(17,24,39,.88)",
        "color:#f8fafc",
        "box-shadow:0 3px 12px rgba(0,0,0,.38)",
        "cursor:pointer",
        "pointer-events:auto",
        "backdrop-filter:blur(8px)",
        "-webkit-backdrop-filter:blur(8px)"
    ].join(";");
    const svgNamespace = "http://www.w3.org/2000/svg";
    const svg = document.createElementNS(svgNamespace, "svg");
    for (const [name, value] of Object.entries({
        width: "19",
        height: "19",
        viewBox: "0 0 24 24",
        fill: "none",
        stroke: "currentColor",
        "stroke-width": "1.9",
        "stroke-linecap": "round",
        "stroke-linejoin": "round",
        "aria-hidden": "true"
    })) {
        svg.setAttribute(name, value);
    }
    const rectangle = document.createElementNS(svgNamespace, "rect");
    for (const [name, value] of Object.entries({
        x: "3", y: "5", width: "15", height: "12", rx: "2"
    })) {
        rectangle.setAttribute(name, value);
    }
    const expandCorner = document.createElementNS(svgNamespace, "path");
    expandCorner.setAttribute("d", "M14 9h7v7");
    const expandArrow = document.createElementNS(svgNamespace, "path");
    expandArrow.setAttribute("d", "m21 9-8 8");
    svg.append(rectangle, expandCorner, expandArrow);
    button.append(svg);
    shadow.append(button);

    const hide = () => {
        currentVideo = null;
        host.style.display = "none";
    };
    const updatePosition = () => {
        if (!currentVideo || !currentVideo.isConnected || document.fullscreenElement) {
            hide();
            return;
        }
        const rect = currentVideo.getBoundingClientRect();
        if (rect.width < minimumVideoWidth || rect.height < minimumVideoHeight
            || rect.bottom <= 0 || rect.right <= 0
            || rect.top >= innerHeight || rect.left >= innerWidth) {
            hide();
            return;
        }
        const inset = 8;
        host.style.left = `${Math.max(inset, Math.min(
            innerWidth - 36 - inset,
            rect.right - 36 - inset
        ))}px`;
        host.style.top = `${Math.max(inset, Math.min(
            innerHeight - 36 - inset,
            rect.top + (rect.height - 36) / 2
        ))}px`;
        host.style.display = "block";
    };
    const videoAtPoint = (x, y) => {
        for (const element of document.elementsFromPoint(x, y)) {
            if (element instanceof HTMLVideoElement)
                return element;
        }
        return null;
    };
    Object.defineProperty(globalThis, "__panBrowserVideoPopoutController", {
        configurable: false,
        enumerable: false,
        value: Object.freeze({
            showFor(video) {
                if (!(video instanceof HTMLVideoElement) || !video.isConnected)
                    return false;
                currentVideo = video;
                updatePosition();
                return host.style.display !== "none";
            }
        })
    });

    document.addEventListener("pointermove", event => {
        const path = typeof event.composedPath === "function" ? event.composedPath() : [];
        const pathVideo = path.find(element => element instanceof HTMLVideoElement) || null;
        const video = pathVideo || videoAtPoint(event.clientX, event.clientY);
        if (video)
            currentVideo = video;
        else if (!path.includes(host))
            hide();
        updatePosition();
    }, true);
    document.addEventListener("scroll", updatePosition, true);
    document.addEventListener("fullscreenchange", updatePosition, true);
    globalThis.addEventListener("resize", updatePosition, true);

    button.addEventListener("click", event => {
        if (!event.isTrusted || !currentVideo || !currentVideo.isConnected
            || typeof currentVideo.requestFullscreen !== "function") {
            return;
        }
        event.preventDefault();
        event.stopPropagation();
        const videoRect = currentVideo.getBoundingClientRect();
        const intrinsicWidth = Number(currentVideo.videoWidth);
        const intrinsicHeight = Number(currentVideo.videoHeight);
        const videoWidth = Number.isFinite(intrinsicWidth) && intrinsicWidth > 0
            ? Math.round(intrinsicWidth)
            : Math.max(1, Math.round(videoRect.width));
        const videoHeight = Number.isFinite(intrinsicHeight) && intrinsicHeight > 0
            ? Math.round(intrinsicHeight)
            : Math.max(1, Math.round(videoRect.height));
        console.info(messagePrefix + JSON.stringify({
            token,
            url: location.origin,
            videoWidth,
            videoHeight
        }));
        try {
            const result = currentVideo.requestFullscreen();
            if (result && typeof result.catch === "function")
                result.catch(() => {});
        } catch (_) {
        }
    }, true);

    const attach = () => {
        const root = document.documentElement;
        if (root && !host.isConnected)
            root.append(host);
    };
    if (document.readyState === "loading")
        document.addEventListener("DOMContentLoaded", attach, { once: true });
    else
        attach();
})()
)JS").arg(
        javaScriptString(token),
        javaScriptString(buttonLabel),
        javaScriptString(videoPopoutMessagePrefix)
    );
}

} // namespace

void VideoElementBridge::install(
    QWebEnginePage *page,
    const QString &token,
    const QString &buttonLabel
)
{
    if (!page || token.isEmpty() || buttonLabel.trimmed().isEmpty())
        return;

    QWebEngineScript script;
    script.setName(QStringLiteral("PanBrowserVideoPopoutOverlay"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode(overlayScript(token, buttonLabel));
    page->scripts().insert(script);
}
