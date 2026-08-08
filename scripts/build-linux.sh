#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${PANBROWSER_BUILD_DIR:-$project_dir/build-linux}"
dist_dir="${PANBROWSER_DIST_DIR:-$project_dir/dist}"
architecture="${PANBROWSER_ARCHITECTURE:-$(uname -m)}"
package_name="PanBrowser-linux-$architecture"
package_dir="$dist_dir/$package_name"
archive="$dist_dir/$package_name.tar.gz"

for command_name in cmake ctest ldd ninja; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Required command not found: $command_name" >&2
        exit 1
    fi
done

cmake_arguments=(
    -S "$project_dir"
    -B "$build_dir"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
)
if [[ -n "${QT_ROOT:-}" ]]; then
    cmake_arguments+=("-DCMAKE_PREFIX_PATH=$QT_ROOT")
fi

cmake "${cmake_arguments[@]}"
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

mkdir -p "$dist_dir"
cmake -E remove_directory "$package_dir"
cmake -E rm -f "$archive"
cmake --install "$build_dir" --prefix "$package_dir"

executable="$package_dir/bin/PanBrowser"
if [[ ! -x "$executable" ]]; then
    echo "Deployment is missing the PanBrowser executable: $executable" >&2
    exit 1
fi
if ! find "$package_dir" -type f -name QtWebEngineProcess -perm -u+x -print -quit \
    | grep -q .; then
    echo "Deployment is missing QtWebEngineProcess" >&2
    exit 1
fi
if ! find "$package_dir" -type f -name qtwebengine_resources.pak -print -quit \
    | grep -q .; then
    echo "Deployment is missing Qt WebEngine resources" >&2
    exit 1
fi
if ! find "$package_dir" -type f -path '*/qtwebengine_locales/*.pak' -print -quit \
    | grep -q .; then
    echo "Deployment is missing Qt WebEngine locales" >&2
    exit 1
fi

dependency_report="$(ldd "$executable")"
if [[ "$dependency_report" == *"not found"* ]]; then
    echo "Deployment has unresolved runtime dependencies:" >&2
    echo "$dependency_report" >&2
    exit 1
fi

(
    cd "$dist_dir"
    cmake -E tar czf "$archive" --format=gnutar "$package_name"
)

echo "$archive"
