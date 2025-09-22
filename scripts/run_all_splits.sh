#!/usr/bin/env bash
set -Eeuo pipefail

# Dry-run first
./scripts/split_globals.sh "$@"
./scripts/split_constants.sh "$@"
./scripts/split_palette_luts_api.sh "$@"
./scripts/split_debug_manager.sh "$@"
./scripts/split_perf_trace.sh "$@"

if [[ "$*" == *"--apply"* ]]; then
  exit 0
fi

echo "=== Review plans above. If OK, apply: ==="
read -rp "Proceed with --apply? [y/N] " ans
[[ "${ans,,}" == "y" ]] || exit 0

./scripts/split_globals.sh --apply
./scripts/split_constants.sh --apply
./scripts/split_palette_luts_api.sh --apply
./scripts/split_debug_manager.sh --apply
./scripts/split_perf_trace.sh --apply
