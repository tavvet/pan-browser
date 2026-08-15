(() => {
    "use strict";

    const options = globalThis.__panBrowserReaderOptions;
    const report = (action, value) => {
        console.info(options.messagePrefix + JSON.stringify({
            token: options.token,
            action,
            value
        }));
    };

    const existing = globalThis.__panBrowserReader;
    if (existing && existing.active)
        return { ok: true, active: true };

    if (typeof Readability !== "function"
        || !globalThis.DOMPurify
        || !globalThis.DOMPurify.isSupported) {
        return { ok: false, error: "libraries-unavailable" };
    }

    const elementCount = document.getElementsByTagName("*").length;
    if (elementCount > options.maximumElements)
        return { ok: false, error: "document-too-large" };

    let article;
    try {
        article = new Readability(document.cloneNode(true), {
            charThreshold: options.minimumArticleLength,
            maxElemsToParse: options.maximumElements,
            keepClasses: false
        }).parse();
    } catch (_) {
        return { ok: false, error: "parse-failed" };
    }

    if (!article
        || typeof article.content !== "string"
        || typeof article.textContent !== "string"
        || article.textContent.trim().length < options.minimumArticleLength
        || article.textContent.length > options.maximumTextLength) {
        return { ok: false, error: "article-unavailable" };
    }

    let sanitizedContent;
    try {
        sanitizedContent = globalThis.DOMPurify.sanitize(article.content, {
            ALLOWED_TAGS: [
                "a", "abbr", "address", "article", "b", "bdi", "bdo",
                "blockquote", "br", "caption", "cite", "code", "col",
                "colgroup", "dd", "del", "details", "dfn", "div", "dl",
                "dt", "em", "figcaption", "figure", "h1", "h2", "h3",
                "h4", "h5", "h6", "hr", "i", "img", "ins", "kbd", "li",
                "main", "mark", "ol", "p", "pre", "q", "rp", "rt",
                "ruby", "s", "samp", "section", "small", "span", "strong",
                "sub", "summary", "sup", "table", "tbody", "td", "tfoot",
                "th", "thead", "time", "tr", "u", "ul", "var", "wbr"
            ],
            ALLOWED_ATTR: [
                "alt", "cite", "colspan", "datetime", "dir", "height",
                "href", "hreflang", "lang", "loading", "open", "rel",
                "reversed", "rowspan", "scope", "src", "start", "title",
                "width"
            ],
            RETURN_DOM_FRAGMENT: true,
            ALLOW_ARIA_ATTR: false,
            ALLOW_DATA_ATTR: false,
            FORBID_TAGS: [
                "audio", "button", "canvas", "embed", "form", "iframe",
                "input", "object", "option", "script", "select", "source",
                "style", "textarea", "track", "video"
            ],
            FORBID_ATTR: [
                "autofocus", "form", "integrity", "nonce", "srcdoc", "style"
            ]
        });
    } catch (_) {
        return { ok: false, error: "sanitize-failed" };
    }

    if (!(sanitizedContent instanceof DocumentFragment))
        return { ok: false, error: "sanitize-failed" };
    const safeUrl = (raw, purposes) => {
        if (!raw)
            return null;
        if (purposes === "link" && raw.startsWith("#"))
            return raw;
        try {
            const parsed = new URL(raw, document.baseURI);
            if (parsed.protocol === "http:" || parsed.protocol === "https:")
                return parsed.href;
            if (purposes === "link" && parsed.protocol === "mailto:")
                return parsed.href;
            if (purposes === "image"
                && (parsed.protocol === "blob:"
                    || (parsed.protocol === "data:" && /^data:image\//i.test(raw)))) {
                return parsed.href;
            }
        } catch (_) {
        }
        return null;
    };

    for (const anchor of sanitizedContent.querySelectorAll("a[href]")) {
        const href = safeUrl(anchor.getAttribute("href"), "link");
        if (href)
            anchor.setAttribute("href", href);
        else
            anchor.removeAttribute("href");
        anchor.removeAttribute("target");
        anchor.setAttribute("rel", "noopener noreferrer");
    }
    for (const image of sanitizedContent.querySelectorAll("img")) {
        const source = safeUrl(image.getAttribute("src"), "image");
        image.removeAttribute("srcset");
        image.removeAttribute("usemap");
        if (source) {
            image.setAttribute("src", source);
            image.setAttribute("loading", "lazy");
            image.setAttribute("decoding", "async");
        } else {
            image.remove();
        }
    }

    const host = document.createElement("div");
    host.setAttribute("aria-label", options.labels.readerMode);
    host.style.setProperty("all", "initial", "important");
    host.style.setProperty("position", "fixed", "important");
    host.style.setProperty("inset", "0", "important");
    host.style.setProperty("display", "block", "important");
    host.style.setProperty("z-index", "2147483647", "important");
    const shadow = host.attachShadow({ mode: "closed" });

    const style = document.createElement("style");
    style.textContent = options.css;
    shadow.append(style);

    const root = document.createElement("div");
    root.className = "reader";
    root.setAttribute("role", "document");
    root.tabIndex = -1;

    const toolbar = document.createElement("div");
    toolbar.className = "toolbar";
    const controls = document.createElement("div");
    controls.className = "controls";
    controls.setAttribute("role", "toolbar");
    controls.setAttribute("aria-label", options.labels.appearance);

    const makeButton = (text, title, action) => {
        const button = document.createElement("button");
        button.type = "button";
        button.textContent = text;
        button.title = title;
        button.setAttribute("aria-label", title);
        button.addEventListener("click", action);
        controls.append(button);
        return button;
    };

    const theme = document.createElement("select");
    theme.title = options.labels.theme;
    theme.setAttribute("aria-label", options.labels.theme);
    for (const themeOption of options.labels.themes) {
        const item = document.createElement("option");
        item.value = themeOption.value;
        item.textContent = themeOption.label;
        theme.append(item);
    }
    theme.value = options.appearance.theme;
    controls.append(theme);

    const typeface = makeButton("Aa", options.labels.typeface, () => {
        const value = root.dataset.typeface === "serif" ? "sans" : "serif";
        root.dataset.typeface = value;
        report("typeface", value);
    });
    typeface.className = "typeface";
    makeButton("A−", options.labels.smallerText, () => {
        const value = Math.max(
            options.minimumTextSize,
            Number.parseInt(root.style.getPropertyValue("--reader-text-size"), 10) - 1
        );
        root.style.setProperty("--reader-text-size", `${value}px`);
        report("text-size", value);
    });
    makeButton("A+", options.labels.largerText, () => {
        const value = Math.min(
            options.maximumTextSize,
            Number.parseInt(root.style.getPropertyValue("--reader-text-size"), 10) + 1
        );
        root.style.setProperty("--reader-text-size", `${value}px`);
        report("text-size", value);
    });
    makeButton("↤↦", options.labels.narrower, () => {
        const value = Math.max(
            options.minimumContentWidth,
            Number.parseInt(root.style.getPropertyValue("--reader-width"), 10) - 40
        );
        root.style.setProperty("--reader-width", `${value}px`);
        report("content-width", value);
    });
    makeButton("↔", options.labels.wider, () => {
        const value = Math.min(
            options.maximumContentWidth,
            Number.parseInt(root.style.getPropertyValue("--reader-width"), 10) + 40
        );
        root.style.setProperty("--reader-width", `${value}px`);
        report("content-width", value);
    });
    const close = makeButton("×", options.labels.close, () => report("close", null));
    close.className = "close";
    toolbar.append(controls);
    root.append(toolbar);

    const documentElement = document.createElement("main");
    documentElement.className = "document";
    const header = document.createElement("header");
    header.className = "header";
    const title = document.createElement("h1");
    title.className = "title";
    title.textContent = article.title || document.title || options.labels.untitled;
    header.append(title);

    const metadata = [article.byline, article.siteName, article.publishedTime]
        .filter(value => typeof value === "string" && value.trim())
        .map(value => value.trim());
    if (metadata.length) {
        const meta = document.createElement("p");
        meta.className = "meta";
        meta.textContent = metadata.join(" · ");
        header.append(meta);
    }
    documentElement.append(header);

    const content = document.createElement("article");
    content.className = "article";
    if (article.lang)
        content.lang = article.lang;
    if (article.dir === "rtl" || article.dir === "ltr")
        content.dir = article.dir;
    content.append(sanitizedContent);
    documentElement.append(content);
    root.append(documentElement);
    shadow.append(root);

    const html = document.documentElement;
    const body = document.body;
    const previous = {
        htmlOverflow: html.style.getPropertyValue("overflow"),
        htmlOverflowPriority: html.style.getPropertyPriority("overflow"),
        bodyOverflow: body ? body.style.getPropertyValue("overflow") : "",
        bodyOverflowPriority: body ? body.style.getPropertyPriority("overflow") : "",
        focused: document.activeElement,
        scrollX: window.scrollX,
        scrollY: window.scrollY
    };

    const restoreProperty = (element, property, value, priority) => {
        if (!element)
            return;
        if (value)
            element.style.setProperty(property, value, priority);
        else
            element.style.removeProperty(property);
    };

    const keyHandler = event => {
        if (event.key === "Escape" && !event.altKey && !event.ctrlKey && !event.metaKey) {
            event.preventDefault();
            event.stopImmediatePropagation();
            report("close", null);
        }
    };

    const state = {
        active: true,
        host,
        root,
        applyAppearance(appearance) {
            root.dataset.theme = appearance.theme;
            root.dataset.typeface = appearance.typeface;
            root.style.setProperty("--reader-text-size", `${appearance.textSize}px`);
            root.style.setProperty("--reader-width", `${appearance.contentWidth}px`);
            theme.value = appearance.theme;
        },
        destroy() {
            if (!state.active)
                return;
            state.active = false;
            window.removeEventListener("keydown", keyHandler, true);
            host.remove();
            restoreProperty(html, "overflow", previous.htmlOverflow, previous.htmlOverflowPriority);
            restoreProperty(body, "overflow", previous.bodyOverflow, previous.bodyOverflowPriority);
            window.scrollTo(previous.scrollX, previous.scrollY);
            if (previous.focused && previous.focused.isConnected) {
                try {
                    previous.focused.focus({ preventScroll: true });
                } catch (_) {
                }
            }
            if (globalThis.__panBrowserReader === state)
                delete globalThis.__panBrowserReader;
        }
    };
    globalThis.__panBrowserReader = state;
    state.applyAppearance(options.appearance);

    theme.addEventListener("change", () => {
        root.dataset.theme = theme.value;
        report("theme", theme.value);
    });
    content.addEventListener("click", event => {
        const target = event.target instanceof Element ? event.target.closest("a[href]") : null;
        if (target)
            report("close", null);
    });
    window.addEventListener("keydown", keyHandler, true);
    html.style.setProperty("overflow", "hidden", "important");
    if (body)
        body.style.setProperty("overflow", "hidden", "important");
    html.append(host);
    root.focus({ preventScroll: true });

    return { ok: true, active: true };
})()
