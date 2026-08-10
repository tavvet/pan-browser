#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$project_dir/build"
destination="$project_dir/dist/PanBrowser.app"
qt_root="${QT_ROOT:-}"

cmake_arguments=(
    -S "$project_dir"
    -B "$build_dir"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0
)
if [[ -n "$qt_root" ]]; then
    cmake_arguments+=("-DCMAKE_PREFIX_PATH=$qt_root")
fi

# macdeployqt mutates the generated bundle and installs a helper symlink.
# Reusing that deployed bundle makes a later deployment recurse through itself.
cmake -E remove_directory "$build_dir/PanBrowser.app"
cmake "${cmake_arguments[@]}"

macdeployqt_path=""
while IFS= read -r cache_line; do
    case "$cache_line" in
        MACDEPLOYQT_EXECUTABLE:FILEPATH=*)
            macdeployqt_path="${cache_line#*=}"
            break
            ;;
    esac
done < "$build_dir/CMakeCache.txt"
if [[ ! -x "$macdeployqt_path" ]]; then
    echo "CMake did not select an executable macdeployqt: $macdeployqt_path" >&2
    exit 1
fi

cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

"$macdeployqt_path" \
    "$build_dir/PanBrowser.app" \
    -always-overwrite \
    -codesign=-

helper_contents="$build_dir/PanBrowser.app/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents"
helper_binary="$helper_contents/MacOS/QtWebEngineProcess"

# Homebrew distributes Qt modules across formulae, so macdeployqt needs a
# second pass to rewrite the nested Chromium helper's absolute dependencies.
"$macdeployqt_path" \
    "$build_dir/PanBrowser.app" \
    -always-overwrite \
    -codesign=- \
    -executable="$helper_binary"

mkdir -p "$project_dir/dist"
cmake -E remove_directory "$destination"
/usr/bin/ditto "$build_dir/PanBrowser.app" "$destination"

# Sign the final copied bundle after deployment has finished mutating it.
/usr/bin/codesign --force --deep --sign - "$destination"
# A second pass seals dylibs signed during the first deep traversal inside
# their containing Qt frameworks.
/usr/bin/codesign --force --deep --sign - "$destination"
/usr/bin/codesign --verify --deep --strict "$destination"

echo "$destination"
