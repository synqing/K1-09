#!/usr/bin/env bash
set -Eeuo pipefail; IFS=$'
	'
# run_batch_splits.sh: run guarded splits listed in scripts/split_targets.txt
# Usage: ./scripts/run_batch_splits.sh [branch_suffix]

SUFFIX="${1:-}"
LIST="scripts/split_targets.txt"

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing dependency: $1" >&2; exit 2; }; }
need bash

if [[ ! -x ./scripts/safe_split_and_pr.sh ]]; then
  echo "scripts/safe_split_and_pr.sh missing or not executable" >&2; exit 2
fi
if [[ ! -f "$LIST" ]]; then
  echo "Missing $LIST"; exit 2
fi

while IFS= read -r line; do
  [[ -z "$line" || "$line" =~ ^# ]] && continue
  hdr="$(echo "$line" | awk '{print $1}')"
  cpp="$(echo "$line" | awk '{print $2}')"
  if [[ -z "$hdr" || -z "$cpp" ]]; then echo "Bad line: $line" >&2; exit 2; fi
  ./scripts/safe_split_and_pr.sh "$hdr" "$cpp" "$SUFFIX"
done < "$LIST"
