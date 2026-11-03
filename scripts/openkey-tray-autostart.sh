#!/usr/bin/env bash
# Opt-in XDG session startup; never starts/stops the service or Fcitx itself.
set -euo pipefail
fail() { printf '%s\n' "$*" >&2; exit 2; }
usage() { printf '%s\n' 'Usage: openkey-tray-autostart --enable /absolute/path/openkey-service | --disable | --status'; }
action=${1:-}
case "$action" in
    --help) usage; exit 0 ;;
    --enable) [[ $# == 2 ]] || { usage; exit 2; } ;;
    --disable|--status) [[ $# == 1 ]] || { usage; exit 2; } ;;
    *) usage; exit 2 ;;
esac
config_root=${XDG_CONFIG_HOME:-${HOME:?HOME is required}/.config}
[[ $config_root == /* && $config_root != / ]] || fail 'XDG_CONFIG_HOME must be an absolute non-root path'
directory=$config_root/autostart
entry=$directory/openkey-tray.desktop
marker='# Managed by openkey-tray-autostart v1'
# Do not overwrite somebody else's desktop entry or follow a leaf symlink.
[[ ! -L $directory && ! -L $entry ]] || fail 'Refusing symlink autostart directory/entry'
if [[ -e $entry ]]; then
    [[ -f $entry && -O $entry ]] || fail 'Autostart entry must be a regular file owned by this user'
    IFS= read -r first_line < "$entry" || true
    [[ $first_line == "$marker" ]] || fail 'Existing entry is not managed by this tool; left unchanged'
fi
if [[ $action == --status ]]; then
    if [[ ! -f $entry ]]; then printf 'not-configured\n'
    elif grep -qx 'Hidden=true' "$entry"; then printf 'disabled\n'
    else printf 'enabled\n'; fi
    exit 0
fi
if [[ $action == --enable ]]; then
    binary=$2
    # Intentionally reject paths requiring Desktop Entry multi-level escaping.
    # Spaces are supported using a quoted Exec argument; no shell is involved.
    [[ $binary =~ ^/[a-zA-Z0-9_./\ -]+$ ]] || fail 'Use an absolute binary path containing only ASCII letters, digits, space, /, ., _ or -'
    [[ -f $binary && -x $binary ]] || fail 'Service binary is missing or not executable'
fi
umask 077
mkdir -p -- "$directory"
temporary=$(mktemp "$directory/.openkey-tray.XXXXXXXX")
trap 'if [[ -n ${temporary:-} ]]; then rm -f -- "$temporary"; fi' EXIT
{
    printf '%s\n' "$marker" '[Desktop Entry]' 'Type=Application' 'Version=1.0' \
        'Name=OpenKey Tray' 'Comment=OpenKey input status indicator' 'Terminal=false'
    if [[ $action == --enable ]]; then
        printf 'TryExec=%s\nExec="%s" --run-tray\nHidden=false\n' "$binary" "$binary"
    else
        # XDG Hidden also overrides a future system-wide entry of the same name.
        printf 'Hidden=true\n'
    fi
} > "$temporary"
mv -T -- "$temporary" "$entry"
temporary=
printf '%s: %s\n' "$action" "$entry"
