# Third-party notices

This document identifies third-party material present in the PanBrowser source
tree and the principal external components used by PanBrowser packages. It does
not replace the complete, version-specific license material that must accompany
a binary distribution. See [docs/LICENSING.md](docs/LICENSING.md) before sharing
any packaged application or CI artifact.

## Assets included in this repository

### Lucide Icons

The SVG files under `src/assets/icons/` are selected and color-adjusted icons
from [Lucide](https://github.com/lucide-icons/lucide). The shield artwork in
`src/assets/app-icon.svg`, and therefore `src/assets/PanBrowser.icns`, is based
on the Lucide `shield-check` icon.

ISC License

Copyright (c) 2026 Lucide Icons and Contributors

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Lucide is a fork of Feather Icons. Some icons retained by Lucide, including
several used by PanBrowser, are derived from the Feather project and are
covered by the following MIT license.

The MIT License (MIT)

Copyright (c) 2013-present Cole Bemis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

The upstream notice and the list of Lucide icons derived from Feather are
available in the
[Lucide license file](https://github.com/lucide-icons/lucide/blob/main/LICENSE).

### Public Suffix List

`src/assets/public_suffix_list.dat` is the Public Suffix List published at
[publicsuffix.org](https://publicsuffix.org/). PanBrowser uses it to determine
registrable site boundaries for the optional third-party connection firewall.
The committed snapshot identifies itself as version
`2026-07-25_14-20-03_UTC`, commit
`e1b8015c3b2f0f4f8c18659c2480fc1a22c07b20`.

The list is licensed under the Mozilla Public License, version 2.0. Its source
form and upstream license notice are retained in the repository. The complete
license is available at
[mozilla.org/MPL/2.0](https://www.mozilla.org/MPL/2.0/).

This Source Code Form is subject to the terms of the Mozilla Public License,
v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain
one at https://mozilla.org/MPL/2.0/.

## Optional external code

### VOT — Voice Over Translation userscript

PanBrowser contains an opt-in compatibility bridge for
[VOT](https://github.com/ilyhalight/voice-over-translation). The VOT source is
not committed to this repository, copied into PanBrowser packages, or
downloaded by the application. A user must obtain the supported `vot.user.js`
independently, and PanBrowser accepts only the exact upstream `1.11.8` file
identified by its pinned SHA-256 digest.

VOT is independently maintained and licensed under the MIT License:

MIT License

Copyright (c) 2021
[sodapng](https://github.com/sodapng/voice-over-translation)

Copyright (c) 2022-present ilyhalight

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Runtime and deployment dependencies

The following components are not copied into the PanBrowser source tree. They
are located by the build system or collected by Qt's deployment tools when a
binary package is created. Their exact contents and licenses depend on the
versions used to build that package.

### Qt 6

PanBrowser directly links to Qt Core, Network, SQL, SVG, Widgets, WebEngine
Core, and WebEngine Widgets. Qt WebEngine deployment also brings in
platform-dependent Qt modules, plugins, helpers, and resources such as Qt GUI,
OpenGL, PDF, Positioning, QML, Quick, Serial Port, and WebChannel.

The Qt-specific library code used by PanBrowser is available from The Qt
Company under commercial terms or, depending on the component, open-source
options including the GNU Lesser General Public License version 3.0. PanBrowser
builds use dynamically linked Qt libraries. Exact license expressions and
third-party component inventories are recorded in the SPDX SBOM files shipped
with Qt 6.8 and newer.

Copyright (C) The Qt Company Ltd. and other contributors.

Official references:

- [Qt licensing](https://doc.qt.io/qt-6/licensing.html)
- [GNU LGPL version 3 used by Qt](https://doc.qt.io/qt-6/lgpl.html)
- [Third-party code used in Qt](https://doc.qt.io/qt-6/licenses-used-in-qt.html)
- [Qt 6.11.1 source archives](https://download.qt.io/official_releases/qt/6.11/6.11.1/submodules/)

### Qt WebEngine and Chromium

Qt WebEngine embeds Chromium in the Qt WebEngine Core library. A distributor
must comply with both the license selected for the Qt-specific WebEngine code
and the licenses of Chromium and its bundled third-party components. Qt states
that the most restrictive license present in the Chromium portion is the GNU
Lesser General Public License version 2.1.

The complete component list is version-specific. For Qt 6.11.1 it is published
in the official
[Qt WebEngine licensing documentation](https://doc.qt.io/qt-6/qtwebengine-licensing.html).

The top-level Chromium code is distributed under the following BSD-style
license:

Copyright 2015 The Chromium Authors. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
- Neither the name of Google Inc. nor the names of its contributors may be used
  to endorse or promote products derived from this software without specific
  prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This Chromium notice alone is not the complete license inventory for Qt
WebEngine. Binary distributions must also carry the version-specific Chromium
third-party notices described in [docs/LICENSING.md](docs/LICENSING.md).

### OpenSSL

The Linux certificate validator dynamically links to the OpenSSL Crypto
library selected by CMake. The macOS deployment may also contain OpenSSL shared
libraries used by the deployed Qt TLS backend; the `0.1.0` macOS candidate
contains OpenSSL 3 and packages its exact notice through the runtime-license
collector. OpenSSL is not vendored in this source repository. OpenSSL 3.0 and
later use the Apache License 2.0; OpenSSL 1.1.1 and earlier use the dual OpenSSL
and SSLeay license. A binary distributor must use the terms and notices matching
the exact linked or bundled OpenSSL version.

Official reference: [OpenSSL licensing](https://openssl-library.org/source/license/index.html).

### Operating-system APIs

PanBrowser uses Security.framework and Core Foundation on macOS and CryptoAPI
on Windows. These are operating-system components and are not redistributed in
the PanBrowser source tree or packages.

## Build and CI tools

CMake, Ninja, compilers, 7-Zip, and the GitHub Actions referenced by the build
configuration are development or CI tools. They are not incorporated into the
PanBrowser application by this repository. Their own licenses continue to
apply to the tools themselves.
