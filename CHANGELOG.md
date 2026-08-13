# Changelog

This file records PanBrowser source milestones. The project does not yet make
the compatibility or security-maintenance guarantees of a stable
general-purpose browser.

## 0.2.0 — Unreleased

Development continues on `main`. The initial scope focuses on browser polish,
portable web-app integration, and safe transfer of trust configuration.

- Added opt-in developer tools with a profile preference that defaults to off,
  an Inspect Element context action, and platform-appropriate shortcuts.
- Added page zoom from the application menu, keyboard, mouse wheel, and
  trackpad, with discrete levels and per-origin persistence.
- Added compact pinned tabs with constrained group reordering and restoration
  on every browser launch, independent of ordinary session-tab settings.
- Separated normal page fullscreen from the explicit video pop-out action;
  videos can now be opened in a frameless, resizable, always-on-top window from
  a browser-provided overlay button. The window exposes a hover close control
  and can be dragged from its content.

## 0.1.0 — 2026-08-10

The first public source milestone turns the original domain-scoped certificate
viewer experiment into a compact, usable Qt WebEngine browser.

### Trust and security boundaries

- Added exact-domain and explicit wildcard trust rules with system-only,
  system-plus-custom, and custom-only modes.
- Added native certificate-chain and hostname validation on macOS, Windows,
  and Linux without installing custom roots into the operating system store.
- Added browser-owned prompts for web permissions, external schemes, HTTP Basic
  authentication, and HTTP proxy authentication.
- Added isolated browser profile storage and configurable session-data
  retention.

### Browser functionality

- Added tabs, session restoration, history, bookmarks, downloads, find in page,
  address completion, configurable search, DNS-over-HTTPS, and proxy settings.
- Added manifest-based web apps and macOS application shortcuts.
- Added English and Russian interfaces, unified settings, and runtime
  diagnostics.

### Packaging

- Verified builds and GUI smoke tests on macOS ARM64, Windows x64 under ARM64
  emulation, and Ubuntu ARM64.
- Added platform build scripts, bundle auditing, Apache-2.0 project licensing,
  and third-party licensing documentation.
- Added a Developer ID-signed macOS release-candidate workflow with deterministic
  runtime-license collection and in-application open-source notices.

### Known limitations

- PanBrowser has no automatic update mechanism and should not be treated as a
  security-maintained general-purpose browser.
- The macOS 26+ ARM64 candidate is signed but not notarized because the Apple
  Developer team is not yet enabled for notarization; Gatekeeper therefore
  requires an explicit user-approved launch.
- Windows and Linux remain build-from-source platforms for this milestone.
- Native Windows x64 and broader Linux x86-64/X11/Wayland coverage remain future
  work.
