# PanBrowser roadmap

Current development target: **0.2.0 (unreleased)**. The `v0.1.0` tag records the
first public project baseline; `main` now contains post-release development and
may be unstable.

## 0.2.0 scope

### Browser polish

- [x] Keep normal page fullscreen in the browser window and expose video
  pop-out as a separate, explicit overlay action.
- [x] Add compact pinned tabs with a left-side group, constrained reordering,
  context-menu controls, and startup-independent session restoration.
- [x] Animate new-tab expansion while respecting the active Qt style's
  animation setting and leaving restored sessions static.
- [x] Add opt-in developer tools that default to disabled and expose both
  Inspect Element and keyboard shortcuts only while enabled.
- [ ] Complete favicon support: persist icons received from WebEngine and show
  them in restored tabs, bookmarks, history, and address-bar suggestions.
- [x] Page zoom controls with keyboard shortcuts, per-site persistence, and
  reset to 100%.
- [ ] Allow user-initiated `chrome://` navigation from the address bar behind a
  safety warning. Keep redirects, page scripts, and popups blocked; offer a
  per-profile "Don't show again" preference that can be reset in settings.
- [x] Add an opt-in experimental third-party connection firewall with
  Public-Suffix-List site boundaries, global allow/block lists, per-site
  session and persistent decisions, browser-owned prompts, and separately
  managed PanBrowser tracker/CDN recommendation sets.
- [x] Add opt-in integration with the exact hash-verified VOT 1.11.8
  userscript, isolated native storage, bounded HTTPS networking, iframe-aware
  response routing, proxy authentication, and Site Connections enforcement.
- [ ] Add opt-in persistent realm-based HTTP Basic/Digest and manual HTTP-proxy
  credentials through native operating-system password managers. The macOS
  Keychain backend and shared controller integration are complete; Windows
  Credential Manager, Linux Secret Service, and management UI remain pending.

### Portable app and trust workflows

- [ ] Add Windows Start Menu and Linux `.desktop` launchers for installed web
  apps.
- [ ] Add import and export for versioned trust profiles without silently
  installing certificates into the operating-system trust store.

## 0.1.0 scope

### Trust and profile isolation

- [x] Domain-scoped custom CA validation on macOS, Windows, and Linux.
- [x] Trust rule editor with certificate import and validation.
- [x] Certificate details and SHA-256 fingerprint viewer.
- [x] Dedicated persistent WebEngine profile for cookies, site data, and cache.
- [x] Configurable retention of session cookies and restored tabs.
- [x] Browsing data controls for cookies, site storage, and HTTP cache.
- [x] Fail-closed handling of invalid trust and proxy configuration. Invalid
  DNS configuration uses the documented System DNS fallback without
  overwriting the unreadable file.

### Browser functionality

- [x] Safe restoration of window size and position.
- [x] Movable browser tabs with per-tab navigation and TLS status.
- [x] Integrated native title-bar tab strip on macOS.
- [x] Native decorated windows and an overflow application menu on Windows and
  Linux, without expanded-client-area or frameless-window flags.
- [x] Unified settings window.
- [x] Configurable start page and startup behavior.
- [x] Download manager with progress controls and persistent history.
- [x] One-time permission prompts for camera, microphone, and location.
- [x] Safe confirmation for external URL schemes and application deep links.
- [x] User-initiated popup windows with shared profile and trust policy.
- [x] Address-bar search with configurable built-in and custom search engines.
- [x] Local history with management controls and ranked address completion.
- [x] Bookmarks with management UI and address-completion integration.
- [x] Find in page with keyboard navigation and match count.
- [x] Runtime diagnostics for application, Chromium, graphics, and profile state.
- [x] English and Russian localization with system-language detection and an
  explicit override.
- [x] Manifest-based web app installation with scoped app windows, management
  UI, and macOS application shortcuts.

### Networking

- [x] Browser-owned HTTP Basic authentication dialog with retry feedback,
  session-only credentials, and an explicit warning on unencrypted HTTP.
- [x] Secure DNS manager with system default, built-in and custom DNS-over-HTTPS
  providers, fallback/strict modes, runtime application, and diagnostics.
- [x] Browser-wide proxy manager with system, direct, HTTP, and unauthenticated
  SOCKS5 modes, session-only HTTP authentication, fail-closed startup, and
  diagnostics.

### Build and release readiness

- [x] Platform-specific build, test, and packaging scripts.
- [x] macOS build, package, and GUI smoke test.
- [x] Windows x64 build, automated tests, packaging, and GUI smoke test on
  Windows 11 ARM64 under x64 emulation.
- [x] Linux ARM64 build, automated tests, packaging, and GUI smoke test on
  Ubuntu ARM64 in VMware Fusion.
- [x] Bundle audit tooling and supported Chromium-locale pruning for Windows.
- [x] Application version information in the UI.
- [x] License PanBrowser's original material under Apache-2.0 and document the
  separate third-party and binary-distribution obligations.
- [ ] Run a final regression pass using the packaged artifact on each tested
  platform.
- [x] Publish the `v0.1.0` tag and GitHub prerelease with the signed,
  explicitly unnotarized macOS ARM64 archive and checksum.

## Backlog after 0.2.0

### Browser improvements

- [ ] Add a controlled update mechanism.

### Trust, platform, and maintenance

- [ ] Add a freshness-checked revocation cache for custom trust anchors.
- [ ] Run native Windows 11 x64 and Windows 10 smoke tests outside emulation.
- [ ] Run Linux package and GUI smoke tests on representative x86-64, X11, and
  Wayland environments.
- [ ] Evaluate further Qt plugin or resource removal with cross-platform smoke
  tests before changing the retained-runtime policy.

### Web app profiles

- [ ] Add an explicit per-app profile mode: shared with PanBrowser or isolated.
- [ ] Isolate cookies, site storage, service workers, and cache for selected apps.
- [ ] Let app settings inherit global values or override trust rules,
  permissions, and data-retention behavior individually.
- [ ] Add per-app data inspection and clearing without affecting the main
  browser profile or other installed apps.
- [ ] Support per-app DNS and proxy overrides through process isolation, since
  Qt WebEngine applies those network settings globally within a process.
- [ ] Extend system launchers and single-instance coordination so isolated apps
  open in the correct profile process.
- [ ] Provide a safe migration path between shared and isolated profiles without
  silently copying cookies or authentication state.

### Distribution

- [ ] Extend version-matched Qt, Qt WebEngine/Chromium, OpenSSL, and asset
  notices to every Windows and Linux binary package. The macOS candidate
  collects its runtime notices, source metadata, and source offer and exposes
  them through Diagnostics.
- [x] Developer ID signing workflow for macOS release candidates.
- [ ] Apple notarization and ticket stapling. The current team is not yet
  enabled for notarization.
- [ ] DMG packaging and first-run onboarding.
- [ ] Evaluate an official or source-built Qt distribution for a macOS
  deployment target older than 26.0; the current Homebrew Qt 6.11.1 frameworks
  require macOS 26.
- [ ] Publish versioned binary archives and checksums when supported downloads
  are ready.

### Proxy extensions

- [ ] Custom PAC URLs and explicit PAC diagnostics.
- [ ] Per-domain proxy routing and bypass rules.
- [ ] Named proxy profiles, chains, health checks, and automatic failover.
- [ ] Independent proxy selection for installed web apps.
