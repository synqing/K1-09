# Phase 3 Operating Plan — Dual‑Strip Aware, 100 FPS, Single‑Core

This document is the authoritative plan for Phase 3. It is the north‑star and the guardrail for implementation and review.

Goals
- 100 FPS target (10 ms frame budget) on a single core.
- Both LED strips are first‑class, never gated. Visual quality or cost adjustments must apply uniformly to both.
- Asynchronous LED transmission with double buffering on both strips; compute frame N while hardware transmits N‑1.
- Static allocations for all hot‑path buffers; zero dynamic allocation during runtime.
- Deterministic five‑phase loop with watchdog protection and bounded waits.

Loop Phases (Single‑Core)
- Phase A: Controls (encoders/buttons/serial), settings check, P2P.
- Phase B: Audio (bounded I²S read into ring buffer, RMS/VU, GDFT, novelty).
- Phase C: Render (per‑channel ModeRunner, post‑FX, UI) into back buffers.
- Phase D: Publish/Show (flip only when controllers report not‑busy; never write in‑flight buffers).
- Phase E: Upkeep (deferred saves, throttled logs/trace, housekeeping). Feed WDT after B, C, and D.

Dual‑Strip Aware Architecture
- Coordinator (thin): produces a Coupling Plan for the frame (pairing of modes for top/bottom, small variations like anti‑phase, hue detune ~±0.08, 1–3 frame temporal offsets). No shared mutable state.
- Operators (pure): param/index transforms only (mirror, anti‑phase, circulate, complementary). No pixel‑buffer mutation.
- Router FSM (musical awareness): dwell (4–8 beats), cooldown (2–4), complementary pair whitelist. Inject variations on strong onsets without thrashing.
- ModeRunner (per channel): owns its mode state; consumes the audio snapshot + Coupling Plan; writes to the channel’s back working buffer.

LED Output (Asynchronous, Double‑Buffered)
- Two asynchronous controllers (one per strip). Maintain front/back quantized buffers per channel.
- Compute frame N into back buffers while hardware transmits N‑1 from front buffers.
- Flip only after controllers are not busy. Never write to a buffer while its controller is busy.

Uniform QoS (No Channel Gating)
- If Phase C risks budget overrun, trim post‑FX cost symmetrically for both channels (e.g., fewer prism iterations) and/or apply a mild, smoothed, uniform brightness scale to both channels. Secondary is never disabled.

Static Allocation Policy
- Pre‑allocate all hot‑path buffers at compile time for 160 WS2812 LEDs per strip:
  - Per strip: CRGB16 working buffers, CRGB quantized buffers, front/back for both working and quantized as required by the pipeline, plus any minimal "previous frame" scratch.
- Remove all `new[]/delete[]` in the LED path; add compile‑time guards on sizes.

Instrumentation (Once/Second Summary)
- Compute FPS (Phase A–C loop) and effective FPS (successful flips).
- Per‑phase timings (avg/p95) for A/B/C/D/E.
- LED overlap % (time wire is in flight vs compute time).
- I²S ring buffer misses/overruns; deadline misses.
- Watchdog feeds count/status.
- Flip violations (write‑while‑busy) — must be zero.
- Router pair changes and variation reasons (throttled).

Branching & Worktrees
- Protected integration branch: `phase3/vp-overhaul` (PR‑only; green CI + CODEOWNERS).
- Feature branches (one topic each):
  - `refactor/visual-boundaries` — mechanical split; static alloc scaffolding.
  - `refactor/visual-registry` — one runner per channel; mechanical.
  - `feat/async-led-double-buffer` — two controllers, safe flips.
  - `feat/audio-ring-buffer` — bounded I²S; deadline fallback.
  - `feat/dual-coordinator` — Coordinator surface; minimal logic.
  - `feat/dual-operators` — mirror/anti‑phase/circulate/complementary; pure.
  - `feat/dual-router` — FSM (dwell/cooldown/whitelist/variations).
  - `refactor/uniform-degrade` — QoS trimming without gating.
  - `feat/dual-current-limiter` — optional, OFF by default.

Tag Cadence (Annotated)
- `phase3-a1` — boundaries + static alloc scaffolding + registry (mechanical).
- `phase3-a2` — async LED double buffer (both controllers) + flip assertions.
- `phase3-a3` — audio ring buffer + counters + deadline fallback.
- `phase3-a4` — Coordinator/Operators/Router minimal pairs.
- `phase3-a5` — uniform degrade; optional current limiter (default OFF).

Acceptance Criteria (Per Landing)
- No dynamic allocation in hot path (`rg` shows zero matches for `\bnew\b|delete\[\]` under src/include hot code).
- Async flips: zero write‑while‑busy violations; measured compute FPS ≥ 100 with wire overlap.
- Bounded I²S: no indefinite waits; counters show misses without frame stall; WDT feeds after B/C/D.
- Dual‑strip parity: both channels render every frame; no conditionals disable a channel.
- QoS trims only post‑FX and uniformly across both channels.

Risks & Mitigations
- FastLED async semantics differ by build → validate on pinned 3.9.x; gate flips on explicit not‑busy API.
- Heavy modes under dual render → per‑mode "cost" knob; Router prefers lighter variants when near budget.
- DRAM headroom → document DRAM usage; retain margin for LUTs; keep scratch minimal.

