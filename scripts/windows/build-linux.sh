#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
  echo "ERROR: Docker not available/running" >&2
  exit 1
fi

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

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
    rm -rf build-linux-docker
    cmake -S . -B build-linux-docker -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON -DENABLE_TESTS=OFF
    cmake --build build-linux-docker -j"$(nproc)" --target solvespace solvespace-cli
  '

cp -f build-linux-docker/bin/solvespace build-linux-docker/bin/SolveSpaceNitro
cp -f build-linux-docker/bin/solvespace-cli build-linux-docker/bin/SolveSpaceNitro-cli

echo "==> Linux outputs"
echo "  build-linux-docker/bin/SolveSpaceNitro"
echo "  build-linux-docker/bin/SolveSpaceNitro-cli"
echo
echo "Linux binary: ${ROOT_DIR}/build-linux-docker/bin/SolveSpaceNitro"
echo "Linux CLI:    ${ROOT_DIR}/build-linux-docker/bin/SolveSpaceNitro-cli"
echo
echo "Compatibility copies also available:"
echo "  ${ROOT_DIR}/build-linux-docker/bin/solvespace"
echo "  ${ROOT_DIR}/build-linux-docker/bin/solvespace-cli"
