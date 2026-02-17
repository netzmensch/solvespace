Param(
  [switch]$BuildLinuxWithDocker = $false
)

$ErrorActionPreference = 'Stop'
$RootDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $RootDir

Write-Host '==> Updating required submodules'
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

Write-Host '==> Building Windows x64 (native)'
cmake -S . -B build-win64-native -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON
cmake --build build-win64-native --config RelWithDebInfo -- -maxcpucount

Write-Host '==> Output'
Write-Host '  Windows: build-win64-native/bin/RelWithDebInfo/solvespace.exe'

if ($BuildLinuxWithDocker) {
  Write-Host '==> Building Linux in Docker'
  docker run --rm -t -v "${RootDir}:/src" -w /src ubuntu:24.04 bash -lc "set -eux; apt-get update; DEBIAN_FRONTEND=noninteractive apt-get install -y git build-essential cmake zlib1g-dev libpng-dev libcairo2-dev libfreetype6-dev libjson-c-dev libfontconfig1-dev libpangomm-1.4-dev libgl-dev libglu-dev libspnav-dev libgtkmm-3.0-dev qt6-base-dev; git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen; cmake -S . -B build-linux-docker -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON; cmake --build build-linux-docker -j\"`$(nproc)\""
  Write-Host '  Linux:   build-linux-docker/bin/solvespace'
} else {
  Write-Host 'Linux build via Docker skipped. Use -BuildLinuxWithDocker to enable.'
}

Write-Host 'macOS build is not supported from Windows in this setup.'
Write-Host 'Use scripts/macos/build-all.sh on a macOS host for arm64/x86_64 macOS builds.'
