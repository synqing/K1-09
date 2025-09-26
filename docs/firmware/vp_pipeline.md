# Visual Pipeline (VP) Overview

This document describes the VP layer that consumes `AudioFrame` from the audio producer and drives visuals. It establishes interfaces, timing, configuration, and debug controls.

## Interfaces
- Input: `AudioFrame` (linear Q16.16) from `src/AP/audio_bus.h:14` via `acquire_spectral_frame()`.
- Public API: `src/VP/vp.h:7`
  - `vp::init()` — initialize VP config and dependencies.
  - `vp::tick()` — snapshot latest `AudioFrame`, render one VP frame.

## Snapshot Discipline
- Use `audio_frame_utils::snapshot_audio_frame()` to copy a frame only when `audio_frame_epoch` is stable. See `src/AP/audio_bus.h:92`.
- VP wraps this in `vp_consumer::acquire()` (see `src/VP/vp_consumer.h:8`).

## Timing
- Producer cadence: ~125 Hz (chunk_size=128 @ 16 kHz). VP runs 1:1 with producer in `src/main.cpp:109`.
- Keep `vp::tick()` bounded and allocation-free.

## Configuration
- Module: `src/VP/vp_config.h:6`
  - `use_smoothed_spectrum` (bool): choose `smooth_spectral` vs `raw_spectral` for displays.
  - `debug_period_ms` (u32): min interval for VP debug prints.
- Persistence: NVS namespace `"vp"` with keys `use_smoothed`, `dbg_period`. See `src/VP/vp_config.cpp:9`.

## Debug Controls
- Toggle VP debug stream: press `4`. See `src/main.cpp:161` and `src/debug/debug_flags.h:7` (`kGroupVP`).
- When enabled, VP prints at most once per `debug_period_ms` a compact line with epoch, bpm/phase/conf, beat flag/strength, flux, and peak spectrum bin. See `src/VP/vp_renderer.cpp:108`.

## Render Passes (Current)
- Hardware output: two identical, physically separated 160‑LED WS2812 strips. CH1 on GPIO9, CH2 on GPIO10. Both strips are always active.
- Center-origin mapping: each strip renders symmetrically about indices 79/80. Visuals originate at the center and expand outwards (or can be inverted later to originate at edges and move inward).
- Band composition: the 80‑pixel radius on each side is split into four equal 20‑pixel radial segments for `band_low`, `band_low_mid`, `band_presence`, and `band_high`. Each segment’s fill length is proportional to its band energy with flux/beat overlays.
- Serial debug (gated by flag 4) continues to report tempo/flux/bands for validation.

## Next Steps
- Expand visuals: layer chroma strip, spectrum bars, and palette modulation onto the hardware buffer.
- Expose runtime config setters (serial or HMI) and persist via NVS.
- Add VP-side profiling hooks to ensure render cost stays well below the 8 ms frame budget.
