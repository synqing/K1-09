# Dependency Risk Assessment – Phase 3 Kickoff
Generated from `analysis/deps_report.json` (commit `fc42d99` when exported). Use this document to prioritise header splits and globals teardown.

## 1. Top-Risk Files (weighted fan-in/fan-out + symbol count)

| Rank | File | Risk Score | Incoming | Outgoing | Symbols | Notes |
|------|------|------------|----------|----------|---------|-------|
| 1 | `src/core/globals.cpp` | 120 | 0 | 2 | 118 | Dense global definitions; highest churn risk. Requires staged extraction plan. |
| 2 | `src/main.cpp` | 48 | 0 | 41 | 7 | Central include hub. Split orchestration from hardware init to reduce coupling. |
| 3 | `src/globals.h` | 31 | 17 | 14 | 0 | Primary dependency hotspot; 17 compilation units depend on it. Candidate for staged interface splits. |
| 4 | `src/constants.h` | 25 | 17 | 5 | 3 | Consumed by nearly every translation unit; verification required after each change. |
| 5 | `src/led_utilities.h` | 19 | 2 | 17 | 0 | High fan-out; should expose slimmer public surface for LED helpers. |
| 6 | `src/lightshow_modes.h` | 11 | 1 | 10 | 0 | Large header-only implementation. Queue for mechanical split (`header_split` template). |
| 7 | `src/encoders.h` | 11 | 0 | 11 | 0 | Controls HMI state machine; dependency pruning needed before additional hardware support. |
| 8 | `src/palettes/palette_luts_api.h` | 9 | 7 | 2 | 0 | Shared palette metadata; ensure API contract before modularization. |
| 9 | `src/debug/debug_manager.h` | 9 | 7 | 2 | 0 | Debug instrumentation crosscuts modules; isolate via forward declarations. |
| 10 | `src/palettes/palette_luts.cpp` | 9 | 0 | 8 | 1 | Heavy include stack due to palette tables. Consider generated data to reduce rebuild cost. |

_No cycles or duplicate symbol definitions were detected in this pass (see `analysis/deps_report.json`)._

## 2. Recommended Header Split Order
1. `src/lightshow_modes.h` – High fan-out, no symbol definitions; mechanical split reduces compile time and risk.
2. `src/led_utilities.h` – Prepare public vs private headers before introducing module boundary guardrails.
3. `src/globals.h` – Execute staged extraction (Audio, Visual, HMI, Comms) following Task-Master tickets.
4. `src/main.cpp` – After dependent headers slimmed, extract platform bootstrap into `core/system_init.*`.

## 3. Preconditions for Each Split
- Issue must reference `docs/firmware/pipelines.md` to certify no producer/consumer drift.
- Run `python tools/aggregate_init_scanner.py --mode=strict --roots src include lib` before staging.
- Build firmware (`pio run`) and capture hardware smoke results per `hardware_validation_checklist.md`.
- Attach analyzer snapshot (`analysis/deps_report.json`, `analysis/deps_graph.dot`) to PR description.

## 4. Next Steps
- Create Task-Master tickets for the first two header splits with validation checklist populated.
- Extend analyzer to tag modules (Audio, Visual, HMI, Systems) for reporting (Phase 3b).
- After each split, regenerate dependency artefacts and update this assessment.

_Last updated: 2025-09-22 19:23:57 ._
