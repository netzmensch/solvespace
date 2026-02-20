#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

echo "==> Building Windows x64 (cross, mingw)"
cmake -S . -B build-win64-cross -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=OFF -DENABLE_TESTS=OFF
cmake --build build-win64-cross -j"$(nproc)" --target solvespace solvespace-cli

cp -f build-win64-cross/bin/solvespace.exe build-win64-cross/bin/SolveSpaceNitro.exe
cp -f build-win64-cross/bin/solvespace-cli.exe build-win64-cross/bin/SolveSpaceNitro-cli.exe

echo "==> Windows outputs"
echo "  build-win64-cross/bin/SolveSpaceNitro.exe"
echo "  build-win64-cross/bin/SolveSpaceNitro-cli.exe"
echo
echo "Windows EXE: ${ROOT_DIR}/build-win64-cross/bin/SolveSpaceNitro.exe"
echo "Windows CLI: ${ROOT_DIR}/build-win64-cross/bin/SolveSpaceNitro-cli.exe"
echo
echo "Compatibility copies also available:"
echo "  ${ROOT_DIR}/build-win64-cross/bin/solvespace.exe"
echo "  ${ROOT_DIR}/build-win64-cross/bin/solvespace-cli.exe"
