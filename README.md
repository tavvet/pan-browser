<p align="center">
  <img src="src/assets/app-icon.png" width="96" height="96" alt="PanBrowser icon">
</p>

<h1 align="center">PanBrowser</h1>

<p align="center">
  A focused Qt WebEngine browser for domain-scoped TLS trust rules.
</p>

PanBrowser is an experimental desktop browser for opening services that use a
private or otherwise non-standard certificate authority without installing
that CA in the operating-system trust store. Ordinary websites continue to use
Chromium's normal trust configuration. A custom CA is considered only for a
hostname covered by an explicit PanBrowser rule.

PanBrowser started as a small trust-policy viewer. It now includes the browser
features needed for practical daily use while keeping its original security
boundary visible and auditable.

> [!WARNING]
> PanBrowser is not a security-maintained replacement for a general-purpose
> browser and has no automatic update mechanism. The latest published milestone
> is `v0.1.0`; its macOS 26+ ARM64 binary is Developer ID-signed but not
> notarized, so Gatekeeper requires an explicit user override. The `main` branch
> targets `0.2.0` and may contain unfinished changes. Windows and Linux remain
> build-from-source platforms.

## Why PanBrowser?

Installing an additional root CA system-wide expands trust for every
application that uses the system store. PanBrowser provides a narrower model:

- configured certificates stay inside the PanBrowser data directory;
- trust rules are scoped to exact domains and explicit wildcard domains;
- custom validation runs only when Chromium reports an unknown CA;
- the complete chain and hostname are checked by the native platform backend;
- unrelated TLS failures remain blocked.

This makes PanBrowser useful for private services, corporate infrastructure,
test environments, and other deliberately scoped trust configurations.

## Highlights

- Domain-scoped custom CA rules with `system-only`, `system-plus-custom`, and
  `custom-only` modes.
- Native certificate validation through Security.framework on macOS, CryptoAPI
  on Windows, and OpenSSL on Linux.
- A dedicated WebEngine profile for Chromium cookies, site storage, and cache,
  plus PanBrowser-owned history, bookmark, download, and installed-app stores.
- Movable and persistent pinned tabs, animated new-tab opening, session
  restoration, popup handling, native page fullscreen, frameless and draggable
  always-on-top video pop-out with aspect-ratio-preserving resize, find in
  page, per-site page zoom, local address completion, configurable search
  engines, a download manager, and a reversible reader mode with persistent
  typography controls.
- Browser-owned prompts for camera, microphone, location, external schemes,
  HTTP Basic authentication, and HTTP proxy authentication.
- Native password-manager integration for explicitly saved website and proxy
  credentials, with a Settings page that lists usernames and metadata without
  revealing passwords and supports confirmed deletion.
- Browser-wide System/Direct/HTTP/SOCKS5 proxy modes and configurable
  DNS-over-HTTPS providers.
- An opt-in experimental firewall for third-party page connections, with
  global exceptions and per-site session or persistent decisions.
- Opt-in integration with the independently maintained VOT userscript for
  voice-over video translation. PanBrowser accepts only its pinned, verified
  release and does not bundle or update the userscript automatically.
- Manifest-based web app installation, including lightweight application
  shortcuts on macOS.
- English and Russian interfaces with system-language detection.
- Opt-in developer tools with Inspect Element and familiar keyboard shortcuts;
  disabled by default.
- Diagnostics for the application, Chromium, graphics, sandbox, profile,
  DNS, and proxy state.

See [CHANGELOG.md](CHANGELOG.md) for milestone notes and
[ROADMAP.md](ROADMAP.md) for planned work.

Page zoom is available from the application menu and through `Command + Plus`,
`Command + Minus`, and `Command + 0` on macOS (`Ctrl` on Windows and Linux).
Holding the same modifier while scrolling over the page also changes zoom.
PanBrowser remembers the selected level separately for each HTTP(S) origin.

Tabs support the familiar browser shortcuts: `Command/Ctrl + T` opens a tab,
`Command/Ctrl + W` closes it, and `Command/Ctrl + Shift + T` restores the most
recently closed tab. `Ctrl + Tab` and `Ctrl + Shift + Tab` cycle through tabs;
`Command + Option + Left/Right`, `Command + Shift + [`, and
`Command + Shift + ]` are also available on macOS, while
`Ctrl + Page Up/Page Down` is available on Windows and Linux.
`Command/Ctrl + 1…8` selects a numbered tab and `Command/Ctrl + 9` selects the
last tab.

On article-like HTTP(S) pages, the book button in the address bar opens Reader
Mode. `F9` toggles it without replacing the source page, changing its URL, or
adding a history entry. The reader toolbar controls the color theme, serif or
sans-serif typeface, text size, and content width; those appearance choices are
kept between launches. Reader Mode is a presentation feature, not a privacy or
security boundary: the original page remains loaded underneath it.

## Trust model

When Chromium encounters a certificate error, PanBrowser follows this flow:

1. Errors other than `CertificateAuthorityInvalid` are rejected.
2. PanBrowser looks for a rule matching the challenged hostname.
3. If the rule permits custom trust, PanBrowser validates the complete chain
   and hostname with the platform-native validator and the configured anchors.
4. Qt WebEngine is allowed to continue only when that validation succeeds.

| Mode | Behavior |
| --- | --- |
| `system-only` | Use ordinary Chromium and operating-system trust. |
| `system-plus-custom` | Allow recovery through either system roots or the configured anchors. |
| `custom-only` | Allow the unknown-CA recovery path only through the configured anchors. |

`custom-only` cannot reject a connection that Chromium has already accepted
through its normal roots; Qt exposes the application hook only after Chromium
reports a certificate error. It is therefore not certificate pinning.

Trusting a CA still allows that CA to issue certificates for every hostname
covered by the rule. Keep rules narrow, obtain certificates from a verified
source, and compare their SHA-256 fingerprints before use.

## Platform status

PanBrowser requires Qt 6.11 or newer and a C++20 compiler.

| Platform | Validation backend | Current verification |
| --- | --- | --- |
| macOS 26+ | Security.framework | ARM64 build, tests, packaged application, and GUI smoke test |
| Windows 10+ | CryptoAPI | Windows x64 build, tests, package, and GUI smoke test on Windows 11 ARM64 under x64 emulation |
| Linux | OpenSSL 1.1.1+ | Linux ARM64 build, tests, package, and GUI smoke test on Ubuntu ARM64 |

The Windows result is not a native ARM64 build. The Linux result does not yet
cover x86-64, X11, and Wayland as separate test environments. Unsupported
platforms use an explicit fail-closed validator boundary.

## Build from source

The platform scripts configure a release build, run the automated tests, and
create a package under `dist/`. The macOS and Windows packages deploy their
application runtimes. The Linux archive deploys Qt and Chromium but intentionally
uses compatible system copies of glibc, OpenSSL, libsecret, and standard desktop
libraries.

### macOS

Requirements: macOS 26 or newer, Xcode command-line tools, Homebrew, CMake,
Ninja, and Qt WebEngine 6.11 or newer.

```sh
brew install qtwebengine cmake ninja
./scripts/build-app.sh
open dist/PanBrowser.app
```

Set `QT_ROOT` when using a standalone Qt installation. The resulting
application is ad-hoc signed for local testing; it is not notarized.

### Windows

Use an x64 Visual Studio 2022 toolchain, CMake, and Qt 6.11 built for MSVC.
From a developer shell:

```powershell
$env:QT_ROOT = "C:\Qt\6.11.1\msvc2022_64"
.\scripts\build-windows.ps1
```

From a regular PowerShell session, let the Visual Studio generator initialize
MSVC:

```powershell
.\scripts\build-windows.ps1 -Generator "Visual Studio 17 2022"
```

The repository can download its pinned minimal Qt SDK from Qt's official
archives. This requires 7-Zip and avoids the interactive Qt installer:

```powershell
.\scripts\install-qt-windows.ps1 `
  -Destination "$PWD\build-tools\Qt\6.11.1\msvc2022_64"

.\scripts\build-windows.ps1 `
  -QtRoot "$PWD\build-tools\Qt\6.11.1\msvc2022_64" `
  -Generator "Visual Studio 17 2022" `
  -Architecture x64
```

The output is `dist\PanBrowser-windows-x64.zip`. Add `-OptimizeBundle` to keep
only the supported English and Russian Chromium locale packs. The manual
[Windows bundle workflow](.github/workflows/windows-bundle.yml) performs the
same build and uploads temporary CI artifacts without publishing a release.

### Linux

Install Qt WebEngine 6.11 development files, OpenSSL development files,
libsecret 0.19 or newer development files, `pkg-config`, CMake, Ninja, and a
C++20 compiler, then run. On Debian and Ubuntu, the non-Qt packages include
`libssl-dev`, `libsecret-1-dev`, and `pkg-config`:

```sh
./scripts/build-linux.sh
```

If Qt is not in CMake's default search path:

```sh
QT_ROOT=/path/to/Qt/6.11.1/gcc_64 ./scripts/build-linux.sh
```

The output is `dist/PanBrowser-linux-<architecture>.tar.gz`. Linux packages
should be built on the oldest distribution they intend to support because
glibc remains a host compatibility boundary. The target system must provide a
compatible OpenSSL runtime and libsecret (`libsecret-1-0` on Debian and Ubuntu),
plus the ordinary desktop libraries reported by `ldd`. The build script rejects
an archive when any of those dependencies is unresolved on the build host; a
release candidate still needs a clean-machine smoke test on every supported
distribution.

To exercise the real Secret Service backend, run the build from an interactive
desktop session with an available keyring:

```sh
PANBROWSER_RUN_CREDENTIAL_STORE_TESTS=1 ./scripts/build-linux.sh
```

The opt-in test stores, updates, lists, and removes one uniquely named temporary
credential and may ask the desktop to unlock the default collection.

### Development build and tests

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Configure trust rules

Open **Settings → Trust Rules** to import CA certificates and manage rules. The
editor validates the complete configuration, writes it atomically, and reloads
the active policy. No administrator privileges are required, and PanBrowser
does not modify the user or machine certificate stores.

Rules are stored in `rules.json`. Imported DER (`.cer`, `.der`) and PEM
(`.pem`) certificates are stored in the adjacent `Certificates` directory.

```json
{
  "version": 1,
  "startPage": "https://example.com",
  "rules": [
    {
      "name": "Example service",
      "enabled": true,
      "domains": [
        "example-service.test",
        "*.example-service.test"
      ],
      "mode": "system-plus-custom",
      "anchors": [
        "Certificates/example-root-ca.cer"
      ]
    }
  ]
}
```

`*.example-service.test` matches subdomains such as
`www.example-service.test`, but not the base domain `example-service.test`;
list the base separately when both are required. Single-label wildcard bases
such as `*.com` are rejected. PanBrowser does not implement Public Suffix List
validation for trust-rule wildcards. API, CDN, and other subresource hosts need
their own matching entries.

Version 1 files retain the legacy `startPage` field for compatibility. After
its first import, the start page selected in Settings is authoritative.

## Restrict third-party connections

Open **Settings → Site Connections** to enable the experimental connection
firewall. It is disabled by default. When enabled, HTTP(S), WebSocket, frame,
script, media, worker, and data requests remain allowed inside the current
registrable site. An unknown request to another host is blocked before reaching
Chromium's network stack and shown in a browser-owned prompt.

Top-level navigation remains allowed: opening another site establishes that
site as the new first party. A prompt can allow or block the exact target host
for the current PanBrowser session, or save the exact-host decision for the
source site. Source-specific rules can also be added manually in Settings so a
bundled recommendation false positive does not require a browser-wide
exception. Global
allow entries let every site connect to a listed host and its subdomains.
Global block entries reject listed hosts for every site without prompting and
take priority over source-specific and global allow rules. Conflicting global
entries are rejected when Settings is saved. Allowing a previously blocked
request reloads the page because Qt WebEngine cannot pause an intercepted
request while PanBrowser waits for UI input.

When Qt WebEngine cannot attribute a web request to an HTTP(S) first party or
initiator, PanBrowser blocks that request without prompting while this
protection is enabled. This fail-closed behavior prevents `blob:`, `data:`, and
other opaque page URLs from bypassing the configured connection policy.

PanBrowser also ships two optional, separately managed recommendation sets: a
small recommended tracker block set and a compatibility-oriented public CDN
allow set. Neither is mixed into personal lists, and existing configurations
are not changed automatically. The CDN set is intentionally opt-in because
allowing shared content hosts weakens third-party isolation. Use **View lists…**
to inspect every bundled hostname and its action. Bundled sets are maintained
by the PanBrowser project and update only with the application; PanBrowser does
not download remote filter subscriptions.

Site boundaries use the bundled Public Suffix List, so domains such as
`example.co.uk` and private suffixes such as `github.io` are not reduced with a
naive last-two-label rule. Personal configuration and enabled recommendation
IDs are stored in `site-connections.json`; the versioned recommendation catalog
is an immutable application resource.

If the primary site-connection file becomes unreadable, PanBrowser first tries
its last valid backup. If neither copy is valid, the original files are
preserved, a visible warning is shown, and unknown third-party connections are
blocked until a valid configuration is saved.

## Experimental video translation

Open **Settings → Video Translation** to enable the experimental integration
with [VOT — Voice Over Translation](https://github.com/ilyhalight/voice-over-translation).
PanBrowser does not download or bundle third-party script code. Download the
official
[`vot.user.js`](https://github.com/ilyhalight/voice-over-translation/releases/download/1.11.8/vot.user.js)
from the VOT `1.11.8` release, select it in Settings, and reload already open
video pages. The file is accepted only when its exact SHA-256 hash and metadata
match the version pinned by PanBrowser; arbitrary userscripts and modified VOT
builds are rejected.

The userscript runs at document-ready time in an isolated JavaScript world and
only on URLs declared by its verified `@match` metadata. Its GM storage is kept
in a PanBrowser-owned file instead of page `localStorage`. Native VOT requests
are limited to HTTPS hosts declared by the verified `@connect` metadata, use
the active application proxy, and pass through the Site Connections policy.
When that firewall is enabled, the first request to an unknown translation or
media host is blocked and produces the ordinary PanBrowser allow/block prompt;
allowing it reloads the affected page.

VOT currently requires **System DNS** because its native Qt Network requests
cannot use Chromium's Secure DNS resolver. Those requests do not share
Chromium cookies or PanBrowser's custom-CA recovery path and instead use Qt
Network's system TLS validation. VOT is third-party code maintained outside
PanBrowser, has access to every matching video page, and can contact its
declared service hosts. Keep the feature disabled unless you accept that trust
boundary.

## Security and privacy notes

- Custom trust recovery is limited to unknown-CA errors. Hostname mismatch,
  expiry, Chromium-known revocation, certificate transparency, and pinning
  failures remain blocked.
- History, bookmarks, address suggestions, downloads, and search-engine
  settings are local. PanBrowser does not request online search suggestions.
- Realm-based HTTP Basic/Digest and manual HTTP-proxy passwords remain
  session-only unless the user explicitly asks PanBrowser to save them. Saved
  credentials use the macOS login Keychain, Windows Credential Manager, or the
  desktop Secret Service on Linux and are never written to PanBrowser settings
  or diagnostics. **Settings → Credentials** lists their non-secret metadata
  and lets the user remove individual or all PanBrowser-managed entries.
- Permission and external-application prompts require a direct action in the
  active tab. Background and cross-origin authentication prompts are rejected.
- Invalid trust or proxy configuration fails closed. Invalid DNS configuration
  falls back to System DNS without overwriting the unreadable file.
- DNS and proxy settings are browser-wide. PanBrowser is not a VPN, and some
  WebRTC traffic may not use Chromium's HTTP proxy path.
- The experimental connection firewall covers URL requests exposed through Qt
  WebEngine. It is not a complete network sandbox and does not claim to block
  every WebRTC, DNS-prefetch, or internal Chromium connection.
- The optional VOT integration executes one exact, hash-verified third-party
  userscript. PanBrowser provides only the compatibility bridge; it does not
  audit or control the translation service, page-processing logic, or future
  upstream releases.
- Installed web apps currently share the main PanBrowser profile, including
  cookies, permissions, proxy, DNS, trust rules, and the site-connection
  firewall.
- Custom recovery validation does not fetch CRLs or OCSP responses. Revocations
  already known to Chromium remain blocked; otherwise unavailable status is a
  soft failure.

For the complete threat boundaries and maintenance invariants, read
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Local data

PanBrowser uses the platform's per-user application-data and cache locations.
Open **Settings → Diagnostics** to see the exact WebEngine profile and cache
paths, or use **Show Configuration Folder** in the application menu to open the
PanBrowser application-data directory. The WebEngine profile contains Chromium
cookies and site data. The surrounding application-data directory contains
session, download, history, bookmark, web-app, search, DNS, proxy, trust, and
feature configuration. It also contains `site-connections.json` with the
opt-in firewall mode, global exceptions, and persistent source-to-target
decisions.

Video translation adds `video-translation.json`, which stores whether the
integration is enabled and the selected userscript path, plus
`vot-storage.json`, which stores script-managed preferences and service state.
Disabling VOT or resetting WebEngine site data does not delete that native
script storage. To remove it completely, close PanBrowser and delete
`vot-storage.json` from the folder opened by **Show Configuration Folder** in
the application menu.

Pinned tabs are stored in `session.json` and reopen on every launch. Ordinary
tabs are stored only when **Continue with previous tabs** is selected. Closing
a pinned tab explicitly removes it from the next session.

The recently closed-tab list is kept in memory only, is capped at 25 entries,
and is discarded when PanBrowser exits.

On macOS, application data is stored under:

```text
~/Library/Application Support/PanBrowser/
```

PanBrowser never stores imported CA certificates in a system trust directory.
On Unix-like systems, managed data directories and private files are restricted
to the current user.

## Documentation

- [Architecture and security boundaries](docs/ARCHITECTURE.md)
- [Bundle auditing and size policy](docs/BUNDLE_POLICY.md)
- [Changelog](CHANGELOG.md)
- [Licensing and binary-distribution checklist](docs/LICENSING.md)
- [macOS release-candidate workflow](docs/RELEASING.md)
- [Roadmap](ROADMAP.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

## Feedback and contributing

Focused bug reports and pull requests are welcome. Please include the operating
system, Qt version, reproduction steps, and relevant output from **Settings →
Diagnostics**. Changes to certificate handling, navigation policy, permissions,
authentication, profile lifetime, or persisted data should preserve the
invariants documented in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

Do not include credentials, private certificate material, account-specific DNS
templates, proxy endpoints, or complete private URLs in an issue.

## License

PanBrowser's original source code, documentation, translations, and build
scripts are licensed under the [Apache License 2.0](LICENSE), copyright 2026
Anton Rudakov. Third-party assets and runtime components remain under their
respective licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Qt, Chromium, and OpenSSL obligations for binary packages are tracked
separately in [docs/LICENSING.md](docs/LICENSING.md).
