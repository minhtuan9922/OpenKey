#!/usr/bin/env bash
# Test-only executable; never open an editor, display, bus or user profile.
set -euo pipefail
: "${OPENKEY_VSCODE_CAPTURE:?test capture path is required}"
printf '%s\0' "$@" > "$OPENKEY_VSCODE_CAPTURE"
exit "${OPENKEY_VSCODE_STUB_EXIT:-0}"
