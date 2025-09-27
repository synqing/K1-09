# Firmware API & Endpoint Guide

Authoritative reference for public firmware APIs: what they do, how to call them, and where they live. Covers Audio Producer (AP), Visual Pipeline (VP), Debug UI, Storage (NVS), plus wiring and guardrails.

## Quick Start
- Setup order: `Sph0645::setup()` → `audio_pipeline_init()` → `vp::init()` → `debug_ui::init()` (src/main.cpp:94)
- Loop order: route HMI keys → `read_q24_chunk()` → `audio_pipeline_tick()` → `debug_ui::tick()` → `vp::tick()` (src/main.cpp:105,109)

## Audio Producer (AP)
- Purpose: capture mic chunks, compute spectral features, publish `AudioFrame` snapshots.
- Init & Tick
  - `bool audio_pipeline_init()` — init windows/backends. src/AP/audio_producer.h:9
  - `void audio_pipeline_tick(const int32_t* q24_chunk, uint32_t t_ms)` — process one chunk, publish frame. src/AP/audio_producer.h:13
- Layer‑1 Mic
  - `void K1Lightwave::Audio::Sph0645::setup()` — I2S/mic init + telemetry. src/AP/sph0645.h:8
  - `bool K1Lightwave::Audio::Sph0645::read_q24_chunk(int* out_q24, unsigned int n)` — blocking read of N Q24 samples. src/AP/sph0645.h:19
- Frame Contract
  - `struct AudioFrame` — waveform, levels, 64‑bin spectra (raw/smoothed), 12‑bin chroma, band sums, flux, tempo lane. src/AP/audio_bus.h:14
  - `const AudioFrame* acquire_spectral_frame()` — stable pointer to latest frame. src/AP/audio_bus.h:55
  - `bool audio_frame_utils::snapshot_audio_frame(AudioFrame& out, uint8_t max_attempts=3)` — epoch‑stable copy (no tearing). src/AP/audio_bus.h:92
- Constraints
  - Single core; one memcpy publish/tick; fixed‑size buffers; Q16.16 linear domain. Consumers must copy via snapshot.

## Visual Pipeline (VP)
- Purpose: safely consume `AudioFrame` and render LED visuals within budget.
- Public API (src/VP/vp.h)
  - `void vp::init()` — init config and renderer. src/VP/vp.h:9
  - `void vp::tick()` — snapshot and render one frame. src/VP/vp.h:10
- HMI Proxies
  - `vp::brightness_up()/down()` — adjust brightness. src/VP/vp.h:13
  - `vp::speed_up()/down()` — scale animation speed. src/VP/vp.h:15
  - `vp::next_mode()/prev_mode()` — cycle modes (0–6). src/VP/vp.h:17
  - `vp::hmi_status()` — `{brightness,speed,mode}`. src/VP/vp.h:20
- Renderer (internal)
  - Two identical 160‑LED WS2812 strips; CH1=GPIO9, CH2=GPIO10; center origin at indices 79/80. src/VP/vp_renderer.cpp:18,97
  - Modes: 0 band segments; 1 center‑out wave; 2 edge‑in wave; 3 center pulse; 4 bilateral comets; 5 flux sparkles; 6 beat strobe. src/VP/vp_renderer.cpp:188
  - No heap in tick; symmetric draw on both strips every frame.
- Constraints
  - Keep <1 ms typical render time; enforce center‑origin mapping; no rainbow palettes.

## Debug UI
- Purpose: centralize debug toggles and telemetry.
- API (src/debug/debug_ui.h)
  - `void debug_ui::init()` — initial status + help. src/debug/debug_ui.h:6
  - `bool debug_ui::handle_key(char c)` — handle 1/2/3/4/0/? keys; returns true if handled. src/debug/debug_ui.h:11
  - `void debug_ui::tick()` — periodic telemetry (timing/tempo/energy/AP input). src/debug/debug_ui.h:14
- Debug Flags (src/debug/debug_flags.h)
  - Groups: `kGroupAPInput`, `kGroupTempoEnergy`, `kGroupTempoFlux`, `kGroupDCAndDrift`, `kGroupVP`. src/debug/debug_flags.h:7
  - Utilities: enable/disable/toggle, global mask. src/debug/debug_flags.h:21

## Storage (NVS)
- Purpose: persist simple config values with debounced writes.
- API (src/storage/NVS.h)
  - `bool init(const char* ns="k1_audio")` — open NVS namespace. src/storage/NVS.h:9
  - `bool read_u32(const char* key, uint32_t& out)` — read u32 if present. src/storage/NVS.h:11
  - `bool write_u32(const char* key, uint32_t value)` — immediate write. src/storage/NVS.h:13
  - `void write_u32_debounced(const char* key, uint32_t value, uint32_t min_interval_ms=5000, bool force=false)` — debounced write. src/storage/NVS.h:15
  - `void poll()` — service pending writes. src/storage/NVS.h:19
- VP Config Keys (ns `vp`)
  - `use_smoothed` (0/1), `dbg_period` (ms). src/VP/vp_config.cpp:9

## HMI Keyboard Map (Serial)
- `+`/`-` brightness
- `]`/`[` speed
- `>`/`<` mode (wraps 0..6)
- `1/2/3/4/0/?` debug groups/help
- Notes: HMI prints “[HMI] …” confirmations; debug prints gated by groups.

## Contracts & Guardrails
- AP → VP contract: `AudioFrame` is linear Q16.16; consumers snapshot with epoch check.
- Timing: AP ~1.1 ms of an 8 ms budget; VP must stay bounded and allocation‑free.
- LEDs: two strips are always active; center‑origin symmetry (79/80) is mandatory.
- Color policy: avoid rainbow palettes; use curated palettes.

## Future Extensions
- VP profiling API (`VP_PROFILE`) for per‑mode timing.
- NVS persistence for brightness/speed/mode under ns `vp`.
- PixelSpork bridge endpoints if adopted alongside FastLED.
