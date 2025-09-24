#!/usr/bin/env bash
set -euo pipefail

labels=(
  "color-refit:#ff8700:Color refit stream"
  "phase-0:#5319e7:Phase 0 — Baseline"
  "phase-1:#0e8a16:Phase 1 — HSV Linearization"
  "phase-2:#fbca04:Phase 2 — Facade & Modes"
  "phase-3:#d93f0b:Phase 3 — Post Processing"
  "phase-4:#0366d6:Phase 4 — Regression & Docs"
  "phase-override:#b60205:Override per-phase file scope"
  "allow-router-touch:#bfd4f2:Allow router/coordinator edits"
  "allow-audio-touch:#bfd4f2:Allow I2S/audio edits"
  "size-override:#bfd4f2:Override size budget guardrail"
  "ready-for-collab:#0e8a16:Ready for tri-agent review"
)

exists=$(gh label list --limit 300 --json name --jq '.[].name' 2>/dev/null || true)

for entry in "${labels[@]}"; do
  name=${entry%%:*}
  rest=${entry#*:}
  color=${rest%%:*}
  desc=${rest#*:}
  if echo "$exists" | rg -x "$name" >/dev/null; then
    echo "Label '$name' already exists"
  else
    gh label create "$name" --color "$color" --description "$desc"
    echo "Created label '$name'"
  fi
  sleep 0.2
done
