#!/usr/bin/env bash
set -euo pipefail
xvfb=$1
test_binary=$2
runtime=$(mktemp -d /tmp/openkey-x11-hotkey.XXXXXXXX)
server_pid=
cleanup() {
    if [[ -n $server_pid ]]; then kill "$server_pid" 2>/dev/null || true; wait "$server_pid" 2>/dev/null || true; fi
    rm -f -- "$runtime/display" "$runtime/server.log"
    rmdir -- "$runtime"
}
trap cleanup EXIT
# -displayfd chooses a free display; never guess or use the user's DISPLAY.
"$xvfb" -displayfd 3 -screen 0 800x600x24 -nolisten tcp -noreset \
    3>"$runtime/display" >"$runtime/server.log" 2>&1 & server_pid=$!
for ((i=0; i<100; ++i)); do
    [[ -s $runtime/display ]] && break
    if ! kill -0 "$server_pid" 2>/dev/null; then sed -n '1,30p' "$runtime/server.log"; exit 1; fi
    sleep 0.02
done
read -r display_number < "$runtime/display"
[[ $display_number =~ ^[0-9]+$ ]]
export DISPLAY=:$display_number
unset WAYLAND_DISPLAY
export XDG_SESSION_TYPE=x11
export OPENKEY_ISOLATED_X11=1
"$test_binary"
