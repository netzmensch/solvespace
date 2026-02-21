$ErrorActionPreference = 'Stop'
$RootDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $RootDir

Write-Host '==> Updating required submodules'
git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen

Write-Host '==> Building Windows x64 (native)'
cmake -S . -B build-win64-native -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENMP=OFF -DENABLE_TESTS=OFF
cmake --build build-win64-native --config RelWithDebInfo --target solvespace solvespace-cli -- -maxcpucount

Copy-Item -Force 'build-win64-native/bin/RelWithDebInfo/solvespace.exe' 'build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro.exe'
Copy-Item -Force 'build-win64-native/bin/RelWithDebInfo/solvespace-cli.exe' 'build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro-cli.exe'

Write-Host '==> Output'
Write-Host '  Windows: build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro.exe'
Write-Host '  Windows: build-win64-native/bin/RelWithDebInfo/SolveSpaceNitro-cli.exe'
Write-Host ''
Write-Host 'Compatibility copies also available:'
Write-Host '  Windows: build-win64-native/bin/RelWithDebInfo/solvespace.exe'
Write-Host '  Windows: build-win64-native/bin/RelWithDebInfo/solvespace-cli.exe'
