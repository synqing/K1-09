# Phase 1 Parity Summary (HSV vs Neutral Palette)

- Guard: ENABLE_NEW_COLOR_PIPELINE=1 (local validation), default is 0 in branch for integration safety.
- Mode used: Waveform + VU (representative audio‑reactive).
- Comparison: `palette 0` (HSV) vs `palette 1` (neutral LUT), identical hue/value inputs.
- Result: Luminance within ~±5%; hue mapping visually consistent; no dimming artifacts.
- Metrics: see metrics_sample.txt (FPS and phase timings stable; rb counters 0 under nominal).

Notes:
- HSV path is now sRGB→linear (gamma 2.2) to match palette LUTs. Value scaling occurs in linear domain.
- Incandescent filter remains bypassed for palette modes; unchanged behavior confirmed.
- Dithering (8‑step) unchanged.

Rollback:
- Flip `src/user_config.h: ENABLE_NEW_COLOR_PIPELINE` to 0 and rebuild.
