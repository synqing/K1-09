# Software Requirements Specification (SRS)

Project: Sensory Bridge Audio/Visual Firmware Upgrade  
Target MCU: ESP32-S3 (DevKitC-1, 8 MB flash, no PSRAM)  
Firmware branch: `feature/lc-transplant-plan`

---

## 1. Introduction

### 1.1 Purpose
This SRS captures the functional and non-functional requirements for the upgraded Sensory Bridge firmware that replaces the legacy LC visual pipeline with a modular Visual Pipeline (VP) while preserving the existing audio producer. It serves firmware engineers, QA, and integration partners as the authoritative requirements baseline.

### 1.2 Scope
The firmware acquires microphone input, computes a rich set of audio metrics (spectra, chroma, tempo), publishes `AudioFrame` snapshots, consumes them in the VP, renders on dual WS2812 LED strips, and exposes runtime controls via serial HMI. Non-goals include host networking, remote configuration, or UI beyond serial.

### 1.3 Definitions, Acronyms, Abbreviations
- **AP**: Audio Producer (`src/audio/`*)
- **VP**: Visual Pipeline (`src/VP/`)
- **HMI**: Human Machine Interface via serial console
- **AudioFrame**: Shared POD struct defined in `src/audio/audio_bus.h`
- **VPFrame**: Wrapper around `AudioFrame` used internally by VP (`src/VP/vp_consumer.h`)
- **Q16.16**: Signed fixed-point (int32_t with 16 fractional bits)
- **Chunk**: 128 audio samples at 16 kHz (8 ms window)

### 1.4 References
- `docs/firmware/pipelines.md`
- `docs/firmware/vp_pipeline.md`
- `docs/firmware/api_endpoints.md`
- `platformio.ini`
- Source code paths referenced throughout

---

## 2. Overall Description

### 2.1 Product Perspective
- Embedded firmware running bare-metal (Arduino framework) on ESP32-S3.
- Interfaces with:
  - I2S MEMS microphone (SPH0645) via `K1Lightwave::Audio::Sph0645` (`src/audio/sph0645.cpp`)
  - Dual WS2812 LED strips (160 pixels each) driven through FastLED (`src/VP/vp_renderer.cpp`)
  - Serial console for telemetry/debug/HMI (`src/main.cpp`)
  - Non-volatile storage (NVS Preferences API) for VP configuration (`src/storage/NVS.cpp`, `src/VP/vp_config.cpp`)

### 2.2 Product Functions
- Acquire microphone data, compute waveform, levels, Goertzel spectra, chroma, band sums, spectral flux, tempo features (`src/audio/audio_producer.cpp`).
- Publish thread-safe `AudioFrame` snapshots once per chunk using double-buffer + epoch.
- Consume `AudioFrame` in VP, maintain per-mode envelopes, render patterns on both strips with center-origin rules (`src/VP/vp_renderer.cpp`).
- Expose runtime controls (brightness, speed, mode) via serial (+/-/[/]/</>), reflect status (`src/main.cpp`).
- Provide debug toggles (1/2/3/4/0) and periodic telemetry (`src/debug/debug_ui.cpp`).
- Persist VP config keys (`use_smoothed_spectrum`, `debug_period_ms`) in NVS (`src/VP/vp_config.cpp`).

### 2.3 User Classes & Characteristics
- **Firmware Developers**: maintain and extend AP/VP modules; require accurate docs and source references.
- **Test/QA Engineers**: run hardware validation (e.g., `docs/firmware/hardware_validation_checklist.md`).
- **Integrators/Artists**: adjust visual modes and runtime controls over serial.

### 2.4 Operating Environment
- ESP32-S3 single core (240 MHz) for audio + VP processing (`platformio.ini`, build flags disable PM). 
- 4 MB flash partition scheme (`partitions_4mb.csv`).
- 128-sample audio chunks at 16 kHz (approx. 125 Hz update rate).
- LED output via FastLED 3.9.2 configured for GPIO9/GPIO10 data lines.

### 2.5 Design & Implementation Constraints
- No dynamic allocation inside hot paths (AP and VP).
- Lock-free data sharing via epoch-stable snapshots (`audio_frame_utils::snapshot_audio_frame`).
- Center-origin LED geometry (indices 79/80) across all modes (requirement derived from hardware layout, enforced in `src/VP/vp_renderer.cpp`).
- Linear domain (Q16.16) for all published metrics; conversions handled at consumption sites.
- FastLED brightness limited to 0–255; default 140 to avoid overcurrent (`src/VP/vp_renderer.cpp:18`).

### 2.6 Assumptions & Dependencies
- High-quality 3.3V power capable of driving both LED strips.
- Serial console available for configuration/testing.
- FastLED library compatible with ESP32-S3 (verified in PlatformIO env).
- Future integration with PixelSpork possible but not required in this release.

---

## 3. Specific Requirements

### 3.1 Functional Requirements
| ID | Requirement | Source / Trace | Status |
|----|-------------|----------------|--------|
| FR-AP-001 | The system shall sample 128 audio frames at 16 kHz per chunk and compute waveform, levels, spectral magnitudes, chroma, band sums, flux, and tempo metrics. | `src/audio/audio_producer.cpp:130-209`; `docs/firmware/pipelines.md` | Implemented |
| FR-AP-002 | The system shall publish one `AudioFrame` snapshot per chunk via a single memcpy and increment `audio_frame_epoch`. | `src/audio/audio_producer.cpp:211-216` | Implemented |
| FR-VP-001 | The VP shall acquire `AudioFrame` snapshots using epoch-stable copying to avoid torn frames. | `src/VP/vp_consumer.cpp:5-11`; `src/audio/audio_bus.h:92` | Implemented |
| FR-VP-002 | The VP shall render both LED strips identically with strict center-origin symmetry. | `src/VP/vp_renderer.cpp:60-172,205-236` | Implemented |
| FR-VP-003 | The VP shall support at least 7 lightshow modes (band segments + 6 animations) and allow cycling via HMI. | `src/VP/vp_renderer.cpp:188-236`; `src/VP/vp.cpp:27-36` | Implemented |
| FR-HMI-001 | The firmware shall expose brightness, speed, and mode controls over serial using +/-/[/]/</> keys and confirm changes via `[HMI]` logs. | `src/main.cpp:103-123` | Implemented |
| FR-DBG-001 | Operators shall be able to toggle debug telemetry groups using keys 1,2,3,4,0 and request help via `?`. | `src/debug/debug_ui.cpp:24-63` | Implemented |
| FR-CONF-001 | VP configuration (`use_smoothed_spectrum`, `debug_period_ms`) shall persist using NVS namespace `"vp"`. | `src/VP/vp_config.cpp:9-28` | Implemented |

### 3.2 External Interface Requirements
- **Microphone Interface**: I2S read via `Sph0645::read_q24_chunk()` (blocking, returns N 32-bit words). Errors handled by returning `false`. (`src/audio/sph0645.cpp`).
- **LED Interface**: FastLED strips added during `vp_renderer::init()`. Color data derived from VP modes and sent via `FastLED.show()` once per frame. (`src/VP/vp_renderer.cpp:97-105`).
- **Serial Interface**: Bidirectional console at 230400 baud (`platformio.ini`). Receives HMI/debug commands and prints telemetry. (`src/main.cpp`, `src/debug/debug_ui.cpp`).
- **Storage Interface**: Arduino Preferences (NVS) with debounced writes (`src/storage/NVS.cpp`).

### 3.3 Performance Requirements
- AP processing must remain below 8 ms per chunk (observed ~1.1 ms; overrun monitoring available when `AUDIO_PROFILE` enabled). (`src/audio/audio_producer.cpp` conditional). 
- VP rendering must maintain <= 1 ms typical per frame (enforced by design: no heap, small loops). Future profiling hook recommended (`docs/firmware/vp_pipeline.md`).
- Serial debug prints throttled via `debug_period_ms` to avoid overruns (`src/VP/vp_renderer.cpp:220-236`).

### 3.4 Reliability & Fault Handling
- Lock-free producer/consumer handshake prevents deadlocks or stalls even under timing jitter. 
- VP gracefully returns if acquisition fails (`src/VP/vp.cpp:15-18`).
- HMI input is stateless; invalid keys logged but ignored (`src/debug/debug_ui.cpp`).
- Flux diagnostics gated by debug flag to avoid unnecessary prints (`src/audio/audio_producer.cpp:34-99`).

### 3.5 Maintainability & Extensibility
- Modules partitioned: AP in `src/audio/`, VP in `src/VP/`, Debug in `src/debug/`, storage in `src/storage/`.
- Configurable smoothing alpha via `audio_params` with stubbed persistence for future runtime tweaks (`src/audio/audio_params.cpp`).
- VP modes use helper functions per pattern for easy extension.
- `.gitignore` ensures documentation and code separation (docs tracked despite global ignore).

### 3.6 Traceability Matrix (excerpt)
| Requirement | Module(s) | Test/Validation |
|-------------|-----------|-----------------|
| FR-AP-001 | `audio_producer.cpp`, `flux.cpp`, `tempo_lane.cpp` | Audio frame telemetry via debug flag 3 and `vp` debug prints |
| FR-VP-002 | `vp_renderer.cpp` (render_center_* helpers) | Visual inspection + HV-LED-01 checklist |
| FR-HMI-001 | `main.cpp`, `vp.cpp`, `vp_renderer.cpp` | Serial console scripting |

---

## 4. Appendices
- **A. Related Documents**: Legacy LC pipeline (`docs/lc_visual_pipeline_migration.md`) for background.
- **B. Future Enhancements**: PixelSpork integration, VP profiling macro, runtime config persistence (see `docs/firmware/vp_pipeline.md` “Next Steps”).

