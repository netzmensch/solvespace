#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
NITRO_FILE="${ROOT_DIR}/NITRO_VERSION"

if [ ! -f "${NITRO_FILE}" ]; then
  echo "ERROR: ${NITRO_FILE} not found" >&2
  exit 1
fi

current_raw="$(cat "${NITRO_FILE}")"
current="$(echo "${current_raw}" | tr -d '[:space:]')"
if [[ ! "${current}" =~ ^[0-9]+$ ]]; then
  echo "ERROR: NITRO_VERSION must be an integer, got '${current_raw}'" >&2
  exit 1
fi

if [ "${1:-}" = "--set" ]; then
  if [ -z "${2:-}" ] || [[ ! "${2}" =~ ^[0-9]+$ ]]; then
    echo "Usage: $0 [--set <integer>]" >&2
    exit 1
  fi
  next="${2}"
elif [ "$#" -gt 0 ]; then
  echo "Usage: $0 [--set <integer>]" >&2
  exit 1
else
  next=$((current + 1))
fi

printf "%s\n" "${next}" > "${NITRO_FILE}"

echo "NITRO_VERSION: ${current} -> ${next}"
echo "Updated: ${NITRO_FILE}"
