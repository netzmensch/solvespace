# SolveSpaceNitro Build Scripts

This folder contains host-specific scripts with per-target entrypoints.

## Host: macOS
- Build only macOS app/dmg:
  - `scripts/macos/build-macos.sh`
- Build only Linux (Docker):
  - `scripts/macos/build-linux.sh`
- Build only Windows x64 (Docker):
  - `scripts/macos/build-windows.sh`
- Build everything available from macOS host:
  - `scripts/macos/build-all.sh`

Notes:
- macOS tests are skipped by default. Enable with `RUN_TESTS=1`.
- Docker parallelism can be reduced with `DOCKER_JOBS=2`.
- Windows cross-builds use `ENABLE_OPENMP=OFF` (standalone `.exe` without `libgomp-1.dll`).

## Host: Linux
- Build only Linux native:
  - `scripts/linux/build-linux.sh`
- Build only Windows x64 (mingw cross):
  - `scripts/linux/build-windows.sh`
- Build everything available from Linux host:
  - `scripts/linux/build-all.sh`

Limitation:
- macOS cross-build is not supported in this setup.

## Nitro Versioning
- The Nitro revision is controlled by:
  - `NITRO_VERSION`
- Build/version format:
  - `<upstream-major>.<upstream-minor>.0-nitro.<revision>+g<git-hash>`
  - Example: `3.2.0-nitro.7+gabcdef12`
- Bump helper:
  - `scripts/release/bump-nitro.sh` (increments revision by 1)
  - `scripts/release/bump-nitro.sh --set 8` (sets explicit revision)

## Host: Windows
Shell (Git Bash):
- Build only Windows native:
  - `scripts/windows/build-windows.sh`
- Build only Linux (Docker):
  - `scripts/windows/build-linux.sh`
- Build everything available from Windows host:
  - `scripts/windows/build-all.sh`

PowerShell:
- Build only Windows native:
  - `scripts/windows/build-windows.ps1`
- Build only Linux (Docker):
  - `scripts/windows/build-linux.ps1`
- Build everything available from Windows host:
  - `scripts/windows/build-all.ps1`

Limitation:
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
