Title: <type>(color): <scope> (phaseX)

Summary
- What changed and why (one paragraph). Link to K1-09_color_refit_plan.md section and phase.

Agents Involved
- [ ] Agent 1 — Core Implementation
- [ ] Agent 2 — Validation & Metrics
- [ ] Agent 3 — Integration & Docs

Checklist
- [ ] Build OK (`./agent_runner.sh pio run` attached)
- [ ] Scanner strict (if headers/interfaces touched) attached
- [ ] Deps report (if headers/interfaces touched) attached
- [ ] Artefacts attached under `docs/retrofit_artifacts/phaseX/`
- [ ] Metrics sample (once/second) conforms to `docs/metrics_and_logging.md`
- [ ] Hardware checklist updated (`docs/firmware/hardware_validation_checklist.md`)
- [ ] Pipelines doc updated if producers/consumers/ranges changed
- [ ] Guard flag defaults respected (`ENABLE_NEW_COLOR_PIPELINE` remains OFF unless approved)
- [ ] Co‑sign: approvals from at least two agents not authoring this PR

Phase Artefacts
- Phase 0: videos, METRICS sample, LUT JSON (before)
- Phase 1: HSV vs palette parity proof + toggles
- Phase 2: Mode list migrated + captures
- Phase 3: Dither/incandescent decision note + secondary parity
- Phase 4: Acceptance checklist proof + docs links

Risk & Rollback
- Impacted modules/files:
- Rollback plan: set guard flags to legacy; revert commits `<hashes>` if needed.

Reviewers
- CODEOWNERS for `src/led_utilities.h`, `src/palettes/palettes_bridge.h`, `src/lightshow_modes.*`
- Co‑authors: @agent1 @agent2 @agent3 (update per PR)
