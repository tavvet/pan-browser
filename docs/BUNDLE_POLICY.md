# Bundle policy

PanBrowser packages Qt WebEngine rather than relying on a browser runtime
installed on the target system. Chromium and its Qt dependencies therefore
dominate both installed and archive size. Optimizations must preserve a
self-contained browser and must not silently remove runtime fallbacks.

## Audit workflow

`scripts/audit-bundle.cmake` records every deployed file and its size in JSON,
plus a Markdown summary containing the 25 largest files. It accepts:

```text
-DBUNDLE_ROOT=<deployment directory>
-DOUTPUT_JSON=<report.json>
-DOUTPUT_MARKDOWN=<report.md>
-DBUNDLE_LABEL=<descriptive label>
```

Reports describe the deployment directory before ZIP or tar compression. Keep
the reports next to release candidates so size changes remain reviewable.

The manually triggered `.github/workflows/windows-bundle.yml` workflow builds
once and produces two Windows archives:

- `PanBrowser-windows-x64-baseline.zip` contains Qt's unmodified deployment;
- `PanBrowser-windows-x64.zip` contains the locale-optimized deployment.

Both versions have JSON and Markdown audit reports. The baseline is comparison
material, not the preferred distribution archive.

### Analysis snapshot

The Qt 6.11.1 macOS deployment used for the initial analysis contained 195
files and occupied 310,985,428 bytes (296.6 MiB) before compression. The
application executable itself was approximately 1.1 MiB, while the WebEngine
core framework was approximately 204 MiB. The complete locale directory was
approximately 44 MiB; retaining only `en-US` and `ru` reduced it to about 1.6
MiB and the complete deployment to about 255 MiB. The ZIP changed from about
116 MiB to 106 MiB because the removed locale packs already compress well.

These macOS numbers identify the dominant components and justify the allowlist;
they are not Windows release-size promises. Every Windows CI run produces fresh
baseline and optimized reports for the actual MSVC deployment.

## Approved optimization

PanBrowser currently supports English and Russian. The optimized bundle keeps
the Chromium locale packs `en-US.pak` and `ru.pak`; other files inside the
single `qtwebengine_locales` directory are removed by
`scripts/prune-webengine-locales.cmake`. The script fails closed if the locale
directory is ambiguous or either required locale is missing.

When adding an interface language, update all of these together:

1. the application translation catalog and language preference;
2. the default `WebEngineLocales` list in `scripts/build-windows.ps1`;
3. release smoke tests for Chromium-provided UI and error pages in that locale.

## Files retained intentionally

Do not remove the following merely to reduce archive size:

- `QtWebEngineProcess` and the Qt WebEngine libraries;
- `icudtl.dat`, V8 snapshots, and `qtwebengine_resources*.pak`;
- FFmpeg/media libraries;
- graphics backends and software-rendering fallbacks;
- the MSVC runtime from Windows archives;
- `qtwebengine_devtools_resources.pak` until a dedicated experiment proves
  that every supported diagnostics and debugging path works without it;
- Qt libraries and plugins that appear as binary runtime dependencies, even if
  PanBrowser does not call their APIs directly.

The deployment tools discover transitive Qt dependencies. Removing one after
deployment can turn an apparent size improvement into a startup failure on a
different machine.

## CI policy

The Windows workflow is manual during the stabilization period. It uses a
Windows runner, an MSVC Qt build, and the same `scripts/build-windows.ps1`
entry point documented for local packaging. `scripts/install-qt-windows.ps1`
downloads the pinned Qt 6.11.1 binary archives over HTTPS from Qt's official
repository and verifies every archive against the Qt-published SHA-1 captured
in the repository-owned package manifest before extraction. The extracted
installation is cached by version, architecture, and installer-manifest hash.

The CI SDK contains Qt Base, SVG, Declarative, Tools, Translations, Positioning,
WebChannel, WebEngine, and the Windows graphics fallbacks needed by deployment.
Documentation, examples, source archives, and debug symbols are deliberately
not downloaded. This reduces CI transfer and cache size without changing the
files selected by Qt's deployment tooling. The installer also writes Qt's
standard relocatable `bin/qt.conf`; qmake-specific patches and IDE shortcuts
from the interactive Qt installer are unnecessary for the CMake build.

The same packaging entry point was locally validated in August 2026 in a
Windows 11 ARM64 VMware Fusion guest with the x64 MSVC/Qt toolchain running
under Windows emulation. The build, tests, deployment, and packaged application
startup completed successfully. This does not establish native ARM64 support;
the package and pinned SDK remain `msvc2022_64`/x64.

Changing the Qt version, architecture, deployment options, locale allowlist,
or GitHub Action revisions requires inspection of a fresh baseline report.
