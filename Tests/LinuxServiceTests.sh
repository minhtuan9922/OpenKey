#!/usr/bin/env bash
set -euo pipefail
service=$1
runtime=$(mktemp -d /tmp/openkey-svc-test.XXXXXXXX)
pid=
cleanup() {
    if [[ -n $pid ]]; then kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; fi
    # Only test-created IPC files in this exact temporary directory.
    rm -f -- "$runtime/openkey/session.sock" "$runtime/openkey/session.lock"
    rmdir -- "$runtime/openkey" 2>/dev/null || true
    rmdir -- "$runtime"
}
trap cleanup EXIT
export XDG_RUNTIME_DIR=$runtime
# Lifecycle tests must never query/activate the real desktop Fcitx bus.
export DBUS_SESSION_BUS_ADDRESS=unix:path=$runtime/nonexistent-bus
expect_code() {
    local wanted=$1 actual=0
    shift
    "$@" || actual=$?
    [[ $actual -eq $wanted ]]
}
ready() {
    for ((i=0; i<100; i++)); do
        if "$service" --status >/dev/null; then return 0; fi
        sleep 0.02
    done
    return 1
}
expect_code 1 "$service" --status
[[ ! -e $runtime/openkey ]]
expect_code 2 "$service" --invalid
expect_code 2 "$service" --run --x11-settings-hotkey
expect_code 2 "$service" --status --x11-settings-hotkey
"$service" --run & pid=$!
ready
[[ $("$service" --status) == running ]]
[[ $("$service" --tray-status) == disabled ]]
[[ $("$service" --hotkey-status) == disabled ]]
for ((i=0; i<100; i++)); do
    input_status=$("$service" --input-status)
    [[ $input_status == unavailable ]] && break
    [[ $input_status == unknown ]]
    sleep 0.02
done
[[ $input_status == unavailable ]]
expect_code 3 "$service" --run
[[ $(stat -c %a "$runtime/openkey") == 700 ]]
[[ $(stat -c %a "$runtime/openkey/session.sock") == 600 ]]
[[ $("$service" --quit) == stopping ]]
wait "$pid"; pid=
[[ ! -e $runtime/openkey/session.sock ]]
expect_code 1 "$service" --status
"$service" --run & pid=$!
ready
kill -TERM "$pid"
wait "$pid"; pid=
[[ ! -e $runtime/openkey/session.sock ]]
# Simulate a crashed process: stale socket must be recoverable under the lock.
"$service" --run & pid=$!
ready
kill -KILL "$pid"
wait "$pid" 2>/dev/null || true; pid=
[[ -S $runtime/openkey/session.sock ]]
"$service" --run & pid=$!
ready
"$service" --quit
wait "$pid"; pid=
# Optional tray must not break service IPC when the bus is unavailable.
"$service" --run-tray & pid=$!
ready
[[ $("$service" --tray-status) == unavailable ]]
"$service" --quit
wait "$pid"; pid=
XDG_SESSION_TYPE=wayland "$service" --run-tray --x11-settings-hotkey & pid=$!
ready
[[ $("$service" --hotkey-status) == unsupported-session ]]
"$service" --quit
wait "$pid"; pid=
# A public runtime directory must never be accepted or silently chmod'ed.
chmod 0755 "$runtime"
expect_code 2 "$service" --run
[[ $(stat -c %a "$runtime") == 755 ]]
chmod 0700 "$runtime"
expect_code 2 env -u XDG_RUNTIME_DIR "$service" --run
expect_code 2 env XDG_RUNTIME_DIR=relative "$service" --run
# Refuse to remove files/symlinks that happen to occupy the IPC path.
touch "$runtime/openkey/session.sock"
expect_code 2 "$service" --run
[[ -f $runtime/openkey/session.sock ]]
rm -- "$runtime/openkey/session.sock"
ln -s "$service" "$runtime/openkey/session.sock"
expect_code 2 "$service" --run
[[ -L $runtime/openkey/session.sock ]]
rm -- "$runtime/openkey/session.sock"
printf 'Service lifecycle, single instance, crash recovery and private runtime passed\n'
