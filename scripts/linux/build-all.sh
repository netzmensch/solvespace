#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

echo "==> Building Linux (native)"
cmake -S . -B build-linux-native -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON
cmake --build build-linux-native -j"$(nproc)"

echo "==> Building Windows x64 (cross, mingw)"
cmake -S . -B build-win64-cross -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON
cmake --build build-win64-cross -j"$(nproc)"

echo "==> Outputs"
echo "  Linux:   build-linux-native/bin/solvespace"
echo "  Windows: build-win64-cross/bin/solvespace.exe"
echo
echo "macOS cross-build is not supported from Linux in this setup."
echo "Use scripts/macos/build-all.sh on a macOS host for arm64/x86_64 macOS builds."
