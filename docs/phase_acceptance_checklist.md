# Phase 3 Acceptance Checklist (Runtime & CI)

Runtime (once/second summary should report all of these):
- FPS: compute_fps >= 100; effective_fps tracks successful flips.
- Phases A/B/C/D/E timings (avg and p95) — all within 10 ms total.
- LED overlap % (wire‑in‑flight time vs compute time).
- I²S ring stats: misses/overruns/deadline_misses counters (expect low, non‑zero under induced stalls; zero under nominal).
- WDT: feeds after B, C, D; no resets.
- Flip safety: write‑while‑busy violations = 0.
- Router logs: pair changes/variation reasons appear throttled; no oscillation.

Fast Checks (manual or scripted):
- `rg -n "\\bnew\\b|delete\\[\\]" src include | wc -l` returns 0 for hot codepaths.
- `rg -n "busy|flip|front|back" src include` shows calls at flip boundaries only.
- Inject a synthetic I²S stall: loop remains responsive; deadline fallback engaged; counters increment; WDT fine.

CI Guardrails:
- Build succeeds (debug/release) with `SECONDARY_FIRST_CLASS=1`.
- Header cop passes (no illegal cross‑module includes; no .cpp includes under include/).
- Size budget diff within threshold; override label required if exceeded.
- Staged files ban: no `analysis/**` or `compile_commands.json` added in PRs.

