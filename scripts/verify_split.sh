#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing $1" >&2; exit 1; }; }
need python3

if [[ ! -x ./agent_runner.sh ]]; then
  echo "agent_runner.sh not found or not executable." >&2
  exit 1
fi

echo "==> Build"
./agent_runner.sh pio run

echo "==> Scanner (STRICT)"
./agent_runner.sh python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib

echo "==> Dependency report"
./agent_runner.sh python3 tools/deps_stub_report.py src include lib
for artefact in analysis/deps_report.json analysis/deps_mermaid.md analysis/deps_graph.dot; do
  if [[ ! -s "$artefact" ]]; then
    echo "Missing artefact: $artefact" >&2
    exit 1
  fi
done
ls -lh analysis/ || true

echo "OK ✅"
