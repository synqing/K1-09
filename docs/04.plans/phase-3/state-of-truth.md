# Phase 3 State of Truth

## Execution Model Divergence
- `src/main.cpp:215` keeps the LED task pinned to core 1 while the control/audio loop remains on core 0 (`src/main.cpp:226`), so shared per-frame state such as `g_coupling_plan` can change mid-frame when consumed on the LED task (`src/main.cpp:624`, `src/main.cpp:709`). This breaks the single-core deterministic A–E loop promised in the plan and introduces unsynchronized access across cores.

## Runtime Feature Gates Disabled
- Newly introduced macros in `src/constants.h:38-47` default to zero, which removes knobs/buttons (`src/main.cpp:302-309`) and P2P sync (`src/main.cpp:327-331`) from the live control phase. The operating plan expects controls and P2P to be serviced every frame; shipping with these gates off violates that assumption.

## Dual-Strip Architecture Incomplete
- `coordinator_update` still mirrors the current primary and secondary modes (`src/dual_coordinator.cpp:39-82`) and never acts on `variation_type` or complementary pairs. The secondary strip is rendered by mutating global `CONFIG` state inside the LED thread and rerunning the primary light modes (`src/main.cpp:660-720`), rather than using per-channel ModeRunners with pure operators as specified.

## Asynchronous Flip Safety Missing
- `show_leds()` re-arms controllers and flips front/back indices after every `FastLED.show()` (`src/led_utilities.h:1170-1187`) without checking controller busy status. The operating plan requires flips only when controllers report not-busy to prevent write-while-busy violations.

## Non-Uniform QoS Handling
- Primary brightness scaling is now linear in `CONFIG.PHOTONS` (`src/led_utilities.h:424-456`), but the secondary pipeline still squares `SECONDARY_PHOTONS` (`src/led_utilities.h:2060-2077`). Any automatic trim will therefore hit each strip differently, conflicting with the “uniform QoS” mandate.

## Instrumentation Gaps
- Metrics only cover average timings for phases A/B and C/D (`src/main.cpp:460-475`, `src/main.cpp:751-770`). Required data such as LED overlap %, watchdog feed summaries, flip-violation reporting (despite `g_flip_violations` in `src/led_utilities.h:136-144`), and router decision logs remain absent, so several acceptance checks cannot be validated.

## Audio Ring Buffer Counters Partially Used
- The bounded-read loop updates `g_rb_reads`/`g_rb_deadline_miss` (`src/i2s_audio.h:78-103`), but `g_rb_partial_bytes` introduced in globals (`src/core/globals.cpp:372`) is never updated. The metric advertised in the plan for partial bytes is therefore non-functional.
