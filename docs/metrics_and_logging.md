# Metrics & Logging (Phase 3)

Summary Line (once/second):
```
METRICS fps=XXX.X eff_fps=XXX.X a_ms=0.2 b_ms=2.7 c_ms=5.3 d_ms=0.2 e_ms=0.4 overlap=62% rb_miss=0 rb_over=0 rb_dead=0 wdt_feed=3 flip_viol=0
```

Fields
- fps: loop FPS (Phases A–C). eff_fps: successful flips per second.
- a_ms..e_ms: avg or p95 phase times over last window.
- overlap: % of time wire transmission overlaps compute.
- rb_miss/over/dead: I²S ring stats (misses/overruns/deadline misses).
- wdt_feed: watchdog feeds count per sec window (expect 3).
- flip_viol: write‑while‑busy violations (expect 0 always).

Router Logs (throttled):
```
ROUTER pair=Waveform|Bloom reason=onset dwell=5/8 var=anti-phase,hue+0.06,offset=2
```

Guidelines
- All high‑rate prints are gated; never spam the serial port in hot paths.
- Metrics are for operators; keep format stable and machine‑parsable.

