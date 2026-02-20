$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

& (Join-Path $ScriptDir 'build-windows.ps1')
& (Join-Path $ScriptDir 'build-linux.ps1')

Write-Host 'macOS build is not supported from Windows in this setup.'
Write-Host 'Use scripts/macos/build-macos.sh on a macOS host for arm64/x86_64 macOS builds.'
