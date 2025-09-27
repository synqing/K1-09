#!/usr/bin/env bash
# safe_split.sh -- Idempotent, guarded wrapper around scripts/split_header.py
# Usage: ./scripts/safe_split.sh <header.h> <target.cpp>
# Moves top-level function and variable definitions out of a header into a .cpp,
# verifies invariants, builds, and rolls back on any failure.
set -Eeuo pipefail
IFS=$'\n\t'

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing dependency: $1" >&2; exit 2; }; }
need python3
need git
need sed
need grep

if [[ $# -ne 2 ]]; then
  echo "Usage: ./scripts/safe_split.sh <header.h> <target.cpp>" >&2
  exit 2
fi

HDR="$1"
CPP="$2"

if [[ ! -f "$HDR" ]]; then
  echo "Header not found: $HDR" >&2
  exit 2
fi

if [[ ! -x ./agent_runner.sh ]]; then
  echo "agent_runner.sh not found or not executable in repo root." >&2
  exit 2
fi

pp_count() {
  local file="$1"
  if [[ -f "$file" ]]; then
    grep -E '^[[:space:]]*#' "$file" | wc -l | tr -d ' '
  else
    echo "0"
  fi
}

# Ensure working tree is clean (tracked and staged)
if ! git diff --quiet --no-ext-diff || ! git diff --cached --quiet --no-ext-diff; then
  echo "Working tree not clean. Commit or stash your changes first." >&2
  exit 2
fi

hdr_bak="$(mktemp -t hdrXXXXXX.bak)"
cp -p "$HDR" "$hdr_bak" >/dev/null 2>&1 || true
if [[ -f "$CPP" ]]; then
  cpp_bak="$(mktemp -t cppXXXXXX.bak)"
  cp -p "$CPP" "$cpp_bak" >/dev/null 2>&1 || true
  cpp_existed=1
else
  cpp_bak=""
  cpp_existed=0
fi
applied=0

cleanup_temp() {
  rm -f "$hdr_bak" >/dev/null 2>&1 || true
  if [[ -n "$cpp_bak" ]]; then
    rm -f "$cpp_bak" >/dev/null 2>&1 || true
  fi
  rm -f "${HDR}.bak" "${CPP}.bak" >/dev/null 2>&1 || true
}

on_error() {
  local rc=$?
  if [[ $applied -eq 1 ]]; then
    cp -p "$hdr_bak" "$HDR" >/dev/null 2>&1 || true
    if [[ $cpp_existed -eq 1 && -n "$cpp_bak" ]]; then
      cp -p "$cpp_bak" "$CPP" >/dev/null 2>&1 || true
    else
      rm -f "$CPP" >/dev/null 2>&1 || true
    fi
  fi
  echo "safe_split.sh failed (rc=$rc). Rolled back." >&2
  cleanup_temp
  exit $rc
}
trap on_error ERR
trap cleanup_temp EXIT

echo "==> Dry-run plan"
set +e
PLAN=$(python3 scripts/split_header.py "$HDR" "$CPP" 2>&1)
plan_rc=$?
set -e
if [[ $plan_rc -ne 0 ]]; then
  printf '%s\n' "$PLAN" >&2
  exit $plan_rc
fi
echo "$PLAN"

move_count="$(printf "%s\n" "$PLAN" | sed -n -E 's/.*move ([0-9]+) item\(s\).*/\1/p' | tail -n1)"
if [[ -z "$move_count" ]]; then
  move_count=0
fi

if printf "%s\n" "$PLAN" | grep -qE '^\s*#'; then
  echo "Invariant failed: dry-run plan shows preprocessor directive inside a moved block." >&2
  exit 1
fi

if [[ "$move_count" -eq 0 ]]; then
  echo "Nothing to move (declarations-only header). Exiting."
  exit 0
fi

pp_hdr_before="$(pp_count "$HDR")"

python3 scripts/split_header.py "$HDR" "$CPP" --apply
applied=1

rm -f "${HDR}.bak" "${CPP}.bak" >/dev/null 2>&1 || true

pp_hdr_after="$(pp_count "$HDR")"
if [[ "$pp_hdr_before" -ne "$pp_hdr_after" ]]; then
  echo "Invariant failed: preprocessor line count changed in header ($pp_hdr_before -> $pp_hdr_after)." >&2
  exit 1
fi

cpp_hash_lines="$(grep -n '^[[:space:]]*#' "$CPP" || true)"
if printf "%s\n" "$cpp_hash_lines" | grep -vE '^[[:digit:]]+:[[:space:]]*#include[[:space:]]+"[^"]+"' | grep -q '#'; then
  echo "Invariant failed: found disallowed preprocessor directives in $CPP." >&2
  printf "%s\n" "$cpp_hash_lines" >&2
  exit 1
fi

set +e
REPLAN=$(python3 scripts/split_header.py "$HDR" "$CPP" 2>&1)
replan_rc=$?
set -e
if [[ $replan_rc -ne 0 ]]; then
  printf '%s\n' "$REPLAN" >&2
  exit $replan_rc
fi

remaining="$(printf "%s\n" "$REPLAN" | sed -n -E 's/.*move ([0-9]+) item\(s\).*/\1/p' | tail -n1)"
if [[ -z "$remaining" ]]; then
  remaining=0
fi
if [[ "$remaining" -ne 0 ]]; then
  echo "Invariant failed: re-plan still shows $remaining move(s)." >&2
  printf "%s\n" "$REPLAN" >&2
  exit 1
fi

while IFS= read -r status_line; do
  [[ -z "$status_line" ]] && continue
  path="${status_line:3}"
  if [[ "$path" != "$HDR" && "$path" != "$CPP" ]]; then
    echo "Invariant failed: files outside $HDR and $CPP changed during split." >&2
    printf '%s\n' "$status_line" >&2
    exit 1
  fi
done < <(git status --porcelain)

echo "==> Verify"
./agent_runner.sh pio run
./agent_runner.sh python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib
./agent_runner.sh python3 tools/deps_stub_report.py src include lib

echo "safe_split.sh completed successfully."
exit 0
