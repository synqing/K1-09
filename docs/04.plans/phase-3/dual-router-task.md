# Phase 3 – Dual Router + QoS (Agent B)

Scope
- Implement Router FSM (dwell/cooldown/variation) driving `g_coupling_plan` while avoiding thrash.
- Add uniform post‑FX degrade controls to trim cost symmetrically when near frame budget.

Starting Points
- Coordinator scaffolding and plan export are present (see `src/dual_coordinator.*`).
- Plan metadata available to modes via `frame_config.coordinator_*`.
- LED pipeline phases and flip/quantize safety must remain intact.

Deliverables
- Router FSM: deterministic dwell 4–8 beats, cooldown 2–4; complementary pair whitelist; throttled variation logs.
- QoS degrade: smooth cost trimming (e.g., fewer prism iterations) + optional smoothed brightness scaler (OFF by default).
- Acceptance: no channel gating; safe flips; compute stays under budget; logs show stable selections.

TODOs
1. router_fsm: formalize state struct + tick() API (time/novelty/vu inputs).
2. pair selection: whitelist + anti-phase/hue detune combos with reasons.
3. degrade policy: C/D timing metrics → mild/symmetric trims (Stage 2/3/FX only).
4. logs/metrics: once/4s summary; reason codes; variation cadence.

