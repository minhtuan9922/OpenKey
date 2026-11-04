#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_dir/build/ubuntu-ui-deb"
for tool in cmake cpack ctest ninja g++ dpkg dpkg-deb dpkg-shlibdeps desktop-file-validate; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Missing build tool: %s\n' "$tool" >&2
        exit 1
    fi
done

# Set every packaging switch explicitly; preserve existing build files.
# CPack stages only openkey_ui, and verification uses a fresh extraction tree.
cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DOPENKEY_BUILD_UI=ON -DOPENKEY_BUILD_UI_DEB=ON \
    -DOPENKEY_BUILD_SMOKE_TESTS=ON -DOPENKEY_WARNINGS_AS_ERRORS=ON \
    -DOPENKEY_BUILD_FCITX5=OFF -DOPENKEY_BUILD_SERVICE=OFF \
    -DOPENKEY_BUILD_DEB=OFF -DOPENKEY_BUILD_X11_HOTKEY=OFF
cmake --build "$build_dir" --parallel 2
ctest --test-dir "$build_dir" -j2 --output-on-failure
cpack --config "$build_dir/CPackConfig.cmake"
cmake --build "$build_dir" --target verify_linux_ui_deb
cmake --build "$build_dir" --target verify_linux_ui_deb_lifecycle
printf '\nVerified standalone UI DEB and SHA-256: %s/dist/linux/\n' "$repo_dir"
