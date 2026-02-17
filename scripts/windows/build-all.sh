#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

echo "==> Building Windows x64 (native, Visual Studio 2022)"
cmake -S . -B build-win64-native -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON
cmake --build build-win64-native --config RelWithDebInfo -- -maxcpucount

echo "==> Windows output"
echo "  build-win64-native/bin/RelWithDebInfo/solvespace.exe"

if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
  echo "==> Building Linux via Docker"
  docker run --rm -t \
    -v "$ROOT_DIR:/src" \
    -w /src \
    ubuntu:24.04 \
    bash -lc '
      set -eux
      apt-get update
      DEBIAN_FRONTEND=noninteractive apt-get install -y \
        git build-essential cmake zlib1g-dev libpng-dev libcairo2-dev libfreetype6-dev \
        libjson-c-dev libfontconfig1-dev libpangomm-1.4-dev libgl-dev libglu-dev \
        libspnav-dev libgtkmm-3.0-dev qt6-base-dev
      git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen
      cmake -S . -B build-linux-docker -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON
      cmake --build build-linux-docker -j"$(nproc)"
    '
  echo "==> Linux output"
  echo "  build-linux-docker/bin/solvespace"
else
  echo "==> Docker not available/running: skipping Linux build"
fi

echo "macOS build is not supported from Windows in this setup."
echo "Use scripts/macos/build-all.sh on a macOS host for arm64/x86_64 macOS builds."
