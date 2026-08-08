# Licensing and binary distribution

This document records the licensing boundary of the PanBrowser `0.1.0` source
tree and the work required before distributing a packaged application. It is a
maintenance checklist, not legal advice. A distributor remains responsible for
reviewing the exact source, toolchain, and runtime components in each package.

## 1. Source repository

The repository contains three licensing groups:

1. PanBrowser-owned source code, documentation, translations, build scripts,
   and original visual design, licensed under the Apache License 2.0.
2. Lucide and Feather-derived artwork committed under `src/assets/`. Their ISC
   and MIT notices are reproduced in
   [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
3. References to external build and runtime dependencies. Qt, Chromium, and
   OpenSSL source or binaries are not committed to this repository.

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
  Qt version, OpenSSL version on Linux, and bundle audit report.
- [ ] Inspect the final package rather than assuming that deployment collected
  the same modules on macOS, Windows, and Linux.
- [ ] Confirm that pruning locales or plugins did not remove license material.

The Qt Company summarizes its open-source distribution expectations in
[Obligations of the GPL and LGPL](https://www.qt.io/development/open-source-lgpl-obligations).
The exact license texts, rather than this checklist, control.

## 5. Current package gap

The existing scripts create local testing packages. They include the PanBrowser
README, Apache license, third-party notice, and this licensing guide in the
platform package. None of the scripts currently assembles the full Qt/Chromium
license set, Qt source offer, or in-application open-source notice.

Therefore, the current `dist/` outputs and Windows CI artifacts should remain
development/test artifacts until the binary-distribution checklist above is
implemented and verified. This limitation does not block publication of a
properly licensed source repository and source-only `v0.1.0` tag.

## 6. Maintenance rule

Whenever the Qt version, deployed module set, OpenSSL baseline, icon set, or
packaging layout changes:

1. regenerate and compare the dependency inventory;
2. review the upstream license and SBOM files for that exact version;
3. update `THIRD_PARTY_NOTICES.md` and the packaged license directory;
4. rerun the final package audit on all supported platforms.
