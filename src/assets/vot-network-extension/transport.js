(() => {
  "use strict";

  const responsePrefix = "__PANBROWSER_VOT_CHROMIUM_RESPONSE__";
  const maximumResponseBytes = 32 * 1024 * 1024;
  const controllers = new Map();

  const report = (token, value) => {
    console.info(responsePrefix + JSON.stringify({ ...value, token }));
  };

  const bytesFromBase64 = encoded => {
    if (!encoded)
      return undefined;
    const binary = atob(encoded);
    const bytes = new Uint8Array(binary.length);
    for (let index = 0; index < binary.length; ++index)
      bytes[index] = binary.charCodeAt(index);
    return bytes;
  };

  const base64FromBytes = bytes => {
    let binary = "";
    const chunkSize = 32768;
    for (let offset = 0; offset < bytes.length; offset += chunkSize)
      binary += String.fromCharCode(...bytes.subarray(offset, offset + chunkSize));
    return btoa(binary);
  };

  const readResponseBody = async response => {
    if (!response.body)
      return new Uint8Array();
    const reader = response.body.getReader();
    const chunks = [];
    let total = 0;
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done)
          break;
        total += value.byteLength;
        if (total > maximumResponseBytes) {
          try {
            await reader.cancel();
          } catch (_) {
          }
          throw new Error("Response body is too large");
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
    return bytes;
  };

  const responseHeaders = response => {
    const lines = [];
    response.headers.forEach((value, name) => lines.push(`${name}: ${value}`));
    return lines.join("\r\n");
  };

  const normalizedRedirectMode = value => {
    const mode = String(value || "follow").toLowerCase();
    return mode === "error" || mode === "manual" ? mode : "follow";
  };

  const request = details => {
    if (!details || typeof details !== "object")
      return false;
    const id = String(details.id || "");
    const token = String(details.token || "");
    if (!id || !token || controllers.has(id))
      return false;

    const controller = new AbortController();
    const timeout = Math.max(1, Number(details.timeout) || 30000);
    let timedOut = false;
    const timeoutHandle = setTimeout(() => {
      timedOut = true;
      controller.abort();
    }, timeout);
    controllers.set(id, controller);

    Promise.resolve().then(async () => {
      const headers = new Headers(details.headers || {});
      const method = String(details.method || "GET").toUpperCase();
      const body = method === "GET" || method === "HEAD"
        ? undefined
        : bytesFromBase64(String(details.body || ""));
      const response = await fetch(String(details.url || ""), {
        method,
        headers,
        body,
        redirect: normalizedRedirectMode(details.redirect),
        credentials: "omit",
        cache: "no-store",
        referrerPolicy: "no-referrer",
        signal: controller.signal
      });
      const bytes = await readResponseBody(response);
      report(token, {
        id,
        type: "load",
        status: response.status,
        statusText: response.statusText,
        finalUrl: response.url || String(details.url || ""),
        responseHeaders: responseHeaders(response),
        body: base64FromBytes(bytes)
      });
    }).catch(error => {
      const aborted = controller.signal.aborted;
      report(token, {
        id,
        type: timedOut ? "timeout" : (aborted ? "abort" : "error"),
        error: timedOut
          ? "Request timed out"
          : String(error && error.message ? error.message : error)
      });
    }).finally(() => {
      clearTimeout(timeoutHandle);
      controllers.delete(id);
    });
    return true;
  };

  const abort = id => {
    const controller = controllers.get(String(id || ""));
    if (!controller)
      return false;
    controller.abort();
    return true;
  };

  Object.defineProperty(globalThis, "panBrowserVotTransport", {
    value: Object.freeze({ request, abort }),
    configurable: false,
    enumerable: false,
    writable: false
  });
})();
