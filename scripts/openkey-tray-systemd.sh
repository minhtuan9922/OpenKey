#!/usr/bin/env bash
# Write an opt-in unit only. Activation/migration is explicitly done by the user.
set -euo pipefail
fail() { printf '%s\n' "$*" >&2; exit 2; }
if [[ ${1:-} == --help ]]; then
    printf '%s\n' 'Usage: openkey-tray-systemd --install /absolute/path/openkey-service' \
        'Writes the user unit only; does not enable, start, stop or change desktop autostart.'
    exit 0
fi
[[ $# == 2 && $1 == --install ]] || fail 'Usage: openkey-tray-systemd --install /absolute/path/openkey-service'
binary=$2
[[ $binary =~ ^/[a-zA-Z0-9_./\ -]+$ ]] || fail 'Unsupported binary path: use ASCII letters, digits, space, /, ., _ or -'
[[ -f $binary && -x $binary ]] || fail 'Service binary is missing or not executable'
config_root=${XDG_CONFIG_HOME:-${HOME:?HOME is required}/.config}
[[ $config_root == /* && $config_root != / ]] || fail 'XDG_CONFIG_HOME must be an absolute non-root path'
directory=$config_root/systemd/user
entry=$directory/openkey-tray.service
marker='# Managed by openkey-tray-systemd v1'
[[ ! -L $config_root/systemd && ! -L $directory && ! -L $entry ]] || fail 'Refusing symlink unit directory/entry'
if [[ -e $entry ]]; then
    [[ -f $entry && -O $entry ]] || fail 'Unit must be a regular file owned by this user'
    IFS= read -r first_line < "$entry" || true
    [[ $first_line == "$marker" ]] || fail 'Existing unit is not managed by this tool; left unchanged'
fi
umask 077
mkdir -p -- "$directory"
temporary=$(mktemp "$directory/.openkey-tray.XXXXXXXX")
trap 'if [[ -n ${temporary:-} ]]; then rm -f -- "$temporary"; fi' EXIT
{
    printf '%s\n' "$marker" '[Unit]' 'Description=OpenKey session tray' \
        'PartOf=graphical-session.target' 'After=graphical-session-pre.target' \
        'StartLimitIntervalSec=60' 'StartLimitBurst=5' '' '[Service]' 'Type=simple'
    printf 'ExecStart="%s" --run-tray\n' "$binary"
    printf '%s\n' 'Restart=on-failure' 'RestartSec=3' 'RestartPreventExitStatus=3' \
        'TimeoutStopSec=5' 'UMask=0077' 'NoNewPrivileges=true' \
        'StandardOutput=journal' 'StandardError=journal' '' '[Install]' \
        'WantedBy=graphical-session.target'
} > "$temporary"
mv -T -- "$temporary" "$entry"
temporary=
printf 'Unit written (not activated): %s\n' "$entry"
