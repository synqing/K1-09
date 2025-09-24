K1‑09 Colour Refit Plan

Doc version: 2025-09-24
Baseline commit: 59aeb27

Notes
- This file was normalized from `docs/K1-09_color_refit_plan.md`.
- Source touchpoints for this plan:
  - `src/led_utilities.h:281` (HSV gamma‑aware path behind `ENABLE_NEW_COLOR_PIPELINE`)
  - `src/user_config.h:19` (feature flag default)
  - `src/palettes/palettes_bridge.h:1` (palette façade touchpoint for Phase 2)
  - `src/lightshow_modes.cpp:1` (mode implementations consume utilities)
- Artefacts live under `docs/08.artifacts/retrofit/phase-0` … `phase-4/`.

See SSOT docs for pipeline and scheduling details:
- `docs/03.ssot/firmware-pipelines-ssot.md`
- `docs/03.ssot/firmware-scheduling-ssot.md`
- `docs/03.ssot/firmware-magic-numbers-ssot.md`
