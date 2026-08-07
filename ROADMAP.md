# PanBrowser roadmap

Current development baseline: **0.2.0**. Completed items describe the main
branch; unfinished experiments kept on other branches remain unchecked here.

## Completed foundation

- [x] Domain-scoped custom CA validation on macOS.
- [x] Trust rule editor with certificate import and validation.
- [x] Certificate details and SHA-256 fingerprint viewer.
- [x] Safe restoration of window size and position.
- [x] Dedicated persistent WebEngine profile for cookies, site data, and cache.
- [x] Movable browser tabs with per-tab navigation and TLS status.
- [x] Integrated native title-bar tab strip on macOS with a safe decorated-window
  fallback on other platforms and installed web apps.
- [x] Unified settings window with General and Trust Rules sections.
- [x] Configurable start page and startup behavior.
- [x] Optional session-cookie and tab restoration with lazy background tabs.
- [x] Browsing data controls for cookies, site storage, and HTTP cache.
- [x] Download manager with progress controls and persistent history.
- [x] One-time permission prompts for camera, microphone, and location.
- [x] Safe confirmation for external URL schemes and application deep links.
- [x] User-initiated popup windows with shared profile and trust policy.
- [x] Address-bar search with configurable built-in and custom search engines.
- [x] Local browsing history with management controls and ranked address-bar completion.
- [x] Runtime diagnostics page with application, Chromium, graphics, and profile details.
- [x] English and Russian interface localization with system-language detection and explicit override.
- [x] Manifest-based web app installation with scoped app windows, management UI, and macOS application shortcuts.
- [x] Secure DNS manager with system default, built-in and custom DNS-over-HTTPS providers, fallback/strict modes, runtime application, and diagnostics.
- [x] Browser-wide proxy manager with system, direct, HTTP, and unauthenticated SOCKS5 modes, session-only HTTP authentication, fail-closed startup, and diagnostics.

## Browser essentials

- [x] Bookmarks with an address-bar toggle, management UI, and autocomplete integration.
- [x] Find in page with `Command+F`, next/previous match navigation, and match count.
- [ ] Complete favicon support: persist icons received from WebEngine and show
  them in restored tabs, bookmarks, history, and address-bar suggestions.
- [ ] Page zoom controls with keyboard shortcuts, per-site persistence, and reset to 100%.

## Cross-platform

- [x] Windows certificate validator using the platform trust APIs.
- [x] Linux certificate validator with an explicit OpenSSL trust backend.
- [ ] Freshness-checked revocation cache for custom trust anchors.
- [x] Platform-specific build and packaging scripts.
- [ ] Windows Start Menu and Linux `.desktop` launchers for installed web apps.
- [ ] Validate and polish the experimental integrated title bar on Windows
  10/11, including caption controls, drag/Snap, and mixed-DPI displays.

## Packaging and maintenance

- [ ] Audit the release bundle and remove unused Qt plugins and resources.
- [x] Add application version information to the UI.
- [ ] Add a controlled update mechanism.

## Web app profiles

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

## Deferred distribution work

- [ ] Developer ID signing and notarization.
- [ ] DMG packaging and first-run onboarding.
- [ ] Importable and exportable trust profiles.

## Deferred proxy extensions

- [ ] Custom PAC URLs and explicit PAC diagnostics.
- [ ] Per-domain proxy routing and bypass rules.
- [ ] Named proxy profiles, chains, health checks, and automatic failover.
- [ ] Independent proxy selection for installed web apps.
