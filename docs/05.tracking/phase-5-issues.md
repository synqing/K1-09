# Phase 5 Issue Backlog

| Title | Labels | Paths | Acceptance Checks |
|-------|--------|-------|-------------------|
| Phase 5: CI size-budget job and baseline | infrastructure, phase5, agent-a, stream:04-docs-qa | `.github/workflows/**`, `ci/size-baseline.json`, `scripts/verify_size_budget.py` | `pio run -t size`; `python scripts/verify_size_budget.py` |
| Phase 5: PR template refresh + docs lint job | infrastructure, phase5, agent-a, stream:04-docs-qa | `.github/PULL_REQUEST_TEMPLATE.md`, `.github/workflows/**` | `rg -n "doc-impact" .github/PULL_REQUEST_TEMPLATE.md`; `gh workflow run docs-lint --dry-run` |
| Phase 5: Optional current limiter (default OFF) | feature, phase5, agent-b, stream:05-limiter | `src/led_utilities.h`, `src/main.cpp`, `docs/03.ssot/firmware-magic-numbers-ssot.md` | `rg -n "CURRENT_LIMITER_ENABLED" src include`; `rg -n "g_current_limit_engaged" src include` |
| Phase 5: Metrics extension for limiter + overlap | feature, phase5, agent-b, stream:05-limiter | `src/main.cpp`, `src/led_utilities.h`, `docs/03.ssot/firmware-scheduling-ssot.md` | `rg -n "overlap" src/main.cpp`; `rg -n "g_flip_violations" docs/03.ssot/firmware-scheduling-ssot.md` |

> Use `gh issue create --title "…" --body-file docs/05.tracking/phase-5-issues.json` (or paste details above) to populate the tracker.
