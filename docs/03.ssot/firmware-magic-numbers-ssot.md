# Magic Numbers & Tunables (SSOT)

_Authoritative list of numeric constants that influence Sensory Bridge firmware behaviour._

Each entry documents the default value, its source, the rationale, and safe adjustment guidance. When modifying a value, update this table and reference the commit in the “Notes” column.

## Audio Capture & Analysis

| Symbol / Field | Default | Location | Purpose | Notes / Safe Range |
|----------------|---------|----------|---------|---------------------|
| `CONFIG.SAMPLE_RATE` | `DEFAULT_SAMPLE_RATE` (16_000 Hz) | `src/core/globals.cpp:21`, `src/constants.h:21` | Global audio sample rate | Maintain 16 kHz unless re-generating Goertzel tables (`system.h`). |
| `CONFIG.SAMPLES_PER_CHUNK` | `128` | `src/core/globals.cpp:28` | Chunk size passed to I²S and analysis | 128 samples @ 16 kHz ≈ 8 ms. Changing requires updating DMA config (`src/i2s_audio.h:20-33`) and history math (`SAMPLE_HISTORY_LENGTH`). |
| `soft_deadline_us` | `2500` µs | `src/i2s_audio.h:82` | Soft deadline for bounded `i2s_read` loop | Keeps Phase B < 2.5 ms. Increase only if repeated deadline misses occur; monitor `g_rb_deadline_miss`. |
| `MIN_STATE_DURATION_MS` | `1500` ms | `src/i2s_audio.h:68-72` | Minimum time in a sweet-spot state before toggling | Prevents flicker in silence detection. |
| `CONFIG.SENSITIVITY` | `1.0` | `src/core/globals.cpp:29` | Linear gain for audio scaling | Safe range 0.5–4.0. Values >2 risk clipping; adjust with caution. |
| `CONFIG.DC_OFFSET` | `-14800` | `src/core/globals.cpp:33` | Measured DC offset for ESP32-S3 microphone input | Update after hardware re-calibration; ensure `acquire_sample_chunk()` order preserved. |
| `CONFIG.SWEET_SPOT_MIN_LEVEL` | `750` | `src/core/globals.cpp:31` | Silence threshold lower bound | Derived empirically; keep below `CONFIG.SWEET_SPOT_MAX_LEVEL`. |
| `CONFIG.SWEET_SPOT_MAX_LEVEL` | `30000` | `src/core/globals.cpp:32` | Silence threshold upper bound | Protects against runaway AGC; adjust only with bench validation. |
| `AGC_FLOOR_MIN_CLAMP_RAW` / `MAX` | `400` / `4000` | `src/i2s_audio.h:194-199` | Clamp range for dynamic AGC floor | Prevents AGC collapse in silence and saturation. |

## Visual Rendering & Output

| Symbol / Field | Default | Location | Purpose | Notes / Safe Range |
|----------------|---------|----------|---------|---------------------|
| `NATIVE_RESOLUTION` | `160` | `src/constants.h:14` | Working resolution (CRGB16 buffers) | Compile-time guard; secondary strips assume matching size. |
| `CONFIG.LED_COUNT` | `160` | `src/core/globals.cpp:25` | Active LED count for primary strip | Must be ≤160; change requires verifying `scale_to_strip()` lerp tables. |
| `SECONDARY_LED_COUNT_CONST` | `160` | `src/constants.h:33` | Fixed LED count for secondary strip | Keep equal to primary to avoid scaling mismatches. |
| `CONFIG.PHOTONS` | `1.0` | `src/core/globals.cpp:14` | Global brightness scalar | Adjust via UI/serial; Phase 3 introduced linear scaling (no squaring). |
| `SECONDARY_PHOTONS` | `1.0` | `src/core/globals.cpp:318` | Secondary brightness scalar | Currently squared inside `apply_brightness_secondary()` (`src/led_utilities.h:2060-2077`); Phase 3 plan calls for linear alignment. |
| `CONFIG.PRISM_COUNT` | `1.42f` | `src/core/globals.cpp:45` | Post-FX prism iterations | Non-integer treated as blend factor; larger values increase cost. |
| `CONFIG.MAX_CURRENT_MA` | `1500` mA | `src/core/globals.cpp:38` | Software current limiter ceiling | Used by FastLED `setMaxPowerInVoltsAndMilliamps`; keep ≥ measured draw. |
| `CONFIG.TEMPORAL_DITHERING` | `true` | `src/core/globals.cpp:39` | Enables 8-step temporal dithering | Quantiser honours this flag when mapping to 8-bit. |
| `g_led_front_idx` / `g_led_back_idx` | `0` / `1` | `src/core/globals.cpp:377-378` | Async double-buffer indices | Toggle after each `FastLED.show()` (`src/led_utilities.h:1179-1187`). |

### Current Limiter (Phase 5)

| Symbol / Field | Default | Location | Purpose | Notes / Safe Range |
|----------------|---------|----------|---------|---------------------|
| `ENABLE_CURRENT_LIMITER` | `1` | `src/constants.h:…` | Compile-time gate for limiter scaffold | Keep enabled (build-time); runtime is off by default. |
| `g_current_limiter_enabled` | `false` | `src/core/globals.cpp:…` | Runtime gate to activate limiter | Toggle via serial/UI once exposed. |
| `CURRENT_LIMITER_MA_PER_CHANNEL` | `20.0` mA | `src/constants.h:…` | Per-channel max current for WS2812 variant | IMPORTANT: depends on LED package (e.g., 5050 WS2812B ≈ 20 mA/channel; 3535/2020 may be lower: 5–12 mA). Confirm package and update. |
| `CONFIG.MAX_CURRENT_MA` | `1500` mA | `src/core/globals.cpp:38` | Total budget (both strips combined) | Limiter scales uniformly when estimate exceeds budget. |
| `g_current_limit_engaged` | counter | `src/core/globals.cpp:…` | Counts limiter activations | Planned to surface in METRICS in Phase 5 metrics extension. |
| `g_current_estimate_ma_ema` | mA (float) | `src/core/globals.cpp:…` | Smoothed current estimate (EMA) | For debugging/telemetry; not persisted. |

Estimation method (uniform): sum of 8-bit RGB across pixels scaled by `CURRENT_LIMITER_MA_PER_CHANNEL / 255`, aggregated across both strips; uniform post-quantization scaling applied when over budget. See `show_leds()` in `src/led_utilities.h`.

## Coordinator, Router & Coupling

| Symbol / Field | Default | Location | Purpose | Notes / Safe Range |
|----------------|---------|----------|---------|---------------------|
| `RouterState.cooldown_remaining` | 2–4 beats | `src/dual_coordinator.cpp:123-137` | Cooldown after pair change | Beats approximated using 500 ms/beat assumption; adjust heuristics in router FSM when audio BPM detection improves. |
| `variation_chance` scaling | `*20` (max 40%) | `src/dual_coordinator.cpp:151` | Probability of applying variation | Combine novelty and energy factors; review alongside RouterReason metrics. |
| `plan.phase_offset` default | `0.0–0.1` random | `src/dual_coordinator.cpp:69-73` | Small temporal offsets between strips | Expand only after verifying async buffer alignment on hardware. |
| `plan.hue_detune` default | ±0.04 | `src/dual_coordinator.cpp:69-73` | Hue variation between strips | Cap ensures small colour detune; ±0.08 applied when variation pending. |

## Runtime Safety & Instrumentation

| Symbol / Field | Default | Location | Purpose | Notes / Safe Range |
|----------------|---------|----------|---------|---------------------|
| `esp_task_wdt_init(2, true)` | 2 s | `src/main.cpp:156-164` | Task watchdog timeout | Keep ≥1 s while loops remain non-blocking; revisit if Phase 5 tightening occurs. |
| `METRICS` interval | ~4 s | `src/main.cpp:469-475`, `src/main.cpp:764-770` | Emit averaged phase timings | Controlled by `metrics_win_start_ms` logic. |
| `g_rb_reads` / `g_rb_deadline_miss` | Counters | `src/core/globals.cpp:371-374` | Audio ring buffer health | Phase 5 instrumentation should expose these via metrics stream. |
| `g_flip_violations` | Counter | `src/core/globals.cpp:385` | Guard against write-while-busy | Incremented via `report_flip_violation()` (`src/led_utilities.h:136-144`); keep at zero. |

## How to Update This Document
1. Locate the constant or heuristic in source.
2. Record the default, file:line (use `nl -ba`), and a brief rationale.
3. Document safe adjustment practices (bench steps, metrics to watch).
4. Cross-reference dependent code paths or docs (e.g., if changing `CONFIG.SAMPLES_PER_CHUNK`, update `docs/03.ssot/firmware-pipelines-ssot.md`).
