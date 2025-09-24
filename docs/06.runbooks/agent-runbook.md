# Agent Runbook (Quickstart)

## Build & Size
- Build: `platformio run`
- Size: `python3 scripts/verify_size_budget.py --baseline ci/size-baseline.json`

## Docs
- SSOT: `docs/03.ssot/*`
- Run docs lint (if enabled): see CI job `docs-lint`

## Metrics
- Watch for consolidated `METRICS …` lines (see `docs/99.reference/metrics-and-logging.md`).

## PR Checklist
- Attach build/size logs, link to updated SSOT sections
- Check “SSOT docs reviewed/updated” in PR template
