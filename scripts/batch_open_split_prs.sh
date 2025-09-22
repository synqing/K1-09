#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing $1" >&2; exit 1; }; }
need git
need gh

SPLITS=(
  "src/globals.h|src/core/globals.cpp|3|globals"
  "src/constants.h|src/core/constants.cpp|4|constants"
  "src/palettes/palette_luts_api.h|src/palettes/palette_luts_api.cpp|5|palette-luts-api"
  "src/debug/debug_manager.h|src/debug/debug_manager.cpp|6|debug-manager"
  "include/performance_optimized_trace.h|src/trace/performance_optimized_trace.cpp|7|perf-trace"
)

for entry in "${SPLITS[@]}"; do
  IFS='|' read -r HDR CPP ISSUE SLUG <<< "$entry"
  echo "=== Processing: $HDR → $CPP (Issue #$ISSUE) ==="
  ./scripts/do_split_and_pr.sh "$HDR" "$CPP" "$ISSUE" "$SLUG"
  echo

done

echo "All header-split PRs opened ✅"
