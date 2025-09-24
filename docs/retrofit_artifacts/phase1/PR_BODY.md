feat(color): hsv linearization (phase1)

Phase 1 (Agent A): Implement gamma-aware HSV (sRGB→linear, gamma=2.2) under ENABLE_NEW_COLOR_PIPELINE (currently ON for validation). Palette path unchanged.

Build
- platformio run (env: esp32-s3-devkitc-1): SUCCESS

Runtime
- Verified stable FPS and I²S OK on device
- Parity check target: HSV (palette 0) vs neutral palette (palette 1) within ~±5% luminance

Guardrails
- No I²S/router/coordinator changes in this stream
- Defaults preserved (sample rate, chunk size, dithering)

Guard Flag
- src/user_config.h: ENABLE_NEW_COLOR_PIPELINE (set to 1 for validation; flip to 0 to rollback)

Touched Files
- src/led_utilities.h — guarded hsv() now gamma-aware (linear output)
- src/user_config.h — added guard macro

Next Steps
- Reviewer: confirm parity on your rig; if good, merge and tag color-a1
