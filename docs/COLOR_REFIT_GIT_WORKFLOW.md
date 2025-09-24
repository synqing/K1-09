# K1‑09 Color Refit — Git Workflow (3‑Agent Plan)

Purpose
- Execute the palette‑first Color Refit safely and incrementally with a protected integration branch, parallel topic branches via worktrees, and strict phase acceptance gates. Conforms to ENGINEERING_PLAN.md, branching_and_worktrees.md, metrics_and_logging.md, phase_acceptance_checklist.md, pipelines.md, agent_ASSIGNMENTS.md, and K1-09_color_refit_plan.md.

Agents & Collaboration
- Three agents collaborate in every phase, operating in parallel with clear swimlanes and mutual review:
  - Agent 1 — Core Implementation: color pipeline code changes (HSV linearization, palette facade internals, quantize/post filters).
  - Agent 2 — Validation & Metrics: test harnesses, METRICS conformance, hardware captures, acceptance evidence, micro‑probes.
  - Agent 3 — Integration & Docs: integration branch stewardship, PR hygiene, docs/pipeline updates, CI wiring, feature flags.
  - All PRs require cross‑agent review (at least one reviewer from each of the two other roles).

Collaboration Model
- Short slices per phase (1–2 day cycles): each slice includes a small code change (A1), its validation assets (A2), and updated docs/CI (A3).
- Interfaces first: before code moves, A1 and A3 agree on headers/contracts; A2 drafts acceptance checks for the slice.
- Feature flags everywhere: partial functionality merges behind `ENABLE_NEW_COLOR_PIPELINE` and related toggles to keep the integration branch releasable.

Branches
- Integration (protected): `stream/03-color/refit` — PRs only, CI green + CODEOWNERS approvals. Freeze router/coordinator behavior here.
- Feature/topic branches:
  - `feat/color-hsv-linearization` (Agent 1, Phase 1)
  - `feat/color-palette-facade` (Agent 2, Phase 2 scaffolding)
  - `refactor/color-modes-migrate-a` (Agent 2, Phase 2 group A)
  - `refactor/color-modes-migrate-b` (Agent 2, Phase 2 group B)
  - `feat/color-postproc-harmonize` (Agent 3, Phase 3)
  - `chore/color-regression-and-docs` (Agent 3, Phase 4)

Worktrees (local convenience)
- From repo root:
  - `git worktree add ../K1-09-color-refit -b stream/03-color/refit`
  - `git worktree add ../K1-09-color-core -b feat/color-hsv-linearization`
  - `git worktree add ../K1-09-color-facade -b feat/color-palette-facade`
  - `git worktree add ../K1-09-color-modes-1 -b refactor/color-modes-migrate-a`
  - `git worktree add ../K1-09-color-modes-2 -b refactor/color-modes-migrate-b`
  - `git worktree add ../K1-09-color-post -b feat/color-postproc-harmonize`
  - `git worktree add ../K1-09-color-regress -b chore/color-regression-and-docs`

Phase Plan (tri‑agent slices; see K1-09_color_refit_plan.md)
- Phase 0 — Baseline Capture (All agents)
  - Agent 1: Introduce no‑op guard flags in `src/user_config.h`; stub log taps if needed.
  - Agent 2: Capture videos, METRICS once/second samples, LUT dumps (JSON), min/max brightness logs after `apply_master_brightness()`.
  - Agent 3: Create artefact buckets (`docs/retrofit_artifacts/phase0/`, `docs/palette_snapshots/`), open umbrella issue; ensure PR templates/CI are ready.
- Phase 1 — Unify HSV Output (All agents)
  - Agent 1: Implement gamma‑aware HSV → linear CRGB16 in `src/led_utilities.h:hsv(...)` behind `ENABLE_NEW_COLOR_PIPELINE` (default OFF). Keep `hsv_or_palette()` invariant.
  - Agent 2: Author parity checks (HSV vs palette) and record captures; verify METRICS format; measure perf deltas; populate hardware checklist.
  - Agent 3: Update pipelines doc to reflect HSV in linear space; wire any CI checks for metrics sample presence; manage the integration PR.
- Phase 2 — Palette Facade & Mode Migrations (All agents)
  - Agent 1: Add `src/palettes/palette_facade.h` helpers; expose only stable entry points used by modes.
  - Agent 2: Migrate modes in cohesive groups (A/B) to `pal_*` helpers; create test scenes and parity captures per group.
  - Agent 3: Review mode diffs for contract adherence, update docs/menus if UI toggles surface, maintain slice PR sequencing.
- Phase 3 — Post‑Processing Harmonization (All agents)
  - Agent 1: Implement chosen dithering/incandescent policy; ensure secondary parity and quantize symmetry.
  - Agent 2: Validate visual parity across HSV/palette and primary/secondary; log METRICS changes; update hardware checklist.
  - Agent 3: Document policy decisions; update acceptance matrix and any new serial toggles; guard defaults remain OFF.
- Phase 4 — Regression, Metrics, Docs (All agents)
  - Agent 1: Any small corrective code fixes discovered in validation; keep flags and contracts stable.
  - Agent 2: Full acceptance run per `docs/phase_acceptance_checklist.md`; compile artefacts; final parity runs.
  - Agent 3: Finalize docs, link artefacts, ensure CI/size budgets, prepare release notes; coordinate `color-a5` tag and main merge PR.

PR Requirements (all phases; tri‑agent co‑sign)
- Build via runner: `./agent_runner.sh pio run` (attach log).
- If headers/interfaces changed: strict scanner `./agent_runner.sh python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib` and deps report `./agent_runner.sh python3 tools/deps_stub_report.py src include lib` (attach artifacts).
- Artefacts:
  - Phase 0: videos, METRICS sample, LUT JSON.
  - Phase 1: HSV parity captures (HSV vs palette), toggle list, guard flag location, performance note.
  - Phase 2: list of modes migrated, screenshots/videos.
  - Phase 3: decision note (dither/incandescent), secondary parity proof.
  - Phase 4: acceptance checklist filled, doc links.
- Metrics: include a once/second sample conforming to `docs/metrics_and_logging.md` format.
- Hardware: update `docs/firmware/hardware_validation_checklist.md` Results column per run.
- Docs: update `docs/firmware/pipelines.md` if producers/consumers/ranges changed.
- Co‑sign: at least two reviewers representing the other two agent roles must approve; list co‑authors in the PR.

Acceptance Gates (integration branch)
- Align with `docs/phase_acceptance_checklist.md`:
  - Compute FPS ≥ 100; LED flips healthy; no flip violations.
  - Phase timings A–E within budget; overlap % sane.
  - I²S ring stats sane; WDT feeds present; system responsive under induced stalls.
  - Router/coordinator unchanged (frozen for this stream).
- Rollback: set `ENABLE_NEW_COLOR_PIPELINE=0`, rebuild, and verify baseline behavior.

Labels & Issues
- Epic: `Color Refit (03)` with child issues per phase and mode groupings.
- Labels: `color-refit`, `phase-0/1/2/3/4`, `hardware-required`, `docs-required`, `risk:low|med|high`.
- PR title conventions: `feat(color): hsv linearization (phase1)`, `refactor(color): migrate modes A (phase2)`.

Tags & Release Flow
- Tags (annotated) on integration branch:
  - `color-a0`: baseline
  - `color-a1`: HSV linearization merged (guarded)
  - `color-a2`: facade + first migration
  - `color-a3`: remaining migrations
  - `color-a4`: post‑processing harmonized
  - `color-a5`: regression/docs complete (ready to merge to main)
- Final merge: PR `stream/03-color/refit` → `main` with release notes and guard flip plan.

Guardrails
- Do not change I²S read semantics (full‑frame blocking); keep CONFIG defaults for sample rate/chunk.
- Router FSM remains frozen; no coordinator behavior changes in this stream.
- Post‑FX buffers: do not mutate after Stage 4/5; preserve flip/quantize safety.
- Keep guard flag default OFF until Phase 4 acceptance.

Concurrency Rules
- Interface handshakes: any change to `hsv_or_palette`, `palette_facade`, or quantize/post‑processing must be prefaced by a brief header‑only PR that A2 validates and A3 documents; code follows in the next slice.
- Conflict avoidance: A1 avoids editing mode logic in the same slice A2 migrates; A3 stages doc‑only changes separately. Use `git rebase --rebase-merges` on topic branches before pushing.
- Stacked PRs: keep slices small and stackable; link parent/child PRs; ensure each slice is releasable under flags.

Folder Structure (artifacts & snapshots)
- `docs/retrofit_artifacts/phase0/..phase4/` — store logs, videos, METRICS samples, parity proofs per phase.
- `docs/palette_snapshots/` — store JSON LUT dumps before/after.
- `.github/pull_request_template.md` — checklist aligned to this workflow.
- `.github/ISSUE_TEMPLATE/color_refit.md` — issue scaffolding for phase tasks.

Quick Commands
```bash
# Build & basic artifacts
./agent_runner.sh pio run
./agent_runner.sh python3 tools/deps_stub_report.py src include lib > analysis/deps_report.json
./agent_runner.sh python3 tools/aggregate_init_scanner.py --mode=strict --roots src include lib

# Worktrees (example)
git worktree add ../K1-09-color-refit -b stream/03-color/refit
git worktree add ../K1-09-color-core -b feat/color-hsv-linearization
```
