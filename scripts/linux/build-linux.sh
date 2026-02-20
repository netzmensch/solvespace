#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

echo "==> Building Linux (native)"
cmake -S . -B build-linux-native -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON -DENABLE_TESTS=OFF
cmake --build build-linux-native -j"$(nproc)" --target solvespace solvespace-cli

cp -f build-linux-native/bin/solvespace build-linux-native/bin/SolveSpaceNitro
cp -f build-linux-native/bin/solvespace-cli build-linux-native/bin/SolveSpaceNitro-cli

echo "==> Linux outputs"
echo "  build-linux-native/bin/SolveSpaceNitro"
echo "  build-linux-native/bin/SolveSpaceNitro-cli"
echo
echo "Linux binary: ${ROOT_DIR}/build-linux-native/bin/SolveSpaceNitro"
echo "Linux CLI:    ${ROOT_DIR}/build-linux-native/bin/SolveSpaceNitro-cli"
echo
echo "Compatibility copies also available:"
echo "  ${ROOT_DIR}/build-linux-native/bin/solvespace"
echo "  ${ROOT_DIR}/build-linux-native/bin/solvespace-cli"
