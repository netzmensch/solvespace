#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "Working tree is not clean. Commit or stash changes first." >&2
  exit 1
fi

if ! git show-ref --verify --quiet refs/heads/master; then
  echo "Branch 'master' does not exist." >&2
  exit 1
fi
if ! git show-ref --verify --quiet refs/heads/master_nitro; then
  echo "Branch 'master_nitro' does not exist." >&2
  exit 1
fi

REMOTE="${1:-}"
if [[ -z "$REMOTE" ]]; then
  if git remote get-url upstream >/dev/null 2>&1; then
    REMOTE="upstream"
  else
    REMOTE="origin"
  fi
fi

if ! git remote get-url "$REMOTE" >/dev/null 2>&1; then
  echo "Remote '$REMOTE' not found." >&2
  exit 1
fi

echo "Using remote: $REMOTE"
git fetch "$REMOTE" --prune

git checkout master
git pull --ff-only "$REMOTE" master

git checkout master_nitro
git rebase master

echo
echo "Sync complete."
echo "master is at:      $(git rev-parse --short master)"
echo "master_nitro is at: $(git rev-parse --short master_nitro)"
echo "If needed, push with: git push --force-with-lease origin master_nitro"
