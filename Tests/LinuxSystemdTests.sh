#!/usr/bin/env bash
set -euo pipefail
helper=$1
runtime=$(mktemp -d /tmp/openkey-systemd-test.XXXXXXXX)
cleanup() {
    rm -f -- "$runtime/config/systemd/user/openkey-tray.service" "$runtime/service binary" "$runtime/foreign"
    rmdir -- "$runtime/config/systemd/user" "$runtime/config/systemd" "$runtime/config" "$runtime"
}
trap cleanup EXIT
export XDG_CONFIG_HOME=$runtime/config
unit=$XDG_CONFIG_HOME/systemd/user/openkey-tray.service
expect_failure() { if bash "$helper" "$@"; then exit 1; fi; }
expect_failure --install relative
expect_failure --install "$runtime/missing"
expect_failure --install "$runtime/bad%h"
[[ ! -e $XDG_CONFIG_HOME ]]
ln -s /usr/bin/true "$runtime/service binary"
bash "$helper" --install "$runtime/service binary"
[[ $(stat -c %a "$unit") == 600 ]]
grep -Fx "ExecStart=\"$runtime/service binary\" --run-tray" "$unit"
grep -qx 'Restart=on-failure' "$unit"
grep -qx 'RestartPreventExitStatus=3' "$unit"
grep -qx 'PartOf=graphical-session.target' "$unit"
grep -qx 'WantedBy=graphical-session.target' "$unit"
[[ ! -e $XDG_CONFIG_HOME/autostart && ! -e $XDG_CONFIG_HOME/systemd/user/graphical-session.target.wants ]]
if command -v systemd-analyze >/dev/null; then systemd-analyze --user verify "$unit"; fi
bash "$helper" --install "$runtime/service binary"
printf 'Foreign unit\n' > "$runtime/foreign"
cp -- "$runtime/foreign" "$unit"
expect_failure --install "$runtime/service binary"
cmp "$runtime/foreign" "$unit"
rm -- "$unit"
ln -s "$runtime/foreign" "$unit"
expect_failure --install "$runtime/service binary"
[[ -L $unit ]]
rm -- "$unit"
rmdir -- "$XDG_CONFIG_HOME/systemd/user"
ln -s "$runtime" "$XDG_CONFIG_HOME/systemd/user"
expect_failure --install "$runtime/service binary"
rm -- "$XDG_CONFIG_HOME/systemd/user"
mkdir -- "$XDG_CONFIG_HOME/systemd/user"
if XDG_CONFIG_HOME=relative bash "$helper" --install "$runtime/service binary"; then exit 1; fi
printf 'Systemd unit generation, restart policy and preservation passed\n'
