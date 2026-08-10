#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
source_bundle="$project_dir/dist/PanBrowser.app"
release_dir="$project_dir/dist/release-macos"
staged_bundle="$release_dir/PanBrowser.app"
main_entitlements="$project_dir/packaging/macos/PanBrowser.entitlements"
helper_entitlements="$project_dir/packaging/macos/QtWebEngineProcess.entitlements"
runtime_license_collector="$project_dir/scripts/collect-macos-runtime-licenses.sh"

codesign_identity="${PANBROWSER_CODESIGN_IDENTITY:-}"
notary_profile="${PANBROWSER_NOTARY_PROFILE:-}"
notarize=false
skip_build=false

usage() {
    cat <<'EOF'
Usage: scripts/release-macos.sh [options]

Build and Developer ID-sign a macOS PanBrowser release candidate. With
--notarize, submit it to Apple's notary service and staple the accepted ticket.

Options:
  --identity NAME_OR_SHA1  Developer ID Application identity. If omitted, the
                           only unique Developer ID identity is selected.
  --notarize               Submit, wait for acceptance, and staple the ticket.
  --notary-profile NAME    notarytool Keychain profile used with --notarize.
  --skip-build             Reuse dist/PanBrowser.app instead of rebuilding.
  -h, --help               Show this help.

Environment equivalents:
  PANBROWSER_CODESIGN_IDENTITY
  PANBROWSER_NOTARY_PROFILE

This script never accepts an Apple password or API key. Store credentials in
the macOS Keychain with `xcrun notarytool store-credentials` first.
EOF
}

fail() {
    echo "error: $*" >&2
    exit 1
}

require_value() {
    local option_name="$1"
    local option_value="${2:-}"
    [[ -n "$option_value" ]] || fail "$option_name requires a value"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --identity)
            require_value "$1" "${2:-}"
            codesign_identity="$2"
            shift 2
            ;;
        --notarize)
            notarize=true
            shift
            ;;
        --notary-profile)
            require_value "$1" "${2:-}"
            notary_profile="$2"
            shift 2
            ;;
        --skip-build)
            skip_build=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
done

[[ "$(uname -s)" == "Darwin" ]] || fail "this script must run on macOS"
[[ -f "$main_entitlements" ]] || fail "missing $main_entitlements"
[[ -f "$helper_entitlements" ]] || fail "missing $helper_entitlements"
[[ -x "$runtime_license_collector" ]] || fail "missing executable $runtime_license_collector"

for required_command in \
    brew cmake codesign ditto file git lipo otool plutil security shasum spctl \
    xcodebuild xcrun; do
    command -v "$required_command" >/dev/null 2>&1 \
        || fail "required command is unavailable: $required_command"
done

temporary_dir="$(mktemp -d "${TMPDIR:-/tmp}/panbrowser-release.XXXXXX")"
cleanup() {
    if [[ -n "${temporary_dir:-}" && -d "$temporary_dir" ]]; then
        cmake -E remove_directory "$temporary_dir"
    fi
}
trap cleanup EXIT

identity_list="$temporary_dir/developer-id-identities.txt"
security find-identity -v -p codesigning \
    | sed -n 's/^[[:space:]]*[0-9]*) \([0-9A-F][0-9A-F]*\) "\(Developer ID Application:.*\)"/\1\t\2/p' \
    | sort -u > "$identity_list"

codesign_identity_hash=""
codesign_identity_name=""
if [[ -z "$codesign_identity" ]]; then
    identity_count="$(wc -l < "$identity_list" | tr -d '[:space:]')"
    if [[ "$identity_count" != "1" ]]; then
        echo "Available Developer ID Application identities:" >&2
        sed 's/^/  /' "$identity_list" >&2
        fail "pass --identity when there is not exactly one unique identity"
    fi
    IFS=$'\t' read -r codesign_identity_hash codesign_identity_name \
        < "$identity_list"
else
    while IFS=$'\t' read -r candidate_hash candidate_name; do
        if [[ "$codesign_identity" == "$candidate_hash" \
            || "$codesign_identity" == "$candidate_name" ]]; then
            codesign_identity_hash="$candidate_hash"
            codesign_identity_name="$candidate_name"
            break
        fi
    done < "$identity_list"
fi

[[ -n "$codesign_identity_hash" && -n "$codesign_identity_name" ]] \
    || fail "Developer ID Application identity is unavailable: $codesign_identity"
codesign_identity="$codesign_identity_name"

if [[ "$notarize" == true && -z "$notary_profile" ]]; then
    fail "--notarize requires --notary-profile or PANBROWSER_NOTARY_PROFILE"
fi
if [[ "$notarize" == true ]]; then
    xcrun notarytool history --keychain-profile "$notary_profile" >/dev/null \
        || fail "notarytool Keychain profile is unavailable: $notary_profile"
fi

if [[ "$skip_build" == false ]]; then
    "$project_dir/scripts/build-app.sh"
fi
[[ -d "$source_bundle" ]] || fail "application bundle is missing: $source_bundle"

cmake -E remove_directory "$release_dir"
cmake -E make_directory "$release_dir"
ditto "$source_bundle" "$staged_bundle"
ditto "$project_dir/docs/RELEASING.md" \
    "$staged_bundle/Contents/Resources/Documentation/RELEASING.md"

build_cache="$project_dir/build/CMakeCache.txt"
[[ -f "$build_cache" ]] || fail "CMake cache is missing: $build_cache"

cache_value() {
    local cache_key="$1"
    local cache_line=""
    while IFS= read -r cache_line; do
        case "$cache_line" in
            "$cache_key":*)
                printf '%s\n' "${cache_line#*=}"
                return 0
                ;;
        esac
    done < "$build_cache"
    return 1
}

macdeployqt_path="$(cache_value MACDEPLOYQT_EXECUTABLE)" \
    || fail "MACDEPLOYQT_EXECUTABLE is missing from the CMake cache"
qtpaths_path="$(dirname "$macdeployqt_path")/qtpaths"
[[ -x "$qtpaths_path" ]] || fail "matching qtpaths is unavailable: $qtpaths_path"

qt_version="$("$qtpaths_path" --qt-version)"
qt_archdata="$("$qtpaths_path" --query QT_INSTALL_ARCHDATA)"
deployment_target="$(cache_value CMAKE_OSX_DEPLOYMENT_TARGET)" \
    || fail "CMAKE_OSX_DEPLOYMENT_TARGET is missing from the CMake cache"
[[ -n "$deployment_target" ]] \
    || fail "CMAKE_OSX_DEPLOYMENT_TARGET must not be empty for a release"
qt_sbom_dir="$qt_archdata/sbom"
qt_notice_dir="$staged_bundle/Contents/Resources/Documentation/ThirdParty/Qt"
qt_sbom_target="$qt_notice_dir/SBOM"
cmake -E make_directory "$qt_sbom_target"

sbom_count=0
for sbom_file in "$qt_sbom_dir"/*-"$qt_version".spdx; do
    [[ -f "$sbom_file" ]] || continue
    ditto "$sbom_file" "$qt_sbom_target/$(basename "$sbom_file")"
    sbom_count=$((sbom_count + 1))
done
[[ "$sbom_count" -gt 0 ]] \
    || fail "no Qt $qt_version SPDX documents found in $qt_sbom_dir"

qt_webengine_cmake_dir="$(cache_value Qt6WebEngineCore_DIR)" \
    || fail "Qt6WebEngineCore_DIR is missing from the CMake cache"
qt_webengine_prefix="${qt_webengine_cmake_dir%/lib/cmake/Qt6WebEngineCore}"
chromium_license="$qt_webengine_prefix/LICENSE.Chromium"
[[ -f "$chromium_license" ]] || fail "Qt Chromium license is missing: $chromium_license"
ditto "$chromium_license" "$qt_notice_dir/LICENSE.Chromium"

runtime_notice_dir="$staged_bundle/Contents/Resources/Documentation/ThirdParty/Runtime"
"$runtime_license_collector" "$staged_bundle" "$runtime_notice_dir"

version="$(plutil -extract CFBundleShortVersionString raw -o - \
    "$staged_bundle/Contents/Info.plist")"
cat > "$staged_bundle/Contents/Resources/Documentation/QT-LICENSE-SELECTION.txt" <<EOF
PanBrowser $version uses the dynamically linked open-source Qt libraries in
this package under the GNU Lesser General Public License version 3. Chromium
and other third-party components embedded by Qt WebEngine retain their own
licenses, including components under the GNU Lesser General Public License
version 2.1. See ThirdParty/Qt/SBOM, ThirdParty/Qt/LICENSE.Chromium, and
ThirdParty/Runtime for the version-matched inventory and notices.

PanBrowser itself is licensed under the Apache License 2.0. Recipients may
replace the dynamically linked Qt frameworks with ABI-compatible modified
versions, subject to macOS code-signing and Gatekeeper requirements.
EOF
cat > "$staged_bundle/Contents/Resources/Documentation/SOURCE-OFFER.txt" <<EOF
PanBrowser $version — LGPL Corresponding Source Offer

For at least three years after the last distribution of this release, the
PanBrowser distributor offers any recipient of this binary the complete
machine-readable corresponding source code required by the LGPL for the exact
Qt, Qt WebEngine/Chromium, and other LGPL-covered libraries included here,
including distributor modifications and the information needed to rebuild and
replace those libraries. The source will be provided by download or, on
request, on a physical medium for no more than the reasonable cost of delivery.

Request the source by opening an issue at:
https://github.com/tavvet/pan-browser/issues

The exact versions, upstream source URLs, checksums, Homebrew formula metadata,
Qt SPDX documents, and included license texts are packaged under
Contents/Resources/Documentation/ThirdParty.
EOF
main_binary="$staged_bundle/Contents/MacOS/PanBrowser"
helper_app="$staged_bundle/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app"
helper_binary="$helper_app/Contents/MacOS/QtWebEngineProcess"
[[ -x "$main_binary" ]] || fail "main executable is missing: $main_binary"
[[ -x "$helper_binary" ]] || fail "Qt WebEngine helper is missing: $helper_binary"

# Older local packages added this link as a workaround before the helper's
# Homebrew paths were fully rewritten. It escapes the nested helper bundle and
# therefore fails strict code-signature validation. Current macdeployqt output
# uses direct @loader_path references to the outer Frameworks directory.
helper_framework_link="$helper_app/Contents/Frameworks"
if [[ -L "$helper_framework_link" ]]; then
    cmake -E rm -f "$helper_framework_link"
elif [[ -e "$helper_framework_link" ]]; then
    fail "unexpected real Frameworks entry in Qt WebEngine helper"
fi
if otool -L "$helper_binary" | grep -Eq '/opt/homebrew|/usr/local'; then
    fail "Qt WebEngine helper still contains build-machine library paths"
fi

commit="$(git -C "$project_dir" rev-parse HEAD)"
git_tree_state="clean"
if [[ -n "$(git -C "$project_dir" status --porcelain --untracked-files=all)" ]]; then
    git_tree_state="dirty"
fi
build_utc="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
cat > "$staged_bundle/Contents/Resources/Documentation/BUILD-PROVENANCE.txt" <<EOF
PanBrowser version: $version
Git commit: $commit
Git tree state: $git_tree_state
Qt version: $qt_version
macOS architecture: $(uname -m)
macOS deployment target: $deployment_target
Xcode: $(xcodebuild -version | tr '\n' ' ' | sed 's/[[:space:]]*$//')
Built at UTC: $build_utc
Signing identity: $codesign_identity
EOF

release_notice="$staged_bundle/Contents/Resources/Documentation/RELEASE-CANDIDATE-NOTICE.txt"
if [[ "$notarize" == true ]]; then
    cat > "$release_notice" <<'EOF'
This package is signed with a Developer ID Application certificate and was
prepared for Apple notarization. Only the archive created after the notary
service accepts the submission and the ticket is stapled is notarized.

Before publishing it, complete and verify the binary-distribution checklist in
LICENSING.md, including the full version-matched Qt and Chromium license
material and the corresponding-source offer. Apple notarization is a security
scan, not a licensing review.
EOF
else
    cat > "$release_notice" <<'EOF'
This package is signed with a Developer ID Application certificate but is NOT
notarized by Apple and does not contain a stapled notary ticket. macOS may block
its first launch after download. Keep Gatekeeper enabled and use the documented
Open Anyway flow in System Settings if you trust the download source.

Before publishing it, complete and verify the binary-distribution checklist in
LICENSING.md, including the full version-matched Qt and Chromium license
material and the corresponding-source offer. Code signing is not a licensing
review.
EOF
fi

sign_code() {
    codesign --force --options runtime --timestamp \
        --sign "$codesign_identity_hash" "$1"
}

while IFS= read -r -d '' candidate; do
    if [[ "$candidate" == "$main_binary" || "$candidate" == "$helper_binary" ]]; then
        continue
    fi
    case "$candidate" in
        "$staged_bundle"/Contents/Frameworks/*.framework/*)
            # Signing the framework bundle below signs its primary binary.
            continue
            ;;
    esac
    if file -b "$candidate" | grep -q 'Mach-O'; then
        sign_code "$candidate"
    fi
done < <(find "$staged_bundle/Contents" -type f -print0)

codesign --force --options runtime --timestamp \
    --entitlements "$helper_entitlements" \
    --sign "$codesign_identity_hash" "$helper_app"

while IFS= read -r -d '' framework; do
    sign_code "$framework"
done < <(find "$staged_bundle/Contents/Frameworks" -type d -name '*.framework' -print0)

codesign --force --options runtime --timestamp \
    --entitlements "$main_entitlements" \
    --sign "$codesign_identity_hash" "$staged_bundle"

codesign --verify --deep --strict --verbose=1 "$staged_bundle"
codesign --verify --strict --verbose=1 "$helper_app"

main_signature_info="$(codesign -d --verbose=4 "$staged_bundle" 2>&1)"
expected_team_identifier="$(printf '%s\n' "$main_signature_info" \
    | sed -n 's/^TeamIdentifier=//p')"
[[ -n "$expected_team_identifier" ]] \
    || fail "main signature does not contain a TeamIdentifier"

verify_signature_metadata() {
    local code_path="$1"
    local signature_info=""
    signature_info="$(codesign -d --verbose=4 "$code_path" 2>&1)"
    printf '%s\n' "$signature_info" \
        | grep -Fqx "Authority=$codesign_identity" \
        || fail "unexpected signing authority: $code_path"
    printf '%s\n' "$signature_info" \
        | grep -Fqx "TeamIdentifier=$expected_team_identifier" \
        || fail "unexpected signing team: $code_path"
    printf '%s\n' "$signature_info" | grep -q '^Timestamp=' \
        || fail "secure timestamp is missing: $code_path"
}

while IFS= read -r -d '' candidate; do
    if file -b "$candidate" | grep -q 'Mach-O'; then
        verify_signature_metadata "$candidate"
    fi
done < <(find "$staged_bundle/Contents" -type f -print0)
verify_signature_metadata "$helper_app"
verify_signature_metadata "$staged_bundle"

main_signed_entitlements="$temporary_dir/main-entitlements.plist"
helper_signed_entitlements="$temporary_dir/helper-entitlements.plist"
codesign -d --entitlements :- "$staged_bundle" \
    > "$main_signed_entitlements" 2>/dev/null
codesign -d --entitlements :- "$helper_app" \
    > "$helper_signed_entitlements" 2>/dev/null

for entitlement in \
    com.apple.security.device.camera \
    com.apple.security.device.microphone; do
    grep -Fq "<key>$entitlement</key>" "$main_signed_entitlements" \
        || fail "main signature is missing entitlement: $entitlement"
    grep -Fq "<key>$entitlement</key>" "$helper_signed_entitlements" \
        || fail "Qt WebEngine helper is missing entitlement: $entitlement"
done
for entitlement in \
    com.apple.security.cs.allow-jit \
    com.apple.security.cs.allow-unsigned-executable-memory \
    com.apple.security.cs.disable-executable-page-protection \
    com.apple.security.cs.disable-library-validation; do
    grep -Fq "<key>$entitlement</key>" "$helper_signed_entitlements" \
        || fail "Qt WebEngine helper is missing entitlement: $entitlement"
done
if grep -Fq '<key>com.apple.security.get-task-allow</key>' \
    "$main_signed_entitlements" "$helper_signed_entitlements"; then
    fail "release signatures must not contain com.apple.security.get-task-allow"
fi

architecture="$(lipo -archs "$main_binary")"
case " $architecture " in
    *' arm64 '*' x86_64 '*|*' x86_64 '*' arm64 '*) architecture_label="universal2" ;;
    *) architecture_label="${architecture// /-}" ;;
esac

archive_base="PanBrowser-$version-macOS-$architecture_label"
submission_archive="$temporary_dir/$archive_base-submission.zip"
final_archive="$release_dir/$archive_base-signed-unnotarized.zip"
ditto -c -k --keepParent "$staged_bundle" "$submission_archive"

if [[ "$notarize" == true ]]; then
    notary_response="$release_dir/notarytool-response.json"
    if ! xcrun notarytool submit "$submission_archive" \
        --keychain-profile "$notary_profile" \
        --wait \
        --output-format json > "$notary_response"; then
        if plutil -extract id raw -o - "$notary_response" \
            > "$temporary_dir/submission-id.txt" 2>/dev/null; then
            submission_id="$(cat "$temporary_dir/submission-id.txt")"
            notary_log="$release_dir/notarytool-log.json"
            xcrun notarytool log "$submission_id" \
                --keychain-profile "$notary_profile" "$notary_log" || true
            fail "notarization command failed; inspect $notary_log"
        fi
        fail "notarization command failed before returning a submission ID"
    fi
    cat "$notary_response"

    notary_status="$(plutil -extract status raw -o - "$notary_response")"
    submission_id="$(plutil -extract id raw -o - "$notary_response")"
    if [[ "$notary_status" != "Accepted" ]]; then
        notary_log="$release_dir/notarytool-log.json"
        xcrun notarytool log "$submission_id" \
            --keychain-profile "$notary_profile" "$notary_log" || true
        fail "notarization was not accepted; inspect $notary_log"
    fi

    xcrun stapler staple -v "$staged_bundle"
    xcrun stapler validate -v "$staged_bundle"
    spctl --assess --type execute --verbose=4 "$staged_bundle"
    final_archive="$release_dir/$archive_base.zip"
fi

ditto -c -k --keepParent "$staged_bundle" "$final_archive"
shasum -a 256 "$final_archive" \
    | sed "s#  .*#  $(basename "$final_archive")#" \
    > "$final_archive.sha256"

cmake \
    -DBUNDLE_ROOT="$staged_bundle" \
    -DBUNDLE_LABEL="$archive_base" \
    -DOUTPUT_JSON="$release_dir/bundle-audit.json" \
    -DOUTPUT_MARKDOWN="$release_dir/bundle-audit.md" \
    -P "$project_dir/scripts/audit-bundle.cmake"

echo
echo "Signed application: $staged_bundle"
echo "Archive: $final_archive"
echo "Checksum: $final_archive.sha256"
if [[ "$notarize" == false ]]; then
    echo "Notarization was not requested; this archive is explicitly unnotarized."
fi
