<p align="center">
  <img src="src/assets/app-icon.svg" width="96" height="96" alt="PanBrowser icon">
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
- A dedicated WebEngine profile for cookies, storage, cache, history,
  bookmarks, downloads, and installed web apps.
- Tabs, session restoration, popup handling, find in page, per-site page zoom,
  local address completion, configurable search engines, and a download manager.
- Browser-owned prompts for camera, microphone, location, external schemes,
  HTTP Basic authentication, and HTTP proxy authentication.
- Browser-wide System/Direct/HTTP/SOCKS5 proxy modes and configurable
  DNS-over-HTTPS providers.
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
create a self-contained package under `dist/`.

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

Install Qt WebEngine 6.11 development files, OpenSSL development files, CMake,
Ninja, and a C++20 compiler, then run:

```sh
./scripts/build-linux.sh
```

If Qt is not in CMake's default search path:

```sh
QT_ROOT=/path/to/Qt/6.11.1/gcc_64 ./scripts/build-linux.sh
```

The output is `dist/PanBrowser-linux-<architecture>.tar.gz`. Linux packages
should be built on the oldest distribution they intend to support because
glibc remains a host compatibility boundary.

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
validation. API, CDN, and other subresource hosts need their own matching
entries.

Version 1 files retain the legacy `startPage` field for compatibility. After
its first import, the start page selected in Settings is authoritative.

## Security and privacy notes

- Custom trust recovery is limited to unknown-CA errors. Hostname mismatch,
  expiry, Chromium-known revocation, certificate transparency, and pinning
  failures remain blocked.
- History, bookmarks, address suggestions, downloads, and search-engine
  settings are local. PanBrowser does not request online search suggestions.
- HTTP Basic and proxy passwords are kept only for the current process and are
  not written to PanBrowser settings or diagnostics.
- Permission and external-application prompts require a direct action in the
  active tab. Background and cross-origin authentication prompts are rejected.
- Invalid trust or proxy configuration fails closed. Invalid DNS configuration
  falls back to System DNS without overwriting the unreadable file.
- DNS and proxy settings are browser-wide. PanBrowser is not a VPN, and some
  WebRTC traffic may not use Chromium's HTTP proxy path.
- Installed web apps currently share the main PanBrowser profile, including
  cookies, permissions, proxy, DNS, and trust rules.
- Custom recovery validation does not fetch CRLs or OCSP responses. Revocations
  already known to Chromium remain blocked; otherwise unavailable status is a
  soft failure.

For the complete threat boundaries and maintenance invariants, read
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Local data

PanBrowser uses the platform's per-user application-data and cache locations.
Open **Settings → Diagnostics** to see their exact paths. The profile contains
WebEngine cookies and site data together with PanBrowser's session, download,
history, bookmark, web-app, search, DNS, proxy, and trust configuration.

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
