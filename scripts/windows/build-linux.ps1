$ErrorActionPreference = 'Stop'
$RootDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $RootDir

Write-Host '==> Updating required submodules'
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

Write-Host '==> Building Linux in Docker'
docker run --rm -t -v "${RootDir}:/src" -w /src ubuntu:24.04 bash -lc "set -eux; apt-get update; DEBIAN_FRONTEND=noninteractive apt-get install -y git build-essential cmake zlib1g-dev libpng-dev libcairo2-dev libfreetype6-dev libjson-c-dev libfontconfig1-dev libpangomm-1.4-dev libgl-dev libglu-dev libspnav-dev libgtkmm-3.0-dev qt6-base-dev; git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen; rm -rf build-linux-docker; cmake -S . -B build-linux-docker -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=ON -DENABLE_TESTS=OFF; cmake --build build-linux-docker -j\"`$(nproc)\" --target solvespace solvespace-cli"

Copy-Item -Force 'build-linux-docker/bin/solvespace' 'build-linux-docker/bin/SolveSpaceNitro'
Copy-Item -Force 'build-linux-docker/bin/solvespace-cli' 'build-linux-docker/bin/SolveSpaceNitro-cli'

Write-Host '==> Output'
Write-Host '  Linux: build-linux-docker/bin/SolveSpaceNitro'
Write-Host '  Linux: build-linux-docker/bin/SolveSpaceNitro-cli'
Write-Host ''
Write-Host 'Compatibility copies also available:'
Write-Host '  Linux: build-linux-docker/bin/solvespace'
Write-Host '  Linux: build-linux-docker/bin/solvespace-cli'
