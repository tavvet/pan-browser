# Releasing PanBrowser

This document describes the macOS release-candidate workflow for PanBrowser
0.x. Windows and Linux currently remain build-from-source platforms. A signed
or notarized artifact is not automatically ready for public distribution: the
binary-distribution checklist in [LICENSING.md](LICENSING.md) remains the
publication gate.

## 1. Prerequisites

- macOS 26 or newer with Xcode command-line tools and `notarytool`. The current
  Homebrew Qt 6.11.1 frameworks require release builds to pin
  `CMAKE_OSX_DEPLOYMENT_TARGET` to `26.0`.
- The same Qt installation used by the normal macOS build.
- Cached source archives used to extract version-matched license texts. Fetch
  them once with `brew fetch --build-from-source qtbase freetype`.
- A valid `Developer ID Application` certificate and its private key in a
  local Keychain.
- An Apple Developer team allowed to submit Developer ID software for
  notarization.

Confirm the signing identity:

```sh
security find-identity -v -p codesigning
```

The output must contain a valid identity named like:

```text
Developer ID Application: OWNER NAME (TEAMID)
```

Do not commit certificate files, private keys, App Store Connect API keys,
Apple passwords, or notarization credentials to this repository.

## 2. Store notarization credentials

The simplest local workflow uses an Apple Account app-specific password stored
by `notarytool` in the macOS Keychain. Create the app-specific password in the
Apple Account security settings, then run this interactively once:

```sh
xcrun notarytool store-credentials "PanBrowser-notary" \
  --apple-id "APPLE_ACCOUNT_EMAIL" \
  --team-id "APPLE_TEAM_ID"
```

`notarytool` prompts for the app-specific password, validates the credentials,
and stores them in the Keychain. The password must not be passed to the release
script or saved in a shell environment file.

Verify the stored profile without uploading software:

```sh
xcrun notarytool history --keychain-profile "PanBrowser-notary"
```

## 3. Build a signed candidate

Run:

```sh
./scripts/release-macos.sh
```

When exactly one unique Developer ID Application identity is available, the
script selects its SHA-1 automatically. Otherwise provide the complete identity
name or SHA-1:

```sh
./scripts/release-macos.sh \
  --identity "Developer ID Application: OWNER NAME (TEAMID)"
```

The script:

1. runs the normal release build and tests;
2. stages a separate bundle under `dist/release-macos/`;
3. copies the installed Qt SPDX manifests, Chromium top-level license, exact
   notices for loose Homebrew runtime libraries, common LGPL/GPL texts from the
   exact QtBase source archive, the FreeType License from its exact source
   archive, raw Homebrew source metadata, and an LGPL corresponding-source
   offer; the build fails if a deployed loose library has no inventory mapping;
4. signs Mach-O files, the Qt WebEngine helper, Qt frameworks, and the outer
   application in inside-out order;
5. applies Hardened Runtime, secure timestamps, and the required WebEngine,
   camera, and microphone entitlements;
6. rejects signatures containing `com.apple.security.get-task-allow`;
7. verifies Developer ID authority, Team ID, and secure timestamps for every
   Mach-O file as well as the nested and outer bundle signatures;
8. records whether the source tree was clean or dirty;
9. records the configured macOS deployment target in build provenance;
10. produces a versioned ZIP, SHA-256 checksum, build provenance, and bundle
   audit. Without `--notarize`, the archive name ends in
   `-signed-unnotarized.zip` so its Gatekeeper status cannot be mistaken for a
   notarized release.

Use `--skip-build` only when `dist/PanBrowser.app` was produced from the exact
commit being released:

```sh
./scripts/release-macos.sh --skip-build
```

## 4. Distribute a signed but unnotarized build

When the Apple team is not yet enabled for notarization, the default script
output can be used as an explicitly unnotarized Developer ID-signed build:

```sh
./scripts/release-macos.sh
```

The `v0.1.0` macOS ARM64 candidate uses this temporary distribution mode. It is
not equivalent to notarization and remains gated by the binary-distribution
checklist in `LICENSING.md`. Release notes and direct-download instructions
must clearly say that the build is signed but not notarized.

After downloading it, a user may need to attempt the first launch once and
then open **System Settings > Privacy & Security** and choose **Open Anyway**.
If Finder offers **Open** in the application's context menu, that is also an
acceptable user-authorized path. Do not advise users to disable Gatekeeper or
remove quarantine attributes.

## 5. Notarize and staple

After the Keychain profile is configured, run:

```sh
./scripts/release-macos.sh \
  --notarize \
  --notary-profile "PanBrowser-notary"
```

Alternatively, keep the non-secret profile name in the environment:

```sh
PANBROWSER_NOTARY_PROFILE="PanBrowser-notary" \
  ./scripts/release-macos.sh --notarize
```

The script submits a ZIP with `notarytool`, waits for a terminal result, saves
the JSON response, retrieves the service log on rejection, staples and
validates an accepted ticket, runs Gatekeeper assessment, and recreates the
final ZIP after stapling.

Apple status code `7000` with `Team is not yet configured for notarization`
is an account-side rejection. Re-submitting the same binary does not fix it;
Developer Programs Support must enable notarization for the team. Until then,
use the explicitly unnotarized workflow above or publish a source-only tag.

## 6. Release verification

Before publishing a candidate:

1. finish every item in [LICENSING.md](LICENSING.md), including complete
   version-matched Qt and Chromium notices and corresponding-source handling;
2. confirm `git status` is clean and the provenance commit is the intended
   release commit;
3. download the ZIP through a browser on a clean macOS account or machine so
   it receives a quarantine attribute;
4. for a notarized release, launch without a Gatekeeper override; for an
   explicitly unnotarized release, confirm the documented warning and Open
   Anyway flow match the target macOS versions;
5. smoke-test navigation, trust rules, downloads, HTTP authentication and, with
   a disposable entry, the saved-credentials manager,
   camera/microphone/location permissions, Site Connections, animated and
   pinned tabs, Reader Mode detection/exit and appearance persistence, video
   pop-out, and an installed web app;
6. when the target version exposes VOT integration, select the exact supported
   upstream userscript from outside the bundle, verify System DNS and
   Site Connections behavior, then confirm that neither that source file nor
   `vot-storage.json` was copied into the release archive;
7. compare the ZIP checksum with the committed or published checksum;
8. create and push the annotated `v<version>` tag only after verification.

The release script deliberately keeps the ordinary `build-app.sh` workflow
ad-hoc signed. Developer ID signing and notarization are explicit release
operations and must never be triggered by a routine developer build.
