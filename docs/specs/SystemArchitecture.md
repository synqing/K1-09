# System Architecture Specification

## 1. Overview
The firmware implements a layered audio-to-visual pipeline on ESP32-S3 hardware. The architecture is organized into:
- **Layer 1 (L1)**: Microphone capture (SPH0645) over I2S.
- **Layer 2 (L2)**: Audio Producer (AP) computing `AudioFrame` snapshots.
- **Layer 3 (L3)**: Visual Pipeline (VP) consuming frames and rendering WS2812 LED strips.
- **Support Modules**: Debug UI, storage (NVS), fast serial HMI, and configuration.

All components execute on Core 0 under the Arduino framework. The main loop owns orchestration with deterministic ordering: mic read → audio tick → VP tick → background services.

## 2. Context Diagram
```
+------------------------------+
|        Serial Console        |
| (HMI commands, debug output) |
+-------------+----------------+
              |
              v
+-------------+----------------+
|            ESP32             |
|  - Main loop                 |
|  - Layer 1/2 Audio           |
|  - Visual Pipeline           |
|  - Debug UI & Storage        |
+------+------+----------------+
       |      |
       |      v
       |   +--+-----------------+
       |   |  Dual WS2812 Strips |
       |   |  (CH1 GPIO9, CH2 GPIO10)
       |   +---------------------+
       v
+------+------------------------+
|    SPH0645 MEMS Microphone    |
|    (I2S, 16 kHz, 128 sample)  |
+-------------------------------+
```

## 3. Logical Architecture

### 3.1 Modules and Responsibilities
| Module | Responsibilities | Key Files |
|--------|------------------|-----------|
| L1 Mic Interface | Configure SPH0645 I2S, read Q24 chunks, maintain telemetry | `src/AP/sph0645.cpp`, `src/AP/sph0645.h` |
| Audio Producer (AP) | Convert chunks into waveform, levels, spectra, chroma, flux, tempo; publish `AudioFrame` | `src/audio/audio_producer.cpp`, `src/audio/audio_bus.h` |
| Visual Pipeline (VP) | Snapshot `AudioFrame`, maintain envelopes, run render modes, push to WS2812 | `src/VP/vp.cpp`, `src/VP/vp_consumer.cpp`, `src/VP/vp_renderer.cpp` |
| VP Config | Load/persist VP-specific settings from NVS | `src/VP/vp_config.cpp` |
| VP HMI | Keyboard proxies for brightness/speed/mode + status | `src/VP/vp.cpp`, `src/main.cpp` |
| Debug UI | Serial debug toggles, periodic telemetry | `src/debug/debug_ui.cpp`, `src/debug/debug_flags.cpp` |
| Storage | Debounced NVS wrapper | `src/storage/NVS.cpp` |

### 3.2 Data Flow
```
[Mic SPH0645] --Q24 chunk--> [audio_pipeline_tick]
    -> waveform_q15
    -> levels (peak, rms)
    -> raw_spectral[64]
    -> smooth_spectral[64]
    -> chroma[12], band sums, flux
    -> tempo lane outputs
    => publish AudioFrame (memcpy + epoch++)

[vp_consumer::acquire] --snapshot--> VPFrame (AudioFrame + epoch + t_ms)
[vp_renderer::render]
    - update envelopes (band, flux, beat)
    - mode dispatcher (0..6)
    - center-origin LED mapping
    - FastLED.show()
```

### 3.3 Control Flow
1. `setup()` (main): initialize serial → L1 mic → `audio_pipeline_init()` → `vp::init()` → `debug_ui::init()`.
2. `loop()` (main):
   - process incoming serial char (HMI keys handled inline, debug keys forwarded to `debug_ui::handle_key`).
   - if `Sph0645::read_q24_chunk()` succeeds:
     1. call `audio_pipeline_tick(q24_chunk, millis())`.
     2. call `vp::tick()` (internally snapshots and renders).
   - call `storage::nvs::poll()` for debounced writes.
3. Debug UI timer prints (timing, tempo, energy, silence) based on toggles.

### 3.4 Timing & Performance
- Audio chunk every 8 ms (~125 Hz). Producer completes in ~1.1 ms typical; overrun detection available (`AUDIO_PROFILE`).
- VP tick occurs immediately after AP; renderer is allocation-free and operates within remaining frame budget.
- Serial HMI processing uses non-blocking polling before each chunk.

### 3.5 Deployment Architecture
- Single firmware binary deployed via PlatformIO environment `esp32-s3-devkitc-1` with Arduino framework `3.20009.0`.
- Build flags disable power-management DFS to ensure constant CPU frequency (see `platformio.ini`).
- Flash partitions defined in `partitions_4mb.csv` (app, OTA, NVS).

## 4. Data Structures
- `AudioFrame` (src/audio/audio_bus.h): shared POD; includes `audio_frame_epoch` for synchronization.
- `VPFrame` (src/VP/vp_consumer.h): contains `AudioFrame` copy, epoch, and `t_ms`.
- VP state: brightness, speed, mode, band envelopes, flux/beat envelopes, sparkle pool (`src/VP/vp_renderer.cpp`).

## 5. Error Handling & Logging
- Mic read failure (returns false) simply skips AP/VP ticks.
- VP acquisition failures return early without affecting LEDs.
- Debug logs gated by flag groups to prevent overwhelming serial bandwidth.
- Flux diagnostics optional (only when debug group 3 enabled).

## 6. Security & Safety Considerations
- Firmware runs offline; no network stack.
- Serial console unrestricted; recommended to use trusted environment.
- Brightness capped to prevent overcurrent. Center-origin rule enforced to avoid asymmetric lightshows.

## 7. Future Architecture Hooks
- Runtime-adjustable smoothing (via `audio_params::init()` stub).
- VP profiling macro (planned) to monitor `vp::tick()` cost.
- PixelSpork interoperability (legacy code references preserved under `archive/legacy_src/`).
- Additional HMI control surfaces (encoders, network) can reuse the `vp::` proxy functions.

