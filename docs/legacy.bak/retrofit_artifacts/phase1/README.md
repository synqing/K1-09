# Phase 1 – Gamma‑Aware HSV (Flagged)

> Owner: Colour retrofit strike team  
> Build flag: `ENABLE_NEW_COLOR_PIPELINE=1` (compile‑time)

## Artefacts to Collect

| Item | Description | Drop Here | Status |
|------|-------------|-----------|--------|
| HSV vs Palette parity | Side‑by‑side video/LED dumps comparing HSV (index 0) vs calibrated palette | `docs/08.artifacts/retrofit/phase-1/parity/` | ☐ |
| LED buffer dumps | CSV/JSON dumps for representative frames (before/after flag) | `docs/08.artifacts/retrofit/phase-1/dumps/` | ☐ |
| Brightness metrics | Post‑`apply_master_brightness()` min/max logs with flag ON | `docs/08.artifacts/retrofit/phase-1/brightness_metrics.log` | ☐ |
| Size budget output | Output of `python3 scripts/verify_size_budget.py --baseline ci/size-baseline.json` | `docs/08.artifacts/retrofit/phase-1/size_check.txt` | ☐ |
| Notes & toggles | Short README of toggles/serial cmds used during capture | `docs/08.artifacts/retrofit/phase-1/SESSION_NOTES.md` | ☐ |

## Suggested Procedure

1. Enable the pipeline flag in `src/user_config.h`:
   - Set `#define ENABLE_NEW_COLOR_PIPELINE 1` (src/user_config.h:25)
2. Build and flash:
   - `platformio run --target upload`
3. Capture parity evidence:
   - Record the same scene twice: palette index 0 (HSV) and a calibrated palette (>0).
   - Save raw videos/dumps under `parity/` and brief notes in `SESSION_NOTES.md`.
4. Record brightness headroom:
   - Use the existing logging hook or a temporary `USBSerial.printf` around `leds_16` after `apply_master_brightness()`.
5. Record size budget output:
   - `python3 scripts/verify_size_budget.py --baseline ci/size-baseline.json > docs/08.artifacts/retrofit/phase-1/size_check.txt`.
6. Commit artefacts and link them in the PR.

## Notes

- Keep the flag guarded; do not touch palette code paths yet (Phase 2).
- If parity visibly drifts, capture and annotate with frame/timecodes; we will adjust gamma constants in Phase 3 tuning.
- Ensure the PR template checklist is satisfied before requesting review.

