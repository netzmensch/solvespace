#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

echo "==> Building macOS arm64 + x86_64"
.github/scripts/install-macos.sh ci
rm -rf build build-arm64
.github/scripts/build-macos.sh release arm64
.github/scripts/build-macos.sh release x86_64

APP_BASE_NAME="SolveSpace"
if [ -d "build/bin/SolveSpaceNitro.app" ]; then
  APP_BASE_NAME="SolveSpaceNitro"
fi

echo "==> Creating unsigned universal SolveSpaceNitro.app"
lipo -create \
  build/bin/${APP_BASE_NAME}.app/Contents/Resources/libomp.dylib \
  build-arm64/bin/${APP_BASE_NAME}.app/Contents/Resources/libomp.dylib \
  -output build/bin/${APP_BASE_NAME}.app/Contents/Resources/libomp.dylib

lipo -create \
  build/bin/${APP_BASE_NAME}.app/Contents/MacOS/${APP_BASE_NAME} \
  build-arm64/bin/${APP_BASE_NAME}.app/Contents/MacOS/${APP_BASE_NAME} \
  -output build/bin/${APP_BASE_NAME}.app/Contents/MacOS/${APP_BASE_NAME}

lipo -create \
  build/bin/${APP_BASE_NAME}.app/Contents/MacOS/solvespace-cli \
  build-arm64/bin/${APP_BASE_NAME}.app/Contents/MacOS/solvespace-cli \
  -output build/bin/${APP_BASE_NAME}.app/Contents/MacOS/solvespace-cli

rm -rf build/bin/SolveSpaceNitro.app
cp -R build/bin/${APP_BASE_NAME}.app build/bin/SolveSpaceNitro.app

hdiutil create -ov -srcfolder build/bin/SolveSpaceNitro.app build/bin/SolveSpaceNitro-unsigned.dmg

echo "==> macOS outputs"
echo "  build/bin/SolveSpaceNitro.app"
echo "  build/bin/SolveSpaceNitro-unsigned.dmg"

if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
  echo "==> Building Linux + Windows (x64) via Docker"
  docker run --rm -t \
    -v "$ROOT_DIR:/src" \
    -w /src \
    ubuntu:24.04 \
    bash -lc '
      set -eux
      apt-get update
      DEBIAN_FRONTEND=noninteractive apt-get install -y \
        git build-essential cmake mingw-w64 \
        zlib1g-dev libpng-dev libcairo2-dev libfreetype6-dev libjson-c-dev \
        libfontconfig1-dev libpangomm-1.4-dev libgl-dev libglu-dev libspnav-dev \
        libgtkmm-3.0-dev qt6-base-dev
      git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen
      cmake -S . -B build-linux-docker -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON
      cmake --build build-linux-docker -j"$(nproc)"
      cmake -S . -B build-win64-docker -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON
      cmake --build build-win64-docker -j"$(nproc)"
    '
  echo "==> Linux/Windows outputs"
  echo "  build-linux-docker/bin/solvespace"
  echo "  build-win64-docker/bin/solvespace.exe"
else
  echo "==> Docker not available/running: skipping Linux and Windows cross-builds"
fi
