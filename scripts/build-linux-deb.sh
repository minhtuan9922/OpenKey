#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_dir/build/ubuntu-deb"
for tool in cmake cpack ctest ninja g++ dpkg dpkg-shlibdeps dpkg-deb; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Missing build tool: %s\n' "$tool" >&2
        exit 1
    fi
done

cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DOPENKEY_BUILD_FCITX5=ON -DOPENKEY_BUILD_SMOKE_TESTS=ON \
    -DOPENKEY_WARNINGS_AS_ERRORS=ON -DOPENKEY_BUILD_DEB=ON
cmake --build "$build_dir" --parallel 2
ctest --test-dir "$build_dir" --output-on-failure
cpack --config "$build_dir/CPackConfig.cmake"
cmake --build "$build_dir" --target verify_linux_deb
printf '\nVerified DEB and SHA-256: %s/dist/linux/\n' "$repo_dir"
