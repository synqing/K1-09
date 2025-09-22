# Sensory Bridge Audio & Visual Pipelines

> **Source of truth** for data producers, consumers, types, ranges, and guardrails spanning the audio capture stack through visual rendering. Every change MUST reference this document and update impacted entries before merge.

## 1. Operating Assumptions
- Build target: `esp32-s3-devkitc-1` (`platformio.ini`)
- Default configuration: `CONFIG` aggregate in `src/core/globals.cpp:11`
- All timing assumes `CONFIG.SAMPLE_RATE = 16_000` Hz and `CONFIG.SAMPLES_PER_CHUNK = 256`
- CPU affinity: both audio and LED loops execute on Core 0 (see `src/main.cpp:134`)
- Fixed-point math uses `SQ15x16` (signed Q15.16) unless stated otherwise.

## 2. Audio Pipeline Overview
```
[I2S Mic] --(32-bit PCM)--> audio_raw_state.samples_raw_ (int32_t[1024])
   -> scaling/offset clamp (int16_t)
   -> audio_processed_state.waveform_ (short[1024])
   -> sample_window (short[4096]) sliding buffer
   -> process_GDFT() -> magnitudes_normalized (float[NUM_FREQS])
   -> smoothing/noise gates -> spectrogram (SQ15x16[NUM_FREQS])
   -> chromagram/novelty calculations
```

### 2.1 Stage Catalog

| Stage | Producer (File:Line) | Output Symbol | Type / Shape | Nominal Range | Consumers | Notes & Magic Numbers |
|-------|----------------------|---------------|--------------|---------------|-----------|-----------------------|
| I2S DMA read | `i2s_read()` via `acquire_sample_chunk()` (`src/i2s_audio.h:36-73`) | `audio_raw_state.getRawSamples()` | `int32_t[CONFIG.SAMPLES_PER_CHUNK]` | Raw I2S left-justified 32-bit samples (±8M) | Local scaler | `portMAX_DELAY` ensures full chunks; `bytes_expected = CONFIG.SAMPLES_PER_CHUNK * 4`.
| Linear scaling & DC removal | `acquire_sample_chunk()` (`src/i2s_audio.h:79-132`) | `waveform[i]` & history | `short[256]` (±32767) | Clamped to ±32767 | `audio_processed_state`, `sample_window` | Uses `CONFIG.SENSITIVITY` (default 1.0) and `CONFIG.DC_OFFSET` (-14800). `MIN_STATE_DURATION_MS = 1500` governs sweet-spot transitions.
| Raw peak tracking | `acquire_sample_chunk()` (`src/i2s_audio.h:134-207`) | `audio_processed_state.updatePeak`, `max_waveform_val_raw` | `float` | 0 – 32767 | Downstream AGC & sweet spot | Magic factors: attack `0.5`, decay `0.02`, follower clamp `CONFIG.SWEET_SPOT_MIN_LEVEL` 750.
| Silent detection & AGC | same | `silence`, `silent_scale`, `current_punch` | `bool`, `float` | `silent_scale` 0–10 | LED thread gating | Uses dynamic floor clamps: `AGC_FLOOR_MIN_CLAMP_RAW = 400`, `MAX` constants from `src/i2s_audio.h:158-173` (tuned for ESP32-S3).
| Sample history | same | `audio_raw_state.waveform_history_[4][1024]` | `short` ring buffer | Clamped waveform | `process_GDFT` (indirect) | History advances each chunk; guard magic `0xABCD1234` ensures integrity.
| Sliding window | `sample_window` update in `acquire_sample_chunk()` (`src/i2s_audio.h:209-248`) | `sample_window[SAMPLE_HISTORY_LENGTH]` | `short[4096]` | ±32767 | `process_GDFT()` | Window shift cost mitigated via `memmove`. Window length constant `SAMPLE_HISTORY_LENGTH = 4096` (256 chunks @ 16 kHz ≈ 256 ms context).
| Goertzel transform | `process_GDFT()` (`src/GDFT.h:60-166`) | `magnitudes_normalized[i]` | `float[NUM_FREQS]` | ≥0 (A-weighted) | LED spectral consumers | `NUM_FREQS = 64` (constants), `frequencies[i].block_size` from `system.h:287`. Magic constant `0x5f375a86` used for reciprocal sqrt optimisation.
| Noise calibration | `process_GDFT()` (`src/GDFT.h:168-210`) | `noise_samples`, `noise_complete` | `SQ15x16[64]` | 0–1 normalized | Same stage | Calibration window: 256 iterations; `CONFIG.DC_OFFSET` recomputed and persisted.
| Spectrogram smoothing | `process_GDFT()` (`src/GDFT.h:212-302`) | `spectrogram`, `spectrogram_smooth`, `chromagram_smooth` | `SQ15x16[]`, `float[]` | 0–1 normalized | Light modes, serial debug | Exponential smoothing factors: `0.3` for magnitude EMA, `0.1` for novelty.
| Audio metrics export | `process_GDFT()` and `serial_menu` | `note_spectrogram`, `chromagram` etc. | `float`, `SQ15x16` arrays | Varies 0–1 | `lightshow_modes.h`, `serial_menu` | `CONFIG.CHROMAGRAM_RANGE` default 60 (notes), ensures index safety via `safe_notes_access()` in `system.h:242`.

### 2.2 Producer/Consumer Matrix

| Symbol | Producer | Primary Consumers |
|--------|----------|-------------------|
| `audio_raw_state.samples_raw_` | `i2s_audio` | Scaling stage only |
| `waveform` | `audio_processed_state` | `lightshow_modes`, `serial_menu`, `test_audio_diagnostics` |
| `waveform_fixed_point` | `audio_processed_state` | `GDFT` (fixed-point helpers), `lightshow_modes` |
| `sample_window` | `i2s_audio` | `process_GDFT` |
| `magnitudes_normalized` | `process_GDFT` | `spectrogram`, `noise_cal`, `lightshow_modes` |
| `spectrogram` / `_smooth` | `process_GDFT` | `lightshow_modes` (spectral render paths), `palettes_bridge` |
| `chromagram` | `process_GDFT` | `LIGHT_MODE_GDFT_CHROMAGRAM*`, serial reporting |
| `silent_scale`, `current_punch` | `i2s_audio` | `lightshow_modes` gating (e.g., Bloom energy), `led_utilities` |

## 3. Visual Pipeline Overview
```
[spectrogram/chromagram] --> lightshow_modes::<mode>() --> leds_16 buffers (CRGB16)
   -> led_utilities::apply_gamma/dither -> FastLED controller --> strips
```

### 3.1 Stage Catalog

| Stage | Producer | Output | Type / Shape | Nominal Range | Consumers | Notes |
|-------|----------|--------|--------------|---------------|-----------|-------|
| LED Frame Init | `led_thread()` (`src/main.cpp:406-520`) | `leds_16`, `leds_16_fx`, `leds_16_ui` | `CRGB16[CONFIG.LED_COUNT]` | 0.0–1.0 (Q8.8) | Mode renderers | Frame seq counters `g_frame_seq_*` ensure producer/consumer sync.
| Palette preparation | `led_utilities::update_palette_buffers()` (`src/led_utilities.h:52-162`) | `palette_*` LUTs | `CRGB16[]`, `SQ15x16[]` | 0.0–1.0 | Light modes | Magic numbers: dithering table `dither_table[8]`, clamp `SATURATION` 0–1.
| Mode dispatch | `lightshow_modes::render_active_mode()` (`src/lightshow_modes.h:38-173`) | Mode-specific buffers | `CRGB16[]`, floats | 0–1 | LED output, debug overlay | Uses `CONFIG.LIGHTSHOW_MODE`, `CONFIG.MIRROR_ENABLED`.
| GDFT mode (default) | `light_mode_gdft()` (`src/lightshow_modes.h:175-370`) | `leds_16_fx` spectral columns | `CRGB16[160]` | 0–1 | LED compositing | Consumes `spectrogram_smooth`, uses `CONFIG.PHOTONS` brightness and notes-based hue via `hue_lookup[NUM_FREQS]`.
| Chromagram variants | `light_mode_gdft_chromagram_*` | `leds_16_fx` | `CRGB16[]` | 0–1 | LED compositing | Depend on `chromagram_smooth[12]`; requires `CONFIG.CHROMAGRAM_RANGE` alignment.
| Bloom mode | `light_mode_bloom()` (`src/lightshow_modes.h:520-704`) | `leds_16_fx`, `leds_16_prev_secondary` | `CRGB16[]` | 0–1 (with decay) | LED compositing | Magic numbers: `BLOOM_DECAY = 0.78`, `SPARKLE_THRESHOLD = 0.45`. Relies on `current_punch` and `silent_scale` for gating.
| Quantum Collapse | `light_mode_quantum_collapse()` (`src/lightshow_modes.h:707-1040`) | Particle buffers & `leds_16_fx` | `float[]`, `CRGB16[]` | 0–1 / world units | LED compositing | Uses physics constants: `FLUID_DIFFUSION = 0.035`, `PARTICLE_DRAG = 0.98`, `collapse_probability` formula tuned for drum hits.
| Waveform mode | `light_mode_waveform()` (`src/lightshow_modes.h:1095-1300`) | `leds_16_fx` | `CRGB16[]` | 0–1 | LED compositing | Directly consumes `audio_processed_state.getWaveform()`; requires preserved int16 scaling.
| Frame compositing | `led_utilities::compose_frame()` (`src/led_utilities.h:240-402`) | `leds_out` (FastLED) | `CRGB[160]` | 0–255 per channel | FastLED controller | Applies: temporal dithering (`dither_table`), incandescent filter (`CONFIG.INCANDESCENT_FILTER`), mirror (`CONFIG.MIRROR_ENABLED`), current limiter (`CONFIG.MAX_CURRENT_MA`).
| Output | `FastLED.show()` invoked inside LED thread | Physical LEDs | WS2812 data stream | — | Hardware | Must respect `CONFIG.MAX_CURRENT_MA` to avoid brownouts.

### 3.2 Producer/Consumer Matrix

| Symbol | Producer | Consumers |
|--------|----------|-----------|
| `spectrogram`, `chromagram` | `process_GDFT` | All GDFT-derived light modes, serial diagnostics |
| `leds_16` | `led_thread` (init) & modes | `led_utilities::compose_frame`, Mabutrace traces |
| `leds_16_prev`, `leds_16_prev_secondary` | Modes (Bloom, Quantum) | Provide inertia across frames |
| `ui_mask`, `leds_16_ui` | `led_utilities` & HMI layer | compositing + menu overlays |
| `hue_lookup[NUM_FREQS][3]` | Constants (`src/constants.h:94`) | Direct conversion of note index -> CRGB16 | Preserve aggregate syntax to avoid FixedPoints corruption.

## 4. Magic Number Registry

| Identifier | Value | Location | Purpose | Tuning Constraints |
|------------|-------|----------|---------|-------------------|
| `CONFIG.SAMPLES_PER_CHUNK` | 256 | `src/core/globals.cpp:24` | Window size for audio chunks; 256 samples @ 16kHz ≈ 16 ms latency | Must remain power-of-two for `memmove` optimisations; reduces/increases latency. Update `i2s_config.dma_buf_len` in `src/i2s_audio.h:20` if changed.
| `MIN_STATE_DURATION_MS` | 1500 | `src/i2s_audio.h:51` | Prevents AGC thrash between loud/quiet states | Shorter → flicker; Longer → sluggish response.
| `AGC_*_CLAMP_*` | see `src/i2s_audio.h:158-173` | Define min/max floors for dynamic AGC | Changing affects quiet-room sensitivity; keep 300–4000 range to avoid clipping.
| `noise_iterations >= 256` | `src/GDFT.h:188` | Completes noise calibration | Lower value risks under-sampling; higher delays startup.
| `Magic sqrt constant` | `0x5f375a86` | `src/GDFT.h:121` | Fast inverse sqrt for magnitude normalization | Do not change unless replacing with precise math; tied to floating-point bit structure.
| `BLOOM_DECAY` | 0.78 | `src/lightshow_modes.h:603` | Energy persistence for Bloom | <0.5 eliminates trails; >0.9 smears.
| `Quantum collapse cooldown` | `audio_level > average * 1.3 && >0.15` | `src/lightshow_modes.h:854-873` | Beat detection gating | Lower thresholds trigger constant collapses.
| `dither_table` entries | `see src/constants.h:167` | Temporal dither offsets | Maintain ascending fractional pattern; required for even energy distribution.
| `CONFIG.MAX_CURRENT_MA` | 1500 | `src/core/globals.cpp:31` | Current limiter for LED supply | Exceeding hardware limit can brown-out supply; adjust in tandem with PSU rating.

All magic numbers above require **impact analysis tickets** before modification. Include producer/consumer references in Task-Master entries.

## 5. Validation & Instrumentation

| Validation Gate | Command / Procedure | Expected Output |
|-----------------|---------------------|-----------------|
| Firmware build | `pio run` | Success with firmware image under `.pio/build/...` |
| Aggregate-init guard | `python tools/aggregate_init_scanner.py --mode=strict --roots src include lib` | `Aggregate-init scan: OK` |
| Pipeline documentation sync | `scripts/export_pipeline_map.py` *(to be implemented Phase 1b)* | Generates JSON/Markdown summary committed with doc |
| Hardware smoke (post-change) | Follow `docs/firmware/hardware_validation_checklist.md` | Fill `Result` column + link logs |
| Spectrogram sanity | Enable `DEBUG` logging, run `serial_menu` `diag` commands | Peaks align with played tones |

## 6. Change Management Rules
1. No producer datatype changes without evaluating all consumers listed in the matrices above.
2. If `CONFIG` defaults are adjusted, update this document **and** add regression notes to `docs/firmware/baseline_snapshot.md`.
3. New lightshow modes must describe inputs (spectrogram/chromagram/waveform) and expected ranges here before merge.
4. When adding DSP stages, extend Stage Catalog tables and annotate new magic numbers.
5. Hardware validation entries must be updated with run IDs or log links for each release candidate.

---
_Last synced: see baseline commit `chore(init): capture baseline before refactor` (Git SHA 7de570f)._ 
