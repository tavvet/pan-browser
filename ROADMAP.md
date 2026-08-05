# PanBrowser roadmap

## Completed foundation

- [x] Domain-scoped custom CA validation on macOS.
- [x] Trust rule editor with certificate import and validation.
- [x] Certificate details and SHA-256 fingerprint viewer.
- [x] Safe restoration of window size and position.
- [x] Dedicated persistent WebEngine profile for cookies, site data, and cache.
- [x] Movable browser tabs with per-tab navigation and TLS status.
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

## Browser essentials

- [x] Bookmarks with an address-bar toggle, management UI, and autocomplete integration.
- [x] Find in page with `Command+F`, next/previous match navigation, and match count.
- [ ] Page zoom controls with keyboard shortcuts, per-site persistence, and reset to 100%.

## Cross-platform

- [x] Windows certificate validator using the platform trust APIs.
- [x] Linux certificate validator with an explicit OpenSSL trust backend.
- [ ] Freshness-checked revocation cache for custom trust anchors.
- [x] Platform-specific build and packaging scripts.

## Packaging and maintenance

- [ ] Audit the release bundle and remove unused Qt plugins and resources.
- [x] Add application version information to the UI.
- [ ] Add a controlled update mechanism.

## Deferred distribution work

- [ ] Developer ID signing and notarization.
- [ ] DMG packaging and first-run onboarding.
- [ ] Importable and exportable trust profiles.
