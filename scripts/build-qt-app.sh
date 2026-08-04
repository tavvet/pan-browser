#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$project_dir/build-qt"
destination="$project_dir/dist/PanBrowser-Qt.app"

# macdeployqt mutates the generated bundle and installs a helper symlink.
# Reusing that deployed bundle makes a later deployment recurse through itself.
cmake -E remove_directory "$build_dir/PanBrowser.app"
cmake -S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

/opt/homebrew/opt/qtbase/bin/macdeployqt \
    "$build_dir/PanBrowser.app" \
    -always-overwrite \
    -codesign=-

helper_contents="$build_dir/PanBrowser.app/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents"
helper_binary="$helper_contents/MacOS/QtWebEngineProcess"

# Homebrew distributes Qt modules across formulae, so macdeployqt needs a
# second pass to rewrite the nested Chromium helper's absolute dependencies.
/opt/homebrew/opt/qtbase/bin/macdeployqt \
    "$build_dir/PanBrowser.app" \
    -always-overwrite \
    -codesign=- \
    -executable="$helper_binary"

# Some Qt framework dependencies are resolved relative to the nested helper.
if [[ ! -e "$helper_contents/Frameworks" ]]; then
    ln -s ../../../../../.. "$helper_contents/Frameworks"
fi

/usr/bin/codesign --force --deep --sign - "$build_dir/PanBrowser.app"
# A second pass seals dylibs that macdeployqt may have touched while signing
# their containing frameworks.
/usr/bin/codesign --force --deep --sign - "$build_dir/PanBrowser.app"
/usr/bin/codesign --verify --deep --strict "$build_dir/PanBrowser.app"

mkdir -p "$project_dir/dist"
cmake -E remove_directory "$destination"
/usr/bin/ditto "$build_dir/PanBrowser.app" "$destination"

echo "$destination"
