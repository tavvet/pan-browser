# PanBrowser

PanBrowser is a small Qt WebEngine browser with domain-scoped TLS trust rules.
Normal sites use Chromium's ordinary trust configuration. If Chromium rejects a
certificate solely because its CA is unknown, PanBrowser can verify that chain
against CA certificates selected by a matching domain rule. It never installs
those certificates in the operating-system trust store.

The current implementation uses Qt 6.11 or newer, Security.framework on macOS,
Windows CryptoAPI on Windows 10 or newer, and OpenSSL 1.1.1 or newer on Linux.
Unsupported platforms retain an explicit fail-closed validator boundary.

The current development baseline is **0.2.0**. PanBrowser is usable for local
testing, but its release archives are not yet a supported public distribution:
macOS builds are ad-hoc signed rather than notarized, and automatic updates are
not implemented.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for component ownership,
startup order, security boundaries, data formats, tests, and maintenance
invariants. See [ROADMAP.md](ROADMAP.md) for completed work and planned browser
features.

Keyboard shortcuts below use macOS notation. Qt maps standard shortcuts to
their platform equivalents, normally <kbd>Ctrl</kbd> instead of
<kbd>Command</kbd> on Windows and Linux.

## Build and package

### macOS

Requirements: Xcode command-line tools, Homebrew, and Qt 6.11 or newer.

```sh
brew install qtwebengine cmake ninja
./scripts/build-app.sh
open dist/PanBrowser.app
```

The script creates a self-contained, ad-hoc signed application at
`dist/PanBrowser.app`. Qt WebEngine makes the bundle approximately 300 MB.

### Windows

Run the packaging script from an x64 Visual Studio 2022 developer shell with
Qt 6.11 MSVC, CMake, and Ninja available. Set `QT_ROOT` when Qt is not already
discoverable:

```powershell
$env:QT_ROOT = "C:\Qt\6.11.0\msvc2022_64"
.\scripts\build-windows.ps1
```

It produces `dist\PanBrowser-windows-x64.zip`. The archive includes the
application, compiler runtime selected by Qt, plugins, `QtWebEngineProcess`,
Chromium resources, and locales.

### Linux

Install Qt WebEngine 6.11 development files, OpenSSL development files, CMake,
Ninja, and a C++20 compiler, then run:

```sh
./scripts/build-linux.sh
```

Set `QT_ROOT=/path/to/Qt/6.11.0/gcc_64` if Qt is outside the normal CMake search
path. The output is `dist/PanBrowser-linux-<architecture>.tar.gz`, containing a
relocatable Qt deployment directory. Build and distribution packages should be
created on the oldest supported Linux distribution because glibc remains a
host compatibility boundary.

The scripts use `build-linux`, `build-windows`, and `dist` by default. Override
Linux paths with `PANBROWSER_BUILD_DIR` and `PANBROWSER_DIST_DIR`, or Windows
paths with the `-BuildDir` and `-DistDir` parameters.

### Development build

For a development build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The Windows build evaluates configured anchors in temporary memory stores. It
does not require administrator rights and does not modify the user or machine
certificate stores.

On Linux, install the Qt WebEngine development package for Qt 6.11 or newer,
CMake, Ninja, a C++20 compiler, and the OpenSSL development package. The build
uses OpenSSL's configured default CA file/directory for `system-plus-custom` and
an in-memory store for configured anchors; it does not modify distro CA files.
Custom recovery validation does not fetch CRLs or OCSP responses: Chromium-known
revocations remain blocked, while otherwise unavailable status is soft-fail.

## Local data and profile

PanBrowser stores its profile below the platform's per-user application-data
directory. The exact application-data and cache paths are shown under
**Settings → Diagnostics**. On macOS the data directory is
`~/Library/Application Support/PanBrowser/`; its layout is:

```text
~/Library/Application Support/PanBrowser/
├── WebEngine/
├── session.json        # when tab restoration is enabled
├── downloads.json      # download history
├── history.sqlite      # browsing history and autocomplete data
├── bookmarks.sqlite    # bookmarks and their autocomplete data
├── web-apps.json       # installed web app metadata and icons
├── search-engines.json # address-bar search configuration
├── dns-settings.json   # secure DNS mode and custom providers
├── proxy-settings.json # browser-wide proxy mode and endpoint (no password)
├── rules.json
└── Certificates/
```

Web content uses a dedicated disk-backed profile. Persistent cookies and site
storage are kept under `WebEngine/Profile`. By default, session cookies are
discarded when the application exits; they can be retained from Settings when
the user explicitly chooses to keep sign-ins. The HTTP cache is isolated at
the platform's per-user cache location (`~/Library/Caches/PanBrowser/WebEngine/`
on macOS).

## Browser behavior

### Tabs and popup windows

Tabs share that profile while keeping their own navigation history, loading
progress, and TLS status. Press <kbd>⌘T</kbd> to open a tab and <kbd>⌘W</kbd> to
close the current tab. User-initiated links that request a new tab are opened
inside the same PanBrowser window. User-initiated requests for a separate
window open a full PanBrowser window with visible URL and TLS status, even when
the site asks for a chromeless dialog. Popup windows share cookies, cache,
downloads, permissions, and trust rules with the primary window, but are not
saved in the restored tab session. Automatic popups remain blocked.

On macOS, normal browser and popup windows place the tab strip in the native
title-bar area while retaining system window controls. Safe-area margins keep
tabs clear of those controls, and the unused part of the strip can drag or
maximize the window without interfering with tab reordering. The equivalent
Windows path is implemented but remains experimental until it has been tested
against Windows 10/11 caption controls, Snap, and mixed-DPI displays. Installed
web apps and platforms without reliable expanded-title-bar support retain a
normal native title bar.

### Installed web apps

HTTPS pages with a same-origin web app manifest can be installed from the
PanBrowser menu. Installed web apps open in focused windows without tabs or an
address field, while retaining compact navigation controls and the
browser-owned TLS status bar. They share the main PanBrowser profile, so
sign-ins, permissions, downloads, and domain trust rules behave consistently.
User-clicked HTTP/HTTPS links outside the manifest scope open in a normal
PanBrowser tab. Automatic out-of-scope navigation is blocked, as are form
submissions whose POST body cannot be transferred safely to a browser tab.
Installed apps are listed under **Settings → Web Apps**, where they can be
opened, removed, or have their system shortcut repaired. On macOS, installation
also creates a small signed launcher application under
`~/Applications/PanBrowser Apps`. Launchers reuse an existing PanBrowser
process through a user-only local command channel, so every app continues to
use the same browser profile. Removing an app also removes its managed
shortcut, while keeping cookies and site data. Offline behavior depends on the
service worker supplied by the website.

The current web app implementation does not yet create Windows Start Menu
shortcuts or Linux `.desktop` files, and it does not implement manifest share
targets, protocol/file handlers, badges, or manifest-defined app shortcuts.

### Address bar, search, history, and bookmarks

The address bar opens explicit HTTP/HTTPS URLs, domains, IP addresses, and
`localhost` directly. Other input is sent to the selected search engine only
after Enter is pressed. Prefix a query with `?` to force a search, or use an
engine keyword such as `@g qt webengine` or `@ddg certificate validation` for a
one-off choice. Dotless intranet hosts must be entered with an explicit
`http://` or `https://` prefix.

The **Search** settings section selects the default engine and manages enabled
engines. DuckDuckGo, Google, Bing, and Brave Search are included by default;
custom engines use an HTTP or HTTPS URL template containing `{searchTerms}`
exactly once. HTTP templates are allowed but expose search queries in transit.
Search suggestions are deliberately not requested. Configuration is saved
atomically in `search-engines.json`, with the previous version retained as
`search-engines.json.backup`.

Successful top-level HTTP and HTTPS visits are stored locally in
`history.sqlite`. Failed loads, external schemes, and tabs loaded only as part
of session restoration are not added. Credentials and URL fragments are
removed before storage; query parameters are retained so pages can be reopened
accurately. History is limited to the 50,000 most recent visits.

Address-bar completion combines local bookmarks and history and never contacts
a suggestion service. Match quality is ranked first; a bookmark wins when two
results are equally relevant, while history results are otherwise ordered by
the most recent visit with a small preference for manually entered and
frequently visited addresses. Duplicate URLs are shown once. Use
<kbd>↑</kbd>/<kbd>↓</kbd>, Enter, Escape, or the mouse to choose a result. When
the best matching address begins with the text already entered, its remaining
suffix is shown as a muted inline completion. Enter opens it; <kbd>Tab</kbd> or
<kbd>→</kbd> accepts the suffix without navigating immediately.

Use the star in the address bar or press <kbd>⌘D</kbd> to add, edit, or remove
the current HTTP or HTTPS page. Open **PanBrowser → Bookmarks…** or press
<kbd>⌥⌘B</kbd> to filter bookmarks, edit their names and URLs, remove selected
items, clear the list, or open an item in the current or a new tab. Bookmark
completion remains available when browsing-history collection is disabled.

Press <kbd>⌘F</kbd> to search within the current page. The find bar reports the
active match and total count; Enter or <kbd>⌘G</kbd> moves forward, while
<kbd>⇧</kbd>+Enter or <kbd>⇧⌘G</kbd> moves backward. Matches wrap at the end of
the page. Escape closes the bar and clears WebEngine's match highlights. When
the bar remains open, switching tabs or finishing a new navigation reruns the
same query on the current page.

Open **PanBrowser → Settings… → History** to filter visits, view their local
dates, remove selected entries, clear all history, or stop saving new history.
Disabling history removes history results from address-bar completion but does
not erase existing records or disable bookmark results. Clearing browsing
history does not remove bookmarks, cookies, sign-ins, site data, downloads, or
trust settings.

### Startup, language, and privacy

Open **PanBrowser → Settings…** or press <kbd>⌘,</kbd> to choose the start page,
restore previous tabs, or retain session-cookie sign-ins. Restored background
tabs are loaded only when selected. Tab URLs and titles are stored atomically in
`session.json`; form contents, scroll position, and navigation history are not
restored.

The General page also offers **System default**, **English**, and **Русский**
for the interface language. With no saved choice, PanBrowser follows the first
supported system UI language and falls back to English. Language changes apply
after restarting the application. Application and standard dialog translations
are embedded in the executable, so release bundles do not depend on a separate
Qt translation package.

The **Privacy & Data** section can clear the HTTP cache or all cookies
immediately. A full site-data reset is scheduled for the next launch so the
WebEngine profile can be removed before Chromium opens it. This reset removes
cookies, local storage, IndexedDB, service workers, and cache while preserving
settings, trust rules, certificates, and saved tabs; a scheduled reset can be
cancelled before restarting.

### DNS and proxy

The **DNS** section uses the operating-system resolver by default. It can
instead configure DNS-over-HTTPS with system-DNS fallback or in strict mode.
AdGuard DNS (default, unfiltered, and family protection), Cloudflare, Quad9,
and Google Public DNS are included; custom providers accept one to four HTTPS
server templates. DNS mode is browser-wide and applies to normal tabs, popup
windows, and installed web apps without changing the operating-system DNS.
Configuration is saved atomically in `dns-settings.json`, with the previous
version retained as `dns-settings.json.backup`.

The **Proxy** section uses the operating-system proxy by default. It can also
disable proxy use or route browser traffic through one manually configured
HTTP or SOCKS5 proxy. The HTTP option covers both HTTP traffic and HTTPS via
CONNECT. Manual settings contain only proxy type, host, port, and an optional
HTTP-proxy username. If HTTP proxy authentication is required, PanBrowser asks
for the password when the proxy is first used and keeps it only for the current
browser session. Chromium does not support SOCKS5 authentication, so SOCKS5
servers must allow unauthenticated connections.
Proxy changes take effect after restarting PanBrowser.

Proxy mode is browser-wide: normal tabs, popup windows, installed web apps, and
downloads share it. A failed manual proxy does not silently fall back to a
direct connection. This feature is not a VPN and does not configure other
applications; protocols that do not use Chromium's HTTP proxy path, including
some WebRTC traffic, are outside its guarantee. PAC and platform-specific
automatic configuration can be inherited in **System proxy** mode, but custom
PAC URLs and per-domain routing are not yet exposed by PanBrowser.
Configuration is saved atomically in `proxy-settings.json`, with the previous
version retained as `proxy-settings.json.backup`.
If an existing proxy configuration cannot be loaded, PanBrowser blocks all
WebEngine network requests instead of falling back to a direct or system
connection. The configuration can then be repaired in Settings and applied by
restarting the browser.

### Downloads, permissions, and external links

Downloads always use a system destination picker and never open files
automatically. The toolbar download button shows active progress and persistent
history with pause, resume, cancel, open, and reveal actions. History stores the
local path and source hostname, but not the complete source URL, and is limited
to the 200 most recent records. Clearing history does not delete downloaded
files.

Camera, microphone, and location requests are shown in a browser-owned prompt
only for the active tab on a secure HTTPS origin. Access is granted for the
current request only and permission decisions are never stored. Requests from
HTTP pages or background tabs are denied, as are notifications and unsupported
sensitive permissions. macOS may show its own system prompt after PanBrowser's
**Allow once** action.

Links that leave the browser, such as `mailto:`, `tel:`, or application deep
links, require an explicit confirmation that displays both the requesting site
and destination. Only direct actions in the active tab can produce this prompt;
scripted requests and background tabs are rejected. Local and executable
schemes such as `file:` and `javascript:` are always blocked. PanBrowser passes
an approved URL directly to the operating system without invoking a shell.

### Trust settings and diagnostics

Open **PanBrowser → Settings… → Trust Rules** to edit domain trust policy. The
editor imports certificates into the application data directory, validates the
complete configuration, saves it
atomically, and reloads the active policy. The previous file is retained as
`rules.json.backup`.

The **Diagnostics** settings section shows the PanBrowser, Qt WebEngine, and
Chromium versions; operating-system details; configured graphics and sandbox
state; active DNS mode and provider; active and configured proxy modes; runtime
overrides; and profile paths. It can copy a plain-text report for
troubleshooting. Custom DNS endpoint URLs are
not included because they may contain account-specific identifiers. Proxy
hostnames and usernames are likewise omitted from the report. Exact
active GPU and ANGLE/RHI backend information remains available through Qt
WebEngine diagnostic logging.

For advanced troubleshooting, use **PanBrowser → Show Configuration Folder**
to reveal the files in the system file manager. After editing `rules.json`
manually, select
**PanBrowser → Reload Trust Rules** or press <kbd>⇧⌘R</kbd>.

## Trust rules

Place DER (`.cer`, `.der`) or PEM (`.pem`) CA certificates in the
`Certificates` directory. Paths in `anchors` are relative to `rules.json`.

```json
{
  "version": 1,
  "startPage": "https://example.com",
  "rules": [
    {
      "name": "Example service",
      "enabled": true,
      "domains": [
        "example-service.ru",
        "*.example-service.ru"
      ],
      "mode": "custom-only",
      "anchors": [
        "Certificates/russian-trusted-root-ca.cer"
      ]
    }
  ]
}
```

For compatibility, version 1 trust files still contain `startPage`. PanBrowser
imports it into the application preferences on first launch; the value selected
in Settings is authoritative afterwards.

Modes:

- `system-only`: use ordinary Chromium/system trust evaluation.
- `system-plus-custom`: accept chains ending at either a system root or one of
  the configured anchors.
- `custom-only`: accept only chains ending at one of the configured anchors.

In the Qt implementation, custom validation runs when Chromium reports an
unknown-CA error. Consequently, `custom-only` is exclusive for the recovery
path, but it cannot reject a certificate that Chromium already accepted through
its system roots. `system-plus-custom` is therefore the most accurate mode for
this MVP; full exclusive pinning of otherwise valid Chromium connections
requires a lower-level network hook.

`*.example.com` matches subdomains such as `www.example.com`, but deliberately
does not match the base domain `example.com`; list the base separately if it is
needed. A wildcard like `*.com` is rejected.

Rules are matched against the host of every TLS authentication challenge,
including subresources. Add API or CDN hostnames explicitly. If configuration
loading fails, PanBrowser loads no custom anchors and reports the error in its
status bar.

## Security boundary

PanBrowser only considers overriding Chromium's `CertificateAuthorityInvalid`
error. All other errors—including hostname mismatch, expiration, revocation,
certificate transparency, and pinning failures—are rejected. It then verifies
the complete chain and hostname through the platform backend—Security.framework
on macOS, CryptoAPI on Windows, or OpenSSL on Linux—before asking Qt to
continue. No backend writes configured anchors to a system trust store. A
matching hostname is never a blanket “ignore certificate errors” exception.

Nevertheless, trusting a CA means that CA can issue certificates for every
hostname covered by the rule. Keep rules narrow, obtain certificates from a
verified source, and compare their SHA-256 fingerprints before use.

## Tests

```sh
ctest --test-dir build --output-on-failure
```
