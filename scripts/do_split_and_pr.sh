#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'

if [[ $# -lt 4 ]]; then
  echo "Usage: $0 <header> <target.cpp> <issue-number> <slug>" >&2
  exit 2
fi

HDR="$1"
CPP="$2"
ISSUE="$3"
SLUG="$4"

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing $1" >&2; exit 1; }; }
need git
need gh
need python3

if [[ ! -f scripts/split_header.py ]]; then
  echo "scripts/split_header.py not found. Run from repo root." >&2
  exit 1
fi

DEFAULT_BRANCH="$(git remote show origin 2>/dev/null | sed -n '/HEAD branch/s/.*: //p')"
DEFAULT_BRANCH="${DEFAULT_BRANCH:-main}"

git fetch origin "$DEFAULT_BRANCH" --quiet

BRANCH="refactor/header-split-${SLUG}"

echo "==> Creating branch $BRANCH from origin/$DEFAULT_BRANCH"
if git rev-parse --verify "$BRANCH" >/dev/null 2>&1; then
  git switch "$BRANCH"
else
  git switch -c "$BRANCH" "origin/$DEFAULT_BRANCH"
fi

echo "==> Running mechanical split for $HDR → $CPP"
python3 scripts/split_header.py "$HDR" "$CPP"
python3 scripts/split_header.py "$HDR" "$CPP" --apply

./scripts/verify_split.sh

git add "$HDR" "$CPP"
COMMIT_MSG="refactor(header): split ${HDR} into .h/.cpp (mechanical)"
if git diff --cached --quiet; then
  echo "No staged changes to commit. Abort." >&2
  exit 1
fi

git commit -m "$COMMIT_MSG"
git push -u origin "$BRANCH"

TITLE="$COMMIT_MSG"
read -r -d '' BODY <<MD
**Goal**  
Mechanical split of \
`${HDR}` — declarations stay in the header, definitions move to \
`${CPP}`. No behavior change.

**What moved**
- Non-template, non-inline function definitions
- Static data member definitions
- Large tables/LUTs (attributes such as \`PROGMEM\`, \`alignas\` preserved; header now declares them via \`extern\`)

**What stayed in header**
- Templates, inline functions, type aliases, macros, constexpr integral constants
- Declarations + \`extern\` statements for moved variables

**Verification**
- \`./agent_runner.sh pio run\`
- \`./agent_runner.sh python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib\`
- \`./agent_runner.sh python3 tools/deps_stub_report.py src include lib && test -s analysis/deps_report.json\`

**Scope & Safety**
- ❌ No new firmware functions/vars; ❌ No signature changes
- ✅ Behavior preserved 1:1

Closes #${ISSUE}
MD

gh pr create \
  --title "$TITLE" \
  --body "$BODY" \
  --base "$DEFAULT_BRANCH" \
  --head "$BRANCH"
