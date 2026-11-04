#!/usr/bin/env bash
# Run from a terminal in the guest desktop, not the SSH/agent terminal.
set -euo pipefail

usage() {
    printf '%s\n' \
        'Usage: bash scripts/test-vscode-input.sh MODE [BACKEND]' \
        'MODE: editcontext | textarea | accessible' \
        'BACKEND: wayland (default) | x11 (XWayland on a Wayland desktop)' \
        'Creates a fresh temporary profile, disables extensions, and waits for its window to close.' \
        'Does not modify the normal VS Code profile, IM settings, or desktop launcher.' \
        'Temporary profile is retained for inspection; its path is printed at startup.' \
        'TMPDIR may select an existing absolute temporary directory (default /tmp).' \
        'Optional OPENKEY_VSCODE_BIN must name a local VS Code launcher executable.'
}
if [[ $# == 1 && (${1:-} == --help || ${1:-} == -h) ]]; then usage; exit 0; fi
if [[ $# -lt 1 || $# -gt 2 ]]; then usage >&2; exit 2; fi
mode=$1
backend=${2:-wayland}
case "$mode" in editcontext|textarea|accessible) ;; *) usage >&2; exit 2 ;; esac
case "$backend" in wayland|x11) ;; *) usage >&2; exit 2 ;; esac
if [[ $backend == wayland && -z ${WAYLAND_DISPLAY:-} ]] ||
   [[ $backend == x11 && -z ${DISPLAY:-} ]]; then
    printf 'No %s display in this terminal. Run inside the Ubuntu desktop; do not guess/export a display.\n' "$backend" >&2
    exit 2
fi
repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
vscode_bin=${OPENKEY_VSCODE_BIN:-/usr/share/code/bin/code}
if [[ ! -x $vscode_bin ]]; then
    printf 'Local VS Code launcher not found: %s\n' "$vscode_bin" >&2
    exit 2
fi
profile_root=${TMPDIR:-/tmp}
if [[ $profile_root != /* || ! -d $profile_root || ! -w $profile_root ]]; then
    printf 'TMPDIR must be an existing writable absolute directory.\n' >&2
    exit 2
fi
umask 077
profile_dir=$(mktemp -d "$profile_root/openkey-vscode-input.XXXXXXXX")
mkdir -p -- "$profile_dir/User" "$profile_dir/extensions"
install -m 600 "$repo_dir/Tests/fixtures/vscode-input/$mode.json" "$profile_dir/User/settings.json"
install -m 600 "$repo_dir/Tests/fixtures/vscode-input/input-test.js" "$profile_dir/input-test.js"
printf 'OpenKey case: %s / %s\nProfile: %s\nStarted: %s\n' \
    "$mode" "$backend" "$profile_dir" "$(date '+%F %T %z')"
printf '%s\n' \
    'Select OpenKey Telex + Direct and enable Direct diagnostics in Fcitx5.' \
    'Type coongj hoaf inside the empty quotes. Record accents, underline, and completion separately.' \
    'Close only the test window when finished. Settings here are experimental, not a promised fix.'
status=0
"$vscode_bin" --ozone-platform="$backend" --user-data-dir="$profile_dir" \
    --extensions-dir="$profile_dir/extensions" --disable-extensions --new-window \
    --skip-welcome --skip-release-notes --wait --goto "$profile_dir/input-test.js:3:17" || status=$?
printf 'Finished: %s (exit %s)\n' "$(date '+%F %T %z')" "$status"
exit "$status"
