# Agent Assignments (Phase 3)

Agent A — Branch `feat/dual-coordinator`
- Task: Modes consume coordinator plan
- Doc: `docs/phase3_modes_plan_consumer_task.md`
- Code: read `frame_config.coordinator_*` inside Waveform/Bloom/GDFT/Chromagram, apply transforms during synthesis only.

Agent B — Branch `feat/dual-router`
- Task: Router FSM + uniform degrade
- Doc: `docs/phase3_dual_router_task.md`
- Code: extend `coordinator_update()` or `router_fsm_tick()` to drive `g_coupling_plan`; add QoS trims post‑FX, symmetric for both strips.

Shared Rules
- Do not mutate Stage‑4/5 buffers post‑render.
- Keep mirroring contracts intact; no midpoint reorders.
- Preserve flip/quantize safety; acceptance checks must pass.

