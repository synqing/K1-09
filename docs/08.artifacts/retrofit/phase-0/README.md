# Phase 0 – Baseline Capture Checklist

> Owner: Colour retrofit strike team  
> Target hardware: K1-09 single-core build @ commit 59aeb27 (pre-retrofit)

## Artefacts to Collect

| Item | Description | Drop Here | Status |
|------|-------------|-----------|--------|
| HSV reference capture | Short video/LED dump using palette index 0 (HSV mode), representative audio input | `docs/08.artifacts/retrofit/phase-0/hsv_reference/` | ☐ |
| Palette reference capture | Same scene with palette index >0 (e.g. Calibrated palette 1) | `docs/08.artifacts/retrofit/phase-0/palette_reference/` | ☐ |
| Audio-reactive scene | Capture of spectrogram-driven mode (e.g. `light_mode_gdft`) | `docs/08.artifacts/retrofit/phase-0/audio_scene/` | ☐ |
| Coordinator dual-strip | Capture showing primary + secondary strips (anti-phase / hue detune) | `docs/08.artifacts/retrofit/phase-0/dual_strip/` | ☐ |
| Palette LUT dump | JSON/CSV export of `PALETTE_LUTS` (use profiler or quick script) | `docs/08.artifacts/retrofit/phase-0/palette_luts_before.json` | ☐ |
| Brightness metrics | Log containing min/max values of `leds_16` post `apply_master_brightness()` | `docs/08.artifacts/retrofit/phase-0/brightness_metrics.log` | ☐ |

## Suggested Procedure

1. **Build & flash baseline firmware** (tag `retrofit-baseline`):
   ```bash
   git checkout retrofit-baseline   # or commit 59aeb27
   platformio run --target upload
   ```
2. **Capture visual evidence** using your preferred method (camera, LED capture rig, or logic analyser). Dump raw files plus a short README noting the scene, palette index, and audio track used.
3. **Dump palette LUTs**: use the existing `palette_profiler.cpp` (or temporarily instrument `palette_luts.cpp`) to print the 256-entry CRGB16 table, then convert to JSON.
4. **Log brightness headroom**: temporarily enable the helper in `led_utilities` (`ENABLE_BASELINE_BRIGHTNESS_LOG` or manual patch) to record min/max brightness per frame while running the reference scene. Store the console output in `brightness_metrics.log`.
5. Mark each item complete above and link to the exact file(s) committed for traceability.

## Notes

- Keep raw footage uncompressed where possible. If file size is an issue, include both the raw dump (in external storage) and a pointer/link in this directory.
- Record the serial commands used (`palette`, `ptype`, `status`, etc.) inside a small `SESSION_NOTES.md` so reviewers can reproduce the setup.
- Do not change any colour-related flags for Phase 0 (e.g. keep `ENABLE_NEW_COLOR_PIPELINE` disabled). The goal is to snapshot current behaviour.

Once all boxes are ticked, push the artefacts and notify the team to proceed to Phase 1.

