# Licensing and binary distribution

This document records the licensing boundary of the current PanBrowser source
tree, including `0.2.0` development on `main`, and the work required before
distributing a packaged application. It is a maintenance checklist, not legal
advice. A distributor remains responsible for reviewing the exact source,
toolchain, and runtime components in each package.

## 1. Source repository

The repository contains five licensing groups:

1. PanBrowser-owned source code, documentation, translations, build scripts,
   and original visual design, licensed under the Apache License 2.0.
2. Lucide and Feather-derived interface icons committed under
   `src/assets/icons/`. Their ISC and MIT notices are reproduced in
   [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
3. The bundled Public Suffix List under `src/assets/`, licensed under MPL-2.0
   and identified in the third-party notices.
4. Mozilla Readability and DOMPurify source used by Reader Mode. PanBrowser
   selects their Apache-2.0 licensing terms and retains the upstream notices
   under `src/assets/reader/third_party/`.
5. References to external build and runtime dependencies and to the optional
   VOT userscript integration. Qt, Chromium, OpenSSL, and VOT source or binaries
   are not committed to this repository.

Publishing a source tag does not require publishing a GitHub Release or a
binary package. The source tag should contain the PanBrowser project license,
the third-party notices, and this document.

## 2. PanBrowser application license

PanBrowser's original material is licensed under the
[Apache License 2.0](../LICENSE), copyright 2026 Anton Rudakov. The license is
permissive and includes explicit copyright and patent grants. Contributions
intentionally submitted for inclusion are governed by section 5 unless a
separate agreement states otherwise.

This project license is independent from the licenses of dynamically linked
libraries. A distributed combination must still satisfy every applicable Qt,
Chromium, OpenSSL, and asset license.

## 3. External runtime inventory

### Qt

PanBrowser directly uses these Qt modules:

- Core
- Network
- SQL
- SVG
- Widgets
- WebEngine Core
- WebEngine Widgets

Qt deployment tools add transitive libraries, plugins, helper executables,
resources, and translations. The exact set differs by platform and Qt build.
The macOS Qt 6.11.1 package tested during development additionally deployed Qt
DBus, GUI, OpenGL, PDF, Positioning, Print Support, QML, Quick, Serial Port, and
WebChannel components.

For open-source Qt builds, PanBrowser intends to use the dynamically linked
LGPL option where the component offers it. The Qt-specific part of Qt WebEngine
is offered under LGPL-3.0, GPL-3.0, or GPL-2.0 alternatives. Chromium inside Qt
WebEngine has its own license set, including LGPL-2.1 components. Do not infer a
package's full license inventory from the list of direct CMake targets.

Qt 6.8 and newer ship SPDX SBOM files. Archive the SBOMs for every deployed Qt
module alongside the build provenance. For Qt 6.11.1 these normally include:

```text
qtbase-6.11.1.spdx
qtdeclarative-6.11.1.spdx
qtpositioning-6.11.1.spdx
qtsvg-6.11.1.spdx
qtwebchannel-6.11.1.spdx
qtwebengine-6.11.1.spdx
```

The required set must be derived from the final package, not copied blindly
from this example.

### Chromium

Qt WebEngine integrates Chromium into the WebEngine Core library. Qt publishes
the version-matched third-party component notices in its
[WebEngine licensing documentation](https://doc.qt.io/qt-6/qtwebengine-licensing.html).
The small `LICENSE.Chromium` file shipped by Qt covers Chromium's top-level
BSD-style license only; it is not a substitute for Chromium's complete
third-party credits.

### OpenSSL

Linux builds link to the OpenSSL Crypto library found by CMake. Record the
resolved library and version in the build provenance. If that library is
included in a package, include its exact license and notice files. If the
package relies on a system copy, document that runtime dependency. OpenSSL 3.x
and 1.1.1 use different licenses.

### libsecret

Linux builds dynamically link to libsecret 0.19 or newer and use the desktop
Secret Service rather than bundling a password-store implementation. libsecret
is licensed under LGPL-2.1-or-later. The current Linux tar archive relies on the
target operating system's copy and documents that runtime requirement rather
than bundling libsecret. Record the resolved version in build provenance. If a
future package copies the library, include its matching license material and
corresponding-source offer obligations in the final package audit.

### Optional VOT userscript

PanBrowser implements a compatibility and security bridge for the independently
maintained VOT userscript. The application does not bundle, download, modify,
or redistribute VOT. A user supplies the supported upstream `vot.user.js`, and
PanBrowser verifies its exact version, metadata, and SHA-256 digest before
execution. The upstream project is MIT-licensed; its notice is reproduced in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

Because the current source and binary packages contain no VOT code, the
userscript is not part of PanBrowser's distributed binary inventory. If a
future package downloads, embeds, mirrors, or modifies it, that distribution
must preserve the upstream MIT notice and update the package inventory,
security documentation, source handling, and update policy before release.

## 4. Checklist before sharing any binary

This checklist applies to a GitHub Release, a direct download, a package sent
to another person, and a downloadable CI artifact. The absence of a formal
"Release" page does not remove the obligations attached to the binary.

- [x] Commit the selected PanBrowser `LICENSE`.
- [ ] Keep the PanBrowser `LICENSE` in every package.
- [ ] Include `THIRD_PARTY_NOTICES.md` and the complete applicable third-party
  license texts in a clearly named documentation or licenses directory.
- [ ] Show a prominent notice that the application uses Qt under the selected
  open-source terms and provide an in-application path to the notices.
- [ ] Include the GNU GPL v3 and LGPL v3 texts required for LGPL-3.0-covered Qt
  libraries.
- [ ] Include all Qt WebEngine and Chromium notices for the exact Qt version,
  including applicable LGPL-2.1 text and component-specific notices.
- [ ] Preserve the Lucide ISC and Feather MIT notices.
- [ ] Preserve the Mozilla Readability and DOMPurify Apache-2.0 notices and
  bundled license files.
- [ ] Preserve the Public Suffix List MPL-2.0 notice and make its source form
  available with the distributed version.
- [ ] If VOT is ever included in a package rather than selected separately by
  the user, preserve its MIT notice and record the exact distributed source.
- [ ] Include the exact OpenSSL notice when OpenSSL is bundled.
- [ ] Keep Qt dynamically linked and verify that recipients can replace the Qt
  libraries with ABI-compatible modified builds. Document any platform signing
  or launch steps needed to run such a replacement.
- [ ] Provide the complete corresponding source for the exact LGPL-covered Qt
  binaries, including local modifications, or a compliant written offer under
  the distributor's control. Qt's own download link alone is not the source
  offer described by The Qt Company's LGPL guidance.
- [ ] Retain the matching Qt and Chromium source archives and their checksums
  for the required offer period.
- [ ] Archive the final package inventory, Qt SBOMs, compiler/toolchain version,
  Qt version, OpenSSL and libsecret versions on Linux, and bundle audit report.
- [ ] Inspect the final package rather than assuming that deployment collected
  the same modules on macOS, Windows, and Linux.
- [ ] Confirm that pruning locales or plugins did not remove license material.

The Qt Company summarizes its open-source distribution expectations in
[Obligations of the GPL and LGPL](https://www.qt.io/development/open-source-lgpl-obligations).
The exact license texts, rather than this checklist, control.

## 5. Current package status

The generic platform build scripts create local testing packages. They include
the PanBrowser README, Apache license, third-party notice, this licensing guide,
and the bundled Readability and DOMPurify license files, but they do not yet
implement the complete binary-distribution checklist above. They do not copy a
configured VOT userscript or native VOT storage from the developer's
application-data directory. Windows CI artifacts and Linux packages should
therefore remain development/test artifacts.

The macOS release-candidate script goes further. It collects the installed Qt
SPDX documents, Chromium's top-level license, exact notices for loose Homebrew
runtime libraries, the common LGPL/GPL texts, raw Homebrew source metadata,
build provenance, an LGPL corresponding-source offer, and the final bundle
audit. The settings UI exposes the packaged files under **Diagnostics >
Open-source notices** and exposes Chromium's embedded component notices through
the runtime `chrome://credits` page.

For the macOS candidate, the remaining distributor task is operational rather
than a missing UI or packaging feature: retain the exact corresponding source,
patches, build information, and checksums for the full offer period and be able
to fulfil the packaged written offer. Homebrew cache entries are not an
adequate long-term archive. This repository does not claim that the generated
package is legally complete merely because the collector succeeds.

These limitations do not block publication of a properly licensed source
repository and `v0.1.0` source tag. That milestone may be accompanied by the
separately audited macOS ARM64 candidate described above. A person distributing
a binary remains responsible for completing and verifying the checklist for
that exact package.

## 6. Maintenance rule

Whenever the Qt version, deployed module set, OpenSSL baseline, supported VOT
version, icon set, or packaging layout changes:

1. regenerate and compare the dependency inventory;
2. review the upstream license and SBOM files for that exact version;
3. update `THIRD_PARTY_NOTICES.md` and the packaged license directory;
4. rerun the final package audit on all supported platforms.

The bundled site-connection recommendation catalog is currently curated by the
PanBrowser project and is not copied from an external tracker or advertising
filter database. Every catalog change must remain reviewable in source control.
Do not import, derive, or automate updates from a third-party filter list until
its database/content license has been reviewed and the applicable attribution,
share-alike, source, and redistribution requirements have been added to the
package inventory and notices.
