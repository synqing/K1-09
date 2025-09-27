# Module & API Design Specification

This document details public interfaces, module responsibilities, and key data flows for the upgraded Sensory Bridge firmware. File references use repo-relative paths.

---

## 1. Audio Producer (AP)

### 1.1 Interfaces
| Symbol | Signature | Description |
|--------|-----------|-------------|
| `audio_pipeline_init` | `bool audio_pipeline_init()` | Initialize Gaussian window, flux/chroma/tempo subsystems, reset smooth state and epoch, init Goertzel backend. (`src/audio/audio_producer.cpp:116-128`) |
| `audio_pipeline_tick` | `void audio_pipeline_tick(const int32_t* q24_chunk, uint32_t t_ms)` | Process one 128-sample chunk (Q24), populate staging `AudioFrame`, publish via memcpy + epoch increment. (`src/audio/audio_producer.cpp:130-216`) |
| `acquire_spectral_frame` | `const AudioFrame* acquire_spectral_frame()` | Legacy pointer access for diagnostics; VP uses snapshot helper instead. (`src/audio/audio_producer.cpp:113`) |

### 1.2 Data Structures
- `AudioFrame` (`src/audio/audio_bus.h:14`)
  - `audio_frame_epoch` (uint32_t): increments each publish
  - `t_ms`: producer timestamp `millis()`
  - `waveform[chunk_size]` (Q15), `vu_peak`, `vu_rms`
  - `raw_spectral[64]`, `smooth_spectral[64]`, `chroma[12]`
  - `band_low`, `band_low_mid`, `band_presence`, `band_high`
  - `flux` (Q16.16, clamped 0–1.25)
  - Tempo lane outputs: `tempo_bpm`, `beat_phase`, `beat_strength`, `tempo_confidence`, `tempo_silence`, `beat_flag`, `tempo_ready`

- Internal state (`src/audio/audio_producer.cpp`):
  - `g_staging`, `g_public` (AudioFrame)
  - `smooth_state_q16[freq_bins]`
  - `audio_frame_epoch` (volatile uint32_t)

### 1.3 Processing Steps (per chunk)
1. Convert Q24 chunk to Q15 waveform, clamp to [-32768, 32767].
2. Levels: peak & RMS via `levels::*` helpers.
3. Goertzel spectral magnitudes → `raw_spectral`.
4. EMA smoothing using runtime alpha from `audio_params::get_smoothing_alpha_q16()`.
5. Chroma accumulation over raw bins.
6. Band sum aggregation (four segments defined by `K1Lightwave::Audio::Bands4_64`).
7. Spectral flux calculation (`flux::compute`) with clamp to 1.25.
8. Tempo lane ingest/update; set `beat_flag`, `tempo_ready` etc.
9. Publish: set `t_ms`, pre-set `audio_frame_epoch` in staging, `memcpy` to public, bump `audio_frame_epoch` global.

### 1.4 Notes
- No dynamic allocation; static arrays sized at compile time.
- Flux diagnostics gated by `AUDIO_DIAG_FLUX` and debug flags.
- Future runtime smoothing persistence via `audio_params::init()` stub.

---

## 2. Visual Pipeline (VP)

### 2.1 Entry Points
| Symbol | Signature | Description |
|--------|-----------|-------------|
| `vp::init` | `void vp::init()` | Initialize VP config (NVS) and hardware renderer. (`src/VP/vp.cpp:9-12`)
| `vp::tick` | `bool vp::tick()` | Acquire `VPFrame` snapshot and dispatch to renderer. Returns `true` when a frame renders. (`src/VP/vp.cpp:14-23`)

### 2.2 Frame Acquisition
- `VPFrame` (`src/VP/vp_consumer.h`): contains `AudioFrame af`, `uint32_t epoch`, `uint32_t t_ms`.
- `vp_consumer::acquire(VPFrame& out)` wraps `audio_frame_utils::snapshot_audio_frame` with up to 3 attempts (retry on epoch mismatch). (`src/VP/vp_consumer.cpp:5-11`)

### 2.3 Renderer APIs
| Symbol | Signature | Purpose |
|--------|-----------|---------|
| `vp_renderer::init()` | `void init()` | Add FastLED strips (CH1 GPIO9, CH2 GPIO10), set brightness, clear. |
| `vp_renderer::render(const VPFrame&)` | `void render(const VPFrame& f)` | Update envelopes, render mode (0..6), push `FastLED.show()`, emit debug logs. |
| `vp_renderer::adjust_brightness(int delta)` | `void adjust_brightness(int delta)` | Change global brightness (0–255) and call `FastLED.setBrightness`. |
| `vp_renderer::adjust_speed(float factor)` | `void adjust_speed(float factor)` | Multiply animation speed, clamp to [0.1, 5.0]. |
| `vp_renderer::next_mode()/prev_mode()` | Cycle modes (0..6). |
| `vp_renderer::status()` | `Status status()` returns `{brightness, speed, mode}` for HMI echo. |

### 2.4 Modes & Helpers (src/VP/vp_renderer.cpp)
- **Mode 0** `render_leds` (Band Segments):
  - Smooth band envelopes (`g_band_env[]`), apply beat/flux boosts, draw symmetric radial segments via `render_center_bands_strip()`.
- **Mode 1** (Center-Out Wave): `render_center_wave_strip(..., outward=true)` + beat brightness lift.
- **Mode 2** (Edge-In Wave): same helper with `outward=false`.
- **Mode 3** (Center Pulse): Gaussian pulse via `render_center_pulse_strip`.
- **Mode 4** (Bilateral Comets): Moving heads with fading tail via `render_bilateral_comets_strip`.
- **Mode 5** (Flux Sparkles): `decay_sparkles`, spawn based on flux, render with `render_flux_sparkles_strip`.
- **Mode 6** (Beat Strobe): Center strobe width proportional to beat envelope via `render_beat_strobe_strip`.

All modes:
- Honor center indices 79/80 for both strips (CH1, CH2).
- Use curated palette `kPalette` (blue→magenta) to avoid rainbow patterns.
- Call FastLED show exactly once per frame.

### 2.5 State Variables
- `g_leds[320]`: CRGB buffer (two 160-strip segments).
- `g_band_env[4]`: smoothed band sums (float).
- `g_flux_env`: smoothed flux (float).
- `g_beat_env`: beat envelope (float).
- `g_brightness` (uint8_t), `g_speed` (float), `g_mode` (uint8_t).
- Sparkle pool `g_spark[20]` for mode 5.

### 2.6 Config Persistence
- `vp_config::init()`: open NVS namespace `"vp"`, read keys `use_smoothed`, `dbg_period`. (`src/VP/vp_config.cpp:9-22`)
- `vp_config::set(...)`: write back via `storage::nvs::write_u32`. Future HMI hooks can call this.
- `vp_config::get()`: returns current `VPConfig` struct.

### 2.7 HMI Integration
- `vp::brightness_up/down`, `vp::speed_up/down`, `vp::next_mode/prev_mode` simply forward to renderer adjustments. (`src/VP/vp.cpp:24-33`)
- `vp::hmi_status()` returns `HMIStatus` for logging.
- Binding in `src/main.cpp`: keys +/–/[/]/</> modify state, echo via `[HMI]` logs.

---

## 3. Debug & Diagnostics

### 3.1 Debug Flags (`src/debug/debug_flags.h`)
- Bitmask groups: `kGroupAPInput`, `kGroupTempoEnergy`, `kGroupTempoFlux`, `kGroupDCAndDrift`, `kGroupVP`.
- Inline helpers: `enabled(bits)`, `toggle(bits)`, `set(bits,on)`, `set_mask(mask)`, `mask()`.

### 3.2 Debug UI API (`src/debug/debug_ui.h`)
| API | Description |
|-----|-------------|
| `debug_ui::init()` | Print current mask + help. |
| `debug_ui::handle_key(char)` | Toggle groups or print help; returns true if handled. |
| `debug_ui::tick()` | Periodic telemetry: timing, tempo BPM/phase, beat strength/confidence, silence. |

### 3.3 Usage in main loop
- Serial key dispatch in `src/main.cpp:103-123`. HMI keys handled inline; others go to `debug_ui::handle_key`.
- `debug_ui::tick()` called once per chunk after AP/VP ticks.
- VP renderer also checks `debug_flags::enabled(kGroupVP)` before printing `[vp]` lines.

---

## 4. Storage Module

### 4.1 Interfaces (`src/storage/NVS.h`)
| API | Description |
|-----|-------------|
| `init(ns)` | Open Preferences namespace. |
| `read_u32(key, out)` | Return false if key missing or not initialized. |
| `write_u32(key, value)` | Immediate write. |
| `write_u32_debounced(key, value, min_interval_ms, force)` | Queue or write depending on elapsed time. |
| `poll()` | Iterate pending entries and commit if interval elapsed. |

### 4.2 Usage
- VP config loads at boot and stores values through `storage::nvs`. No other modules persist data yet.
- `storage::nvs::poll()` called every loop iteration to flush pending writes.

---

## 5. Main Application (`src/main.cpp`)

### 5.1 Initialization Sequence
1. `Serial.begin(115200)`
2. `Sph0645::setup()` — mic bring-up.
3. `audio_pipeline_init()` — AP state.
4. `vp::init()` — VP config + renderer.
5. `print_debug_status()` & `print_debug_help()` (via debug UI).

### 5.2 Loop Sequence
1. Read serial buffer; handle HMI keys or debug toggles.
2. If `read_q24_chunk(q24_chunk, chunk_size)` returns true:
   - `audio_pipeline_tick(q24_chunk, millis())`
   - `vp_test_render()` (legacy debug prints using `acquire_spectral_frame`) — optional to disable.
  - `vp::tick()` (returns `true` when LEDs updated, `false` when no frame rendered).
3. `storage::nvs::poll()`.

### 5.3 Legacy Debug Hook
- `vp_test_render()` retains colorized serial prints for tempo/energy/silence to help during migration; uses `acquire_spectral_frame()` directly.
- Can be disabled once `debug_ui::tick()` encapsulates all needed telemetry.

---

## 6. Sequence Diagram (per chunk)
```
Main Loop
  │
  │ read_q24_chunk()
  │──────────▶ SPH0645
  │◀───────── bool success
  │ if true:
  │  audio_pipeline_tick(q24_chunk, millis())
  │──────────▶ Audio Producer
  │◀───────── (publishes AudioFrame, epoch++)
  │  vp::tick()
  │──────────▶ VP Consumer
  │◀───────── VPFrame
  │──────────▶ VP Renderer
  │◀───────── (FastLED.show)
  │  debug_ui::tick()
  │──────────▶ Debug UI
  │ storage::nvs::poll()
  │──────────▶ NVS module
  │
```

---

## 7. Extensibility Guidance
- Add new VP modes by extending switch-case in `vp_renderer::render` and supplying helper similar to existing ones.
- Introduce new HMI controls via `vp::` proxies and main loop key handling (ensure `[HMI]` echo).
- Persist new config by extending `VPConfig` and storing via `storage::nvs` (define new NVS keys).
- Integrate additional debug categories by extending `debug_flags` enum and update `debug_ui` handler.
