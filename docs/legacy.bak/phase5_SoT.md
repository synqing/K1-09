# Phase 5 State of Truth (Collaborative Dual-Agent Execution)

This plan extends the SoT series to automation hardening and runtime safety. Responsibilities are split for parallel execution by two Codex agents.

---

## Objective
- Harden CI with size budgets and documentation lint while introducing the optional current limiter, preserving 120 FPS dual-strip guarantees.
- Agent A leads infrastructure (CI, templates, guardrails); Agent B delivers runtime limiter + instrumentation and validation.

## Workstreams & Parallel Breakdown

Reference collateral:
- Issues: `docs/05.tracking/phase-5-issues.json` (raw) and `docs/05.tracking/phase-5-issues.md`
- Worktrees ready: `../K1-09-ci-size`, `../K1-09-pr-template`, `../K1-09-docs-lint`, `../K1-09-current-limit`, `../K1-09-metrics-limit`

| Track | Agent | Scope |
|-------|-------|-------|
| CI & Size Budgets | Agent A | Establish size baseline (`pio run -t size`), craft diff script vs baseline, integrate into CI (GitHub Actions job `size-budget`). Maintain override label `size-budget-exempt`. |
| PR Template & Docs Lint | Agent A | Update PR template with doc-impact/instrumentation checklist; add docs lint job (`vale`/`markdownlint`). Ensure verify gate (`ci/verify.sh`) runs size check. |
| Optional Current Limiter | Agent B | Implement EMA-based limiter in `led_utilities` (post-quantize, pre-dither), default OFF (`CONFIG.CURRENT_LIMITER_ENABLED`). Add instrumentation counters (`g_current_limit_engaged`) and CLI/serial toggle. |
| Bench Validation & Metrics | Agent B | Stress-test worst-case mode pairs with limiter on/off, capture rail voltage and FPS impact (<5%). Extend once-per-second metrics to include limiter engagement counts. |

## Acceptance Checklist
- `./agent_runner.sh pio run` (debug + release)
- `pio run -t size` + verification script confirming usage within ±5 % of baseline
- CI workflow passes (`size-budget`, `docs-lint`, existing verify gate)
- `rg -n "current_limit" src include` includes instrumentation; bench log demonstrates limiter behavior with <5 % FPS drop

## Risks & Mitigations
- **Toolchain drift → false size failures** → Store tool versions with baseline; require approval label for intentional increases.
- **Limiter causes visible dimming** → Keep default OFF, expose runtime flag; rollback documented in `docs/03.ssot/firmware-magic-numbers-ssot.md`.

## Git & Worktree Instructions
- Feature branches:
  - `infra/ci-size-budget` (Agent A)
  - `infra/pr-template-refresh` (Agent A)
  - `infra/docs-lint` if separated (Agent A)
  - `feat/current-limiter` (Agent B)
  - `feat/metrics-limit` (Agent B) for instrumentation updates if needed
- Suggested worktrees:
  ```bash
  git worktree add ../K1-09-ci-size       -b infra/ci-size-budget
  git worktree add ../K1-09-pr-template   -b infra/pr-template-refresh
  git worktree add ../K1-09-docs-lint     -b infra/docs-lint
  git worktree add ../K1-09-current-limit -b feat/current-limiter
  git worktree add ../K1-09-metrics-limit -b feat/metrics-limit
  ```
- Coordination: use `phase5/infra` (Agent A) and `phase5/runtime` (Agent B) integration branches, merging through reviewed PRs.

## Transition Notes
- Leverage Phase 4 documentation outputs for limiter references and instrumentation expectations.
- Capture baseline size numbers immediately after Phase 4 lands to avoid drift.
