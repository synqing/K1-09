# Phase 4 State of Truth (Collaborative Dual-Agent Execution)

This plan mirrors the Phase 3 SoT style and focuses on establishing the firmware SSOT documentation set. Work is split so two Codex agents can execute in parallel while keeping outputs aligned.

---

## Objective
- Produce authoritative documentation under `docs/` that reflects the 120 FPS / 8 ms single-core dual-strip architecture.
- Maintain continuous alignment between the agents: Agent A owns pipeline/scheduling narratives, Agent B covers constants, verification, and doc tooling.

## Workstreams & Parallel Breakdown

Reference collateral:
- Issues: `docs/05.tracking/phase-4-issues.json` (raw) and `docs/05.tracking/phase-4-issues.md` (summary for tracker)
- Worktrees ready: `../K1-09-docs-pipelines`, `../K1-09-docs-scheduling`, `../K1-09-docs-magic`, `../K1-09-docs-tooling`

| Track | Agent | Scope |
|-------|-------|-------|
| Doc Architecture | Agent A | Draft `docs/03.ssot/firmware-pipelines-ssot.md` (dual-strip flow, ModeRunner → Coordinator → Operator → Router, buffer hand-offs, timing diagrams). Update `docs/99.reference/visual-pipeline-overview.md` references and FPS target. |
| Scheduling & Timing | Agent A | Create `docs/03.ssot/firmware-scheduling-ssot.md` (five-phase loop, per-phase budget within 8 ms frame, watchdog feed cadence, async flip discipline, core affinity). Validate timings with current instrumentation outputs. |
| Constants & Safety | Agent B | Build `docs/03.ssot/firmware-magic-numbers-ssot.md` cataloguing tuned constants (AGC thresholds, dwell/cooldown beats, prism iterations, limiter bounds, buffer lengths) including rationale, acceptable ranges, rollback notes. |
| Tooling & Quality Gates | Agent B | Ensure Markdown lint/Vale (if configured) run clean; add doc-sync checklist entry to PR template draft; verify `agent_runner.sh pio run` still passes after doc changes. |

## Acceptance Checklist
- `./agent_runner.sh pio run`
- `vale docs/03.ssot` or `markdownlint` (if tooling available)
- `rg -n "TODO|TBD" docs/03.ssot` → returns zero lines
- Manual confirmation that diagrams, phase budgets, and constants match current code paths (`coordinator_update`, `show_leds`, `acquire_sample_chunk`).

## Risks & Mitigations
- **Stale measurements** → Capture fresh per-phase timings before publishing; note tool versions.
- **Documentation drift** → Add “SSOT docs reviewed/updated” checkbox to PR template (handled by Agent B).

## Git & Worktree Instructions
- Protected branch stays `phase3/vp-overhaul` until Phase 3 merges; Phase 4 branches PR into that branch.
- Per-workstream branches:
  - `docs/pipelines-ssot` (Agent A)
  - `docs/scheduling-ssot` (Agent A)
  - `docs/magic-numbers` (Agent B)
  - `infra/docs-tooling` for lint/checklist updates (Agent B)
- Suggested worktrees:
  ```bash
  git worktree add ../K1-09-docs-pipelines   -b docs/pipelines-ssot
  git worktree add ../K1-09-docs-scheduling  -b docs/scheduling-ssot
  git worktree add ../K1-09-docs-magic       -b docs/magic-numbers
  git worktree add ../K1-09-docs-tooling     -b infra/docs-tooling
  ```
- Coordination: Agents sync via a `phase4/docs` integration branch, merging sequentially with reviews.

## Transition Notes
- Confirm all existing materials reference the 120 FPS / 8 ms targets before drafting the SSOT docs.
- Phase 4 deliverables provide the foundation for Phase 5 tooling (size baselines, limiter documentation); cross-review for consistency.
