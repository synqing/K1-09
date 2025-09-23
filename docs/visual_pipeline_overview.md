# Visual Pipeline & Architecture Overview (Single-Core Edition)

This document captures the single-core runtime architecture of the Sensory Bridge visualiser, illustrating how audio data moves through the system, where configuration inputs influence the visuals, and how scheduling changes under the unified-core mandate.

## 1. Runtime Scheduling Topology

```
+------------------------------------------------------+
| Core 0 (Unified Audio + Visual Processing)           |
|------------------------------------------------------|
| primary_loop_task (high priority)                    |
|   Phase A: Input/controls (buttons, encoders, serial)|
|   Phase B: Audio capture & analysis                  |
|   Phase C: Visual rendering (primary + secondary)    |
|   Phase D: Frame publish & LED output                |
|   Phase E: Housekeeping (save queues, diagnostics)   |
|                                                      |
| trace_consumer_task (low priority, cooperative)      |
|   • drains trace buffers & debug output              |
+------------------------------------------------------+
         |                         |
         | shared state structs    | FastLED driver + peripherals
         v                         v
 CONFIG / Audio state         LED strip outputs
```

All time-critical work now executes on a single core inside `primary_loop_task`. Auxiliary tasks (e.g., trace consumer) run cooperatively and must yield quickly so the main loop retains deterministic cadence.

## 2. Phase Timeline per Frame

```
┌──────────────────────────────────────────────────────────────────────────┐
│ primary_loop_task                                                        │
├──────────────────────────────────────────────────────────────────────────┤
│ Phase A │ Phase B │ Phase C │ Phase D │ Phase E │ idle/yield │ Phase A…   │
│ control│ audio    │ render  │ publish │ upkeep  │ (if time)   │ next frame │
└──────────────────────────────────────────────────────────────────────────┘

Phase ordering is fixed, guaranteeing that freshly captured audio immediately feeds the next visual frame without cross-core synchronisation.
```

## 3. Audio Acquisition & Analysis Flow

```
[I2S Mic]
    |
    v
+------------------------------+
| acquire_sample_chunk()       |
|  - i2s_read -> AudioRawState |
|  - waveform[] / peaks        |
+------------------------------+
    |
    v
+------------------------------+
| calculate_vu()               |
|  - RMS & noise floor         |
|  - audio_vu_level            |
+------------------------------+
    |
    v
+------------------------------+
| process_GDFT()               |
|  - Goertzel bins (64)        |
|  - noise subtraction         |
|  - magnitudes_final[]        |
+------------------------------+
    |
    v
+------------------------------+
| calculate_novelty() &        |
| process_color_shift()        |
|  - novelty_curve[]           |
|  - hue_position, shifts      |
+------------------------------+
```

The outputs (spectrogram, chromagram, novelty, VU, hue state) are ready before Phase C begins, so the renderer consumes consistent data each iteration.

## 4. LED Frame Pipeline (executed sequentially in Phase C/D)

```
begin_frame()
    |
    v
cache_frame_config()  --> snapshot of CONFIG & palette LUT pointers
    |
    v
+------------------------------+
| Lightshow mode switchboard   |
|  • light_mode_gdft           |
|  • ...                       |
|  • light_mode_waveform       |
+------------------------------+
    |
    v
[Optional secondary channel pass]
    |
    v
apply_prism_effect() / render_bulb_cover()
    |
    v
render_ui() overlays
    |
    v
apply_brightness() & incandescent/base coat gates
    |
    v
scale_to_strip()  ->  leds_scaled[]
    |
    v
quantize_color()  ->  leds_out[]
    |
    v
FastLED.show()  (Phase D)
```

Since audio and rendering share the same core, no cross-core fencing or mailbox hand-offs are required—`cache_frame_config()` simply snapshots globals immediately after audio analysis.

## 5. Configuration & Control Surfaces

```
               +--------------------+
               | Dual Encoders (HMI)|
               |  - brightness/HSI  |
               |  - mode select     |
               +---------+----------+
                         |
         +---------------+-------------------+
         |                                   |
+--------v-------+                   +-------v-------+
| Serial Menu    |                   | Future Web UI |
|  - parameter   |                   |  - REST / UI   |
|    commands    |                   |  - live tweaks |
+--------+-------+                   +-------+--------+
         |                                   |
         +---------------+-------------------+
                         |
                   save_config_delayed()
                         |
          +--------------v--------------+
          | CONFIG / frame_config /     |
          | secondary settings          |
          +--------------+--------------+
                         |
                         v
            Phase A (control read) --> Phase C (render use)
```

Inputs are polled in Phase A, applied to `CONFIG`, and then frozen for the current frame via `cache_frame_config()`. Any persistence work queued by `save_config_delayed()` is serviced during Phase E to avoid jitter within the audio/render path.

## 6. Key Data Stores

```
AudioRawState  : raw I2S samples + waveform history (owned by Phase B)
AudioProcessed : waveform[], peaks, VU state (read by Phase C)
CONFIG         : user-facing parameters (mutated Phase A/E, persisted)
frame_config   : per-frame snapshot consumed by renderer
spectrogram_*  : NUM_FREQS arrays holding latest magnitudes
novelty_curve  : temporal change history for auto-color shift
led buffers    : leds_16 -> leds_scaled -> leds_out (Phase C workflow)
```

Single-core execution ensures each store has a clear owner per phase, eliminating the previous need for cross-core mutexes or frame sequence counters.

---

Keep this layout in mind when adjusting task priorities, adding new UI surfaces, or expanding lightshow parameters: every addition must respect the Phase A–E cadence so audio capture and visual output remain tightly coupled on the single processing core.
