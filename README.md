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
inside the same PanBrowser window; automatic popups remain blocked.

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

Open **PanBrowser → Trust Rules…** to jump directly to the Trust Rules section
of the same settings window. The editor imports certificates into the
application data directory, validates the complete configuration, saves it
atomically, and reloads the active policy. The previous file is retained as
`rules.json.backup`.

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
