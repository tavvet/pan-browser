# PanBrowser

PanBrowser is a small Qt WebEngine viewer with domain-scoped TLS trust rules.
Normal sites use Chromium's ordinary trust configuration. If Chromium rejects a
certificate solely because its CA is unknown, PanBrowser can verify that chain
against CA certificates selected by a matching domain rule. It never installs
those certificates in the macOS Keychain.

The current implementation uses Qt 6.11 or newer and Security.framework on
macOS. The certificate validator has an explicit platform boundary so Windows
and Linux implementations can be added next.

## Run

Requirements: macOS, Xcode command-line tools, Homebrew, and Qt 6.11 or newer.

```sh
brew install qtwebengine cmake ninja
./scripts/build-app.sh
open dist/PanBrowser.app
```

For a development build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The deploy script creates a self-contained, ad-hoc signed application at
`dist/PanBrowser.app`. Qt WebEngine makes the bundle approximately 300 MB.

See [ROADMAP.md](ROADMAP.md) for completed work and planned browser features.

On first launch PanBrowser creates:

```text
~/Library/Application Support/PanBrowser/
├── WebEngine/
├── session.json        # when tab restoration is enabled
├── downloads.json      # download history
├── history.sqlite      # browsing history and autocomplete data
├── search-engines.json # address-bar search configuration
├── rules.json
└── Certificates/
```

Web content uses a dedicated disk-backed profile. Persistent cookies and site
storage are kept under `WebEngine/Profile`. By default, session cookies are
discarded when the application exits; they can be retained from Settings when
the user explicitly chooses to keep sign-ins. The HTTP cache is isolated at
`~/Library/Caches/PanBrowser/WebEngine/`.

Tabs share that profile while keeping their own navigation history, loading
progress, and TLS status. Press <kbd>⌘T</kbd> to open a tab and <kbd>⌘W</kbd> to
close the current tab. User-initiated links that request a new tab are opened
inside the same PanBrowser window. User-initiated requests for a separate
window open a full PanBrowser window with visible URL and TLS status, even when
the site asks for a chromeless dialog. Popup windows share cookies, cache,
downloads, permissions, and trust rules with the primary window, but are not
saved in the restored tab session. Automatic popups remain blocked.

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

Address-bar completion uses only this local history and never contacts a
suggestion service. Match quality is ranked first; equally relevant results are
ordered by the most recent visit, followed by a small preference for manually
entered and frequently visited addresses. Use <kbd>↑</kbd>/<kbd>↓</kbd>, Enter,
Escape, or the mouse to choose a result. When the best matching address begins
with the text already entered, its remaining suffix is shown as a muted inline
completion. Enter opens it; <kbd>Tab</kbd> or <kbd>→</kbd> accepts the suffix
without navigating immediately.

Open **PanBrowser → Settings… → History** to filter visits, view their local
dates, remove selected entries, clear all history, or stop saving new history.
Disabling history also disables address-bar history completion but does not
erase existing records. Clearing browsing history does not remove cookies,
sign-ins, site data, downloads, or trust settings.

Open **PanBrowser → Settings…** or press <kbd>⌘,</kbd> to choose the start page,
restore previous tabs, or retain session-cookie sign-ins. Restored background
tabs are loaded only when selected. Tab URLs and titles are stored atomically in
`session.json`; form contents, scroll position, and navigation history are not
restored.

The **Privacy & Data** section can clear the HTTP cache or all cookies
immediately. A full site-data reset is scheduled for the next launch so the
WebEngine profile can be removed before Chromium opens it. This reset removes
cookies, local storage, IndexedDB, service workers, and cache while preserving
settings, trust rules, certificates, and saved tabs; a scheduled reset can be
cancelled before restarting.

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

Open **PanBrowser → Settings… → Trust Rules** to edit domain trust policy. The
editor imports certificates into the
application data directory, validates the complete configuration, saves it
atomically, and reloads the active policy. The previous file is retained as
`rules.json.backup`.

The **Diagnostics** settings section shows the PanBrowser, Qt WebEngine, and
Chromium versions; operating-system details; configured graphics and sandbox
state; runtime overrides; and profile paths. It can copy a plain-text report
for troubleshooting. Exact active GPU and ANGLE/RHI backend information remains
available through Qt WebEngine diagnostic logging.

For advanced troubleshooting, use **PanBrowser → Show Configuration Folder**
to reveal the files in Finder. After editing `rules.json` manually, select
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
the complete chain and hostname through Security.framework before asking Qt to
continue. A matching hostname is never a blanket “ignore certificate errors”
exception.

Nevertheless, trusting a CA means that CA can issue certificates for every
hostname covered by the rule. Keep rules narrow, obtain certificates from a
verified source, and compare their SHA-256 fingerprints before use.

## Tests

```sh
ctest --test-dir build --output-on-failure
```
