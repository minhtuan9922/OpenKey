#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_dir/build/ubuntu-beta-deb"
python3 "$repo_dir/Tests/LinuxUserUninstallTests.py"
cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DOPENKEY_BUILD_FCITX5=ON -DOPENKEY_BUILD_UI=ON -DOPENKEY_BUILD_SERVICE=ON \
    -DOPENKEY_BUILD_DEB=OFF -DOPENKEY_BUILD_UI_DEB=OFF -DOPENKEY_BUILD_BETA_DEB=ON \
    -DOPENKEY_BUILD_SMOKE_TESTS=ON -DOPENKEY_WARNINGS_AS_ERRORS=ON
cmake --build "$build_dir" --parallel 4
ctest --test-dir "$build_dir" --output-on-failure
cpack --config "$build_dir/CPackConfig.cmake"
python3 "$repo_dir/scripts/verify-linux-beta-deb.py" \
    "$repo_dir/dist/linux/openkey_3.1.0~beta.1-1_$(dpkg --print-architecture).deb" "$build_dir"
