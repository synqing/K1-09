#!/usr/bin/env bash
set -euo pipefail

gh label create "header-split"     -c "#0ea5e9" -d "Mechanical header split PRs" || true
gh label create "mechanical"       -c "#64748b" -d "No behavioural changes" || true
gh label create "safe-change"      -c "#22c55e" -d "Guardrail-compliant change" || true
