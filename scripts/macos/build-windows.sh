#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

DOCKER_JOBS="${DOCKER_JOBS:-4}"
CLEAN_DOCKER_BUILD="${CLEAN_DOCKER_BUILD:-0}"
CLEAN_CCACHE="${CLEAN_CCACHE:-0}"
REBUILD_DOCKER_IMAGE="${REBUILD_DOCKER_IMAGE:-0}"

IMAGE_NAME="solvespace-builder:win64-ubuntu24.04"
DOCKERFILE_PATH="scripts/docker/win64/Dockerfile"
CACHE_ROOT="$ROOT_DIR/.docker-cache/win64"
BUILD_CACHE_DIR="$CACHE_ROOT/build"
CCACHE_DIR_HOST="$CACHE_ROOT/ccache"

if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
  echo "ERROR: Docker not available/running" >&2
  exit 1
fi

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

mkdir -p "$BUILD_CACHE_DIR" "$CCACHE_DIR_HOST"

DOCKER_BUILD_ARGS=()
if [ "$REBUILD_DOCKER_IMAGE" = "1" ]; then
  DOCKER_BUILD_ARGS+=(--no-cache)
fi

echo "==> Building win64 builder image (${IMAGE_NAME})"
docker build "${DOCKER_BUILD_ARGS[@]}" -t "$IMAGE_NAME" -f "$DOCKERFILE_PATH" "$ROOT_DIR"

echo "==> Building Windows x64 via Docker image ${IMAGE_NAME} (jobs=${DOCKER_JOBS})"
docker run --rm -t \
  -v "$ROOT_DIR:/src" \
  -v "$BUILD_CACHE_DIR:/build" \
  -v "$CCACHE_DIR_HOST:/ccache" \
  -e CCACHE_DIR=/ccache \
  -e DOCKER_JOBS="$DOCKER_JOBS" \
  -e CLEAN_DOCKER_BUILD="$CLEAN_DOCKER_BUILD" \
  -e CLEAN_CCACHE="$CLEAN_CCACHE" \
  -w /src \
  "$IMAGE_NAME" \
  bash -lc '
    set -eux

    if [ "$CLEAN_DOCKER_BUILD" = "1" ]; then
      rm -rf /build/*
    fi
    if [ "$CLEAN_CCACHE" = "1" ]; then
      rm -rf /ccache/*
    fi

    cmake -S . -B /build \
      -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_OPENMP=OFF \
      -DENABLE_TESTS=OFF \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

    cmake --build /build -j"$DOCKER_JOBS" --target solvespace solvespace-cli
  '

if [ -f "${ROOT_DIR}/.docker-cache/win64/build/bin/solvespace.exe" ]; then
  cp -f "${ROOT_DIR}/.docker-cache/win64/build/bin/solvespace.exe" \
    "${ROOT_DIR}/.docker-cache/win64/build/bin/SolveSpaceNitro.exe"
fi
if [ -f "${ROOT_DIR}/.docker-cache/win64/build/bin/solvespace-cli.exe" ]; then
  cp -f "${ROOT_DIR}/.docker-cache/win64/build/bin/solvespace-cli.exe" \
    "${ROOT_DIR}/.docker-cache/win64/build/bin/SolveSpaceNitro-cli.exe"
fi

echo "==> Windows outputs"
echo "  ${ROOT_DIR}/.docker-cache/win64/build/bin/SolveSpaceNitro.exe"
echo "  ${ROOT_DIR}/.docker-cache/win64/build/bin/SolveSpaceNitro-cli.exe"
echo
echo "Compatibility copies also available:"
echo "  ${ROOT_DIR}/.docker-cache/win64/build/bin/solvespace.exe"
echo "  ${ROOT_DIR}/.docker-cache/win64/build/bin/solvespace-cli.exe"
