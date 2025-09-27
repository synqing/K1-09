# Realtime Firmware Optimizer

Goal
- Hit {budget_us} μs/tick and stabilize {signal} on {MCU} at {rate}/chunk.

Relevance / Audience
- Firmware engineers optimizing ISR/tick loops, DSP paths, and real‑time systems.

Inputs
- Critical paths: {functions/files}
- Observed issues: {jitter/overruns/missed beats}
- Target build: {env/flags}

Acceptance
- Max tick time ≤ {budget_us} μs (p50/p95), no overruns over {N} minutes, {signal} variance ≤ {spec}.

Plan
1) Insert timing probes around suspects; throttle prints (every N ticks).
2) Rank hotspots (≥80/20); identify quick wins.
3) Propose micro‑optimizations (layout, LUTs, fixed‑point, branches).
4) Add compile/runtime gating for risky changes.
5) Implement smallest diffs; verify timings.
6) Document toggles and commit.

Action Steps
- Add `t0=micros()`/`dt=micros()-t0` around: {list}.
- Emit `[perf]` lines every {N} ticks; capture 1–2 min.
- Convert {float} to fixed‑point where safe; unroll or fuse loops with care.
- Align data, reduce cache misses, precompute constants.

Deliverables
- Before/after timing table, diffs, toggles, commit message.

Diagnostics
- `[perf] tick_us=... max=... budget=... overruns=...` with cadence.

Risks & Guardrails
- Maintain bit‑exactness where required; use feature flags; provide rollback.

