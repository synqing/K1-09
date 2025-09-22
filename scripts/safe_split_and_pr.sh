#!/usr/bin/env bash
# safe_split_and_pr.sh -- run safe_split, then open a PR if there is a diff.
# Usage: ./scripts/safe_split_and_pr.sh <header.h> <target.cpp> [branch_suffix]
set -Eeuo pipefail
IFS=$'\n\t'

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing dependency: $1" >&2; exit 2; }; }
need git

if [[ $# -lt 2 ]]; then
  echo "Usage: ./scripts/safe_split_and_pr.sh <header.h> <target.cpp> [branch_suffix]" >&2
  exit 2
fi

HDR="$1"
CPP="$2"
SUFFIX="${3:-}"

if [[ ! -x ./scripts/safe_split.sh ]]; then
  echo "scripts/safe_split.sh is missing or not executable." >&2
  exit 2
fi

if ! git diff --quiet --no-ext-diff || ! git diff --cached --quiet --no-ext-diff; then
  echo "Working tree not clean. Commit or stash your changes first." >&2
  exit 2
fi

./scripts/safe_split.sh "$HDR" "$CPP"

if git diff --quiet -- "$HDR" "$CPP"; then
  echo "No diff on $HDR or $CPP; nothing to commit or PR."
  exit 0
fi

base="$(basename "$HDR")"
name="${base%.h}"
branch="split/${name}"
if [[ -n "$SUFFIX" ]]; then
  branch="${branch}-${SUFFIX}"
fi

if git show-ref --verify --quiet "refs/heads/$branch"; then
  git checkout "$branch"
else
  git checkout -b "$branch"
fi

git add -- "$HDR" "$CPP"
commit_msg="refactor(header): split ${HDR} into ${CPP} (mechanical)"
git commit -m "$commit_msg"

if command -v gh >/dev/null 2>&1; then
  git push -u origin "$branch"
  cat > .git/MECH_SPLIT_PR_BODY <<'MD'
**Goal**
Mechanical split only -- declarations remain in the header; moved definitions live in the target .cpp. No behavior change.

**Verification**
```bash
./agent_runner.sh pio run
./agent_runner.sh python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib
./agent_runner.sh python3 tools/deps_stub_report.py src include lib
```

Scope and Safety

* No new firmware symbols or signature changes
* Behavior preserved 1:1
MD
  gh pr create \
    --title "$commit_msg" \
    --body-file .git/MECH_SPLIT_PR_BODY \
    --head "$branch" \
    --base main || true
  rm -f .git/MECH_SPLIT_PR_BODY
else
  echo "gh not found; committed on branch '$branch'. Push and open PR manually if desired."
fi

echo "safe_split_and_pr.sh completed."
