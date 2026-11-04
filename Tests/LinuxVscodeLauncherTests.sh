#!/usr/bin/env bash
# Integration tests of the launcher, not input-method/GUI acceptance tests.
set -euo pipefail
repo_dir=${1:?repository root is required}
launcher="$repo_dir/scripts/test-vscode-input.sh"
fixtures="$repo_dir/Tests/fixtures/vscode-input"
test_root=$(mktemp -d /tmp/openkey-vscode-launcher-tests.XXXXXXXX)
case "$test_root" in /tmp/openkey-vscode-launcher-tests.*) ;; *) exit 1 ;; esac
[[ -d $test_root && ! -L $test_root ]] || exit 1
# Only this freshly generated fixture tree is removed, never retained real profiles.
trap 'rm -rf -- "$test_root"' EXIT
mkdir -p -- "$test_root/profile root" "$test_root/launcher tools" \
    "$test_root/config/Code/User" "$test_root/runtime" "$test_root/data"
chmod 700 "$test_root/runtime"
install -m 700 "$fixtures/code-stub.sh" "$test_root/launcher tools/code stub"
printf '%s\n' 'main profile must not change' > "$test_root/config/Code/User/settings.json"
export TMPDIR="$test_root/profile root"
export OPENKEY_VSCODE_BIN="$test_root/launcher tools/code stub"
export XDG_CONFIG_HOME="$test_root/config" XDG_RUNTIME_DIR="$test_root/runtime" XDG_DATA_HOME="$test_root/data"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$test_root/no-session-bus"
# Deliberately non-existent displays, only for the stub; never connect to desktop.
export DISPLAY=:openkey-test-only WAYLAND_DISPLAY=openkey-test-only
export OPENKEY_VSCODE_STUB_EXIT=0
unset DBUS_STARTER_ADDRESS DBUS_STARTER_BUS_TYPE
case_number=0
assert() {
    if ! "$@"; then printf 'Assertion failed (case %s): %s\n' "$case_number" "$*" >&2; exit 1; fi
}
run_case() {
    local expected=$1
    shift
    case_number=$((case_number + 1))
    export OPENKEY_VSCODE_CAPTURE="$test_root/capture-$case_number"
    local actual=0
    bash "$launcher" "$@" > "$test_root/output-$case_number" 2>&1 || actual=$?
    if [[ $actual != "$expected" ]]; then
        printf 'Case %s: expected exit %s, got %s\n' "$case_number" "$expected" "$actual" >&2
        exit 1
    fi
}
assert_no_launch() {
    assert test ! -e "$OPENKEY_VSCODE_CAPTURE"
}
check_launch() {
    local mode=$1 backend=$2
    local -a arguments
    mapfile -d '' -t arguments < "$OPENKEY_VSCODE_CAPTURE"
    assert test "${#arguments[@]}" -eq 10
    assert test "${arguments[0]}" = "--ozone-platform=$backend"
    assert test "${arguments[1]#--user-data-dir=}" != "${arguments[1]}"
    local profile=${arguments[1]#--user-data-dir=}
    case "$profile" in "$TMPDIR"/openkey-vscode-input.*) ;; *) exit 1 ;; esac
    assert test -d "$profile"
    assert test ! -L "$profile"
    assert test "$(stat -c %a "$profile")" = 700
    assert test "$(stat -c %a "$profile/User/settings.json")" = 600
    assert test "$(stat -c %a "$profile/input-test.js")" = 600
    assert cmp -s "$fixtures/$mode.json" "$profile/User/settings.json"
    assert cmp -s "$fixtures/input-test.js" "$profile/input-test.js"
    assert test "${arguments[2]}" = "--extensions-dir=$profile/extensions"
    assert test -d "$profile/extensions"
    assert test "${arguments[3]}" = --disable-extensions
    assert test "${arguments[4]}" = --new-window
    assert test "${arguments[5]}" = --skip-welcome
    assert test "${arguments[6]}" = --skip-release-notes
    assert test "${arguments[7]}" = --wait
    assert test "${arguments[8]}" = --goto
    assert test "${arguments[9]}" = "$profile/input-test.js:3:17"
    assert grep -Fq -- "OpenKey case: $mode / $backend" "$test_root/output-$case_number"
    assert grep -Fq -- "Profile: $profile" "$test_root/output-$case_number"
    assert grep -Eq '^Finished: .*\(exit [0-9]+\)$' "$test_root/output-$case_number"
    printf '%s\n' "$profile" >> "$test_root/profiles"
}

bash -n "$launcher"
for mode in textarea accessible editcontext; do
    for backend in wayland x11; do
        run_case 0 "$mode" "$backend"
        check_launch "$mode" "$backend"
    done
done
# A second run of the same case must not reuse the previous profile.
run_case 0 accessible wayland
check_launch accessible wayland
assert test "$(sort -u "$test_root/profiles" | wc -l)" -eq 7
run_case 0 textarea
check_launch textarea wayland
export OPENKEY_VSCODE_STUB_EXIT=17
run_case 17 editcontext x11
check_launch editcontext x11
export OPENKEY_VSCODE_STUB_EXIT=0

# The checkout itself may also live under a path containing spaces.
mkdir -p -- "$test_root/repo with spaces/scripts" "$test_root/repo with spaces/Tests/fixtures/vscode-input"
install -m 600 "$launcher" "$test_root/repo with spaces/scripts/test-vscode-input.sh"
for fixture in textarea.json accessible.json editcontext.json input-test.js; do
    install -m 600 "$fixtures/$fixture" "$test_root/repo with spaces/Tests/fixtures/vscode-input/$fixture"
done
saved_script=$launcher
launcher="$test_root/repo with spaces/scripts/test-vscode-input.sh"
run_case 0 accessible x11
check_launch accessible x11
launcher=$saved_script

for help in --help -h; do run_case 0 "$help"; assert_no_launch; done
run_case 2; assert_no_launch
run_case 2 invalid; assert_no_launch
run_case 2 textarea invalid; assert_no_launch
run_case 2 textarea wayland extra; assert_no_launch
run_case 2 --help extra; assert_no_launch
saved_wayland=$WAYLAND_DISPLAY
unset WAYLAND_DISPLAY
run_case 2 accessible wayland; assert_no_launch
export WAYLAND_DISPLAY=$saved_wayland
saved_display=$DISPLAY
unset DISPLAY
run_case 2 accessible x11; assert_no_launch
export DISPLAY=$saved_display
saved_launcher=$OPENKEY_VSCODE_BIN
export OPENKEY_VSCODE_BIN="$test_root/missing-code"
run_case 2 textarea; assert_no_launch
# Use a private non-executable copy rather than assuming checkout permissions.
install -m 600 "$fixtures/code-stub.sh" "$test_root/non-executable-code"
export OPENKEY_VSCODE_BIN="$test_root/non-executable-code"
run_case 2 textarea; assert_no_launch
export OPENKEY_VSCODE_BIN=$saved_launcher
saved_tmpdir=$TMPDIR
for bad_tmpdir in relative "$test_root/missing-temp" "$test_root/non-executable-code"; do
    export TMPDIR=$bad_tmpdir
    run_case 2 textarea; assert_no_launch
done
export TMPDIR=$saved_tmpdir
assert test "$(< "$test_root/config/Code/User/settings.json")" = 'main profile must not change'
profiles=("$TMPDIR"/openkey-vscode-input.*)
assert test "${#profiles[@]}" -eq 10
printf 'VS Code launcher: %s cases passed; no real editor, clipboard or input framework used.\n' "$case_number"
