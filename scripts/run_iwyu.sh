#!/usr/bin/env bash
set -euo pipefail

if ! command -v iwyu-tool >/dev/null 2>&1; then
  echo "Install include-what-you-use (iwyu) first." >&2
  exit 1
fi

iwyu-tool -p . | tee analysis/iwyu.txt || true
echo "Review analysis/iwyu.txt and adjust includes minimally."
