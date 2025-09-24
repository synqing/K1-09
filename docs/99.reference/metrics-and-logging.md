# Metrics & Logging Reference

This reference defines the canonical metrics emitted by the firmware and how to parse them in tools/CI.

## Consolidated METRICS line (preferred)
- Format (single line, printed ~every 4s):
  - `METRICS fps=<SYSTEM_FPS> eff_fps=<LED_FPS> a_ms=<Aavg_ms> b_ms=<Bavg_ms> c_ms=<Cavg_ms> d_ms=<Davg_ms> e_ms=<Eavg_ms> overlap=<D/(C+D) percent> rb_miss=<deadline_miss> rb_over=<0> rb_dead=<0> wdt_feed=<B+C+D feeds> flip_viol=<count>`
- Example:
  - `METRICS fps=128.5 eff_fps=122.3 a_ms=0.40 b_ms=2.10 c_ms=3.85 d_ms=2.35 e_ms=0.00 overlap=37.9% rb_miss=0 rb_over=0 rb_dead=0 wdt_feed=512 flip_viol=0`
- Fields:
  - `fps` — System FPS (frames computed per second)
  - `eff_fps` — LED FPS (successful flips per second)
  - `a_ms..e_ms` — Average Phase A–E durations in milliseconds
  - `overlap` — Percentage of time spent on wire vs (C+D)
  - `rb_miss` — I²S soft-deadline misses (partial chunks skipped)
  - `rb_over`/`rb_dead` — Reserved for ringbuffer overruns/underruns
  - `wdt_feed` — Total watchdog feeds within the window (B+C+D)
  - `flip_viol` — Write-while-busy violations detected by LED guards

Notes
- Source: see `src/main.cpp` consolidated emission in the render/output loop and `src/led_utilities.h` flip guards.
- Consumers should treat fields as optional; add forward-compatible parsing.

## Legacy/auxiliary lines
In some builds the main loop also emits auxiliary lines (helpful during bring-up):
- `METRICS A(us)=<avg_us> B(us)=<avg_us> rb=<reads>/<deadline_miss>`
- `FPS sys=<SYSTEM_FPS> led=<LED_FPS>`

If both formats are present, prefer the consolidated `METRICS` line for automation.

## Ringbuffer counters
- `g_rb_reads` increments on fully accumulated chunks.
- `g_rb_deadline_miss` increments when the bounded I²S loop times out.
- `g_rb_partial_bytes` tracks carried-over bytes (optional).

## Flip safety
- `g_flip_violations` counts write-while-busy incidents, should remain 0 under normal conditions.
