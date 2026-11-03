#!/usr/bin/env bash
set -euo pipefail
helper=$1
runtime=$(mktemp -d /tmp/openkey-autostart-test.XXXXXXXX)
cleanup() {
    # Only fixtures created in this test's private temporary directory.
    rm -f -- "$runtime/config/autostart/openkey-tray.desktop" "$runtime/service binary" \
        "$runtime/foreign.desktop" "$runtime/config/linked-autostart"
    rmdir -- "$runtime/config/autostart" "$runtime/config" "$runtime"
}
trap cleanup EXIT
export XDG_CONFIG_HOME=$runtime/config
entry=$XDG_CONFIG_HOME/autostart/openkey-tray.desktop
expect_failure() { if bash "$helper" "$@"; then printf 'Unexpected success\n' >&2; exit 1; fi; }
[[ $(bash "$helper" --status) == not-configured ]]
[[ ! -e $XDG_CONFIG_HOME ]]
expect_failure --enable relative
expect_failure --enable "$runtime/missing"
expect_failure --enable "$runtime/bad%f"
expect_failure --disable extra
expect_failure --unknown
ln -s /usr/bin/true "$runtime/service binary"
bash "$helper" --enable "$runtime/service binary"
[[ $(bash "$helper" --status) == enabled ]]
grep -Fx "Exec=\"$runtime/service binary\" --run-tray" "$entry"
[[ $(stat -c %a "$entry") == 600 ]]
if command -v desktop-file-validate >/dev/null; then desktop-file-validate "$entry"; fi
bash "$helper" --enable "$runtime/service binary"
bash "$helper" --disable
[[ $(bash "$helper" --status) == disabled ]]
if command -v desktop-file-validate >/dev/null; then desktop-file-validate "$entry"; fi
! grep -q '^Exec=' "$entry"
bash "$helper" --enable "$runtime/service binary"
# Unknown files and symlinks must remain untouched.
printf 'foreign entry\n' > "$runtime/foreign.desktop"
cp -- "$runtime/foreign.desktop" "$entry"
expect_failure --enable "$runtime/service binary"
expect_failure --disable
cmp "$runtime/foreign.desktop" "$entry"
rm -- "$entry"
ln -s "$runtime/foreign.desktop" "$entry"
expect_failure --disable
[[ -L $entry ]]
rm -- "$entry"
rmdir -- "$XDG_CONFIG_HOME/autostart"
ln -s "$runtime" "$XDG_CONFIG_HOME/autostart"
expect_failure --enable "$runtime/service binary"
rm -- "$XDG_CONFIG_HOME/autostart"
mkdir -- "$XDG_CONFIG_HOME/autostart"
expect_failure --enable "$runtime/equals=sign"
if XDG_CONFIG_HOME=relative bash "$helper" --status; then exit 1; fi
printf 'Autostart opt-in, disable, paths, validation and preservation passed\n'
