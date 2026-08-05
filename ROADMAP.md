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

## Browser essentials

- [ ] Popup handling in a new PanBrowser window with the same profile and trust policy.
- [ ] Downloads: destination picker, progress, completion, and error reporting.
- [ ] Explicit permission prompts for camera, microphone, location, and notifications.
- [ ] Safe handling of external URL schemes such as `mailto:` and application deep links.

## Cross-platform

- [ ] Windows certificate validator using the platform trust APIs.
- [ ] Linux certificate validator with an explicit trust backend.
- [ ] Platform-specific build and packaging scripts.

## Packaging and maintenance

- [ ] Audit the release bundle and remove unused Qt plugins and resources.
- [ ] Add application version information to the UI.
- [ ] Add a controlled update mechanism.

## Deferred distribution work

- [ ] Developer ID signing and notarization.
- [ ] DMG packaging and first-run onboarding.
- [ ] Importable and exportable trust profiles.
