# Phase 3 – Modes Consume Coordinator Plan (Agent A)

Scope
- Update key lightshow modes to read `frame_config.coordinator_*` and apply small, deterministic transforms before writing `leds_16`.
- Never reorder or mutate buffers post‑render; keep mirroring/index contracts intact.

Targets
- Waveform, Bloom, GDFT, GDFT Chromagram (initial set).

Plan
1. Read-only plan ingest: phase_offset, anti_phase, hue_detune, intensity_balance, variation_type.
2. Apply within mode synthesis:
   - temporal offset via sampling shift or light trail seed (no buffer rotate).
   - anti-phase via index symmetry in writes (not output reversal).
   - hue detune in color math prior to Stage 4/5.
3. Guardrails: mirrored halves must remain paired; add sanity check (throttled warn) if disparity detected.

Acceptance
- Visual parity preserved; no mid‑strip distortions.
- Stage 4/5 unchanged; flip assertions remain green.
- Logs show plan values consumed (throttled).
