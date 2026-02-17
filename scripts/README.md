# SolveSpaceNitro Build Scripts

This folder contains host-specific scripts.

## Host: macOS
- Script: `scripts/macos/build-all.sh`
- Builds:
  - macOS `arm64` + `x86_64` and merges to unsigned universal app/dmg
  - Linux + Windows x64 via Docker (if Docker daemon is running)

## Host: Linux
- Script: `scripts/linux/build-all.sh`
- Builds:
  - Linux native
  - Windows x64 via mingw cross toolchain
- Limitation:
  - macOS cross-build is not supported in this setup.

## Host: Windows
- Scripts:
  - `scripts/windows/build-all.ps1` (PowerShell)
  - `scripts/windows/build-all.sh` (Git Bash)
- Builds:
  - Windows x64 native
  - Linux via Docker (optional)
- Limitation:
  - macOS cross-build is not supported in this setup.

## Branch Sync (master -> master_nitro)
- Script: `scripts/git/sync-master-into-master_nitro.sh`
- Purpose:
  1. Update `master` from remote (`upstream` if present, else `origin`)
  2. Rebase `master_nitro` on top of updated `master`

Usage:

```bash
./scripts/git/sync-master-into-master_nitro.sh
# optional explicit remote
./scripts/git/sync-master-into-master_nitro.sh upstream
```
