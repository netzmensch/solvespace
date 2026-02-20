#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

echo "==> Building Windows x64 (native, Visual Studio 2022)"
cmake -S . -B build-win64-native -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=OFF -DENABLE_TESTS=OFF
cmake --build build-win64-native --config RelWithDebInfo --target solvespace solvespace-cli -- -maxcpucount

cp -f build-win64-native/bin/RelWithDebInfo/solvespace.exe build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro.exe
cp -f build-win64-native/bin/RelWithDebInfo/solvespace-cli.exe build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro-cli.exe

echo "==> Windows outputs"
echo "  build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro.exe"
echo "  build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro-cli.exe"
echo
echo "Windows EXE: ${ROOT_DIR}/build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro.exe"
echo "Windows CLI: ${ROOT_DIR}/build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro-cli.exe"
echo
echo "Compatibility copies also available:"
echo "  ${ROOT_DIR}/build-win64-native/bin/RelWithDebInfo/solvespace.exe"
echo "  ${ROOT_DIR}/build-win64-native/bin/RelWithDebInfo/solvespace-cli.exe"
