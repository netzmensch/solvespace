#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"${SCRIPT_DIR}/build-windows.sh"
"${SCRIPT_DIR}/build-linux.sh"

echo

echo "macOS build is not supported from Windows in this setup."
echo "Use scripts/macos/build-macos.sh on a macOS host for arm64/x86_64 macOS builds."
