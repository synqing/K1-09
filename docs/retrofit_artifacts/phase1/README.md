Phase 1 — HSV Linearization (Guarded)
- ENABLE_NEW_COLOR_PIPELINE=1 runtime test succeeded per operator log.
- hsv() outputs gamma-aware linear CRGB16 under the guard.
- Next: capture HSV vs palette parity evidence and perf delta.
