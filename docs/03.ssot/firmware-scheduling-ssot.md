# Scheduling & Timing (SSOT)

_Authoritative reference for how Sensory Bridge firmware schedules audio, visual, and output work on the ESP32-S3._

## 1. Targets & Reality Check
- **Performance target:** 120 FPS effective visual output (8 ms budget) as defined in `docs/phase3_operating_plan.md`.
- **Current implementation:**
  - `primary_loop_task` on Core 0 executes Phases A–E (`src/main.cpp:230-620`).
  - LED rendering/output now happens inside Phase C/D of the same task; no separate LED thread is created.
  - `trace_consumer_task` continues to run cooperatively with low priority.
- **Watchdog:** Task WDT configured for 2 s (`src/main.cpp:157-164`); core loops feed WDT after Phases B, C, and D.

## 2. Phase Timeline (Core 0)

| Phase | Entry point | Responsibilities | Notes |
|-------|-------------|------------------|-------|
| A – Controls | `main_loop_core0()` `#if ENABLE_INPUTS_RUNTIME` (`src/main.cpp:296-327`) | Sampling encoders/buttons, serial commands, settings sync, optional P2P | Run-time gates (`ENABLE_INPUTS_RUNTIME`, `ENABLE_P2P_RUNTIME`) default to 0 in `src/constants.h:38-47`; enable to restore full Phase A workload. |
| B – Audio | `acquire_sample_chunk()` / `process_GDFT()` (`src/main.cpp:329-353`) | I²S bounded read, sweet spot update, VU, Goertzel, novelty | Bounded loop with 2.5 ms soft deadline (`src/i2s_audio.h:78-101`); increments `g_rb_reads`/`g_rb_deadline_miss`. |
| C – Coordination/Render | `coordinator_update()` + render staging (`src/main.cpp:360-520`) | Produce `g_coupling_plan`, cache frame config, render primary + secondary buffers | Secondary render uses per-frame snapshot (`frame_config`) without mutating `CONFIG`. |
| D – Publish Bookkeeping | `publish_frame()` + `show_leds()` (`src/main.cpp:520-560`) | Flip writer sequence, async LED output, metrics accumulation | METRICS output now emitted from the same task with C/D timing, overlap %, WDT totals. |
| E – Upkeep | `do_config_save()` + yield (`src/main.cpp:486-517`) | Deferred saves, WDT feed, `taskYIELD()` | Maintains responsiveness for other FreeRTOS tasks. |

## 3. Phase C/D Breakdown (Core 0)

| Stage | Function | Details |
|-------|----------|---------|
| Begin frame | `begin_frame()` (`src/frame_sync.h:10-14`) | Clears primary working buffer, increments writer seq. |
| Primary render | `light_mode_*` via switchboard (`src/main.cpp:360-430`) | Uses `frame_config` snapshot; consumes Coupling Plan metadata. |
| Secondary render | Per-frame snapshot (`src/main.cpp:430-520`) | Renders using `frame_config` overrides (`coordinator_is_secondary`); no `CONFIG` mutation. |
| Quantise & show | `show_leds()` (`src/main.cpp:520-560`, `src/led_utilities.h:928-1204`) | Applies brightness, scaling, quantisation, alignment checks, async `FastLED.show()` with front/back flip. |
| Metrics | Consolidated METRICS line (`src/main.cpp:770-840`) | Emits A/B/C/D/E timings, LED overlap %, WDT feeds, flip violations. |

## 4. Instrumentation & Metrics
- **Performance Monitor:** `ENABLE_PERFORMANCE_MONITORING` collects per-phase timings (`src/debug/performance_monitor.cpp`).
- **METRICS logs:** consolidated line emitted every ~4 s from Phase E (`src/main.cpp:770-840`), reporting A/B/C/D/E ms, LED FPS, overlap %, watchdog feeds, ring-buffer misses, flip violations.
- **Overlap & flip safety:** `show_leds()` uses controller alignment guards (`src/led_utilities.h:142-176`) and increments `g_flip_violations` (`src/led_utilities.h:136-144`).
- **Ring buffer counters:** `g_rb_reads` / `g_rb_deadline_miss` defined in `src/core/globals.cpp:371-374` and surfaced in METRICS output.

## 5. Guardrails
- Keep Phases A–D non-blocking: bounded waits only inside audio acquisition, async LED flip waits respect `last_wire_us`.
- When re-enabling inputs/P2P macros, revisit Phase A timing budget (currently effectively zero when disabled).
- Any new tasks must yield quickly and remain low priority to avoid starving the main loop.

## 6. Quick Reference
- Target FPS budget: **8 ms** per frame (120 FPS).
- Watchdog feeds: Phase B (twice), C, D.
- Async double buffer: `g_led_front_idx`/`g_led_back_idx` (`src/core/globals.cpp:377-384`).
- Shared buffers: `leds_16`, `leds_16_secondary`, `g_leds_out_*` defined in `src/core/globals.cpp:137-160,308-313`.

