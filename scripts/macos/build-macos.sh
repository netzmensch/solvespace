#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

RUN_TESTS="${RUN_TESTS:-0}"

build_macos_arch() {
  local arch="$1"
  local build_dir="$2"
  local prefix_root="/tmp/libomp-${arch}/libomp"
  local prefix_path=""
  local jobs
  jobs="$(sysctl -n hw.logicalcpu)"

  if [ -d "$prefix_root" ]; then
    prefix_path="$(find "$prefix_root" -mindepth 1 -maxdepth 1 -type d | head -n 1 || true)"
    if [ -z "$prefix_path" ]; then
      prefix_path="$prefix_root"
    fi
  fi

  if [ -z "$prefix_path" ]; then
    echo "ERROR: libomp prefix for ${arch} not found at ${prefix_root}" >&2
    exit 1
  fi

  cmake \
    -S . \
    -B "$build_dir" \
    -G "Unix Makefiles" \
    -D CMAKE_PREFIX_PATH="$prefix_path" \
    -D CMAKE_OSX_ARCHITECTURES="$arch" \
    -D CMAKE_BUILD_TYPE="RelWithDebInfo" \
    -D ENABLE_OPENMP="ON" \
    -D ENABLE_SANITIZERS="OFF" \
    -D ENABLE_LTO="ON"

  cmake --build "$build_dir" --config RelWithDebInfo -j"$jobs"

  if [ "$RUN_TESTS" = "1" ] && [ "$(uname -m)" = "$arch" ]; then
    cmake --build "$build_dir" --config RelWithDebInfo --target test_solvespace -j"$jobs"
  fi
}

echo "==> Updating required submodules"
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

echo "==> Building macOS arm64 + x86_64"
.github/scripts/install-macos.sh ci
rm -rf build build-arm64
build_macos_arch arm64 build-arm64
build_macos_arch x86_64 build

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

echo "==> Refreshing ad-hoc code signature for unsigned app bundle"
codesign --remove-signature build/bin/SolveSpaceNitro.app/Contents/MacOS/${APP_BASE_NAME} 2>/dev/null || true
codesign --remove-signature build/bin/SolveSpaceNitro.app/Contents/MacOS/solvespace-cli 2>/dev/null || true
codesign --remove-signature build/bin/SolveSpaceNitro.app/Contents/Resources/libomp.dylib 2>/dev/null || true
codesign --remove-signature build/bin/SolveSpaceNitro.app 2>/dev/null || true
rm -rf build/bin/SolveSpaceNitro.app/Contents/_CodeSignature
codesign --force --sign - --timestamp=none build/bin/SolveSpaceNitro.app/Contents/Resources/libomp.dylib
codesign --verify --strict build/bin/SolveSpaceNitro.app/Contents/Resources/libomp.dylib
codesign --force --sign - --timestamp=none build/bin/SolveSpaceNitro.app/Contents/MacOS/solvespace-cli
codesign --force --sign - --timestamp=none --deep build/bin/SolveSpaceNitro.app
codesign --verify --deep --strict build/bin/SolveSpaceNitro.app

hdiutil create -ov -srcfolder build/bin/SolveSpaceNitro.app build/bin/SolveSpaceNitro-unsigned.dmg

echo "==> macOS outputs"
echo "  build/bin/SolveSpaceNitro.app"
echo "  build/bin/SolveSpaceNitro-unsigned.dmg"
echo
echo "==> Build artifact paths (absolute)"
echo "macOS app: ${ROOT_DIR}/build/bin/SolveSpaceNitro.app"
echo "macOS dmg: ${ROOT_DIR}/build/bin/SolveSpaceNitro-unsigned.dmg"
