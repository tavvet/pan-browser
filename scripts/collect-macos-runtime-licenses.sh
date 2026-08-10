#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 BUNDLE_PATH OUTPUT_DIRECTORY" >&2
    exit 2
fi

bundle_path="$1"
output_directory="$2"
frameworks_directory="$bundle_path/Contents/Frameworks"

fail() {
    echo "error: $*" >&2
    exit 1
}

[[ "$(uname -s)" == "Darwin" ]] || fail "this collector must run on macOS"
[[ -d "$frameworks_directory" ]] || fail "framework directory is missing: $frameworks_directory"
for required_command in brew ditto find shasum sort tar; do
    command -v "$required_command" >/dev/null 2>&1 \
        || fail "required command is unavailable: $required_command"
done

rm -rf "$output_directory"
mkdir -p "$output_directory/Homebrew"

temporary_directory="$(mktemp -d "${TMPDIR:-/tmp}/panbrowser-licenses.XXXXXX")"
cleanup() {
    if [[ -n "${temporary_directory:-}" && -d "$temporary_directory" ]]; then
        rm -rf "$temporary_directory"
    fi
}
trap cleanup EXIT
matched_libraries_file="$temporary_directory/matched-libraries.txt"
collected_formulas_file="$temporary_directory/collected-formulas.txt"
: > "$matched_libraries_file"
: > "$collected_formulas_file"

# Each row maps loose dylibs deployed by macdeployqt to the Homebrew formula
# that supplied them and to the notices installed by that exact formula build.
# Qt frameworks and code embedded in Qt are tracked separately by Qt's SPDX
# documents; this table covers libraries copied next to those frameworks.
runtime_components=(
    'libbrotli*.dylib|brotli|MIT|LICENSE'
    'libdbus-1*.dylib|dbus|AFL-2.1 OR GPL-2.0-or-later|COPYING'
    'libdouble-conversion*.dylib|double-conversion|BSD-3-Clause|LICENSE,COPYING'
    'libfreetype*.dylib|freetype|FTL|LICENSE.TXT'
    'libintl*.dylib|gettext|GPL-3.0-or-later AND LGPL-2.1-or-later|COPYING'
    'libglib-2*.dylib|glib|LGPL-2.1-or-later|LGPL-2.1-or-later.txt'
    'libgio-2*.dylib|glib|LGPL-2.1-or-later|LGPL-2.1-or-later.txt'
    'libgmodule-2*.dylib|glib|LGPL-2.1-or-later|LGPL-2.1-or-later.txt'
    'libgobject-2*.dylib|glib|LGPL-2.1-or-later|LGPL-2.1-or-later.txt'
    'libgthread-2*.dylib|glib|LGPL-2.1-or-later|LGPL-2.1-or-later.txt'
    'libgraphite2*.dylib|graphite2|MIT OR MPL-2.0 OR LGPL-2.1-or-later OR GPL-2.0-or-later|LICENSE,COPYING'
    'libharfbuzz*.dylib|harfbuzz|MIT|COPYING'
    'libicu*.dylib|icu4c@78|Unicode-3.0|LICENSE'
    'libjpeg*.dylib|jpeg-turbo|IJG AND Zlib AND BSD-3-Clause|LICENSE.md'
    'libb2*.dylib|libb2|CC0-1.0|COPYING'
    'libpng*.dylib|libpng|libpng-2.0|LICENSE'
    'libmd4c*.dylib|md4c|MIT|LICENSE.md'
    'libcrypto*.dylib|openssl@3|Apache-2.0|LICENSE.txt'
    'libssl*.dylib|openssl@3|Apache-2.0|LICENSE.txt'
    'libpcre2-*.dylib|pcre2|BSD-3-Clause|COPYING'
    'libzstd*.dylib|zstd|(BSD-3-Clause OR GPL-2.0-only) AND BSD-2-Clause AND MIT|LICENSE,COPYING'
)

for component in "${runtime_components[@]}"; do
    IFS='|' read -r library_pattern formula license_expression license_paths \
        <<< "$component"
    matched=false
    while IFS= read -r library; do
        [[ -n "$library" ]] || continue
        matched=true
        printf '%s|%s\n' "$(basename "$library")" "$formula" \
            >> "$matched_libraries_file"
    done < <(find "$frameworks_directory" -maxdepth 1 -type f -name "$library_pattern" -print)
    [[ "$matched" == true ]] || continue

    if grep -Fq "$formula|" "$collected_formulas_file"; then
        continue
    fi
    formula_prefix="$(brew --prefix "$formula")" \
        || fail "Homebrew formula is unavailable: $formula"
    formula_real_prefix="$(cd "$formula_prefix" && pwd -P)"
    formula_version="$(basename "$formula_real_prefix")"
    formula_directory="$output_directory/Homebrew/$formula"
    mkdir -p "$formula_directory"

    IFS=',' read -r -a notice_paths <<< "$license_paths"
    for notice_path in "${notice_paths[@]}"; do
        source_notice="$formula_real_prefix/$notice_path"
        [[ -f "$source_notice" ]] \
            || fail "$formula notice is unavailable: $source_notice"
        ditto "$source_notice" "$formula_directory/$(basename "$notice_path")"
    done

    printf '%s|%s|%s\n' "$formula" "$formula_version" "$license_expression" \
        >> "$collected_formulas_file"
done

sort -u "$matched_libraries_file" -o "$matched_libraries_file"
sort -u "$collected_formulas_file" -o "$collected_formulas_file"

unmatched_file="$output_directory/unmatched-runtime-libraries.txt"
: > "$unmatched_file"
while IFS= read -r library; do
    library_name="$(basename "$library")"
    if ! grep -Fq "$library_name|" "$matched_libraries_file"; then
        printf '%s\n' "$library_name" >> "$unmatched_file"
    fi
done < <(find "$frameworks_directory" -maxdepth 1 -type f -name '*.dylib' -print)
if [[ -s "$unmatched_file" ]]; then
    cat "$unmatched_file" >&2
    fail "unmapped runtime libraries were found; update the license inventory"
fi
rm "$unmatched_file"

common_license_directory="$output_directory/CommonLicenses"
mkdir -p "$common_license_directory"

extract_source_notice() {
    archive_path="$1"
    member_pattern="$2"
    destination_path="$3"
    [[ -f "$archive_path" ]] || fail "source archive is unavailable: $archive_path"
    archive_member="$(tar -tf "$archive_path" | grep -E "$member_pattern" || true)"
    [[ -n "$archive_member" && "$archive_member" != *$'\n'* ]] \
        || fail "expected exactly one source notice matching $member_pattern"
    tar -xOf "$archive_path" "$archive_member" > "$destination_path"
    [[ -s "$destination_path" ]] || fail "extracted source notice is empty: $destination_path"
}

qtbase_source_archive="$(brew --cache --build-from-source qtbase)"
freetype_source_archive="$(brew --cache --build-from-source freetype)"
[[ -f "$qtbase_source_archive" ]] || fail \
    "QtBase source archive is not cached; run: brew fetch --build-from-source qtbase"
[[ -f "$freetype_source_archive" ]] || fail \
    "FreeType source archive is not cached; run: brew fetch --build-from-source freetype"

for license_name in \
    GFDL-1.3-no-invariants-only \
    GPL-2.0-only \
    GPL-2.0-or-later \
    GPL-3.0-only \
    LGPL-2.1-or-later \
    LGPL-3.0-only; do
    extract_source_notice \
        "$qtbase_source_archive" \
        "/LICENSES/$license_name\\.txt$" \
        "$common_license_directory/$license_name.txt"
done
extract_source_notice \
    "$freetype_source_archive" \
    '/docs/FTL\.TXT$' \
    "$output_directory/Homebrew/freetype/FTL.TXT"

index_file="$output_directory/RUNTIME-LICENSES.md"
{
    echo '# Bundled macOS runtime libraries'
    echo
    echo 'This inventory is generated from the loose dynamic libraries in the final'
    echo 'PanBrowser application bundle. Qt frameworks and their embedded components'
    echo 'are documented separately by the packaged Qt SPDX SBOM files.'
    echo
    echo '| Homebrew formula | Version | Declared license | Bundled libraries |'
    echo '| --- | --- | --- | --- |'
    while IFS='|' read -r formula formula_version license_expression; do
        libraries=""
        while IFS='|' read -r library_name library_formula; do
            if [[ "$library_formula" == "$formula" ]]; then
                if [[ -n "$libraries" ]]; then
                    libraries="$libraries, "
                fi
                libraries="$libraries\`$library_name\`"
            fi
        done < "$matched_libraries_file"
        printf "| \`%s\` | \`%s\` | \`%s\` | %s |\n" \
            "$formula" \
            "$formula_version" \
            "$license_expression" \
            "$libraries"
    done < "$collected_formulas_file"
} > "$index_file"

qt_formulas=(
    qtbase
    qtdeclarative
    qtpositioning
    qtserialport
    qtsvg
    qttools
    qtwebchannel
    qtwebengine
)
all_formulas=("${qt_formulas[@]}")
while IFS= read -r formula; do
    all_formulas+=("$formula")
done < <(cut -d '|' -f 1 "$collected_formulas_file")

# Homebrew records the exact upstream source URLs, checksums, formula revision,
# bottle identity, and patches. Preserve the raw metadata rather than parsing a
# lossy subset into the human-readable table above.
brew info --json=v2 "${all_formulas[@]}" \
    > "$output_directory/Homebrew-formula-metadata.json"

shasum -a 256 "$qtbase_source_archive" "$freetype_source_archive" \
    | while IFS=' ' read -r checksum archive_path; do
        printf '%s  %s\n' "$checksum" "$(basename "$archive_path")"
    done > "$output_directory/license-source-archives.sha256"

formula_count="$(wc -l < "$collected_formulas_file" | tr -d '[:space:]')"
echo "Collected notices for $formula_count runtime formulae."
